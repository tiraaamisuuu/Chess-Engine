#!/usr/bin/env python3
"""End-to-end smoke test for the local web GUI and a real UCI engine."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
from urllib.error import URLError
from urllib.request import Request, urlopen


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

        state = request(base, "/api/move", {"uci": "e2e4"})
        assert state["ply"] == 1 and state["needsEngineMove"] is True
        state = request(base, "/api/engine-move", {})
        assert state["ply"] == 2 and state["canMove"] is True
        assert state["engine"]["lastMove"]["depth"] >= 1

        analysis = request(base, "/api/analyse", {})
        assert analysis["depth"] >= 1
        assert isinstance(analysis["pv"], list)

        state = request(base, "/api/undo", {})
        assert state["ply"] == 0
        assert state["fen"].startswith("rnbqkbnr/pppppppp/")

        state = request(base, "/api/new", {"mode": "engine", "side": "black", "engineTimeMs": 100})
        assert state["playerColor"] == "black" and state["needsEngineMove"] is True
        state = request(base, "/api/engine-move", {})
        assert state["ply"] == 1 and state["canMove"] is True
        state = request(base, "/api/undo", {})
        assert state["ply"] == 1, "the engine's opening move is not a human turn to undo"

        state = request(base, "/api/new", {"mode": "local", "side": "black"})
        assert state["mode"] == "local" and state["playerColor"] is None
        assert state["canMove"] is True
        with urlopen(base + "/", timeout=5) as response:
            assert b"TIRAMISU" in response.read()
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
