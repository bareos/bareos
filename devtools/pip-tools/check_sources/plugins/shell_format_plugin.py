#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

from shutil import which
import logging
import subprocess
import re

shfmt_exe = which("shfmt")
if shfmt_exe:
    logging.getLogger(__name__).info("using executable {}".format(shfmt_exe))
else:
    logging.getLogger(__name__).error("cannot find a shfmt executable")


shebang_pattern = r"^#!\s*/(?:\S*/)*(?:bash|sh)\b"


def is_shell_content(file_content):
    return re.match(shebang_pattern, file_content) is not None


def invoke_shell_format(file_path, file_content, *argv):
    if not is_shell_content(file_content):
        return file_content

    invocation = [shfmt_exe] + list(argv) + [file_path]
    try:
        proc = subprocess.run(
            invocation,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding="utf-8",
            universal_newlines=True,
        )
    except OSError as exc:
        raise Exception(
            "Command '{}' failed to start: {}".format(
                subprocess.list2cmdline(invocation), exc
            )
        )
    return proc.stdout


if __name__ == "__main__":
    logging.getLogger().setLevel(logging.DEBUG)
    exit(0)


from ..registry import register_modifier


@register_modifier("*", name="shell-format check")
def check_shell_format(file_path, file_content, **kwargs):
    return invoke_shell_format(
        file_path,
        file_content,
        "--indent=2",
        "--binary-next-line",
        "--case-indent",
        "--func-next-line",
    )
