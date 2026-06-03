#!/usr/bin/env python3
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

from __future__ import annotations

"""Generate a support timeline SVG for Bareos major releases."""

import argparse
import json
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from pathlib import Path


SVG_WIDTH = 1120
SVG_LEFT = 220
SVG_RIGHT = 70
TIMELINE_WIDTH = SVG_WIDTH - SVG_LEFT - SVG_RIGHT
ROW_HEIGHT = 56
BAR_HEIGHT = 34
TOP_MARGIN = 90


@dataclass(frozen=True)
class MajorRelease:
    major: int
    release_date: date
    projected: bool = False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def add_years(value: date, years: int) -> date:
    try:
        return value.replace(year=value.year + years)
    except ValueError:
        return value.replace(month=2, day=28, year=value.year + years)


def parse_date(value: str) -> date:
    return datetime.strptime(value, "%Y-%m-%d").date()


def load_releases(path: Path) -> tuple[int, int, list[MajorRelease]]:
    payload = json.loads(path.read_text())
    releases = [
        MajorRelease(
            major=entry["major"],
            release_date=parse_date(entry["release_date"]),
        )
        for entry in payload["major_releases"]
    ]
    releases.sort(key=lambda release: release.release_date)
    return (
        payload["supported_major_releases"],
        payload["projected_major_release_interval_days"],
        releases,
    )


def extend_with_projections(
    releases: list[MajorRelease], window_end: date, interval_days: int
) -> list[MajorRelease]:
    if not releases:
        raise ValueError("major_releases must contain at least one release")
    if interval_days <= 0:
        raise ValueError("projected_major_release_interval_days must be > 0")

    projected_releases = list(releases)
    while True:
        latest = projected_releases[-1]
        next_release = MajorRelease(
            major=latest.major + 1,
            release_date=latest.release_date + timedelta(days=interval_days),
            projected=True,
        )
        if next_release.release_date > window_end:
            break
        projected_releases.append(next_release)
    return projected_releases


def x_position(moment: date, window_start: date, window_end: date) -> float:
    total_days = (window_end - window_start).days
    offset_days = (moment - window_start).days
    return round(offset_days / total_days * TIMELINE_WIDTH, 1)


def clamp_segment(
    start: date, end: date, window_start: date, window_end: date
) -> tuple[date, date] | None:
    clamped_start = max(start, window_start)
    clamped_end = min(end, window_end)
    if clamped_start >= clamped_end:
        return None
    return clamped_start, clamped_end


def tick_dates(window_start: date, window_end: date) -> list[date]:
    ticks = []
    year = window_start.year
    while True:
        for month in (1, 7):
            tick = date(year, month, 1)
            if tick <= window_start:
                continue
            if tick >= window_end:
                return ticks
            ticks.append(tick)
        year += 1


def tick_label(value: date) -> str:
    return value.strftime("%b %Y")


def release_lookup(releases: list[MajorRelease]) -> dict[int, MajorRelease]:
    return {release.major: release for release in releases}


def support_end_date(
    release: MajorRelease,
    lookup: dict[int, MajorRelease],
    supported_major_releases: int,
    window_end: date,
) -> date:
    support_end_release = lookup.get(release.major + supported_major_releases)
    return support_end_release.release_date if support_end_release else window_end


def build_rows(
    releases: list[MajorRelease],
    supported_major_releases: int,
    today: date,
    window_end: date,
) -> list[MajorRelease]:
    lookup = release_lookup(releases)
    supported_rows = []
    latest_unsupported_release = None
    next_projected_release = None
    for release in releases:
        support_end = support_end_date(
            release, lookup, supported_major_releases, window_end
        )
        if not release.projected and release.release_date <= today < support_end:
            supported_rows.append(release)
        elif not release.projected and support_end <= today:
            latest_unsupported_release = release
        if (
            next_projected_release is None
            and release.projected
            and release.release_date > today
        ):
            next_projected_release = release
    rows = supported_rows
    if latest_unsupported_release is not None:
        rows.insert(0, latest_unsupported_release)
    if next_projected_release is not None:
        rows.append(next_projected_release)
    return rows


def estimated_label_width(text: str, font_size: int) -> float:
    return len(text) * font_size * 0.45


def append_bar_label(
    parts: list[str],
    text_class: str,
    text_value: str,
    x: float,
    width: float,
    y: float,
) -> None:
    padding = 14
    font_size = 15 if text_class == "label" else 13
    if estimated_label_width(text_value, font_size) + padding * 2 > width:
        return
    parts.append(
        f'      <text class="{text_class}" x="{x + padding}" '
        f'y="{y}">{text_value}</text>\n'
    )


def svg_header(height: int, window_start: date, window_end: date) -> str:
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{SVG_WIDTH}" height="{height}" viewBox="0 0 {SVG_WIDTH} {height}"
data-window-start="{window_start.isoformat()}" data-window-end="{window_end.isoformat()}">
  <defs>
    <style type="text/css"><![CDATA[
      .bg {{ fill: #ffffff; }}
      .title {{ fill: #123047; font: 700 24px sans-serif; }}
      .tick text {{ fill: #7b8d9b; font: 14px sans-serif; text-anchor: middle; }}
      .row-label {{ fill: #4b5d6b; font: 16px sans-serif; text-anchor: end; dominant-baseline: middle; }}
      .grid {{ stroke: #dde6eb; stroke-width: 1; }}
      .axis {{ stroke: #96a9b5; stroke-width: 1.2; }}
      .development {{ fill: #e39a3b; }}
      .supported {{ fill: #1f8acb; }}
      .current {{ fill: #4f9f46; }}
      .unsupported {{ fill: #bcc7cf; }}
      .projected-supported {{ fill: #8cc8eb; stroke: #1f8acb; stroke-width: 1.2; stroke-dasharray: 5 4; }}
      .projected-current {{ fill: #9fd18b; stroke: #4f9f46; stroke-width: 1.2; stroke-dasharray: 5 4; }}
      .label {{ fill: #ffffff; font: 600 15px sans-serif; dominant-baseline: middle; }}
      .small-label {{ fill: #334854; font: 13px sans-serif; dominant-baseline: middle; }}
      .note {{ fill: #5c7285; font: 13px sans-serif; }}
    ]]></style>
  </defs>

"""


def render_timeline(
    actual_releases: list[MajorRelease],
    releases: list[MajorRelease],
    supported_major_releases: int,
    today: date,
    generated_at: datetime,
) -> str:
    window_start = add_years(today, -3)
    window_end = add_years(today, 3)
    rows = build_rows(releases, supported_major_releases, today, window_end)
    lookup = release_lookup(releases)
    timeline_height = 24 + ROW_HEIGHT * (len(rows) + 1)
    legend_y = TOP_MARGIN + timeline_height + 36
    footer_y = legend_y + 45
    svg_height = footer_y + 70

    parts = [svg_header(svg_height, window_start, window_end)]
    parts.append(
        f'  <rect class="bg" x="0" y="0" width="{SVG_WIDTH}" height="{svg_height}"/>\n'
    )
    parts.append(
        '  <text class="title" x="40" y="36">'
        "Bareos supported versions over time</text>\n"
    )
    parts.append(
        f'  <g id="timeline" transform="translate({SVG_LEFT},{TOP_MARGIN})">\n'
    )
    parts.append('    <g class="tick">\n')
    for tick in tick_dates(window_start, window_end):
        x = x_position(tick, window_start, window_end)
        dash = ' stroke-dasharray="3 3"' if tick.month == 7 else ""
        parts.append(
            f'      <line class="grid" x1="{x}" y1="0" x2="{x}" '
            f'y2="{timeline_height}"{dash}/>\n'
        )
        parts.append(f'      <text x="{x}" y="-12">{tick_label(tick)}</text>\n')
    parts.append("    </g>\n\n")

    parts.append(
        f'    <line class="axis" x1="0" y1="{timeline_height}" '
        f'x2="{TIMELINE_WIDTH}" y2="{timeline_height}"/>\n\n'
    )

    master_y = 24
    master_center = master_y + BAR_HEIGHT / 2
    parts.append("    <g>\n")
    parts.append(
        f'      <line class="grid" x1="0" y1="{master_y}" '
        f'x2="{TIMELINE_WIDTH}" y2="{master_y}"/>\n'
    )
    parts.append(
        f'      <text class="row-label" x="-14" y="{master_center}">master</text>\n'
    )
    parts.append(
        f'      <rect class="development" x="0" y="{master_y}" '
        f'width="{TIMELINE_WIDTH}" height="{BAR_HEIGHT}" rx="4"/>\n'
    )
    parts.append(
        f'      <text class="label" x="14" y="{master_center}">development</text>\n'
    )
    parts.append("    </g>\n\n")

    for index, release in enumerate(rows, start=1):
        row_y = 24 + ROW_HEIGHT * index
        row_center = row_y + BAR_HEIGHT / 2
        next_release = lookup.get(release.major + 1)

        current_end = next_release.release_date if next_release else window_end
        support_end = support_end_date(
            release, lookup, supported_major_releases, window_end
        )

        label = (
            f"Bareos {release.major}"
            if release.projected
            else f"Bareos {release.major}"
        )
        current_class = "projected-current" if release.projected else "current"
        supported_class = "projected-supported" if release.projected else "supported"

        parts.append("    <g>\n")
        parts.append(
            f'      <line class="grid" x1="0" y1="{row_y}" '
            f'x2="{TIMELINE_WIDTH}" y2="{row_y}"/>\n'
        )
        parts.append(
            f'      <text class="row-label" x="-14" y="{row_center}">{label}</text>\n'
        )

        current_segment = clamp_segment(
            release.release_date, current_end, window_start, window_end
        )
        if current_segment:
            current_x = x_position(current_segment[0], window_start, window_end)
            current_width = (
                x_position(current_segment[1], window_start, window_end) - current_x
            )
            parts.append(
                f'      <rect class="{current_class}" x="{current_x}" y="{row_y}" '
                f'width="{current_width}" height="{BAR_HEIGHT}" rx="4"/>\n'
            )
            text_class = "small-label" if release.projected else "label"
            text_value = "current" if release.projected else "current"
            append_bar_label(
                parts, text_class, text_value, current_x, current_width, row_center
            )

        supported_segment = clamp_segment(
            current_end, support_end, window_start, window_end
        )
        if supported_segment:
            supported_x = x_position(supported_segment[0], window_start, window_end)
            supported_width = (
                x_position(supported_segment[1], window_start, window_end) - supported_x
            )
            parts.append(
                f'      <rect class="{supported_class}" x="{supported_x}" '
                f'y="{row_y}" width="{supported_width}" '
                f'height="{BAR_HEIGHT}" rx="4"/>\n'
            )
            text_class = "small-label" if release.projected else "label"
            text_value = "supported" if release.projected else "supported"
            append_bar_label(
                parts,
                text_class,
                text_value,
                supported_x,
                supported_width,
                row_center,
            )

        unsupported_segment = clamp_segment(
            support_end, window_end, window_start, window_end
        )
        if unsupported_segment and not release.projected:
            unsupported_x = x_position(unsupported_segment[0], window_start, window_end)
            unsupported_width = (
                x_position(unsupported_segment[1], window_start, window_end)
                - unsupported_x
            )
            parts.append(
                f'      <rect class="unsupported" x="{unsupported_x}" '
                f'y="{row_y}" width="{unsupported_width}" '
                f'height="{BAR_HEIGHT}" rx="4"/>\n'
            )
            append_bar_label(
                parts,
                "small-label",
                "unsupported",
                unsupported_x,
                unsupported_width,
                row_center,
            )
        parts.append("    </g>\n\n")

    parts.append("  </g>\n\n")

    parts.append(f'  <g id="timeline-legend" transform="translate(40,{legend_y})">\n')
    parts.append(
        '    <rect class="development" x="0" y="0" width="18" height="18" rx="3"/>\n'
    )
    parts.append('    <text class="note" x="26" y="14">development branch</text>\n')
    parts.append(
        '    <rect class="supported" x="170" y="0" width="18" height="18" rx="3"/>\n'
    )
    parts.append(
        '    <text class="note" x="196" y="14">supported major release</text>\n'
    )
    parts.append(
        '    <rect class="current" x="405" y="0" width="18" height="18" rx="3"/>\n'
    )
    parts.append(
        '    <text class="note" x="431" y="14">newest supported major</text>\n'
    )
    parts.append(
        '    <rect class="projected-supported" x="662" y="0" width="18" height="18" rx="3"/>\n'
    )
    parts.append('    <text class="note" x="688" y="14">future state</text>\n')
    parts.append("  </g>\n\n")

    actual_summary = ", ".join(
        f"{release.major}.0.0 ({release.release_date.isoformat()})"
        for release in actual_releases
    )
    parts.append(f'  <text class="note" x="40" y="{footer_y}">\n')
    parts.append(
        "    Actual release starts from GitHub releases: " f"{actual_summary}.\n"
    )
    parts.append("  </text>\n")
    parts.append(f'  <text class="note" x="40" y="{footer_y + 22}">\n')
    parts.append(
        "    Future versions are projections based on the configured "
        "major-release interval.\n"
    )
    parts.append("  </text>\n")
    generated_label = generated_at.strftime("Generated: %Y-%m-%d %H:%M UTC")
    parts.append(
        f'  <text class="note" x="{SVG_WIDTH - 40}" y="{footer_y + 44}" '
        'text-anchor="end">\n'
    )
    parts.append(f"    {generated_label}\n")
    parts.append("  </text>\n")
    parts.append("</svg>\n")
    return "".join(parts)


def main() -> None:
    args = parse_args()
    today = date.today()
    generated_at = datetime.now(timezone.utc)
    supported_major_releases, interval_days, actual_releases = load_releases(args.input)
    releases = extend_with_projections(
        actual_releases, add_years(today, 3), interval_days
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        render_timeline(
            actual_releases,
            releases,
            supported_major_releases,
            today,
            generated_at,
        )
    )


if __name__ == "__main__":
    main()
