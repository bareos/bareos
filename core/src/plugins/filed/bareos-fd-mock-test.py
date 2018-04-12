#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
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
# Bareos-fd-mock-test a simple example for a python Bareos FD Plugin using the baseclass
# and doing nothing
# You may take this as a skeleton for your plugin
#
# Author: Maik Aussendorf
#

# Provided by the Bareos FD Python plugin interface
import bareosfd
import bareos_fd_consts
import BareosFdWrapper
from BareosFdWrapper import *  # noqa
import BareosFdPluginBaseclass


def load_bareos_plugin(context, plugindef):
    bareosfd.DebugMessage(context, 100, "------ Plugin loader called with " + plugindef + "\n")
    BareosFdWrapper.bareos_fd_plugin_object = \
        BareosFdPluginBaseclass.BareosFdPluginBaseclass(context, plugindef)
    return bareos_fd_consts.bRCs['bRC_OK']

# the rest is done in the Plugin module
