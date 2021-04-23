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

from shutil import which
import logging
import subprocess

clang_format_exe = which("clang-format")
if clang_format_exe:
    logging.getLogger(__name__).info("using executable {}".format(clang_format_exe))
else:
    logging.getLogger(__name__).error("cannot find a clang-format executable")


def invoke_clang_format(source_text, *argv):
    invocation = [clang_format_exe] + list(argv)
    try:
        proc = subprocess.run(
            invocation,
            input=source_text,
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

    if proc.returncode:
        raise Exception(
            "Command '{}' returned non-zero exit status {}".format(
                subprocess.list2cmdline(invocation), proc.returncode
            ),
            proc.stderr,
        )
    return proc.stdout


if __name__ == "__main__":
    logging.getLogger().setLevel(logging.DEBUG)
    # print(clang_format_file(Path(sys.argv[1])))
    exit(0)


from ..registry import register_modifier


@register_modifier("*.c", "*.cc", "*.h", name="clang-format check")
def check_clang_format(file_path, file_content, **kwargs):
    return invoke_clang_format(
        file_content, "-style=file", "-assume-filename={}".format(file_path)
    )
