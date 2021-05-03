#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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
import re
from ..registry import register_checker
from ..file_util import get_stemmed_file_path

logger = logging.getLogger(__name__)
comment_regexp = re.compile(r"(/\*.*?\*/|^\s*//.*?$)", flags=re.S | re.M)


def check_line(actual, expected):
    if actual != expected:
        logger.info("got line '{}', expected '{}'".format(actual, expected))
        return False
    return True


@register_checker("*.h", name="missing/wrong include guard")
def check_include_guard(file_path, file_content, **kwargs):
    # strip .in suffix if present
    file_path = get_stemmed_file_path(file_path)
    dir_parts = file_path.parent.parts
    if dir_parts[0] == "core" and dir_parts[1] == "src":
        preproc_sym = "BAREOS_{}_{}_H_".format(
            "_".join(dir_parts[2:]).upper(), file_path.stem.upper()
        ).replace("-", "_")
        lines = list(
            filter(
                lambda x: len(x) > 0, comment_regexp.sub("", file_content).split("\n")
            )
        )
        return (
            check_line(lines[0], "#ifndef {}".format(preproc_sym))
            and check_line(lines[1], "#define {}".format(preproc_sym))
            and check_line(lines[-1], "#endif  // {}".format(preproc_sym))
        )
    return True
