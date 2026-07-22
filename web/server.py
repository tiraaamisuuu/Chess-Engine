#!/usr/bin/env python3
"""Local HTTP bridge between the TiramisuChess UCI engine and its web UI."""

from __future__ import annotations

import argparse
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


class GameService:
    def __init__(self, engine_path: Path, threads: int = 1, hash_mb: int = 256,
                 nnue_path: Path | None = None):
        self.lock = threading.RLock()
        self.board = chess.Board()
        self.start_fen = self.board.fen()
        self.mode = "engine"
        self.player_color: chess.Color | None = chess.WHITE
        self.engine_time_ms = 650
        self.analysis_time_ms = 180
        self.last_engine_info: dict[str, Any] = {}
        self.last_analysis: dict[str, Any] = {}
        self.game_token: object = object()
        self.engine: chess.engine.SimpleEngine | None = None
        self.engine_name = "TiramisuChess"
        self.engine_error: str | None = None
        self.engine_path = engine_path

        try:
            self.engine = chess.engine.SimpleEngine.popen_uci(str(engine_path), timeout=10.0)
            self.engine_name = self.engine.id.get("name", self.engine_name)
            configuration: dict[str, Any] = {}
            if "Threads" in self.engine.options:
                configuration["Threads"] = max(1, threads)
            if "Hash" in self.engine.options:
                configuration["Hash"] = max(1, hash_mb)
            if nnue_path:
                if "EvalFile" in self.engine.options:
                    configuration["EvalFile"] = str(nnue_path.resolve())
                if "Use NNUE" in self.engine.options:
                    configuration["Use NNUE"] = True
            if configuration:
                self.engine.configure(configuration)
        except (OSError, chess.engine.EngineError, TimeoutError) as error:
            self.engine_error = str(error)
            self.engine = None

    def close(self) -> None:
        with self.lock:
            if self.engine:
                try:
                    self.engine.quit()
                except (OSError, chess.engine.EngineError, TimeoutError):
                    pass
                self.engine = None

    def new_game(self, payload: dict[str, Any]) -> dict[str, Any]:
        with self.lock:
            requested_mode = str(payload.get("mode", "engine"))
            self.mode = requested_mode if requested_mode in {"engine", "local"} else "engine"
            self.player_color = None if self.mode == "local" else parse_color(str(payload.get("side", "white")))
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
            self.game_token = object()
            return self._serialize()

    def make_move(self, uci: str) -> dict[str, Any]:
        with self.lock:
            if self.board.is_game_over(claim_draw=True):
                raise ValueError("The game is already over")
            if self.mode == "engine" and self.player_color != self.board.turn:
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
            if self.mode != "engine" or self.player_color == self.board.turn:
                return self._serialize()
            if self.board.is_game_over(claim_draw=True):
                return self._serialize()
            if not self.engine:
                raise RuntimeError(self.engine_error or "The engine is not connected")

            result = self.engine.play(
                self.board,
                chess.engine.Limit(time=self.engine_time_ms / 1000.0),
                game=self.game_token,
                info=chess.engine.INFO_ALL,
            )
            if result.move is None or result.move not in self.board.legal_moves:
                raise RuntimeError("The engine did not return a legal move")
            self.last_engine_info = self._engine_info(result.info, self.board)
            self.board.push(result.move)
            self.last_analysis = {}
            return self._serialize()

    def analyse(self) -> dict[str, Any]:
        with self.lock:
            if self.board.is_game_over(claim_draw=True):
                self.last_analysis = {}
                return self.last_analysis
            if not self.engine:
                raise RuntimeError(self.engine_error or "The engine is not connected")
            information = self.engine.analyse(
                self.board,
                chess.engine.Limit(time=self.analysis_time_ms / 1000.0),
                game=self.game_token,
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
            analysis["pv"] = san_line
            analysis["pvUci"] = [move.uci() for move in principal_variation]
            self.last_analysis = analysis
            return analysis

    def undo(self) -> dict[str, Any]:
        with self.lock:
            if not self.board.move_stack:
                return self._serialize()
            # When playing Black, the sole first move belongs to the engine;
            # there is no human turn to undo yet.
            if (self.mode == "engine" and self.player_color is not None
                    and self.board.turn == self.player_color and len(self.board.move_stack) == 1):
                return self._serialize()
            self.board.pop()
            if self.mode == "engine" and self.player_color is not None:
                while self.board.move_stack and self.board.turn != self.player_color:
                    self.board.pop()
            self.last_engine_info = {}
            self.last_analysis = {}
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

    def _serialize(self) -> dict[str, Any]:
        outcome = self.board.outcome(claim_draw=True)
        game_over = outcome is not None
        can_move = not game_over and (self.mode == "local" or self.player_color == self.board.turn)
        pieces = []
        for square, piece in self.board.piece_map().items():
            pieces.append({
                "square": chess.square_name(square),
                "color": color_name(piece.color),
                "type": chess.piece_name(piece.piece_type),
                "symbol": piece.symbol(),
            })
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
        needs_engine = (
            self.mode == "engine" and not game_over and self.player_color is not None
            and self.board.turn != self.player_color
        )
        status = "Your move" if can_move else "Engine to move" if needs_engine else "Game over" if game_over else ""
        if self.mode == "local" and not game_over:
            status = f"{color_name(self.board.turn).title()} to move"

        result = outcome.result() if outcome else None
        reason = outcome.termination.name.replace("_", " ").title() if outcome else None
        return {
            "fen": self.board.fen(en_passant="fen"),
            "turn": color_name(self.board.turn),
            "mode": self.mode,
            "playerColor": color_name(self.player_color),
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
                "connected": self.engine is not None,
                "name": self.engine_name,
                "path": str(self.engine_path),
                "error": self.engine_error,
                "moveTimeMs": self.engine_time_ms,
                "lastMove": self.last_engine_info,
                "analysis": self.last_analysis,
            },
        }


class WebHandler(BaseHTTPRequestHandler):
    service: GameService
    server_version = "TiramisuWeb/1.0"

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
    if service.engine_error:
        print(f"Engine unavailable; local play remains enabled: {service.engine_error}", flush=True)
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
