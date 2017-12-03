#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2017 Bareos GmbH & Co. KG
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
# Author: Stephan Duehr
#
# Bareos-fd-vmware is a python Bareos FD Plugin intended to backup and
# restore VMware images and configuration
#

import sys
import os.path
libdirs = ['/usr/lib64/bareos/plugins/', '/usr/lib/bareos/plugins/']
sys.path.extend([l for l in libdirs if os.path.isdir(l)])

# newer Python versions, eg. Debian 8/Python >= 2.7.9 and
# CentOS/RHEL since 7.4 by default do SSL cert verification,
# we then try to disable it here.
# see https://github.com/vmware/pyvmomi/issues/212
py_ver = sys.version_info[0:3]
if py_ver[0] == 2 and py_ver[1] == 7 and py_ver[2] >= 5:
    import ssl
    try:
        ssl._create_default_https_context = ssl._create_unverified_context
    except AttributeError:
        pass

# Provided by the Bareos FD Python plugin interface
from bareos_fd_consts import bRCs

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding
# methods from your plugin class
import BareosFdWrapper
from BareosFdWrapper import *  # noqa

# This module contains the used plugin class
import BareosFdPluginVMware


def load_bareos_plugin(context, plugindef):
    '''
    This function is called by the Bareos-FD to load the plugin
    We use it to intantiate the plugin class
    '''
    BareosFdWrapper.bareos_fd_plugin_object = \
        BareosFdPluginVMware.BareosFdPluginVMware(
            context, plugindef)

    return bRCs['bRC_OK']

# the rest is done in the Plugin module
