#!/usr/bin/env python

# -*- coding: utf-8 -*-

# Bareos python class as stub for option plugin
# handle_backup_file gets called for each file in the backup
# 
# (c) Bareos GmbH & Co. KG
# AGPL v.3
# Author: Maik Aussendorf

import bareosfd
from BareosFdPluginBaseclass import BareosFdPluginBaseclass

class BareosFdPluginFileInteract(BareosFdPluginBaseclass):

    def __init__(self, plugindef):
        super(BareosFdPluginFileInteract, self).__init__(plugindef)
        bareosfd.RegisterEvents([bareosfd.bEventHandleBackupFile])

    def handle_backup_file(self, savepkt):
        bareosfd.DebugMessage(100, "handle_backup_file called with " + str(savepkt) + "\n");
        bareosfd.DebugMessage(100, "fname: " + savepkt.fname + " Type: " + str(savepkt.type) + "\n");
        if (savepkt.type == bareosfd.FT_REG):
            bareosfd.DebugMessage(100, "regular file, do something now...\n");
            # Add your stuff here.

        return bareosfd.bRC_OK
