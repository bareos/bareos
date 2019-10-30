# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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
from bareos_sd_consts import *


def load_bareos_plugin(context, plugindef):
    events = []
    events.append(bsdEventType["bsdEventJobStart"])
    events.append(bsdEventType["bsdEventDeviceReserve"])
    events.append(bsdEventType["bsdEventVolumeUnload"])
    events.append(bsdEventType["bsdEventVolumeLoad"])
    events.append(bsdEventType["bsdEventDeviceOpen"])
    events.append(bsdEventType["bsdEventDeviceMount"])
    events.append(bsdEventType["bsdEventLabelRead"])
    events.append(bsdEventType["bsdEventLabelVerified"])
    events.append(bsdEventType["bsdEventLabelWrite"])
    events.append(
        bsdEventType["bsdEventSetupRecordTranslation"]
    )
    events.append(
        bsdEventType["bsdEventWriteRecordTranslation"]
    )
    events.append(bsdEventType["bsdEventDeviceUnmount"])
    events.append(bsdEventType["bsdEventDeviceClose"])
    events.append(bsdEventType["bsdEventJobEnd"])
    RegisterEvents(context, events)
    return bRCs["bRC_OK"]


def parse_plugin_definition(context, plugindef):
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
            return bRCs["bRC_Error"]
        toFile(outputfile)

    return bRCs["bRC_OK"]


def handle_plugin_event(context, event):
    if event == bsdEventType["bsdEventJobStart"]:
        toFile("bsdEventJobStart\n")
    elif event == bsdEventType["bsdEventDeviceReserve"]:
        toFile("bsdEventDeviceReserve\n")
    elif event == bsdEventType["bsdEventVolumeUnload"]:
        toFile("bsdEventVolumeUnload\n")
    elif event == bsdEventType["bsdEventVolumeLoad"]:
        toFile("bsdEventVolumeLoad\n")
    elif event == bsdEventType["bsdEventDeviceOpen"]:
        toFile("bsdEventDeviceOpen\n")
    elif event == bsdEventType["bsdEventDeviceMount"]:
        toFile("bsdEventDeviceMount\n")
    elif event == bsdEventType["bsdEventLabelRead"]:
        toFile("bsdEventLabelRead\n")
    elif event == bsdEventType["bsdEventLabelVerified"]:
        toFile("bsdEventLabelVerified\n")
    elif event == bsdEventType["bsdEventLabelWrite"]:
        toFile("bsdEventLabelWrite\n")
    elif (
        event
        == bsdEventType["bsdEventSetupRecordTranslation"]
    ):
        toFile("bsdEventSetupRecordTranslation\n")
    elif (
        event
        == bsdEventType["bsdEventWriteRecordTranslation"]
    ):
        toFile("bsdEventWriteRecordTranslation\n")
    elif event == bsdEventType["bsdEventDeviceUnmount"]:
        toFile("bsdEventDeviceUnmount\n")
    elif event == bsdEventType["bsdEventDeviceClose"]:
        toFile("bsdEventDeviceClose\n")
    elif event == bsdEventType["bsdEventJobEnd"]:
        toFile("bsdEventJobEnd\n")

    return bRCs["bRC_OK"]


def toFile(text):
    doc = open(outputfile, "a")
    doc.write(text)
    doc.close()
