#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2023 Bareos GmbH & Co. KG
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
# Author: Maik Aussendorf
#
# The BareosFdWrapper module. Here is a global object bareos_fd_plugin_object
# and wrapper functions, which are directly called out of the bareos-fd. They
# are intended to pass the call to a method of an object of type
# BareosFdPluginBaseclass (or derived)

from bareosfd import bRC_OK, bRC_Error, JobMessage, M_FATAL

# use this as global plugin object among your python-fd-plugin modules
bareos_fd_plugin_object = None
_plugin_class = None


def BareosPlugin(plugin_class):
    global _plugin_class
    _plugin_class = plugin_class
    return plugin_class


def load_bareos_plugin(plugindef):
    global _plugin_class
    global bareos_fd_plugin_object
    try:
        bareos_fd_plugin_object = _plugin_class(plugindef)
        return bRC_OK
    except Exception as e:
        bareos_fd_plugin_object = None
        JobMessage(M_FATAL, "load_bareos_plugin() failed: {}\n".format(e))
        return bRC_Error


def parse_plugin_definition(plugindef):
    return bareos_fd_plugin_object.parse_plugin_definition(plugindef)


def handle_plugin_event(event):
    return bareos_fd_plugin_object.handle_plugin_event(event)


def start_backup_file(savepkt):
    return bareos_fd_plugin_object.start_backup_file(savepkt)


def end_backup_file():
    return bareos_fd_plugin_object.end_backup_file()


def start_restore_file(cmd):
    return bareos_fd_plugin_object.start_restore_file(cmd)


def end_restore_file():
    return bareos_fd_plugin_object.end_restore_file()


def restore_object_data(ROP):
    return bareos_fd_plugin_object.restore_object_data(ROP)


def plugin_io(IOP):
    return bareos_fd_plugin_object.plugin_io(IOP)


def create_file(restorepkt):
    return bareos_fd_plugin_object.create_file(restorepkt)


def set_file_attributes(restorepkt):
    return bareos_fd_plugin_object.set_file_attributes(restorepkt)


def check_file(fname):
    return bareos_fd_plugin_object.check_file(fname)


def get_acl(acl):
    return bareos_fd_plugin_object.get_acl(acl)


def set_acl(acl):
    return bareos_fd_plugin_object.set_acl(acl)


def get_xattr(xattr):
    return bareos_fd_plugin_object.get_xattr(xattr)


def set_xattr(xattr):
    return bareos_fd_plugin_object.set_xattr(xattr)


def handle_backup_file(savepkt):
    return bareos_fd_plugin_object.handle_backup_file(savepkt)
