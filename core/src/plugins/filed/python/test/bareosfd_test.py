#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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
    def test_ModuleDicts(self):
        # help (bareosfd)
        print(bareosfd.bCFs)
        print(bareosfd.CF_ERROR)

    #     print bCFs
    #     bEventType
    #     bFileType
    #     CoreFunctions
    #     bIOPS
    #     bJobMessageType
    #     bLevels
    #     bRCs
    #     bVariable

    # def test_bJobMessageType(self):
    #     # bareosfd.DebugMessage( bareosfd.bJobMessageType['M_INFO'], "This is a Job message")
    #     self.assertEqual(str(bareosfd.bJobMessageType), """{'M_MOUNT': 10L, 'M_SECURITY': 14L, 'M_DEBUG': 2L, 'M_WARNING': 5L, 'M_SAVED': 7L, 'M_TERM': 12L, 'M_ABORT': 1L, 'M_INFO': 6L, 'M_ERROR': 4L, 'M_FATAL': 3L, 'M_NOTSAVED': 8L, 'M_RESTORED': 13L, 'M_ERROR_TERM': 11L, 'M_ALERT': 15L, 'M_VOLMGMT': 16L, 'M_SKIPPED': 9L}"""
    # )

    # def test_SetValue(self):
    #     self.assertRaises(RuntimeError, bareosfd.SetValue, 2)

    # def test_DebugMessage(self):
    #     self.assertRaises(RuntimeError, bareosfd.DebugMessage, 100, "This is a debug message")

    def test_RestoreObject(self):
        test_RestoreObject = bareosfd.RestoreObject()
        self.assertEqual(
            'RestoreObject(object_name="", object="", plugin_name="<NULL>", object_type=0, object_len=0, object_full_len=0, object_index=0, object_compression=0, stream=0, jobid=0)',
            str(test_RestoreObject),
        )
        r2 = bareosfd.RestoreObject()
        r2.object_name = "this is a very long object name"
        r2.object = "123456780"
        # r2.plugin_name="this is a plugin name"
        r2.object_type = 3
        r2.object_len = 111111
        r2.object_full_len = 11111111
        r2.object_index = 1234
        r2.object_compression = 1
        r2.stream = 4
        r2.jobid = 123123
        print(r2)
        # self.assertEqual(
        #   'RestoreObject(object_name="this is a very long object name", object="", plugin_name="<NULL>", object_type=3, object_len=111111, object_full_len=11111111, object_index=1234, object_compression=1, stream=4, jobid=123123)',
        #    str(test_RestoreObject),
        # )

    def test_StatPacket(self):
        timestamp = time.time()
        test_StatPacket = bareosfd.StatPacket()

        # check that the initialization of timestamps from current time stamp works
        self.assertAlmostEqual(test_StatPacket.st_atime, timestamp, delta=1.1)
        self.assertAlmostEqual(test_StatPacket.st_mtime, timestamp, delta=1.1)
        self.assertAlmostEqual(test_StatPacket.st_ctime, timestamp, delta=1.1)

        # set fixed values for comparison
        test_StatPacket.st_atime = 999
        test_StatPacket.st_mtime = 1000
        test_StatPacket.st_ctime = 1001
        self.assertEqual(
            "StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=-1, atime=999, mtime=1000, ctime=1001, blksize=4096, blocks=1)",
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
            "StatPacket(dev=0, ino=0, mode=0700, nlink=0, uid=0, gid=0, rdev=0, size=-1, atime=1, mtime=1, ctime=1, blksize=4096, blocks=1)",
            str(sp2),
        )

    def test_SavePacket(self):
        test_SavePacket = bareosfd.SavePacket(fname="testfilename")
        self.assertEqual(
            'SavePacket(fname="testfilename", link="", type=0, flags=<NULL>, no_read=0, portable=0, accurate_found=0, cmd="<NULL>", save_time=0, delta_seq=0, object_name="", object="", object_len=0, object_index=0)',
            str(test_SavePacket),
        )

    def test_RestorePacket(self):
        test_RestorePacket = bareosfd.RestorePacket()
        self.assertEqual(
            'RestorePacket(stream=0, data_stream=0, type=0, file_index=0, linkFI=0, uid=0, statp="<NULL>", attrEx="<NULL>", ofname="<NULL>", olname="<NULL>", where="<NULL>", RegexWhere="<NULL>", replace=0, create_status=0)',
            str(test_RestorePacket),
        )

    def test_IoPacket(self):
        test_IoPacket = bareosfd.IoPacket()
        self.assertEqual(
            'IoPacket(func=0, count=0, flags=0, mode=0000, buf="", fname="<NULL>", status=0, io_errno=0, lerror=0, whence=0, offset=0, win32=0, filedes=-1)',
            str(test_IoPacket),
        )

    def test_AclPacket(self):
        test_AclPacket = bareosfd.AclPacket()
        test_AclPacket.content = bytearray(b"Hello ACL")
        self.assertEqual(
            'AclPacket(fname="<NULL>", content="Hello ACL")', str(test_AclPacket)
        )

    def test_XattrPacket(self):
        test_XattrPacket = bareosfd.XattrPacket()
        self.assertEqual(
            'XattrPacket(fname="<NULL>", name="", value="")', str(test_XattrPacket)
        )


if __name__ == "__main__":
    unittest.main()
