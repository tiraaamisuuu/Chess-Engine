#!/usr/bin/env python3
"""End-to-end smoke test for the local web GUI and a real UCI engine."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
from urllib.error import URLError
from urllib.request import Request, urlopen

import chess.pgn


ROOT = Path(__file__).resolve().parents[1]


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, type=Path)
    return parser.parse_args()


def free_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def request(base: str, path: str, payload: dict | None = None) -> dict:
    data = json.dumps(payload).encode("utf-8") if payload is not None else None
    call = Request(base + path, data=data, headers={"Content-Type": "application/json"})
    with urlopen(call, timeout=15) as response:
        return json.load(response)


def main() -> int:
    args = arguments()
    if not args.engine.is_file():
        raise SystemExit(f"engine not found: {args.engine}")
    port = free_port()
    base = f"http://127.0.0.1:{port}"
    server = subprocess.Popen(
        [sys.executable, str(ROOT / "web" / "server.py"), "--engine", str(args.engine),
         "--port", str(port), "--no-open"],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        deadline = time.monotonic() + 15
        while True:
            if server.poll() is not None:
                output = server.stdout.read() if server.stdout else ""
                raise RuntimeError(f"web server exited early\n{output}")
            try:
                health = request(base, "/api/health")
                break
            except URLError:
                if time.monotonic() >= deadline:
                    raise RuntimeError("web server did not become ready")
                time.sleep(0.05)

        assert health == {"ok": True, "engineConnected": True}
        state = request(base, "/api/state")
        assert len(state["pieces"]) == 32
        assert len(state["legalMoves"]) == 20
        assert state["canMove"] is True
        assert state["mode"] == "pvc"
        assert state["controllers"]["white"]["type"] == "human"
        assert state["controllers"]["black"]["type"] == "engine"
        assert state["profiles"] and state["profiles"][0]["id"] == "current-classical"
        assert state["profiles"][0]["role"] == "development"
        assert state["profiles"][0]["badge"] == "DEV · NEWEST"
        assert state["controllers"]["black"]["badge"] == "DEV · NEWEST"
        assert state["engine"]["activeProfileRole"] == "development"
        profile_id = state["profiles"][0]["id"]

        state = request(base, "/api/move", {"uci": "e2e4"})
        assert state["ply"] == 1 and state["needsEngineMove"] is True
        state = request(base, "/api/engine-move", {})
        assert state["ply"] == 2 and state["canMove"] is True
        assert state["engine"]["lastMove"]["depth"] >= 1

        analysis = request(base, "/api/analyse", {})
        assert analysis["depth"] >= 1
        assert isinstance(analysis["pv"], list)
        assert analysis["profileName"]

        exported = request(base, "/api/export", {
            "format": "pgn", "event": "Web Export Test", "white": "Alice", "black": "Engine",
        })
        assert exported["filename"].endswith(".pgn")
        game = chess.pgn.read_game(io.StringIO(exported["content"]))
        assert game is not None
        assert game.headers["Event"] == "Web Export Test"
        assert game.headers["White"] == "Alice"
        assert len(list(game.mainline_moves())) == 2

        fen_export = request(base, "/api/export", {"format": "fen"})
        assert fen_export["content"].strip() == state["fen"]
        json_export = request(base, "/api/export", {"format": "json"})
        game_log = json.loads(json_export["content"])
        assert len(game_log["moves"]) == 2
        assert game_log["currentFen"] == state["fen"]

        state = request(base, "/api/undo", {})
        assert state["ply"] == 0
        assert state["fen"].startswith("rnbqkbnr/pppppppp/")

        state = request(base, "/api/new", {
            "mode": "pvc", "side": "black", "engineProfile": profile_id,
            "engineTimeMs": 100,
        })
        assert state["playerColor"] == "black" and state["needsEngineMove"] is True
        state = request(base, "/api/engine-move", {})
        assert state["ply"] == 1 and state["canMove"] is True
        state = request(base, "/api/undo", {})
        assert state["ply"] == 1, "the engine's opening move is not a human turn to undo"

        state = request(base, "/api/new", {"mode": "pvp"})
        assert state["mode"] == "pvp" and state["playerColor"] is None
        assert state["controllers"]["white"]["type"] == "human"
        assert state["controllers"]["black"]["type"] == "human"
        assert state["canMove"] is True

        state = request(base, "/api/new", {
            "mode": "cvc", "whiteProfile": profile_id, "blackProfile": profile_id,
            "engineTimeMs": 100,
        })
        assert state["mode"] == "cvc" and state["canMove"] is False
        assert state["controllers"]["white"]["profileId"] == profile_id
        assert state["controllers"]["black"]["profileId"] == profile_id
        state = request(base, "/api/engine-move", {})
        assert state["ply"] == 1 and state["needsEngineMove"] is True
        state = request(base, "/api/engine-move", {})
        assert state["ply"] == 2 and state["needsEngineMove"] is True
        assert state["engine"]["lastMove"]["profileId"] == profile_id

        custom_fen = "8/8/8/8/8/8/4K3/R6k w - - 0 1"
        state = request(base, "/api/new", {"mode": "pvp", "fen": custom_fen})
        state = request(base, "/api/move", {"uci": "e2e3"})
        custom_export = request(base, "/api/export", {"format": "pgn"})
        custom_game = chess.pgn.read_game(io.StringIO(custom_export["content"]))
        assert custom_game is not None
        assert custom_game.headers["SetUp"] == "1"
        assert custom_game.headers["FEN"] == custom_fen
        assert [move.uci() for move in custom_game.mainline_moves()] == ["e2e3"]

        with urlopen(base + "/", timeout=5) as response:
            page = response.read()
            assert b"ENGINE ROOM" in page and b'value="cvc"' in page
            assert b"pvc-profile-summary" in page
            assert b"export-modal" in page and b"run-analysis" in page
        print("Web GUI smoke: PASS")
        return 0
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
