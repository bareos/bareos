#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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
# Author: Bareos Team
#
"""bareos-fd-postgresql is a plugin to backup PostgreSQL cluster
using BareosFdPluginPostgreSQL
"""
from sys import version_info

# Provided by the Bareos FD Python plugin interface
import bareosfd
from bareosfd import *

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding methods from your plugin class
import BareosFdWrapper

# from BareosFdWrapper import parse_plugin_definition, handle_plugin_event,
# start_backup_file, end_backup_file, start_restore_file, end_restore_file,
# restore_object_data, plugin_io, create_file, check_file, handle_backup_file
# # noqa

from BareosFdWrapper import *  # noqa


def load_bareos_plugin(plugindef):
    """
    This function is called by the Bareos-FD to load the plugin
    We use it to instantiate the plugin class
    """
    if version_info.major >= 3 and version_info.minor < 6:
        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            f"Need Python version >=< 3.6 (current version: \
              {version_info.major}.{version_info.minor}.{version_info.micro})\n",
        )
        return bareosfd.bRC_Error

    # Check for needed python modules
    try:
        import pg8000

        # check if pg8000 module is new enough
        major, minor, patch = pg8000.__version__.split(".")
        if int(major) < 1 or (int(major) == 1 and int(minor) < 16):
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                f"FATAL ERROR: pg8000 module is too old({pg8000.__version__}),\
                minimum required version is >= 1.16\n",
            )
    except Exception as err:
        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            f"could not import Python module: pg8000 {err}\n"
        )
        return bareosfd.bRC_Error

    try:
        # This module contains the used plugin class
        import BareosFdPluginPostgreSQL
    except Exception as err:
        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            f"could not import Python module: pg8000 {err}\n"
        )
        return bareosfd.bRC_Error

    # BareosFdWrapper.bareos_fd_plugin_object is the module attribute that
    # holds the plugin class object
    BareosFdWrapper.bareos_fd_plugin_object = (
        BareosFdPluginPostgreSQL.BareosFdPluginPostgreSQL(plugindef)
    )
    return bareosfd.bRC_OK


# the rest is done in the Plugin module
# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
