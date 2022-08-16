#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2022 Bareos GmbH & Co. KG
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

from __future__ import print_function
import unittest


def list_tests_from(path, **kwargs):
    loader = unittest.TestLoader()
    suite = loader.discover(path, **kwargs)
    for atest in suite:
        tests = atest._tests
        for atest in tests:
            for btest in atest._tests:
                btestname = btest.__str__().split()
                print(btestname[1][1:-1] + "." + btestname[0])


if __name__ == "__main__":
    # list_tests_from(".", pattern="*test*.py")
    list_tests_from(".")
