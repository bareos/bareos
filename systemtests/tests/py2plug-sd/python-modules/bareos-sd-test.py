# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2020 Bareos GmbH & Co. KG
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
from bareossd import *

def load_bareos_plugin(plugindef):
    events = []
    events.append(bsdEventJobStart)
    events.append(bsdEventDeviceReserve)
    events.append(bsdEventVolumeUnload)
    events.append(bsdEventVolumeLoad)
    events.append(bsdEventDeviceOpen)
    events.append(bsdEventDeviceMount)
    events.append(bsdEventLabelRead)
    events.append(bsdEventLabelVerified)
    events.append(bsdEventLabelWrite)
    events.append(bsdEventSetupRecordTranslation)
    events.append(bsdEventWriteRecordTranslation)
    events.append(bsdEventDeviceUnmount)
    events.append(bsdEventDeviceClose)
    events.append(bsdEventJobEnd)
    RegisterEvents(events)
    return bRC_OK


def parse_plugin_definition(plugindef):
    plugin_options = plugindef.split(":")
    for current_option in plugin_options:
        key, sep, val = current_option.partition("=")
        if val == "":
            continue
        elif key == "output":
            global outputfile
            outputfile = val
            continue
        elif key == "instance":
            continue
        elif key == "module_path":
            continue
        elif key == "module_name":
            continue
        else:
            return bRC_Error
        toFile(outputfile)

    return bRC_OK


def handle_plugin_event(event):
    if event == bsdEventJobStart:
        toFile("bsdEventJobStart\n")
    elif event == bsdEventDeviceReserve:
        toFile("bsdEventDeviceReserve\n")
    elif event == bsdEventVolumeUnload:
        toFile("bsdEventVolumeUnload\n")
    elif event == bsdEventVolumeLoad:
        toFile("bsdEventVolumeLoad\n")
    elif event == bsdEventDeviceOpen:
        toFile("bsdEventDeviceOpen\n")
    elif event == bsdEventDeviceMount:
        toFile("bsdEventDeviceMount\n")
    elif event == bsdEventLabelRead:
        toFile("bsdEventLabelRead\n")
    elif event == bsdEventLabelVerified:
        toFile("bsdEventLabelVerified\n")
    elif event == bsdEventLabelWrite:
        toFile("bsdEventLabelWrite\n")
    elif event == bsdEventSetupRecordTranslation:
        toFile("bsdEventSetupRecordTranslation\n")
    elif event == bsdEventWriteRecordTranslation:
        toFile("bsdEventWriteRecordTranslation\n")
    elif event == bsdEventDeviceUnmount:
        toFile("bsdEventDeviceUnmount\n")
    elif event == bsdEventDeviceClose:
        toFile("bsdEventDeviceClose\n")
    elif event == bsdEventJobEnd:
        toFile("bsdEventJobEnd\n")

    return bRC_OK


def toFile(text):
    doc = open(outputfile, "a")
    doc.write(text)
    doc.close()
