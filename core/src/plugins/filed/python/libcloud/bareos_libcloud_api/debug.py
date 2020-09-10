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

debuglevel = 100


def jobmessage(message_type, message):
    message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
    bareosfd.JobMessage(message_type, message)


def debugmessage(level, message):
    message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
    #bareosfd.DebugMessage(level, message)
    #bareosfd.DebugMessage(level, message.encode("utf-8"))
    try:
        bareosfd.DebugMessage(level, message)
    except UnicodeError:
        bareosfd.DebugMessage(level, message.encode("utf-8"))
