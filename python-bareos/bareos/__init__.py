#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2020 Bareos GmbH & Co. KG
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

# __all__ = [ "bconsole" ]

import os.path

try:
    base_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    base_dir = None
else:
    try:
        with open(os.path.join(base_dir, "VERSION.txt")) as version_file:
            __version__ = version_file.read().strip()
    except IOError:
        # Fallback version.
        # First protocol implemented
        # has been introduced with this version.
        __version__ = "18.2.5"

from bareos.exceptions import *
from bareos.util.password import Password
import bareos.util
import bareos.bsock
