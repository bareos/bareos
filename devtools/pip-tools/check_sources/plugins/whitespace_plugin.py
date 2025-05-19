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

from ..registry import register_modifier


@register_modifier("*", name="trailing spaces")
def strip_tailing_whitespace(file_path, file_content, **kwargs):
    _ = file_path
    _ = kwargs
    lines = []
    for line in file_content.split("\n"):
        # rst file use ".. " as comment marker.
        if line != ".. ":
            line.rstrip(" ")
        lines.append(line)
    return "\n".join(lines)


@register_modifier("*", name="trailing newlines")
def strip_tailing_newlines(file_path, file_content, **kwargs):
    _ = file_path
    _ = kwargs
    return file_content.rstrip("\n") + "\n"


@register_modifier("*", name="dos line-endings")
def fix_dos_lineendings(file_path, file_content, **kwargs):
    _ = kwargs
    if (
        file_path.match("*.bat")
        or file_path.match("*.cmd")
        or file_path.match("**/win32/**")
    ):
        return file_content
    if file_content.find("\r\n") > -1:
        return file_content.replace("\r\n", "\n")
    else:
        return file_content
