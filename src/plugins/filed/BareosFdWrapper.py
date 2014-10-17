#!/usr/bin/env python
# -*- coding: utf-8 -*-
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


def check_file(context, fname):
    return bareos_fd_plugin_object.check_file(context, fname)


def handle_backup_file(context, savepkt):
    return bareos_fd_plugin_object.handle_backup_file(context, savepkt)
