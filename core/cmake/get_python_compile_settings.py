#!/usr/bin/env python
# -*- coding: utf-8 -*-

#   BAREOS(R) - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation, which is
#   listed in the file LICENSE.
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

#  This program extracts the build configuration from the python interpreter.
#  The output is consumed by cmake where appropriate variables are set.
#  This is required to build python modules without setuptools.

import sys

try:
    import sysconfig
except:
    import distutils.sysconfig as sysconfig

for var in ("CC", "BLDSHARED"):
    value = sysconfig.get_config_var(var)
    print(
        'message(STATUS "Python{0}_{1} is  {2}")'.format(
            sys.version_info[0], var, value
        )
    )
    print('set(Python{0}_{1} "{2}")'.format(sys.version_info[0], var, value))

    # if nothing comes after the compile command, the flags are empty
    try:
        value = value.split(" ", 1)[1]
    except:
        value = ""
    # as these vars contain the compiler itself, we remove the first word and return it as _FLAGS
    print(
        'message(STATUS "Python{0}_{1}_FLAGS is  {2}")'.format(
            sys.version_info[0], var, value
        )
    )
    print('set(Python{0}_{1}_FLAGS "{2}")'.format(sys.version_info[0], var, value))

for var in ("CFLAGS", "CCSHARED", "INCLUDEPY", "LDFLAGS"):
    value = sysconfig.get_config_var(var)
    print(
        'message(STATUS "Python{0}_{1} is  {2}")'.format(
            sys.version_info[0], var, value
        )
    )
    print('set(Python{0}_{1} "{2}")'.format(sys.version_info[0], var, value))

for var in ("EXT_SUFFIX",):
    value = sysconfig.get_config_var(var)
    if value is None:
        value = ""
    print(
        'message(STATUS "Python{0}_{1} is  {2}")'.format(
            sys.version_info[0], var, value
        )
    )
    print('set(Python{0}_{1} "{2}")'.format(sys.version_info[0], var, value))
