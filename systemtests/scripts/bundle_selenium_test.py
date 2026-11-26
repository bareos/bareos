#!/usr/bin/env python3
#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

import argparse
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description="Build a standalone Selenium test.")
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = args.source.read_text(encoding="utf-8")
    helper = Path(__file__).with_name("selenium_screenshots.py")
    _, helper_source = helper.read_text(encoding="utf-8").split("\n", 1)
    bundled, count = re.subn(
        r"^# BEGIN source-only screenshot import\n.*?^# END source-only screenshot import\n",
        lambda _: f"# Embedded from {helper.name}\n{helper_source}\n",
        source,
        flags=re.MULTILINE | re.DOTALL,
    )
    if count != 1:
        parser.error(f"{args.source}: expected exactly one screenshot import block")
    args.output.write_text(bundled, encoding="utf-8")


if __name__ == "__main__":
    main()
