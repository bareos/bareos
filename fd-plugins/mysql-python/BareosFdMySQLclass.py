#!/usr/bin/env python
# -*- coding: utf-8 -*-

#Adapted from examples for mysql full backups
#Copyright 2014 Battelle Memorial Institute
#Written by Evan Felix

from bareosfd import *
from bareos_fd_consts import *
import os
from  BareosFdPluginBaseclass import *
import BareosFdWrapper


class BareosFdMySQLclass (BareosFdPluginBaseclass):
    '''
        Plugin for backing up all mysql databases found in a specific mysql server
    '''       
    def __init__(self, context, plugindef):
        BareosFdPluginBaseclass.__init__(self, context, plugindef)
        self.file=None
        DebugMessage(context, 100, "since: %d\n"%(self.since));
        DebugMessage(context, 100, "level: %d\n"%(self.level));

    def parse_plugin_definition(self,context, plugindef):
        '''
        '''
        BareosFdPluginBaseclass.parse_plugin_definition(self, context, plugindef)

        f = os.popen("mysql -B -N -e 'show databases'")
        self.databases = f.read().splitlines()
        DebugMessage(context, 100, "databases: %s\n"%(self.databases));
        return bRCs['bRC_OK'];


    def start_backup_file(self,context, savepkt):
        '''
        '''
        DebugMessage(context, 100, "start_backup called\n");
        if not self.databases:
            DebugMessages(context,100,"No databases to backup")

        db = self.databases.pop()
        statp = StatPacket()
        savepkt.statp = statp
        savepkt.fname = "/_mysqlbackups_/"+db+".sql.gz"
        savepkt.type = bFileType['FT_REG']

        self.stream = os.popen("/usr/bin/mysqldump %s --events --single-transaction | gzip"%(db))

        JobMessage(context, bJobMessageType['M_INFO'], "Starting backup of " + savepkt.fname + "\n");
        return bRCs['bRC_OK'];

    def plugin_io(self, context, IOP):
        DebugMessage(context, 100, "plugin_io called with " + str(IOP) + "\n");

        if IOP.func == bIOPS['IO_OPEN']:
            try:
                if IOP.flags & (O_CREAT | O_WRONLY):
                    self.file = open(IOP.fname, 'wb');
            except Exception,msg:
                IOP.status = -1;
                DebugMessage(context, 100, "Error opening file: " + msg + "\n");
                return bRCs['bRC_Error'];
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_READ']:
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.stream.readinto(IOP.buf)
            IOP.io_errno = 0
            return bRCs['bRC_OK']
        
        elif IOP.func == bIOPS['IO_WRITE']:
            try:
                self.file.write(IOP.buf);
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError,msg:
                IOP.io_errno = -1
                DebugMessage(context, 100, "Error writing data: " + msg + "\n");
            return bRCs['bRC_OK'];

        elif IOP.func == bIOPS['IO_CLOSE']:
            if self.file:
                self.file.close()
            return bRCs['bRC_OK']
        
        elif IOP.func == bIOPS['IO_SEEK']:
            return bRCs['bRC_OK']
        
        else:
            DebugMessage(context,100,"plugin_io called with unsupported IOP:"+str(IOP.func)+"\n")
            return bRCs['bRC_OK']

    def end_backup_file(self, context):
        '''
        '''
        DebugMessage(context, 100, "end_backup_file() entry point in Python called\n")
        if self.databases:
            return bRCs['bRC_More']
        else:
            return bRCs['bRC_OK'];


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
