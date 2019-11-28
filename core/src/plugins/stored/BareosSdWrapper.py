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
# The BareosSdWrapper module. Here is a global object bareos_sd_plugin_object
# and wrapper functions, which are directly called out of the bareos-sd. They
# are intended to pass the call to a method of an object of type
# BareosSdPluginBaseclass (or derived)

# use this as global plugin object among your python-sd-plugin modules
bareos_sd_plugin_object = None


def parse_plugin_definition(context, plugindef):
    return bareos_sd_plugin_object.parse_plugin_definition(context, plugindef)


def handle_plugin_event(context, event):
    return bareos_sd_plugin_object.handle_plugin_event(context, event)


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
