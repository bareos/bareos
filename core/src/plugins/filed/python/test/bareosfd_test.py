#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2026 Bareos GmbH & Co. KG
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

import unittest
import time
import types
import os
import sys

# for key, value in os.environ.items():
#    print(f'{key}: {value}')

print("PATH", os.environ["PATH"])
print("PYTHONPATH", os.environ["PYTHONPATH"])

import bareosfd

# print(dir(bareosfd))
print("bareosfd.iostat_error: ", bareosfd.iostat_error)
print("bareosfd.iostat_do_in_core: ", bareosfd.iostat_do_in_core)
print("bareosfd.iostat_do_in_plugin: ", bareosfd.iostat_do_in_plugin)
# print "bareosfd.bJobMessageType:", str( bareosfd.bJobMessageType)
# print "bareosfd.bVariable:", str( bareosfd.bVariable)
# print "bareosfd.bEventType:", str( bareosfd.bEventType)
# print "bareosfd.bFileType:", str( bareosfd.bFileType)
# print "bareosfd.CoreFunctions:", str( bareosfd.CoreFunctions)
# print "bareosfd.bIOPS:", str( bareosfd.bIOPS)
# print "bareosfd.bLevels:", str( bareosfd.bLevels)
# print "bareosfd.bRCs:", str( bareosfd.bRCs)
# print "bareosfd.bVariable:", str( bareosfd.bVariable)
# print "bareosfd.bcfs:", str( bareosfd.bcfs)


class TestBareosFd(unittest.TestCase):
    def test_StatPacket(self):
        print("test message", file=sys.stderr)
        timestamp_before = time.time()
        test_StatPacket = bareosfd.StatPacket()
        timestamp_after = time.time()

        # check that initialization uses a current timestamp while tolerating
        # scheduler jitter and coarse timer granularity on CI machines.
        for ts_value in (
            test_StatPacket.st_atime,
            test_StatPacket.st_mtime,
            test_StatPacket.st_ctime,
        ):
            # python time.time() uses CLOCK_REALTIME, while our code
            # uses time(NULL).  These can differ by a few milliseconds,
            # which can cause large differences (>1sec) because of rounding.
            # E.g. we can have
            #   timestamp_before = 1785830999.000088, and
            #   ts_value = 1785830998
            # Which are further apart than 1 second, even tough
            # time(NULL) probably just lagged behind CLOCK_REALTIME by ~90us.

            self.assertGreaterEqual(ts_value, timestamp_before - 2.0)
            self.assertLessEqual(ts_value, timestamp_after + 2.0)

        # set fixed values for comparison
        test_StatPacket.st_atime = 999
        test_StatPacket.st_mtime = 1000
        test_StatPacket.st_ctime = 1001
        self.assertEqual(
            "StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=18446744073709551615, atime=999, mtime=1000, ctime=1001, blksize=4096, blocks=1)",
            str(test_StatPacket),
        )
        sp2 = bareosfd.StatPacket(
            dev=0,
            ino=0,
            mode=0o0700,
            nlink=0,
            uid=0,
            gid=0,
            rdev=0,
            size=-1,
            atime=1,
            mtime=1,
            ctime=1,
            blksize=4096,
            blocks=1,
        )
        self.assertEqual(
            "StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=18446744073709551615, atime=1, mtime=1, ctime=1, blksize=4096, blocks=1)",
            str(sp2),
        )

    def test_SavePacket(self):
        test_SavePacket = bareosfd.SavePacket(fname="testfilename")
        self.assertEqual(
            'SavePacket(fname="testfilename", link="", type=0, flags=<NULL>, no_read=0, portable=0, accurate_found=0, cmd="(null)", save_time=0, delta_seq=0, object_name="", object="", object_len=0, object_index=0)',
            str(test_SavePacket),
        )


if __name__ == "__main__":
    unittest.main()
