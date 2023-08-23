# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2023 Bareos GmbH & Co. KG
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
import bareossd
import BareosSdPluginBaseclass

from sys import version_info


class BareosSdTest(BareosSdPluginBaseclass.BareosSdPluginBaseclass):
    def __init__(self, plugindef):
        bareossd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        bareossd.DebugMessage(
            100,
            "Python Version: %s.%s.%s\n"
            % (version_info.major, version_info.minor, version_info.micro),
        )
        super(BareosSdTest, self).__init__(plugindef)

        self.outputfile = None

        events = []
        events.append(bareossd.bsdEventJobStart)
        events.append(bareossd.bsdEventDeviceReserve)
        events.append(bareossd.bsdEventVolumeUnload)
        events.append(bareossd.bsdEventVolumeLoad)
        events.append(bareossd.bsdEventDeviceOpen)
        events.append(bareossd.bsdEventDeviceMount)
        events.append(bareossd.bsdEventLabelRead)
        events.append(bareossd.bsdEventLabelVerified)
        events.append(bareossd.bsdEventLabelWrite)
        events.append(bareossd.bsdEventSetupRecordTranslation)
        events.append(bareossd.bsdEventWriteRecordTranslation)
        events.append(bareossd.bsdEventDeviceUnmount)
        events.append(bareossd.bsdEventDeviceClose)
        events.append(bareossd.bsdEventJobEnd)
        bareossd.RegisterEvents(events)

    def parse_plugin_definition(self, plugindef):
        super(BareosSdTest, self).parse_plugin_definition(plugindef)
        if "output" in self.options:
            self.outputfile = self.options["output"]
        else:
            self.outputfile = "/tmp/bareos-dir-test-plugin.log"

        return bareossd.bRC_OK

    def handle_plugin_event(self, event):
        super(BareosSdTest, self).handle_plugin_event(event)
        bareossd.DebugMessage(
            100,
            "%s: bsdEventJobStart event %d triggered\n" % (__name__, event),
        )
        if event == bareossd.bsdEventJobStart:
            self.toFile("bareossd.bsdEventJobStart\n")
        elif event == bareossd.bsdEventDeviceReserve:
            self.toFile("bareossd.bsdEventDeviceReserve\n")
        elif event == bareossd.bsdEventVolumeUnload:
            self.toFile("bareossd.bsdEventVolumeUnload\n")
        elif event == bareossd.bsdEventVolumeLoad:
            self.toFile("bareossd.bsdEventVolumeLoad\n")
        elif event == bareossd.bsdEventDeviceOpen:
            self.toFile("bareossd.bsdEventDeviceOpen\n")
        elif event == bareossd.bsdEventDeviceMount:
            self.toFile("bareossd.bsdEventDeviceMount\n")
        elif event == bareossd.bsdEventLabelRead:
            self.toFile("bareossd.bsdEventLabelRead\n")
        elif event == bareossd.bsdEventLabelVerified:
            self.toFile("bareossd.bsdEventLabelVerified\n")
        elif event == bareossd.bsdEventLabelWrite:
            self.toFile("bareossd.bsdEventLabelWrite\n")
        elif event == bareossd.bsdEventSetupRecordTranslation:
            self.toFile("bareossd.bsdEventSetupRecordTranslation\n")
        elif event == bareossd.bsdEventWriteRecordTranslation:
            self.toFile("bareossd.bsdEventWriteRecordTranslation\n")
        elif event == bareossd.bsdEventDeviceUnmount:
            self.toFile("bareossd.bsdEventDeviceUnmount\n")
        elif event == bareossd.bsdEventDeviceClose:
            self.toFile("bareossd.bsdEventDeviceClose\n")
        elif event == bareossd.bsdEventJobEnd:
            self.toFile("bareossd.bsdEventJobEnd\n")

        return bareossd.bRC_OK

    def toFile(self, text):
        doc = open(self.outputfile, "a")
        doc.write(text)
        doc.close()


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
