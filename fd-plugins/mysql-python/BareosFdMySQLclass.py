#!/usr/bin/env python
# -*- coding: utf-8 -*-

#Adapted from examples for mysql full backups
#Copyright 2014 Battelle Memorial Institute
#Written by Evan Felix

from bareosfd import *
from bareos_fd_consts import *
import os
from subprocess import *
from  BareosFdPluginBaseclass import *
import BareosFdWrapper


class BareosFdMySQLclass (BareosFdPluginBaseclass):
    '''
        Plugin for backing up all mysql databases found in a specific mysql server
    '''       
    def __init__(self, context, plugindef):
        BareosFdPluginBaseclass.__init__(self, context, plugindef)
        self.file=None

    def parse_plugin_definition(self,context, plugindef):
        '''
        '''
        BareosFdPluginBaseclass.parse_plugin_definition(self, context, plugindef)

        # mysql host and credentials, by default we use localhost and root and
        # prefer to have a my.cnf with mysql credentials
        self.mysqlconnect = ''

        if 'dumpbinary' in self.options:
            self.dumpbinary = self.options['dumpbinary']
        else:
            self.dumpbinary = "mysqldump"

        if 'dumpoptions' in self.options:
            self.dumpoptions = self.options['dumpoptions']
        else:
            self.dumpoptions = " --events --single-transaction"

        if 'mysqlhost' in self.options:
            self.mysqlhost = self.options['mysqlhost']
            self.mysqlconnect += " -h " + self.mysqlhost

        if 'mysqluser' in self.options:
            self.mysqluser = self.options['mysqluser']
            self.mysqlconnect += " -u " + self.mysqluser

        if 'mysqlpassword' in self.options:
            self.mysqlpassword = self.options['mysqlpassword']
            self.mysqlconnect += " --password=" + self.mysqlpassword

        # if plugin has db configured (a list of comma separated databases to backup
        # we use it here as list of databases to backup
        if 'db' in self.options:
            self.databases = self.options['db'].split(',')
        # Otherwise we backup all existing databases
        else:
            showDbCommand = "mysql %s -B -N -e 'show databases'" %self.mysqlconnect
            showDb = Popen(showDbCommand, shell=True, stdout=PIPE, stderr=PIPE)
            self.databases = showDb.stdout.read().splitlines()
            showDb.wait()
            returnCode = showDb.poll()
            if returnCode != 0:
                (stdOut, stdError) = showDb.communicate()    
                JobMessage(context, bJobMessageType['M_FATAL'], "No databases specified and show databases failed. %s" %stdError);
                DebugMessage(context, 10, "Failed mysql command: '%s'" %showDbCommand)
                return bRCs['bRC_Error'];

        DebugMessage(context, 100, "databases to backup: %s\n" %(self.databases));
        return bRCs['bRC_OK'];


    def start_backup_file(self,context, savepkt):
        '''
        This method is called, when Bareos is ready to start backup a file
        For each database to backup we create a mysqldump subprocess, wrting to
        the pipe self.stream.stdout
        '''
        DebugMessage(context, 100, "start_backup called\n");
        if not self.databases:
            DebugMessage(context,100,"No databases to backup")
            JobMessage(context, bJobMessageType['M_ERROR'], "No databases to backup.\n");
            return bRCs['bRC_Skip']

        db = self.databases.pop()
        statp = StatPacket()
        savepkt.statp = statp
        savepkt.fname = "/_mysqlbackups_/"+db+".sql"
        savepkt.type = bFileType['FT_REG']

        dumpcommand = ("%s %s %s %s" %(self.dumpbinary, db, self.mysqlconnect, self.dumpoptions))
        DebugMessage(context, 100, "Dumper: '" + dumpcommand + "'\n")
        self.stream = Popen(dumpcommand, shell=True, stdout=PIPE, stderr=PIPE)

        JobMessage(context, bJobMessageType['M_DEBUG'], "Starting backup of " + savepkt.fname + "\n");
        return bRCs['bRC_OK'];


    def plugin_io(self, context, IOP):
        '''
        Called for io operations. We read from pipe into buffers or on restore
        create a file for each database and write into it.
        '''
        DebugMessage(context, 100, "plugin_io called with " + str(IOP.func) + "\n");

        if IOP.func == bIOPS['IO_OPEN']:
            try:
                if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                    self.file = open(IOP.fname, 'wb');
            except Exception,msg:
                IOP.status = -1;
                DebugMessage(context, 100, "Error opening file: " + IOP.fname + "\n");
                print msg;
                return bRCs['bRC_Error'];
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_READ']:
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.stream.stdout.readinto(IOP.buf)
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
        Check, if dump was successfull.
        '''
        returnCode = self.stream.poll()
        DebugMessage(context, 100, "end_backup_file() entry point in Python called. Returncode: %d\n" %self.stream.returncode)
        if returnCode != 0:
            (stdOut, stdError) = self.stream.communicate()
            if stdError == None:
                stdError = ''
            JobMessage(context, bJobMessageType['M_ERROR'], "Dump command returned non-zero value: %d, message: %s\n" %(returnCode,stdError));
            
        if self.databases:
                return bRCs['bRC_More']
        else:
            if returnCode == 0:
                return bRCs['bRC_OK'];
            else:
                return bRCs['bRC_Error']


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
