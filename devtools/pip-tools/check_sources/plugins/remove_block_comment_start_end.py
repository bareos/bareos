#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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
 this plugin compresses /* and */ alone in one line for block comments so that
 they are merged into the second and the last line, respecrively.

 It compresses comments like

 /*
  * this is a block
  * comment
  */

to:

 /* this is a block
  * comment */


Comments that start in column one (file license info, function documentation)
should remain untouched

"""

from ..registry import register_modifier
import re

replace_regexp = re.compile(
    r"(?<!^)/\*+\n\s+\*([\s\S]*?)\n\s+\*/", flags=re.MULTILINE | re.DOTALL
)


@register_modifier("*.cc", "*.c", "*.h", name="compress c block comments")
def shrink_block_comment_start_end(file_path, file_content, **kwargs):
    return replace_regexp.sub("/*\\1 */", file_content)
