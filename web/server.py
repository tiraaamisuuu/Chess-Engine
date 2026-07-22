#!/usr/bin/env python3
"""Local HTTP bridge between TiramisuChess engine profiles and its web UI."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import mimetypes
from pathlib import Path
import secrets
import signal
import threading
import webbrowser
from typing import Any
from urllib.parse import urlparse

import chess
import chess.engine


ROOT = Path(__file__).resolve().parents[1]
WEB_ROOT = ROOT / "web"
PIECE_ROOT = ROOT / "assets" / "pieces"
PROFILE_MANIFEST = ROOT / ".tools" / "engine-match" / "profiles.json"


def color_name(color: chess.Color | None) -> str | None:
    if color is None:
        return None
    return "white" if color == chess.WHITE else "black"


def parse_color(value: str) -> chess.Color:
    if value == "random":
        return secrets.choice((chess.WHITE, chess.BLACK))
    if value == "black":
        return chess.BLACK
    return chess.WHITE


def bounded_int(value: Any, minimum: int, maximum: int, fallback: int) -> int:
    try:
        return max(minimum, min(maximum, int(value)))
    except (TypeError, ValueError):
        return fallback


def slug(value: str) -> str:
    return "-".join(filter(None, "".join(
        character.lower() if character.isalnum() else " " for character in value
    ).split()))


@dataclass(frozen=True)
class EngineProfile:
    profile_id: str
    name: str
    detail: str
    kind: str
    role: str
    badge: str
    command: tuple[str, ...]
    eval_file: Path | None = None

    def serialize(self) -> dict[str, Any]:
        return {
            "id": self.profile_id,
            "name": self.name,
            "detail": self.detail,
            "kind": self.kind,
            "role": self.role,
            "badge": self.badge,
            "usesNnue": self.eval_file is not None,
            "available": Path(self.command[0]).is_file(),
        }


def find_built_engine(build: Path) -> Path | None:
    preferred = ("tiramisu-uci", "tiramisu-uci.exe", "gui", "gui.exe")
    candidates = [path for name in preferred for path in build.rglob(name) if path.is_file()]
    return candidates[0].resolve() if candidates else None


def revision_identity(profile_id: str) -> tuple[str, str]:
    if profile_id.startswith("baseline-v0"):
        return "legacy", "LEGACY · BASELINE"
    if profile_id.startswith("baseline-"):
        return "baseline", "REFERENCE · BASELINE"
    if profile_id.startswith("candidate-"):
        return "candidate", "CANDIDATE · COMMITTED"
    return "revision", "BUILT REVISION"


def discover_profiles(engine_path: Path, nnue_path: Path | None) -> list[EngineProfile]:
    profiles = [EngineProfile(
        "current-classical",
        "Development · Classical",
        "Live working-tree build · newest local code",
        "current",
        "development",
        "DEV · NEWEST",
        (str(engine_path), "--uci"),
    )]
    known_commands = {str(engine_path.resolve())}
    used_ids = {profiles[0].profile_id}

    if PROFILE_MANIFEST.is_file():
        try:
            manifest = json.loads(PROFILE_MANIFEST.read_text(encoding="utf-8"))
            entries = manifest.get("profiles", []) if isinstance(manifest, dict) else []
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                binary = Path(str(entry.get("path", ""))).expanduser().resolve()
                if not binary.is_file() or str(binary) in known_commands:
                    continue
                profile_id = slug(str(entry.get("id") or entry.get("name") or binary.stem))
                if not profile_id or profile_id in used_ids:
                    continue
                raw_arguments = entry.get("args", ["--uci"])
                arguments = tuple(str(value) for value in raw_arguments) \
                    if isinstance(raw_arguments, list) else ("--uci",)
                default_role, default_badge = revision_identity(profile_id)
                profiles.append(EngineProfile(
                    profile_id,
                    str(entry.get("name") or profile_id),
                    str(entry.get("detail") or "Built comparison revision"),
                    "revision",
                    str(entry.get("role") or default_role),
                    str(entry.get("badge") or default_badge),
                    (str(binary), *arguments),
                ))
                known_commands.add(str(binary))
                used_ids.add(profile_id)
        except (OSError, json.JSONDecodeError, TypeError):
            pass

    comparison_root = ROOT / ".tools" / "engine-match"
    if comparison_root.is_dir():
        for build in sorted(comparison_root.glob("*/build")):
            binary = find_built_engine(build)
            if binary is None or str(binary) in known_commands:
                continue
            label = build.parent.name
            profile_id = slug(label)
            if not profile_id or profile_id in used_ids:
                continue
            role, badge = revision_identity(profile_id)
            profiles.append(EngineProfile(
                profile_id,
                label.replace("-", " ").title(),
                "Built comparison revision",
                "revision",
                role,
                badge,
                (str(binary), "--uci"),
            ))
            known_commands.add(str(binary))
            used_ids.add(profile_id)

    networks: list[Path] = []
    if nnue_path:
        networks.append(nnue_path.expanduser().resolve())
    network_root = ROOT / "networks"
    if network_root.is_dir():
        networks.extend(sorted(network_root.rglob("*.nnue")))
    seen_networks: set[str] = set()
    for network in networks:
        resolved = str(network.resolve())
        if resolved in seen_networks or not network.is_file():
            continue
        seen_networks.add(resolved)
        profile_id = f"nnue-{slug(network.stem)}"
        suffix = 2
        while profile_id in used_ids:
            profile_id = f"nnue-{slug(network.stem)}-{suffix}"
            suffix += 1
        profiles.append(EngineProfile(
            profile_id,
            f"NNUE · {network.stem}",
            "Development engine with trained evaluation network",
            "nnue",
            "nnue",
            "NNUE · MODEL",
            (str(engine_path), "--uci"),
            network.resolve(),
        ))
        used_ids.add(profile_id)
    return profiles


class EngineSession:
    def __init__(self, profile: EngineProfile, threads: int, hash_mb: int):
        self.profile = profile
        self.engine: chess.engine.SimpleEngine | None = None
        self.engine_name = profile.name
        self.error: str | None = None
        try:
            self.engine = chess.engine.SimpleEngine.popen_uci(list(profile.command), timeout=10.0)
            self.engine_name = self.engine.id.get("name", profile.name)
            configuration: dict[str, Any] = {}
            if "Threads" in self.engine.options:
                configuration["Threads"] = max(1, threads)
            if "Hash" in self.engine.options:
                configuration["Hash"] = max(1, hash_mb)
            if profile.eval_file:
                if "EvalFile" not in self.engine.options:
                    raise RuntimeError(f"{profile.name} does not support an EvalFile option")
                configuration["EvalFile"] = str(profile.eval_file)
                if "Use NNUE" in self.engine.options:
                    configuration["Use NNUE"] = True
            if configuration:
                self.engine.configure(configuration)
        except (OSError, chess.engine.EngineError, TimeoutError, RuntimeError) as error:
            self.error = str(error)
            if self.engine:
                try:
                    self.engine.quit()
                except (OSError, chess.engine.EngineError, TimeoutError):
                    pass
            self.engine = None

    def close(self) -> None:
        if self.engine:
            try:
                self.engine.quit()
            except (OSError, chess.engine.EngineError, TimeoutError):
                pass
            self.engine = None


class GameService:
    def __init__(self, engine_path: Path, threads: int = 1, hash_mb: int = 256,
                 nnue_path: Path | None = None):
        self.lock = threading.RLock()
        discovered = discover_profiles(engine_path, nnue_path)
        self.profiles = {profile.profile_id: profile for profile in discovered}
        self.default_profile_id = discovered[0].profile_id
        self.threads = threads
        self.hash_mb = hash_mb
        self.sessions: dict[str, EngineSession] = {}
        self.board = chess.Board()
        self.start_fen = self.board.fen()
        self.mode = "pvc"
        self.player_color: chess.Color | None = chess.WHITE
        self.controllers: dict[chess.Color, str | None] = {
            chess.WHITE: None,
            chess.BLACK: self.default_profile_id,
        }
        self.engine_time_ms = 650
        self.analysis_time_ms = 180
        self.last_engine_info: dict[str, Any] = {}
        self.last_analysis: dict[str, Any] = {}
        self.game_tokens: dict[str, object] = {}
        self._session(self.default_profile_id)

    @property
    def engine(self) -> chess.engine.SimpleEngine | None:
        session = self.sessions.get(self.default_profile_id)
        return session.engine if session else None

    def _session(self, profile_id: str) -> EngineSession:
        if profile_id not in self.profiles:
            raise ValueError(f"Unknown engine profile: {profile_id}")
        if profile_id not in self.sessions:
            self.sessions[profile_id] = EngineSession(
                self.profiles[profile_id], self.threads, self.hash_mb
            )
        return self.sessions[profile_id]

    def _profile_id(self, value: Any) -> str:
        profile_id = str(value or self.default_profile_id)
        if profile_id not in self.profiles:
            raise ValueError(f"Unknown engine profile: {profile_id}")
        return profile_id

    def close(self) -> None:
        with self.lock:
            for session in self.sessions.values():
                session.close()
            self.sessions.clear()

    def new_game(self, payload: dict[str, Any]) -> dict[str, Any]:
        with self.lock:
            requested_mode = str(payload.get("mode", "pvc"))
            legacy_modes = {"engine": "pvc", "local": "pvp"}
            self.mode = legacy_modes.get(requested_mode, requested_mode)
            if self.mode not in {"pvp", "pvc", "cvc"}:
                self.mode = "pvc"

            if self.mode == "pvp":
                self.player_color = None
                self.controllers = {chess.WHITE: None, chess.BLACK: None}
            elif self.mode == "pvc":
                self.player_color = parse_color(str(payload.get("side", "white")))
                profile_id = self._profile_id(payload.get("engineProfile"))
                self.controllers = {
                    self.player_color: None,
                    not self.player_color: profile_id,
                }
            else:
                self.player_color = None
                self.controllers = {
                    chess.WHITE: self._profile_id(payload.get("whiteProfile")),
                    chess.BLACK: self._profile_id(payload.get("blackProfile")),
                }

            self.engine_time_ms = bounded_int(payload.get("engineTimeMs"), 50, 10_000, 650)
            self.analysis_time_ms = bounded_int(payload.get("analysisTimeMs"), 50, 2_000, 180)
            fen = str(payload.get("fen", "")).strip()
            try:
                self.board = chess.Board(fen) if fen else chess.Board()
            except ValueError as error:
                raise ValueError(f"Invalid FEN: {error}") from error
            self.start_fen = self.board.fen()
            self.last_engine_info = {}
            self.last_analysis = {}
            self.game_tokens = {}
            return self._serialize()

    def make_move(self, uci: str) -> dict[str, Any]:
        with self.lock:
            if self.board.is_game_over(claim_draw=True):
                raise ValueError("The game is already over")
            if self.controllers[self.board.turn] is not None:
                raise ValueError("Wait for the engine to move")
            try:
                move = chess.Move.from_uci(uci)
            except ValueError as error:
                raise ValueError("Malformed move") from error
            if move not in self.board.legal_moves:
                raise ValueError("Illegal move")
            self.board.push(move)
            self.last_analysis = {}
            return self._serialize()

    def engine_move(self) -> dict[str, Any]:
        with self.lock:
            profile_id = self.controllers[self.board.turn]
            if profile_id is None or self.board.is_game_over(claim_draw=True):
                return self._serialize()
            session = self._session(profile_id)
            if not session.engine:
                raise RuntimeError(session.error or f"{session.profile.name} is not connected")
            result = session.engine.play(
                self.board,
                chess.engine.Limit(time=self.engine_time_ms / 1000.0),
                game=self.game_tokens.setdefault(profile_id, object()),
                info=chess.engine.INFO_ALL,
            )
            if result.move is None or result.move not in self.board.legal_moves:
                raise RuntimeError(f"{session.profile.name} did not return a legal move")
            self.last_engine_info = self._engine_info(result.info, self.board)
            self.last_engine_info.update({
                "profileId": profile_id,
                "profileName": session.profile.name,
            })
            self.board.push(result.move)
            self.last_analysis = {}
            return self._serialize()

    def analyse(self) -> dict[str, Any]:
        with self.lock:
            if self.board.is_game_over(claim_draw=True):
                self.last_analysis = {}
                return self.last_analysis
            profile_id = self.controllers[self.board.turn]
            if profile_id is None and self.mode == "pvc":
                profile_id = self.controllers[not self.board.turn]
            profile_id = profile_id or self.default_profile_id
            session = self._session(profile_id)
            if not session.engine:
                raise RuntimeError(session.error or f"{session.profile.name} is not connected")
            information = session.engine.analyse(
                self.board,
                chess.engine.Limit(time=self.analysis_time_ms / 1000.0),
                game=self.game_tokens.setdefault(profile_id, object()),
                info=chess.engine.INFO_ALL,
            )
            analysis = self._engine_info(information, self.board)
            principal_variation = information.get("pv", [])[:10]
            replay = self.board.copy(stack=False)
            san_line: list[str] = []
            for move in principal_variation:
                if move not in replay.legal_moves:
                    break
                san_line.append(replay.san(move))
                replay.push(move)
            analysis.update({
                "profileId": profile_id,
                "profileName": session.profile.name,
                "pv": san_line,
                "pvUci": [move.uci() for move in principal_variation],
            })
            self.last_analysis = analysis
            return analysis

    def undo(self) -> dict[str, Any]:
        with self.lock:
            if not self.board.move_stack:
                return self._serialize()
            if (self.mode == "pvc" and self.player_color is not None
                    and self.board.turn == self.player_color and len(self.board.move_stack) == 1):
                return self._serialize()
            self.board.pop()
            if self.mode == "pvc" and self.player_color is not None:
                while self.board.move_stack and self.board.turn != self.player_color:
                    self.board.pop()
            self.last_engine_info = {}
            self.last_analysis = {}
            self.game_tokens = {}
            return self._serialize()

    def state(self) -> dict[str, Any]:
        with self.lock:
            return self._serialize()

    def _engine_info(self, information: dict[str, Any], board: chess.Board) -> dict[str, Any]:
        score = information.get("score")
        centipawns: int | None = None
        mate: int | None = None
        if score is not None:
            white_score = score.pov(chess.WHITE)
            mate = white_score.mate()
            centipawns = white_score.score(mate_score=100_000)
        return {
            "depth": information.get("depth"),
            "seldepth": information.get("seldepth"),
            "nodes": information.get("nodes"),
            "nps": information.get("nps"),
            "timeMs": round(float(information.get("time", 0.0)) * 1000),
            "scoreCp": centipawns,
            "mate": mate,
            "turn": color_name(board.turn),
        }

    def _move_history(self) -> list[dict[str, Any]]:
        replay = chess.Board(self.start_fen)
        rows: list[dict[str, Any]] = []
        for ply, move in enumerate(self.board.move_stack):
            san = replay.san(move)
            if ply % 2 == 0:
                rows.append({"number": ply // 2 + 1, "white": san, "black": ""})
            else:
                rows[-1]["black"] = san
            replay.push(move)
        return rows

    def _controller(self, color: chess.Color) -> dict[str, Any]:
        profile_id = self.controllers[color]
        if profile_id is None:
            if self.mode == "pvc":
                name = "You"
            else:
                name = color_name(color).title()
            return {"type": "human", "name": name, "profileId": None}
        profile = self.profiles[profile_id]
        return {
            "type": "engine",
            "name": profile.name,
            "profileId": profile_id,
            "kind": profile.kind,
            "role": profile.role,
            "badge": profile.badge,
            "detail": profile.detail,
        }

    def _serialize(self) -> dict[str, Any]:
        outcome = self.board.outcome(claim_draw=True)
        game_over = outcome is not None
        can_move = not game_over and self.controllers[self.board.turn] is None
        pieces = [{
            "square": chess.square_name(square),
            "color": color_name(piece.color),
            "type": chess.piece_name(piece.piece_type),
            "symbol": piece.symbol(),
        } for square, piece in self.board.piece_map().items()]
        legal_moves = []
        if can_move:
            for move in self.board.legal_moves:
                legal_moves.append({
                    "uci": move.uci(),
                    "from": chess.square_name(move.from_square),
                    "to": chess.square_name(move.to_square),
                    "promotion": chess.piece_name(move.promotion) if move.promotion else None,
                    "capture": self.board.is_capture(move),
                })

        last_move = self.board.peek().uci() if self.board.move_stack else None
        check_square = chess.square_name(self.board.king(self.board.turn)) if self.board.is_check() else None
        needs_engine = not game_over and self.controllers[self.board.turn] is not None
        if game_over:
            status = "Game over"
        elif needs_engine:
            status = f"{self.profiles[self.controllers[self.board.turn]].name} to move"
        elif self.mode == "pvc":
            status = "Your move"
        else:
            status = f"{color_name(self.board.turn).title()} to move"

        active_profile_id = self.controllers[self.board.turn]
        if active_profile_id is None and self.mode == "pvc":
            active_profile_id = self.controllers[not self.board.turn]
        active_profile_id = active_profile_id or self.default_profile_id
        active_profile = self.profiles[active_profile_id]
        active_session = self.sessions.get(active_profile_id)
        default_session = self.sessions.get(self.default_profile_id)
        result = outcome.result() if outcome else None
        reason = outcome.termination.name.replace("_", " ").title() if outcome else None
        return {
            "fen": self.board.fen(en_passant="fen"),
            "turn": color_name(self.board.turn),
            "mode": self.mode,
            "playerColor": color_name(self.player_color),
            "controllers": {
                "white": self._controller(chess.WHITE),
                "black": self._controller(chess.BLACK),
            },
            "profiles": [profile.serialize() for profile in self.profiles.values()],
            "canMove": can_move,
            "needsEngineMove": needs_engine,
            "gameOver": game_over,
            "result": result,
            "resultReason": reason,
            "status": status,
            "check": self.board.is_check(),
            "checkSquare": check_square,
            "lastMove": last_move,
            "ply": len(self.board.move_stack),
            "pieces": pieces,
            "legalMoves": legal_moves,
            "moves": self._move_history(),
            "engine": {
                "connected": bool((active_session and active_session.engine)
                                  or (default_session and default_session.engine)),
                "name": active_profile.name,
                "activeProfileId": active_profile_id,
                "activeProfileRole": active_profile.role,
                "activeProfileBadge": active_profile.badge,
                "profileCount": len(self.profiles),
                "moveTimeMs": self.engine_time_ms,
                "error": active_session.error if active_session else None,
                "lastMove": self.last_engine_info,
                "analysis": self.last_analysis,
            },
        }


class WebHandler(BaseHTTPRequestHandler):
    service: GameService
    server_version = "TiramisuWeb/2.0"

    def log_message(self, format_string: str, *args: object) -> None:
        print(f"[web] {self.address_string()} {format_string % args}")

    def do_GET(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        if path == "/api/health":
            self._json({"ok": True, "engineConnected": self.service.engine is not None})
            return
        if path == "/api/state":
            self._json(self.service.state())
            return
        self._static(path)

    def do_POST(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        try:
            payload = self._payload()
            if path == "/api/new":
                response = self.service.new_game(payload)
            elif path == "/api/move":
                response = self.service.make_move(str(payload.get("uci", "")))
            elif path == "/api/engine-move":
                response = self.service.engine_move()
            elif path == "/api/analyse":
                response = self.service.analyse()
            elif path == "/api/undo":
                response = self.service.undo()
            else:
                self._json({"error": "Unknown endpoint"}, HTTPStatus.NOT_FOUND)
                return
            self._json(response)
        except ValueError as error:
            self._json({"error": str(error)}, HTTPStatus.BAD_REQUEST)
        except RuntimeError as error:
            self._json({"error": str(error)}, HTTPStatus.SERVICE_UNAVAILABLE)
        except (chess.engine.EngineError, TimeoutError, OSError) as error:
            self._json({"error": f"Engine failure: {error}"}, HTTPStatus.SERVICE_UNAVAILABLE)

    def _payload(self) -> dict[str, Any]:
        length = bounded_int(self.headers.get("Content-Length"), 0, 64 * 1024, 0)
        if not length:
            return {}
        try:
            value = json.loads(self.rfile.read(length))
        except json.JSONDecodeError as error:
            raise ValueError("Invalid JSON request") from error
        if not isinstance(value, dict):
            raise ValueError("JSON request must be an object")
        return value

    def _json(self, payload: Any, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def _static(self, request_path: str) -> None:
        if request_path in {"/", "/index.html"}:
            path = WEB_ROOT / "index.html"
        elif request_path in {"/styles.css", "/app.js"}:
            path = WEB_ROOT / request_path.removeprefix("/")
        elif request_path.startswith("/assets/pieces/"):
            filename = Path(request_path).name
            path = PIECE_ROOT / filename
            if path.suffix != ".svg" or path.parent != PIECE_ROOT:
                self.send_error(HTTPStatus.NOT_FOUND)
                return
        else:
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        if not path.is_file():
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        body = path.read_bytes()
        content_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache" if path.suffix != ".svg" else "public, max-age=3600")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the local TiramisuChess web interface")
    parser.add_argument("--engine", type=Path, default=ROOT / "build" / "tiramisu-uci")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--hash", type=int, default=256, dest="hash_mb")
    parser.add_argument("--nnue", type=Path)
    parser.add_argument("--no-open", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    service = GameService(args.engine.expanduser().resolve(), args.threads, args.hash_mb, args.nnue)
    WebHandler.service = service
    server = ThreadingHTTPServer((args.host, args.port), WebHandler)
    url = f"http://{args.host}:{server.server_address[1]}"
    print(f"TiramisuChess web GUI: {url}", flush=True)
    default_session = service.sessions.get(service.default_profile_id)
    if default_session and default_session.error:
        print(f"Engine unavailable; PvP remains enabled: {default_session.error}", flush=True)
    if not args.no_open:
        threading.Timer(0.35, lambda: webbrowser.open(url)).start()

    def stop_server(_signum: int, _frame: object) -> None:
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGINT, stop_server)
    signal.signal(signal.SIGTERM, stop_server)
    try:
        server.serve_forever(poll_interval=0.2)
    finally:
        server.server_close()
        service.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
