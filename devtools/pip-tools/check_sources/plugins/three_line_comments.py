#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2025 Bareos GmbH & Co. KG
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

"""
 compress comments like

 /*
  * This is a waste of space
  */

to:

  // This is a waste of space
"""

import re
from ..registry import register_modifier

replace_regexp = re.compile(r"\/\*\*?\n.*\*(.*)\n.*\*/", flags=re.MULTILINE)


@register_modifier("*.cc", "*.c", "*.h", name="compact three-line comments")
def shrink_three_line_comments(file_path, file_content, **kwargs):
    _ = file_path
    _ = kwargs
    return replace_regexp.sub("//\\1", file_content)
