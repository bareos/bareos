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

import os
import bareosfd
from bareos_fd_consts import bJobMessageType

debuglevel = 100
plugin_context = None


def jobmessage(message_type, message):
    global plugin_context
    if plugin_context != None:
        message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
        bareosfd.JobMessage(plugin_context, bJobMessageType[message_type], message)


def debugmessage(level, message):
    global plugin_context
    if plugin_context != None:
        message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
        bareosfd.DebugMessage(plugin_context, level, message)


def set_plugin_context(context):
    global plugin_context
    plugin_context = context
