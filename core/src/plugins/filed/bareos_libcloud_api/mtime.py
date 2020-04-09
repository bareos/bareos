#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2020 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

import time
import dateutil
import dateutil.parser
import datetime


class TestObject:
    def __init__(self):
        self.extra = {}
        self.extra["last_modified"] = "2020-01-02 13:15:16"


class ModificationTime(object):
    def __init__(self):
        is_dst = time.daylight and time.localtime().tm_isdst
        self.timezone_delta = datetime.timedelta(
            seconds=time.altzone if is_dst else time.timezone
        )

    def get_mtime(self, obj):
        mtime = dateutil.parser.parse(obj.extra["last_modified"])
        mtime = mtime - self.timezone_delta
        mtime = mtime.replace(tzinfo=None)

        ts = time.mktime(mtime.timetuple())
        return mtime, ts

    def get_last_run(self):
        last_run = datetime.datetime(1970, 1, 1)
        return last_run.replace(tzinfo=None)


if __name__ == "__main__":
    m = ModificationTime()
    print("mtime: ", m.get_mtime(TestObject()))
    print("last run: ", m.get_last_run())
