#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2015-2015 Planets Communications B.V.
# Copyright (C) 2015-2020 Bareos GmbH & Co. KG
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
# Author: Marco van Wieringen
#
# Bareos-fd-ldap is a python Bareos FD Plugin intended to backup and
# restore LDAP entries.
#

# Provided by the Bareos FD Python plugin interface
from bareosfd import bRC_OK

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding
# methods from your plugin class
import BareosFdWrapper
from BareosFdWrapper import *  # noqa

# This module contains the used plugin class
import BareosFdPluginLDAP


def load_bareos_plugin(plugindef):
    """
    This function is called by the Bareos-FD to load the plugin
    We use it to intantiate the plugin class
    """
    BareosFdWrapper.bareos_fd_plugin_object = BareosFdPluginLDAP.BareosFdPluginLDAP(
        plugindef
    )

    return bRC_OK


# the rest is done in the Plugin module
