#!/usr/bin/env python
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2020 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.


import sys
import os.path

libdirs = ["/usr/lib64/bareos/plugins/", "/usr/lib/bareos/plugins/"]
sys.path.extend([l for l in libdirs if os.path.isdir(l)])

# Provided by the Bareos FD Python plugin interface
from bareosfd import *

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding
# methods from your plugin class
import BareosFdWrapper
from BareosFdWrapper import (
    parse_plugin_definition,
    handle_plugin_event,
    start_backup_file,
    end_backup_file,
    start_restore_file,
    end_restore_file,
    restore_object_data,
    plugin_io,
    create_file,
    set_file_attributes,
    check_file,
    get_acl,
    set_acl,
    get_xattr,
    set_xattr,
    handle_backup_file,
)

# This module contains the used plugin class
import BareosFdPluginOvirt


def load_bareos_plugin(plugindef):
    """
    This function is called by the Bareos-FD to load the plugin
    We use it to intantiate the plugin class
    """
    # BareosFdWrapper.bareos_fd_plugin_object is the module attribute that holds the plugin class object
    BareosFdWrapper.bareos_fd_plugin_object = BareosFdPluginOvirt.BareosFdPluginOvirt(
        plugindef
    )

    return bRC_OK


# the rest is done in the Plugin module
