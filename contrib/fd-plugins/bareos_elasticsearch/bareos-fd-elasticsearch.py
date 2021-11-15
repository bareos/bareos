#!/usr/bin/env python
# -*- coding: utf-8 -*-
# bareos-fd-elasticsearch.py parses any file in the backup job and sends metadata and (optionally) contents
# to an Elasticsearch server.

# Provided by the Bareos FD Python plugin interface
from bareosfd import *
from bareos_fd_consts import *

# This module contains the wrapper functions called by the Bareos-FD, the functions call the corresponding
# methods from your plugin class
from BareosFdWrapper import *

# This module contains the used plugin class
from BareosFdPluginElasticsearch import *

def load_bareos_plugin(context, plugindef):
    '''
    This function is called by the Bareos-FD to load the plugin
    We use it to intantiate the plugin class
    '''
    # BareosFdWrapper.bareos_fd_plugin_object is the module attribute that holds the plugin class object
    BareosFdWrapper.bareos_fd_plugin_object = BareosFdPluginFileElasticsearch (context, plugindef);
    return bRCs['bRC_OK'];

# the rest is done in the Plugin module
