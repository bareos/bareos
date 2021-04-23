#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

from difflib import unified_diff
from sys import stdout


def print_diff(a_content, b_content, *, file_path="unknown", plugin_name=None):
    diff_lines = list(
        unified_diff(
            a_content.splitlines(keepends=True),
            b_content.splitlines(keepends=True),
            fromfile="a/{}".format(file_path),
            tofile="b/{}".format(file_path),
            n=5,
        )
    )
    if stdout.isatty():
        header_start, header_end = "\033[1m", "\033[0m"
        diff_lines = color_diff(diff_lines)
    else:
        header_start, header_end = "", ""
    print(
        "{}diff -u a/{} b/{}\nplugin {}{}".format(
            header_start, file_path, file_path, plugin_name, header_end
        )
    )

    for line in diff_lines:
        print(line, end="")


def color_diff(lines):
    """Inject git-like colors into a unified diff."""
    for i, line in enumerate(lines):
        if line.startswith("+++") or line.startswith("---"):
            line = "\033[1m" + line + "\033[0m"  # bold, reset
        elif line.startswith("@@"):
            line = "\033[36m" + line + "\033[0m"  # cyan, reset
        elif line.startswith("+"):
            line = "\033[32m" + line + "\033[0m"  # green, reset
        elif line.startswith("-"):
            line = "\033[31m" + line + "\033[0m"  # red, reset
        lines[i] = line
    return lines
