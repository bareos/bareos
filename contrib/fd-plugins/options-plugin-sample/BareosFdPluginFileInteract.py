#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Bareos python class as stub for option plugin
# handle_backup_file gets called for each file in the backup
# 
# (c) Bareos GmbH & Co. KG
# AGPL v.3
# Author: Maik Aussendorf

from bareosfd import *
from bareos_fd_consts import *
import os
from  BareosFdPluginBaseclass import *
import BareosFdWrapper

class BareosFdPluginFileInteract  (BareosFdPluginBaseclass):

    def handle_backup_file(self,context, savepkt):
        DebugMessage(context, 100, "handle_backup_file called with " + str(savepkt) + "\n");
        DebugMessage(context, 100, "fname: " + savepkt.fname + " Type: " + str(savepkt.type) + "\n");
        if ( savepkt.type == bFileType['FT_REG'] ):
            DebugMessage(context, 100, "regulaer file, do something now...\n");
            # add your stuff here

        return bRCs['bRC_OK'];

    
