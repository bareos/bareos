#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2020 Bareos GmbH & Co. KG
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
# Bareos-fd-local-fileset a simple example for a python Bareos FD Plugin using
# BareosFdPluginLocalFileset. The plugin argument 'filename' is used to read
# all files listed in that file and add it to the fileset
#
# Author: Maik Aussendorf
#

# Provided by the Bareos FD Python plugin interface
import bareosfd

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding methods from your plugin class
import BareosFdWrapper

from BareosFdWrapper import *  # noqa

# This module contains the used plugin class
import BareosFdPluginLocalFileset


def load_bareos_plugin(gcplugindef):
    """
    This function is called by the Bareos-FD to load the plugin
    We use it to instantiate the plugin class
    """
    # BareosFdWrapper.bareos_fd_plugin_object is the module attribute that
    # holds the plugin class object
    BareosFdWrapper.bareos_fd_plugin_object = (
        BareosFdPluginLocalFileset.BareosFdPluginLocalFileset(gcplugindef)
    )
    return bareosfd.bRC_OK


# the rest is done in the Plugin module
