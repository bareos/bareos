#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
#
# MIT License
#
# Copyright (c) [year] [fullname]
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# Author: Kozlov Alexander
#

# Provided by the Bareos Dir Python plugin interface
import bareos_dir_consts

# This module contains the wrapper functions called by the Bareos-Dir, the
# functions call the corresponding methods from your plugin class
import BareosDirWrapper
from BareosDirWrapper import *

# This module contains the used plugin class
import BareosDirPluginGraphiteSender


def load_bareos_plugin(context, plugindef):
    '''
    This function is called by the Bareos-Dir to load the plugin
    We use it to instantiate the plugin class
    '''
    # BareosDirWrapper.bareos_dir_plugin_object is the module attribute that
    # holds the plugin class object
    BareosDirWrapper.bareos_dir_plugin_object = \
        BareosDirPluginGraphiteSender.BareosDirPluginGraphiteSender(
            context, plugindef)
    return bareos_dir_consts.bRCs['bRC_OK']

# the rest is done in the Plugin module
