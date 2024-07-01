#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

import re
import logging
from datetime import datetime
from ..registry import register_modifier, register_checker

logger = logging.getLogger(__name__)

COPYRIGHT_REGEX = re.compile(
    r"Copyright \((C|c)\) (20[0-9]{2})-(20[0-9]{2}) Bareos GmbH & Co. KG"
)
COPYRIGHT_FORMAT = "Copyright (C) {}-{} Bareos GmbH & Co. KG"


@register_checker(
    "*.c",
    "*.cc",
    "*.h",
    "*.inc",
    "*.cmake",
    "*.py",
    "*.sh",
    "*.php",
    "CMakeLists.txt",
    name="check copyright notice exists",
)
def check_copyright_notice(file_path, file_content, **kwargs):
    # do check .inc (c/c++), but skip ReST includes
    if file_path.match("*.rst.inc"):
        return True
    # the string is split, so we don't disable the check for this file
    if file_content.find("bareos-check-sources:" + "disable-copyright-check") >= 0:
        return True
    m = COPYRIGHT_REGEX.search(file_content)
    return m is not None


@register_modifier("*", name="fix copyright year")
def set_copyright_year(
    file_path, file_content, git_repo, file_history, blame_ignore_revs, **kwargs
):
    m = COPYRIGHT_REGEX.search(file_content)
    if m is None:
        return file_content

    start_year = int(m.group(2))
    end_year = int(m.group(3))

    if file_history.is_changed(file_path):
        change_year = datetime.now().year
        logger.debug(
            "Uncommitted changes in {}, setting year to {}".format(
                file_path, change_year
            )
        )
    else:
        commit = file_history.get_latest_commit(file_path, ignore=blame_ignore_revs)
        if commit:
            change_year = datetime.utcfromtimestamp(commit.committed_date).year
            logger.debug(
                "Latest commit for {} is {} from {}".format(
                    file_path, commit.hexsha, change_year
                )
            )
        else:
            # skip files without commit info
            return file_content

    if end_year != change_year:
        return COPYRIGHT_REGEX.sub(
            COPYRIGHT_FORMAT.format(start_year, change_year), file_content
        )
    else:
        return file_content
