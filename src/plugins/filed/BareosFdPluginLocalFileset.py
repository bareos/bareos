#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Bareos python plugins class that adds files from a local list to the backup fileset
# (c) Bareos GmbH & Co. KG
# AGPL v.3
# Author: Maik Aussendorf

from bareosfd import *
from bareos_fd_consts import *
import os
from  BareosFdPluginBaseclass import *
import BareosFdWrapper


class BareosFdPluginLocalFileset (BareosFdPluginBaseclass):
    '''
    Simple Bareos-FD-Plugin-Class that parses a file and backups all files listed there
    Filename is taken from plugin argument 'filename'
    '''

    def parse_plugin_definition(self,context, plugindef):
        '''
        Parses the plugin argmuents and reads files from file given by argument 'filename'
        '''
        BareosFdPluginBaseclass.parse_plugin_definition(self, context, plugindef);
        if (not 'filename' in self.options):
            DebugMessage(context, 100, "Option \'filename\' not defined.\n");
            return bRCs['bRC_Error'];
        DebugMessage(context, 100, "Using " + self.options['filename'] + " to search for local files\n");
        if os.path.exists (self.options['filename']):
            try:
                config_file = open (self.options['filename'],'rb');
            except:
                DebugMessage(context, 100, "Could not open file " + self.options['filename'] + "\n");
                return bRCs['bRC_Error'];
        else:
            DebugMessage(context, 100, "File " + self.options['filename'] + " does not exist\n");
            return bRCs['bRC_Error'];
        self.files_to_backup = config_file.read().splitlines();
        return bRCs['bRC_OK'];


    def start_backup_file(self,context, savepkt):
        '''
        Defines the file to backup and creates the savepkt. In this example only files (no directories)
        are allowed
        '''
        DebugMessage(context, 100, "start_backup called\n");
        if not self.files_to_backup:
            DebugMessage(context, 100, "No files to backup\n");
            return bRCs['bRC_Skip'];

        file_to_backup = self.files_to_backup.pop();
        DebugMessage(context, 100, "file: " + file_to_backup + "\n");

        statp = StatPacket();
        savepkt.statp = statp;
        savepkt.fname = file_to_backup;
        savepkt.type = bFileType['FT_REG'];

        JobMessage(context, bJobMessageType['M_INFO'], "Starting backup of " + file_to_backup + "\n");
        return bRCs['bRC_OK'];

    def end_backup_file(self, context):
        '''
        Here we return 'bRC_More' as long as our list files_to_backup is not empty and
        bRC_OK when we are done
        '''
        DebugMessage(context, 100, "end_backup_file() entry point in Python called\n")
        if self.files_to_backup:
            return bRCs['bRC_More'];
        else:
            return bRCs['bRC_OK'];


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
