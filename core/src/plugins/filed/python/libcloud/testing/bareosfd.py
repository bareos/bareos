#   BAREOS - Backup Archiving REcovery Open Sourced
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

# def __bootstrap__():
#   global __bootstrap__, __loader__, __file__
#   import sys, pkg_resources, imp
#   __file__ = pkg_resources.resource_filename(__name__,'bareosfd.so')
#   __loader__ = None; del __bootstrap__, __loader__
#   imp.load_dynamic(__name__,__file__)
# __bootstrap__()

M_ERROR = 1
M_INFO = 2


def DebugMessage(level, message):
    print("DEBUG : %d: %s" % (level, message))


def JobMessage(type, message):
    print("JOBMSG: %d: %s" % (type, message))
