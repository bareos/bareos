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

import logging
from cmakelang.format.__main__ import get_config, process_file
from cmakelang.configuration import Configuration
from ..registry import register_modifier


def format_cmake_file(file_path, file_content):
    config_dict = get_config(file_path, None)
    cfg = Configuration(**config_dict)

    out_text, reflow_valid = process_file(cfg, file_content, None)

    # check if formatting was aborted
    if not reflow_valid:
        logging.info(
            "%s: contains long lines that cannot be split automatically", file_path
        )

    return out_text


@register_modifier("CMakeLists.txt", "*.cmake", name="cmake format")
def modify_cmake_format(file_path, file_content, **kwargs):
    _ = kwargs
    return format_cmake_file(file_path, file_content)
