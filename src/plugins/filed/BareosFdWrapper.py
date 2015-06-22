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
# Author: Maik Aussendorf
#
# The BareosFdWrapper module. Here is a global object bareos_fd_plugin_object
# and wrapper functions, which are directly called out of the bareos-fd. They
# are intended to pass the call to a method of an object of type
# BareosFdPluginBaseclass (or derived)

# use this as global plugin object among your python-fd-plugin modules
bareos_fd_plugin_object = None


def parse_plugin_definition(context, plugindef):
    return bareos_fd_plugin_object.parse_plugin_definition(context, plugindef)


def handle_plugin_event(context, event):
    return bareos_fd_plugin_object.handle_plugin_event(context, event)


def start_backup_file(context, savepkt):
    return bareos_fd_plugin_object.start_backup_file(context, savepkt)


def end_backup_file(context):
    return bareos_fd_plugin_object.end_backup_file(context)


def start_restore_file(context, cmd):
    return bareos_fd_plugin_object.start_restore_file(context, cmd)


def end_restore_file(context):
    return bareos_fd_plugin_object.end_restore_file(context)


def restore_object_data(context, ROP):
    return bareos_fd_plugin_object.restore_object_data(context, ROP)


def plugin_io(context, IOP):
    return bareos_fd_plugin_object.plugin_io(context, IOP)


def create_file(context, restorepkt):
    return bareos_fd_plugin_object.create_file(context, restorepkt)


def set_file_attributes(context, restorepkt):
    return bareos_fd_plugin_object.set_file_attributes(context, restorepkt)


def check_file(context, fname):
    return bareos_fd_plugin_object.check_file(context, fname)


def get_acl(context, acl):
    return bareos_fd_plugin_object.get_acl(context, acl)


def set_acl(context, acl):
    return bareos_fd_plugin_object.set_acl(context, acl)


def get_xattr(context, xattr):
    return bareos_fd_plugin_object.get_xattr(context, xattr)


def set_xattr(context, xattr):
    return bareos_fd_plugin_object.set_xattr(context, xattr)


def handle_backup_file(context, savepkt):
    return bareos_fd_plugin_object.handle_backup_file(context, savepkt)
