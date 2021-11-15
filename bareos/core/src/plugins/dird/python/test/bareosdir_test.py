#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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
import bareosdir
import time

# print (help(bareosdir))


class TestBareosFd(unittest.TestCase):
    def test_GetValue(self):
        self.assertRaises(RuntimeError, bareosdir.GetValue, 1)

    def test_SetValue(self):
        self.assertRaises(RuntimeError, bareosdir.SetValue, 2, 2)

    def test_DebugMessage(self):
        self.assertRaises(TypeError, bareosdir.DebugMessage, "This is a debug message")
        self.assertRaises(
            RuntimeError, bareosdir.DebugMessage, 100, "This is a debug message"
        )

    def test_JobMessage(self):
        self.assertRaises(TypeError, bareosdir.JobMessage, "This is a Job message")
        self.assertRaises(
            RuntimeError, bareosdir.JobMessage, 100, "This is a Job message"
        )

    def test_RegisterEvents(self):
        # self.assertRaises(TypeError, bareosdir.RegisterEvents)
        self.assertRaises(RuntimeError, bareosdir.RegisterEvents, 1)

    def test_UnRegisterEvents(self):
        self.assertRaises(RuntimeError, bareosdir.UnRegisterEvents, 1)

    def test_GetInstanceCount(self):
        self.assertRaises(RuntimeError, bareosdir.GetInstanceCount)


if __name__ == "__main__":
    unittest.main()
