#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2022 Bareos GmbH & Co. KG
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
import magic

logger = logging.getLogger(__name__)


def file_is_ignored(file_path, ignore_list):
    check_path = file_path
    while check_path != check_path.parent:
        for pattern in ignore_list:
            if check_path.match(pattern):
                logger.debug(
                    "file {} matches ignore-pattern {}".format(file_path, pattern)
                )
                return True
        check_path = check_path.parent
    return False


def get_mimetype(file_path):
    return magic.from_file(str(file_path), mime=True)


def is_valid_textfile(file_path, ignorelist):
    """check if the file is a non-ignored textfile"""
    if not file_path.is_file():
        return False

    if file_path.is_symlink():
        return False

    if file_is_ignored(file_path, ignorelist):
        return False

    if file_path.stat().st_size == 0:
        return False

    return True


def get_stemmed_file_path(file_path):
    if file_path.suffix == ".in":
        return file_path.with_name(file_path.stem)
    else:
        return file_path
