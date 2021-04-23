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

import logging
from pathlib import Path

from cmakelang.format.__main__ import get_config, process_file
from cmakelang.configuration import Configuration


def format_cmake_file(file_path, file_content):
    config_dict = get_config(file_path, None)
    cfg = Configuration(**config_dict)

    out_text, reflow_valid = process_file(cfg, file_content, None)

    # check if formatting was aborted
    if not reflow_valid:
        logging.info(
            "{}: contains long lines that cannot be split automatically".format(
                file_path
            )
        )

    return out_text


if __name__ == "__main__":
    import sys

    logging.getLogger().setLevel(logging.DEBUG)
    print(format_cmake_file(Path(sys.argv[1])))
    exit(0)


from ..registry import register_modifier


@register_modifier("CMakeLists.txt", "*.cmake", name="cmake format")
def modify_cmake_format(file_path, file_content, **kwargs):
    return format_cmake_file(file_path, file_content)
