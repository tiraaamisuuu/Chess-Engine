#!/usr/bin/env python3
"""Label sampled PGN positions with a UCI teacher for the engine's NNUE."""

from __future__ import annotations

import argparse
import json
import random
from pathlib import Path

import chess
import chess.engine
import chess.pgn


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, help="Path to a strong UCI teacher, normally Stockfish")
    parser.add_argument("--pgn", required=True, type=Path, nargs="+", help="Source PGN file(s)")
    parser.add_argument("--output", required=True, type=Path, help="Output JSONL shard")
    parser.add_argument("--nodes", type=int, default=20_000, help="Teacher nodes per sampled position")
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--hash", type=int, default=512, dest="hash_mb")
    parser.add_argument("--sample-rate", type=float, default=0.25)
    parser.add_argument("--min-ply", type=int, default=8)
    parser.add_argument("--max-ply", type=int, default=180)
    parser.add_argument("--max-positions", type=int, default=0, help="Zero means unlimited")
    parser.add_argument("--seed", type=int, default=1)
    return parser.parse_args()


def result_for_side_to_move(result: str, turn: chess.Color) -> float:
    white_result = {"1-0": 1.0, "0-1": -1.0, "1/2-1/2": 0.0}.get(result, 0.0)
    return white_result if turn == chess.WHITE else -white_result


def main() -> int:
    args = parse_args()
    if not 0.0 < args.sample_rate <= 1.0:
        raise SystemExit("--sample-rate must be in (0, 1]")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    random_generator = random.Random(args.seed)
    written = 0
    games = 0

    engine = chess.engine.SimpleEngine.popen_uci(args.engine)
    try:
        available = engine.options
        configuration: dict[str, int] = {}
        if "Threads" in available:
            configuration["Threads"] = args.threads
        if "Hash" in available:
            configuration["Hash"] = args.hash_mb
        if configuration:
            engine.configure(configuration)

        with args.output.open("w", encoding="utf-8") as destination:
            for pgn_path in args.pgn:
                with pgn_path.open("r", encoding="utf-8", errors="replace") as source:
                    while True:
                        game = chess.pgn.read_game(source)
                        if game is None:
                            break
                        games += 1
                        result = game.headers.get("Result", "*")
                        board = game.board()
                        for ply, move in enumerate(game.mainline_moves(), start=1):
                            board.push(move)
                            if ply < args.min_ply or ply > args.max_ply:
                                continue
                            if board.is_game_over(claim_draw=True) or random_generator.random() > args.sample_rate:
                                continue

                            analysis = engine.analyse(board, chess.engine.Limit(nodes=args.nodes))
                            score = analysis["score"].pov(board.turn).score(mate_score=32_000)
                            if score is None:
                                continue
                            record = {
                                "fen": board.fen(en_passant="fen"),
                                "score_cp": max(-32_000, min(32_000, int(score))),
                                "result": result_for_side_to_move(result, board.turn),
                            }
                            destination.write(json.dumps(record, separators=(",", ":")) + "\n")
                            written += 1
                            if written % 1000 == 0:
                                print(f"positions={written} games={games}", flush=True)
                            if args.max_positions and written >= args.max_positions:
                                print(f"completed positions={written} games={games}")
                                return 0
    finally:
        engine.quit()

    print(f"completed positions={written} games={games}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
