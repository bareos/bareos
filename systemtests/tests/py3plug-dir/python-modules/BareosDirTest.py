# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2021 Bareos GmbH & Co. KG
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
#
# Author: Tobias Plum
#
import bareosdir
import BareosDirPluginBaseclass

from time import time
from sys import version_info


class BareosDirTest(BareosDirPluginBaseclass.BareosDirPluginBaseclass):
    def __init__(self, plugindef):
        bareosdir.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        bareosdir.DebugMessage(
            100,
            "Python Version: %s.%s.%s\n"
            % (version_info.major, version_info.minor, version_info.micro),
        )
        super(BareosDirTest, self).__init__(plugindef)

        self.outputfile = None

    def parse_plugin_definition(self, plugindef):
        super(BareosDirTest, self).parse_plugin_definition(plugindef)
        if "output" in self.options:
            self.outputfile = self.options["output"]
        else:
            self.outputfile = "/tmp/bareos-dir-test-plugin.log"

        return bareosdir.bRC_OK

    def handle_plugin_event(self, event):
        super(BareosDirTest, self).handle_plugin_event(event)
        job_name = repr(bareosdir.GetValue(bareosdir.bDirVarJobName))
        job_id = repr(bareosdir.GetValue(bareosdir.bDirVarJobId))
        microtime = round(time() * 1000)
        msg_f = "%s Job:" + job_name + " JobId: " + job_id + " Time: " + repr(microtime) + "\n"

        if event == bareosdir.bDirEventJobStart:
            self.toFile(msg_f % "bDirEventJobStart")

        elif event == bareosdir.bDirEventJobEnd:
            self.toFile(msg_f % "bDirEventJobEnd")

        elif event == bareosdir.bDirEventJobInit:
            self.toFile(msg_f % "bDirEventJobInit")

        elif event == bareosdir.bDirEventJobRun:
            self.toFile(msg_f % "bDirEventJobRun")

        return bareosdir.bRC_OK

    def toFile(self, text):
        bareosdir.DebugMessage(
            100,
            "Writing string '%s' to '%s'\n"
            % (text, self.outputfile),
        )
        doc = open(self.outputfile, "a")
        doc.write(text)
        doc.close()


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
