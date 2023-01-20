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
import re
from .fileio import read_changelog_file, write_changelog_file

number_re = re.compile(r"#([0-9]+)")
version_re = re.compile(r"([0-9]{1,2})\.([0-9]{1,2})\.([0-9]{1,2})")


def extend_numbers(text):
    """extend numbers in our texts so that it is sorted as with `sort -V`"""
    m = number_re.search(text)
    if m:
        repl = "#{:010d}".format(int(m.group(1)))
        return number_re.sub(repl, text)

    m = version_re.search(text)
    if m:
        repl = "{:02d}.{:02d}.{:02d}".format(
            int(m.group(1)), int(m.group(2)), int(m.group(3))
        )
        return version_re.sub(repl, text)

    return text


class ReferenceFilter:
    regex = re.compile(
        r"""
        (
        (?P<refline>^\[[^]]+\]:\ http)
        |(\[(
          (?P<unreleased>Unreleased)
          |(?P<release>(?P<releasenum>[0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2}))
          |(?P<issue>(Issue\ )?[#](?P<issuenum>[0-9]*))
          |(?P<pr>PR\ [#](?P<prnum>[0-9]*))
        )\])
        |(?P<link>\[[^]]+\]\(http[^)]+\))
        )
        """,
        re.VERBOSE,
    )

    def __init__(self):
        self.references = {}

    def __call__(self, line):
        for m in self.regex.finditer(line):
            if m.group("refline"):
                return False
            elif m.group("unreleased"):
                self.references[
                    "unreleased"
                ] = "https://github.com/bareos/bareos/tree/master"
            elif m.group("release"):
                self.references[
                    m.group("release")
                ] = "https://github.com/bareos/bareos/releases/tag/Release%2F{}".format(
                    m.group("releasenum")
                )
            elif m.group("issue"):
                self.references[
                    m.group("issue")
                ] = "https://bugs.bareos.org/view.php?id={}".format(m.group("issuenum"))
            elif m.group("pr"):
                self.references[
                    m.group("pr")
                ] = "https://github.com/bareos/bareos/pull/{}".format(m.group("prnum"))
        return True

    def get_reflines(self):
        reflines = []
        for k, v in self.references.items():
            reflines.append("[{}]: {}\n".format(k, v))
        reflines.sort(key=extend_numbers)
        return reflines


def update_links(fp):
    ref_filter = ReferenceFilter()
    lines = list(filter(ref_filter, read_changelog_file(fp)))
    write_changelog_file(fp, lines + ref_filter.get_reflines())


def parse_cmdline_args():
    parser = ArgumentParser(description="update links in CHANGELOG.md")
    parser.add_argument(
        "--file",
        "-f",
        default="CHANGELOG.md",
    )
    return parser.parse_args()


def main():
    args = parse_cmdline_args()

    try:
        with open(args.file, "r+") as fp:
            update_links(fp)

    except FileNotFoundError:
        print(f"Could not find file {args.file}", file=stderr)
        exit(2)
