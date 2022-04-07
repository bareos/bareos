#!/usr/bin/env python
# -*- coding: utf-8 -*-

# originally contributed by:
# Copyright 2014 Battelle Memorial Institute
# Written by Evan Felix
# With additions from Maik Aussendorf, Bareos GmbH & Co. KG 2015

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

from bareosfd import *
import os
from subprocess import *
from  BareosFdPluginBaseclass import *
import BareosFdWrapper

class BareosFdMySQLclass(BareosFdPluginBaseclass):
    '''
        Plugin for backing up all mysql databases found in a specific mysql server
    '''       
    def __init__(self, plugindef):
        BareosFdPluginBaseclass.__init__(self, plugindef)
        self.file=None

    def parse_plugin_definition(self, plugindef):
        '''
        '''
        BareosFdPluginBaseclass.parse_plugin_definition(self, plugindef)

        # mysql host and credentials, by default we use localhost and root and
        # prefer to have a my.cnf with mysql credentials
        self.mysqlconnect = ''

        if 'dumpbinary' in self.options:
            self.dumpbinary = self.options['dumpbinary']
        else:
            self.dumpbinary = "mysqldump"

        # if dumpotions is set, we use that completely here, otherwise defaults
        if 'dumpoptions' in self.options:
            self.dumpoptions = self.options['dumpoptions']
        else:
            self.dumpoptions = " --events --single-transaction "
            # default is to add the drop statement
            if not 'drop_and_recreate' in self.options or not self.options['drop_and_recreate'] == 'false':
                self.dumpoptions += " --add-drop-database --databases "

        # if defaultsfile is set
        if 'defaultsfile' in self.options:
            self.defaultsfile = self.options['defaultsfile']
            self.mysqlconnect += " --defaults-file=" + self.defaultsfile

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
            showDbCommand = "mysql %s -B -N -e 'show databases'" % self.mysqlconnect
            showDb = Popen(showDbCommand, shell=True, stdout=PIPE, stderr=PIPE)
            databases = showDb.stdout.read()
            if isinstance(databases, bytes):
                databases = databases.decode('UTF-8')
            self.databases = databases.splitlines()
            if 'performance_schema' in self.databases:
                self.databases.remove('performance_schema')
            if 'information_schema' in self.databases:
                self.databases.remove('information_schema')
            showDb.wait()
            returnCode = showDb.poll()
            if returnCode == None:
                JobMessage(
                    M_FATAL,
                    "No databases specified and show databases failed for unknown reason.\n"
                )
                DebugMessage(
                    10,
                    "Failed mysql command: '%s'"
                    % showDbCommand
                )
                return bRC_Error
            if returnCode != 0:
                (stdOut, stdError) = showDb.communicate()    
                JobMessage(
                    M_FATAL,
                    "No databases specified and show databases failed. %s\n"
                    % stdError
                )
                DebugMessage(
                    10,
                    "Failed mysql command: '%s'"
                    % showDbCommand
                )
                return bRC_Error

        if 'ignore_db' in self.options:
            DebugMessage(
                100,
                "databases in ignore list: %s\n" 
                % (self.options['ignore_db'].split(','))
            )
            for ignored_cur in self.options['ignore_db'].split(','):
                try:
                    self.databases.remove(ignored_cur)
                except:
                    pass
        DebugMessage(
            100,
            "databases to backup: %s\n"
            % (self.databases)
        )
        return bRC_OK


    def start_backup_file(self, savepkt):
        '''
        This method is called, when Bareos is ready to start backup a file
        For each database to backup we create a mysqldump subprocess, wrting to
        the pipe self.stream.stdout
        '''
        DebugMessage(
            100,
            "start_backup called\n"
        )
        if not self.databases:
            DebugMessage(
                100,
                "No databases to backup.\n"
            )
            JobMessage(
                M_ERROR,
                "No databases to backup.\n"
            )
            return bRC_Skip

        db = self.databases.pop()

        sizeDbCommand = "mysql %s -B -N -e 'SELECT (SUM(DATA_LENGTH + INDEX_LENGTH)) FROM information_schema.TABLES WHERE TABLE_SCHEMA = \"%s\"'" % (self.mysqlconnect, db)
        sizeDb = Popen(sizeDbCommand, shell=True, stdout=PIPE, stderr=PIPE)
        size_curr_db = sizeDb.stdout.read()
        sizeDb.wait()
        sizereturnCode = sizeDb.poll()

        statp = StatPacket()
        savepkt.statp = statp
        savepkt.fname = "@mysqlbackup@/{}.sql".format(db)
        savepkt.type = FT_REG

        dumpcommand = ("%s %s %s %s" % (self.dumpbinary, self.mysqlconnect, db, self.dumpoptions))
        DebugMessage(
            100,
            "Dumper: '" + dumpcommand + "'\n"
        )
        self.stream = Popen(dumpcommand, shell=True, stdout=PIPE, stderr=PIPE)

        JobMessage(
            M_INFO,
            "Starting backup of " + savepkt.fname + "\n"
        )
        return bRC_OK


    def plugin_io(self, IOP):
        '''
        Called for io operations. We read from pipe into buffers or on restore
        create a file for each database and write into it.
        '''
        DebugMessage(
            100,
            "plugin_io called with " + str(IOP.func) + "\n"
        )

        if IOP.func == IO_OPEN:
            try:
                if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                    self.file = open(IOP.fname, 'wb');
            except Exception as exception:
                IOP.status = -1;
                DebugMessage(
                    100,
                    "Error opening file: " + IOP.fname + "\n"
                )
                print(exception);
                return bRC_Error
            return bRC_OK

        elif IOP.func == IO_READ:
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.stream.stdout.readinto(IOP.buf)
            IOP.io_errno = 0
            return bRC_OK
        
        elif IOP.func == IO_WRITE:
            try:
                self.file.write(IOP.buf);
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as msg:
                IOP.io_errno = -1
                DebugMessage(
                    100,
                    "Error writing data: " + str(msg) + "\n"
                )
            return bRC_OK

        elif IOP.func == IO_CLOSE:
            if self.file:
                self.file.close()
            return bRC_OK
        
        elif IOP.func == IO_SEEK:
            return bRC_OK
        
        else:
            DebugMessage(
                100,
                "plugin_io called with unsupported IOP:" + str(IOP.func) + "\n"
            )
            return bRC_OK

    def end_backup_file(self):
        '''
        Check, if dump was successfull.
        '''
        # Usually the mysqldump process should have terminated here, but on some servers
        # it has not always.
        self.stream.wait()
        returnCode = self.stream.poll()
        if returnCode == None:
            JobMessage(
                M_ERROR,
                "Dump command not finished properly for unknown reason.\n"
            )
            returnCode = -99
        else:
            DebugMessage(
                100,
                "end_backup_file() entry point in Python called. Returncode: %d\n" 
                % self.stream.returncode
            )
            if returnCode != 0:
                (stdOut, stdError) = self.stream.communicate()
                if stdError == None:
                    stdError = ''
                JobMessage(
                    M_ERROR, 
                    "Dump command returned non-zero value: %d, message: %s\n" 
                    % (returnCode,stdError)
                )
            
        if self.databases:
                return bRC_More
        else:
            if returnCode == 0:
                return bRC_OK
            else:
                return bRC_Error


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
