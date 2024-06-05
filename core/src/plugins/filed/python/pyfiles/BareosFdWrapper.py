#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2024 Bareos GmbH & Co. KG
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

"""The BareosFdWrapper module. Here is a global object bareos_fd_plugin_object
and wrapper functions, which are directly called out of the bareos-fd. They
are intended to pass the call to a method of an object of type
BareosFdPluginBaseclass (or derived)"""

# silence warning about module-name
# pylint: disable=invalid-name
# pylint: enable=invalid-name

from bareosfd import bRC_OK  # pylint: disable=import-error

# use this as global plugin object among your python-fd-plugin modules
bareos_fd_plugin_object = None  # pylint: disable=invalid-name
PLUGIN_CLASS = None

__all__ = ["bareos_fd_plugin_object"]


def add_to_all(thing):
    __all__.append(thing.__name__)
    return thing


@add_to_all
def BareosPlugin(plugin_class):  # pylint: disable=invalid-name
    global PLUGIN_CLASS  # pylint: disable=global-statement
    PLUGIN_CLASS = plugin_class
    return plugin_class


@add_to_all
def load_bareos_plugin(plugindef):
    global bareos_fd_plugin_object  # pylint: disable=global-statement
    bareos_fd_plugin_object = PLUGIN_CLASS(plugindef)
    return bRC_OK


@add_to_all
def parse_plugin_definition(plugindef):
    return bareos_fd_plugin_object.parse_plugin_definition(plugindef)


@add_to_all
def handle_plugin_event(event):
    return bareos_fd_plugin_object.handle_plugin_event(event)


@add_to_all
def start_backup_file(savepkt):
    return bareos_fd_plugin_object.start_backup_file(savepkt)


@add_to_all
def end_backup_file():
    return bareos_fd_plugin_object.end_backup_file()


@add_to_all
def start_restore_file(cmd):
    return bareos_fd_plugin_object.start_restore_file(cmd)


@add_to_all
def end_restore_file():
    return bareos_fd_plugin_object.end_restore_file()


@add_to_all
def restore_object_data(rop):
    return bareos_fd_plugin_object.restore_object_data(rop)


@add_to_all
def plugin_io(iop):
    return bareos_fd_plugin_object.plugin_io(iop)


@add_to_all
def create_file(restorepkt):
    return bareos_fd_plugin_object.create_file(restorepkt)


@add_to_all
def set_file_attributes(restorepkt):
    return bareos_fd_plugin_object.set_file_attributes(restorepkt)


@add_to_all
def check_file(fname):
    return bareos_fd_plugin_object.check_file(fname)


@add_to_all
def get_acl(acl):
    return bareos_fd_plugin_object.get_acl(acl)


@add_to_all
def set_acl(acl):
    return bareos_fd_plugin_object.set_acl(acl)


@add_to_all
def get_xattr(xattr):
    return bareos_fd_plugin_object.get_xattr(xattr)


@add_to_all
def set_xattr(xattr):
    return bareos_fd_plugin_object.set_xattr(xattr)


@add_to_all
def handle_backup_file(savepkt):
    return bareos_fd_plugin_object.handle_backup_file(savepkt)
