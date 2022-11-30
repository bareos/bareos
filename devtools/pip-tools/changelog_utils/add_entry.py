#!/usr/bin/env python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2022 Bareos GmbH & Co. KG
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

from os.path import dirname, realpath, curdir
from sys import stderr
from argparse import ArgumentParser
from re import search, IGNORECASE
from .fileio import read_changelog_file, write_changelog_file

# top_dir = realpath(dirname(__file__) + "/..")
top_dir = curdir
section_order = ["Added", "Changed", "Deprecated", "Removed", "Fixed", "Security"]


def add_changelog_entry(lines, entry, section):

    if section not in section_order:
        raise KeyError(f"{section} is not a valid section name")

    try:
        next_section = section_order[section_order.index(section) + 1]
    except IndexError:
        next_section = "NO-SUCH-SECTION"

    in_unreleased = False
    insert_offset = None

    for idx, line in enumerate(lines):
        text = line.strip()
        if text == "## [Unreleased]":
            in_unreleased = True
        elif text.startswith("## ") or text.startswith("["):
            lines.insert(idx, "\n")
            lines.insert(idx, f"### {section}\n")
            insert_offset = idx + 1
            break
        elif text.startswith("### ") and in_unreleased:
            current_section = text.lstrip("# ")
            if current_section == section:
                while True:
                    idx += 1
                    if not lines[idx].startswith("- "):
                        insert_offset = idx
                        break
                break
            elif current_section == next_section:
                lines.insert(idx, "\n")
                lines.insert(idx, f"### {section}\n")
                insert_offset = idx + 1
                break

    if not insert_offset:
        print("No offset for insertion found", file=stderr)
        exit(1)

    lines.insert(insert_offset, f"- {entry}\n")


def have_pr_entry(lines, pr):
    for line in lines:
        if line.startswith("- ") and f"[PR #{pr}]" in line:
            return True


def guess_section(entry):
    if search("CVE-20[1-9][0-9]-[1-9][0-9]+", entry, flags=IGNORECASE):
        return "Security"
    elif search("^add(|ed|s) ", entry, flags=IGNORECASE):
        return "Added"
    elif search("(^fix(|ed|es) |issue #[1-9][0-9]+)", entry, flags=IGNORECASE):
        return "Fixed"
    elif search("^deprecate(|d|s) ", entry, flags=IGNORECASE):
        return "Deprecated"
    elif search("^remove(|d|s) ", entry, flags=IGNORECASE):
        return "Removed"
    else:
        return "Changed"


def parse_cmdline_args():
    parser = ArgumentParser()
    parser.add_argument(
        "--file",
        "-f",
        default="CHANGELOG.md",
        help="add entry for PR unless already present",
    )
    parser.add_argument("--pr", "-p", help="add entry for PR unless already present")
    parser.add_argument("--section", "-s", help="section to add the entry to")
    parser.add_argument("entry", help="the text you want to add")
    return parser.parse_args()


def add_entry_to_file(fp, entry, *, pr=None, section=None):
    if not section:
        section = guess_section(entry)
    lines = read_changelog_file(fp)
    if pr:
        if have_pr_entry(lines, pr):
            return False
        entry += f" [PR #{pr}]"
    add_changelog_entry(lines, entry, section)
    write_changelog_file(fp, lines)
    return True


def file_has_pr_entry(fp, pr):
    lines = read_changelog_file(fp)
    return have_pr_entry(lines, pr)


def main():
    args = parse_cmdline_args()
    changelog_file = f"{top_dir}/{args.file}"

    try:
        with open(changelog_file, "r+") as fp:
            add_entry_to_file(fp, args.entry, pr=args.pr, section=args.section)

    except FileNotFoundError:
        print(f"Could not find file {changelog_file}", file=stderr)
        exit(2)
