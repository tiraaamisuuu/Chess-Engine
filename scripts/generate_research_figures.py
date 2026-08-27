#!/usr/bin/env python3
"""Generate the checked-in research summary figures from compact CSV data."""

from __future__ import annotations

import argparse
import csv
from html import escape
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "research" / "data"
DEFAULT_OUTPUT = ROOT / "docs" / "assets"


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fail instead of writing when a generated figure is missing or stale",
    )
    return parser.parse_args()


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))


def find(records: list[dict[str, str]], experiment_id: str) -> dict[str, str]:
    for record in records:
        if record["experiment_id"] == experiment_id:
            return record
    raise RuntimeError(f"missing research record: {experiment_id}")


def calibration_campaign(
    records: list[dict[str, str]], experiment_prefix: str
) -> list[dict[str, str]]:
    campaign = [
        record
        for record in records
        if record["experiment_id"].startswith(experiment_prefix)
    ]
    if not campaign:
        raise RuntimeError(f"missing calibration campaign: {experiment_prefix}")
    return campaign


def validate_matches(records: list[dict[str, str]]) -> None:
    for record in records:
        games = int(record["games"])
        wins = int(record["wins"])
        losses = int(record["losses"])
        draws = int(record["draws"])
        if wins + losses + draws != games:
            raise RuntimeError(f"inconsistent WDL total: {record['experiment_id']}")
        calculated_score = 100.0 * (wins + 0.5 * draws) / games
        if abs(calculated_score - float(record["score_pct"])) > 0.051:
            raise RuntimeError(f"inconsistent match score: {record['experiment_id']}")


def validate_nnue_runs(records: list[dict[str, str]]) -> None:
    for record in records:
        total = int(record["total_inputs"])
        seen = int(record["seen_inputs"])
        calculated_coverage = 100.0 * seen / total
        if abs(calculated_coverage - float(record["input_coverage_pct"])) > 0.011:
            raise RuntimeError(f"inconsistent feature coverage: {record['experiment_id']}")


def validate_calibration(records: list[dict[str, str]]) -> None:
    if len(records) < 2:
        raise RuntimeError("calibration requires at least two rungs")
    previous_rung = -1
    for record in records:
        rung = int(record["stockfish_uci_elo"])
        games = int(record["games"])
        wins = int(record["wins"])
        losses = int(record["losses"])
        draws = int(record["draws"])
        if rung <= previous_rung:
            raise RuntimeError("calibration rungs must be strictly increasing")
        if wins + losses + draws != games:
            raise RuntimeError(f"inconsistent calibration WDL: {rung}")
        calculated_score = 100.0 * (wins + 0.5 * draws) / games
        if abs(calculated_score - float(record["score_pct"])) > 0.051:
            raise RuntimeError(f"inconsistent calibration score: {rung}")
        anchored = rung + float(record["relative_elo"])
        if abs(anchored - float(record["anchored_point"])) > 0.051:
            raise RuntimeError(f"inconsistent anchored point: {rung}")
        previous_rung = rung


def calibration_crossing(records: list[dict[str, str]]) -> float:
    for lower, upper in zip(records, records[1:]):
        lower_score = float(lower["score_pct"])
        upper_score = float(upper["score_pct"])
        if (lower_score - 50.0) * (upper_score - 50.0) <= 0:
            lower_rung = float(lower["stockfish_uci_elo"])
            upper_rung = float(upper["stockfish_uci_elo"])
            return lower_rung + (
                (50.0 - lower_score)
                * (upper_rung - lower_rung)
                / (upper_score - lower_score)
            )
    raise RuntimeError("calibration results do not bracket a 50% crossing")


def elo_to_score(relative_elo: float) -> float:
    return 100.0 / (1.0 + math.pow(10.0, -relative_elo / 400.0))


def svg_document(title: str, description: str, body: str, height: int) -> str:
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}" role="img" aria-labelledby="title desc">
  <title id="title">{escape(title)}</title>
  <desc id="desc">{escape(description)}</desc>
  <style>
    .bg {{ fill: #0a0b0b; }}
    .frame {{ fill: none; stroke: #303332; stroke-width: 1; }}
    .title {{ fill: #f3f4f1; font: 500 30px ui-monospace, SFMono-Regular, Consolas, monospace; }}
    .sub {{ fill: #959a96; font: 400 16px ui-monospace, SFMono-Regular, Consolas, monospace; }}
    .label {{ fill: #f3f4f1; font: 500 17px ui-monospace, SFMono-Regular, Consolas, monospace; }}
    .small {{ fill: #a4a9a5; font: 400 14px ui-monospace, SFMono-Regular, Consolas, monospace; }}
    .value {{ fill: #f3f4f1; font: 500 25px ui-monospace, SFMono-Regular, Consolas, monospace; }}
    .win {{ fill: #9dce7a; }}
    .draw {{ fill: #6d736f; }}
    .loss {{ fill: #c77878; }}
    .neutral {{ fill: #c8ccc7; }}
    .accent {{ fill: #9dce7a; }}
    .warning {{ fill: #d7ae68; }}
    .threshold {{ stroke: #d7ae68; stroke-width: 2; stroke-dasharray: 6 5; }}
    .grid {{ stroke: #242725; stroke-width: 1; }}
    .curve {{ fill: none; stroke: #9dce7a; stroke-width: 3; }}
    .ci {{ stroke: #a4a9a5; stroke-width: 2; }}
    .point {{ fill: #9dce7a; stroke: #0a0b0b; stroke-width: 3; }}
  </style>
  <rect class="bg" width="1200" height="{height}" rx="8"/>
  <rect class="frame" x="0.5" y="0.5" width="1199" height="{height - 1}" rx="8"/>
{body}
</svg>
'''


def release_strength(record: dict[str, str]) -> str:
    games = int(record["games"])
    values = [
        ("win", "wins", int(record["wins"])),
        ("draw", "draws", int(record["draws"])),
        ("loss", "losses", int(record["losses"])),
    ]
    bar_x, bar_y, bar_width, bar_height = 70.0, 175.0, 1060.0, 92.0
    cursor = bar_x
    segments: list[str] = []
    for css_class, label, value in values:
        width = bar_width * value / games
        segments.append(
            f'  <rect class="{css_class}" x="{cursor:.2f}" y="{bar_y}" '
            f'width="{width:.2f}" height="{bar_height}"/>'
        )
        if width >= 42:
            segments.append(
                f'  <text class="label" x="{cursor + width / 2:.2f}" y="231" '
                f'text-anchor="middle">{value}</text>'
            )
        cursor += width

    legend_x = [70, 310, 550]
    legend = []
    for x, (css_class, label, value) in zip(legend_x, values):
        percent = 100.0 * value / games
        legend.extend(
            [
                f'  <rect class="{css_class}" x="{x}" y="304" width="14" height="14"/>',
                f'  <text class="small" x="{x + 24}" y="316">{label}  {value} · {percent:.1f}%</text>',
            ]
        )

    body = "\n".join(
        [
            '  <text class="title" x="70" y="67">Forklift v1 release-strength baseline</text>',
            '  <text class="sub" x="70" y="99">400 paired games · 10+0.1 · one thread · reversed openings</text>',
            f'  <text class="value" x="1130" y="67" text-anchor="end">{record["score_pct"]}% score</text>',
            f'  <text class="sub" x="1130" y="99" text-anchor="end">{float(record["relative_elo"]):+.1f} ± {record["elo_uncertainty"]} relative Elo</text>',
            *segments,
            *legend,
            '  <text class="small" x="70" y="375">Opponent: v0.4.0 · zero crashes, illegal moves, disconnects or time forfeits</text>',
            f'  <text class="small" x="1130" y="375" text-anchor="end">source · {escape(record["source"])}</text>',
        ]
    )
    return svg_document(
        "Forklift v1 release-strength baseline",
        "A stacked bar showing 299 wins, 83 draws, and 18 losses in 400 paired games against v0.4.0, corresponding to an 85.1 percent score and plus 303 relative Elo.",
        body,
        420,
    )


def nnue_baseline(
    nnue: dict[str, str],
    original_match: dict[str, str],
    wdl_screen: dict[str, str],
) -> str:
    metrics = [
        (("Dataset feature", "coverage"), float(nnue["input_coverage_pct"]), "neutral", False),
        (("WDL validation", "sign accuracy"), float(nnue["sign_accuracy_pct"]), "accent", False),
        (("WDL 40-game", "screen score"), float(wdl_screen["score_pct"]), "warning", True),
        (("CP 400-game", "match score"), float(original_match["score_pct"]), "loss", True),
    ]
    chart_top, chart_bottom, max_height = 160.0, 390.0, 230.0
    bar_width = 155.0
    centers = [185.0, 455.0, 725.0, 995.0]
    marks: list[str] = []
    for (label_lines, value, css_class, has_threshold), center in zip(metrics, centers):
        height = max_height * value / 100.0
        x = center - bar_width / 2
        y = chart_bottom - height
        value_y = y + 31 if has_threshold else y - 13
        marks.extend(
            [
                f'  <rect class="frame" x="{x}" y="{chart_top}" width="{bar_width}" height="{max_height}"/>',
                f'  <rect class="{css_class}" x="{x}" y="{y:.2f}" width="{bar_width}" height="{height:.2f}"/>',
                f'  <text class="value" x="{center}" y="{value_y:.2f}" text-anchor="middle">{value:.2f}%</text>',
                f'  <text class="label" x="{center}" y="423" text-anchor="middle"><tspan x="{center}" dy="0">{escape(label_lines[0])}</tspan><tspan x="{center}" dy="22">{escape(label_lines[1])}</tspan></text>',
            ]
        )
        if has_threshold:
            threshold_y = chart_bottom - max_height * 0.5
            marks.append(
                f'  <line class="threshold" x1="{x - 8}" y1="{threshold_y}" x2="{x + bar_width + 8}" y2="{threshold_y}"/>'
            )

    marks.extend(
        [
            '  <line class="threshold" x1="929" y1="121" x2="955" y2="121"/>',
            '  <text class="small" x="965" y="126">match promotion boundary · 50%</text>',
        ]
    )
    body = "\n".join(
        [
            '  <text class="title" x="70" y="67">NNUE v1 · offline quality did not prove playing strength</text>',
            f'  <text class="sub" x="70" y="99">{int(nnue["train_positions"]):,} training positions · {int(nnue["validation_positions"]):,} held-out positions · distinct checkpoints and metrics</text>',
            *marks,
            '  <text class="small" x="70" y="494">Coverage describes the shared corpus. Accuracy belongs to the WDL candidate; the two match bars name their checkpoints and samples.</text>',
            '  <text class="small" x="70" y="522">The percentages are shown together to expose a research gap—not as interchangeable measures.</text>',
            f'  <text class="small" x="1130" y="556" text-anchor="end">source · {escape(nnue["source"])}</text>',
        ]
    )
    return svg_document(
        "NNUE v1 offline and playing metrics",
        "Four separate bars show 92.5 percent dataset feature coverage, 84.12 percent validation sign accuracy for a WDL candidate, its 43.8 percent 40-game screen score, and the original centipawn candidate's 32.5 percent 400-game match score.",
        body,
        590,
    )


def stockfish_calibration_curve(
    records: list[dict[str, str]],
    *,
    subtitle: str,
    headline_value: str,
    headline_label: str,
    footer_estimate: str,
    description: str,
    crossing: float | None,
) -> str:
    chart_left, chart_right = 105.0, 1130.0
    chart_top, chart_bottom = 150.0, 455.0
    rungs = [float(record["stockfish_uci_elo"]) for record in records]
    minimum, maximum = min(rungs) - 50.0, max(rungs) + 50.0

    def x(value: float) -> float:
        return chart_left + (value - minimum) * (chart_right - chart_left) / (maximum - minimum)

    def y(value: float) -> float:
        return chart_bottom - value * (chart_bottom - chart_top) / 100.0

    marks: list[str] = []
    for tick in (0, 25, 50, 75, 100):
        tick_y = y(float(tick))
        css_class = "threshold" if tick == 50 else "grid"
        marks.extend(
            [
                f'  <line class="{css_class}" x1="{chart_left}" y1="{tick_y:.2f}" x2="{chart_right}" y2="{tick_y:.2f}"/>',
                f'  <text class="small" x="88" y="{tick_y + 5:.2f}" text-anchor="end">{tick}%</text>',
            ]
        )

    points: list[tuple[float, float]] = []
    for record in records:
        rung = float(record["stockfish_uci_elo"])
        score = float(record["score_pct"])
        relative = float(record["relative_elo"])
        uncertainty = float(record["elo_uncertainty"])
        point_x, point_y = x(rung), y(score)
        low_y = y(elo_to_score(relative - uncertainty))
        high_y = y(elo_to_score(relative + uncertainty))
        points.append((point_x, point_y))
        marks.extend(
            [
                f'  <line class="ci" x1="{point_x:.2f}" y1="{high_y:.2f}" x2="{point_x:.2f}" y2="{low_y:.2f}"/>',
                f'  <line class="ci" x1="{point_x - 7:.2f}" y1="{high_y:.2f}" x2="{point_x + 7:.2f}" y2="{high_y:.2f}"/>',
                f'  <line class="ci" x1="{point_x - 7:.2f}" y1="{low_y:.2f}" x2="{point_x + 7:.2f}" y2="{low_y:.2f}"/>',
                f'  <text class="small" x="{point_x:.2f}" y="480" text-anchor="middle">{int(rung)}</text>',
            ]
        )

    path = " ".join(
        ("M" if index == 0 else "L") + f" {point_x:.2f} {point_y:.2f}"
        for index, (point_x, point_y) in enumerate(points)
    )
    marks.append(f'  <path class="curve" d="{path}"/>')
    for record, (point_x, point_y) in zip(records, points):
        score = float(record["score_pct"])
        label_y = point_y - 14
        marks.extend(
            [
                f'  <circle class="point" cx="{point_x:.2f}" cy="{point_y:.2f}" r="7"/>',
                f'  <text class="label" x="{point_x:.2f}" y="{label_y:.2f}" text-anchor="middle">{score:.1f}%</text>',
            ]
        )

    if crossing is not None:
        crossing_x = x(crossing)
        marks.append(
            f'  <line class="threshold" x1="{crossing_x:.2f}" y1="{y(50):.2f}" x2="{crossing_x:.2f}" y2="{chart_bottom}"/>'
        )
    marks.append(
        f'  <text class="small" x="{(chart_left + chart_right) / 2:.2f}" y="515" text-anchor="middle">Stockfish 18 configured UCI_Elo</text>'
    )
    body = "\n".join(
        [
            '  <text class="title" x="70" y="67">Forklift · Stockfish limited-strength calibration</text>',
            f'  <text class="sub" x="70" y="99">{escape(subtitle)}</text>',
            f'  <text class="value" x="1130" y="67" text-anchor="end">{escape(headline_value)}</text>',
            f'  <text class="sub" x="1130" y="99" text-anchor="end">{escape(headline_label)}</text>',
            *marks,
            '  <text class="small" x="70" y="556">Whiskers transform Cute Chess relative-Elo uncertainty into score bounds.</text>',
            f'  <text class="small" x="1130" y="556" text-anchor="end">{escape(footer_estimate)}</text>',
            '  <text class="small" x="70" y="584">This is a hardware-, opening- and time-control-specific Stockfish anchor—not FIDE, Chess.com or universal Elo.</text>',
        ]
    )
    return svg_document(
        "Forklift Stockfish calibration curve",
        description,
        body,
        620,
    )


def stockfish_calibration_wdl(
    records: list[dict[str, str]],
    *,
    subtitle: str,
    footer: str,
    description: str,
) -> str:
    bar_x, bar_width, bar_height = 210.0, 690.0, 30.0
    rows_svg: list[str] = []
    for index, record in enumerate(records):
        rung = int(record["stockfish_uci_elo"])
        games = int(record["games"])
        wins = int(record["wins"])
        draws = int(record["draws"])
        losses = int(record["losses"])
        score = float(record["score_pct"])
        row_y = 145.0 + index * 66.0
        cursor = bar_x
        for css_class, value in (("win", wins), ("draw", draws), ("loss", losses)):
            width = bar_width * value / games
            rows_svg.append(
                f'  <rect class="{css_class}" x="{cursor:.2f}" y="{row_y:.2f}" width="{width:.2f}" height="{bar_height}"/>'
            )
            cursor += width
        rows_svg.extend(
            [
                f'  <text class="label" x="180" y="{row_y + 21:.2f}" text-anchor="end">{rung}</text>',
                f'  <text class="small" x="930" y="{row_y + 13:.2f}">W {wins} · D {draws} · L {losses}</text>',
                f'  <text class="small" x="930" y="{row_y + 32:.2f}">{score:.1f}% score</text>',
            ]
        )

    footer_y = 145.0 + len(records) * 66.0 + 22.0
    body = "\n".join(
        [
            '  <text class="title" x="70" y="67">Calibration outcomes by Stockfish rung</text>',
            f'  <text class="sub" x="70" y="99">{escape(subtitle)}</text>',
            '  <rect class="win" x="781" y="58" width="14" height="14"/><text class="small" x="805" y="70">wins</text>',
            '  <rect class="draw" x="866" y="58" width="14" height="14"/><text class="small" x="890" y="70">draws</text>',
            '  <rect class="loss" x="963" y="58" width="14" height="14"/><text class="small" x="987" y="70">losses</text>',
            *rows_svg,
            f'  <text class="small" x="70" y="{footer_y:.0f}">No crashes, time forfeits, illegal moves or disconnects.</text>',
            f'  <text class="small" x="1130" y="{footer_y:.0f}" text-anchor="end">{escape(footer)}</text>',
        ]
    )
    return svg_document(
        "Forklift calibration outcomes by Stockfish rung",
        description,
        body,
        int(footer_y + 37),
    )


def main() -> int:
    args = arguments()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    matches = rows(DATA_DIR / "matches.csv")
    nnue_runs = rows(DATA_DIR / "nnue_runs.csv")
    calibration = rows(DATA_DIR / "stockfish_calibration.csv")
    validate_matches(matches)
    validate_nnue_runs(nnue_runs)
    fast_calibration = calibration_campaign(calibration, "absolute-calibration-main-")
    slow_calibration = calibration_campaign(calibration, "absolute-calibration-slow-")
    validate_calibration(fast_calibration)
    validate_calibration(slow_calibration)
    release = find(matches, "v1-vs-v0.4.0")
    nnue_match = find(matches, "nnue-halfkp-v1-vs-classical")
    wdl_screen = find(matches, "nnue-wdl-result0-vs-classical-screen")
    nnue_run = find(nnue_runs, "five-million-halfkp-v1-wdl")

    outputs = {
        "release-strength.svg": release_strength(release),
        "nnue-research-baseline.svg": nnue_baseline(
            nnue_run, nnue_match, wdl_screen
        ),
        "stockfish-calibration.svg": stockfish_calibration_curve(
            fast_calibration,
            subtitle="1,200 games · 200 per rung · 10+0.1 · one thread per engine · paired openings",
            headline_value=f"≈ {calibration_crossing(fast_calibration):.1f}",
            headline_label="local 50% crossing",
            footer_estimate=f"local pool estimate · ≈ {calibration_crossing(fast_calibration):.1f}",
            description="Forklift scored 56.5 percent against Stockfish UCI Elo 2200 and 45.8 percent against 2400, placing the interpolated 50 percent crossing near 2321.5 in this local test pool.",
            crossing=calibration_crossing(fast_calibration),
        ),
        "stockfish-calibration-wdl.svg": stockfish_calibration_wdl(
            fast_calibration,
            subtitle="Forklift perspective · 1,200 games · 200 per rung · 10+0.1",
            footer="commit · beb0571",
            description="Six stacked horizontal bars show Forklift wins, draws and losses in 200 games against each Stockfish limited-strength rung from 2200 through 3190.",
        ),
        "stockfish-calibration-slow.svg": stockfish_calibration_curve(
            slow_calibration,
            subtitle="2,400 games · 600 per rung · 30+0.3 · one thread per engine · paired openings",
            headline_value="2401.7 ± 26.4",
            headline_label="top-rung anchor · formal bracket still open",
            footer_estimate="tested boundary · above 2400",
            description="Forklift scored 50.2 percent over 600 games against Stockfish UCI Elo 2400 at 30+0.3, an anchored result of 2401.7 plus or minus 26.4 Elo; every tested rung remained above 50 percent, so the formal crossing was not bracketed.",
            crossing=None,
        ),
        "stockfish-calibration-slow-wdl.svg": stockfish_calibration_wdl(
            slow_calibration,
            subtitle="Forklift perspective · 2,400 games · 600 per rung · 30+0.3",
            footer="candidate · 071b4b6",
            description="Four stacked horizontal bars show Forklift wins, draws and losses in 600 games against each Stockfish limited-strength rung from 2250 through 2400 at 30+0.3.",
        ),
    }
    stale: list[Path] = []
    for name, content in outputs.items():
        path = output_dir / name
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                stale.append(path)
        else:
            path.write_text(content, encoding="utf-8", newline="\n")
            print(path)
    if stale:
        for path in stale:
            print(f"stale research figure: {path}")
        return 1
    if args.check:
        print("Research figures are current.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
