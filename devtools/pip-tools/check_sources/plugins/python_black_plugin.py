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
from black import format_file_contents, Mode, NothingChanged, InvalidInput
from ..registry import register_modifier

logger = logging.getLogger(__name__)


@register_modifier("*.py", name="python black")
def format_python_black(file_path, file_content, **kwargs):
    _ = kwargs
    try:
        return format_file_contents(file_content, fast=False, mode=Mode())
    except NothingChanged:
        return file_content
    except InvalidInput:
        logger.warning("%s is not valid python code.", file_path)
        return file_content
