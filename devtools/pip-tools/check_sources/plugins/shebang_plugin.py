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

import os
from ..registry import register_checker


@register_checker("*", name="shebang in executables")
def check_shebang(file_path, file_content, **kwargs):
    _ = kwargs
    if os.access(file_path, os.X_OK):
        return file_content[0:2] == "#!"
    else:
        return True


@register_checker("*.py", name="python shebang")
def check_python_shebang(file_path, file_content, **kwargs):
    _ = kwargs
    if os.access(file_path, os.X_OK):
        return (
            file_content.startswith("#!/usr/bin/env python\n")
            or file_content.startswith("#!/usr/bin/env python2\n")
            or file_content.startswith("#!/usr/bin/env python3\n")
        )
    else:
        return True
