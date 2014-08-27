#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Bareos-fd-mock-test a simple example for a python Bareos FD Plugin using the baseclass
# and doing nothing
# You may take this as a skeleton for your plugin

from bareosfd import *
from bareos_fd_consts import *
from os import O_WRONLY, O_CREAT

from BareosFdPluginBaseclass import *
from BareosFdWrapper import *

def load_bareos_plugin(context, plugindef):
    DebugMessage(context, 100, "------ Plugin loader called with " + plugindef + "\n");
    BareosFdWrapper.plugin_object = BareosFdPluginBaseclass (context, plugindef);
    return bRCs['bRC_OK'];

# the rest is done in the Plugin module
