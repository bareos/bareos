#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Bareos-fd-local-fileset a simple example for a python Bareos FD Plugin using BareosFdPluginLocalFileset
# The plugin argument 'filename' is used to read all files listed in that file and add it to the fileset
# License: AGPLv3

# Provided by the Bareos FD Python plugin interface
import bareosfd

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding methods from your plugin class
import BareosFdWrapper
from BareosFdWrapper import *

# This module contains the used plugin class
from bareos_mysql_dump.BareosFdMySQLclass import BareosFdMySQLclass

def load_bareos_plugin(plugindef):
    '''
    This function is called by the Bareos-FD to load the plugin
    We use it to instantiate the plugin class
    '''
    # BareosFdWrapper.bareos_fd_plugin_object is the module attribute that holds the plugin class object
    BareosFdWrapper.bareos_fd_plugin_object = BareosFdMySQLclass(plugindef)
    return bareosfd.bRC_OK

# the rest is done in the Plugin module
