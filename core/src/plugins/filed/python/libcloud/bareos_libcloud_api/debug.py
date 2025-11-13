#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

import bareosfd


def jobmessage(message_type, message):
    """
    Wrapper function for job messages.
    """
    message = f"BareosFdPluginLibcloud: {message}\n"
    bareosfd.JobMessage(message_type, message)


def debugmessage(level, message):
    """
    Wrapper function for debug messages.
    """
    message = f"BareosFdPluginLibcloud: {message}\n"
    bareosfd.DebugMessage(level, message)
