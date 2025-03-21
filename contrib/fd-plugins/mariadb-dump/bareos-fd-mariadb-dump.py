#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2023-2025 Bareos GmbH & Co. KG
#
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
#
# Author: Bareos Team
#
"""
bareos-fd-mariadb-dump is a plugin to dump mariadb database via mariadb-dump command
this is the successor of contrib bareos_mysql_dump
"""
import os
from subprocess import *
from sys import version_info
from sys import version
# Provided by the Bareos FD Python plugin interface
import bareosfd
from bareosfd import *

from BareosFdPluginBaseclass import BareosFdPluginBaseclass
from BareosFdWrapper import *  # noqa


@BareosPlugin
class BareosFdMariadbDump(BareosFdPluginBaseclass):  # noqa
    """
    Plugin for backing up all mariadb databases found in a specific mariadb server
    with native mariadb-dump binary
    """
    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100, f"Constructor called in module {__name__} with plugindef={plugindef}\n"
        )
        super(BareosFdMariadbDump, self).__init__(plugindef)
        bareosfd.DebugMessage(
            100,
            (
                f"Python Version: {version_info.major}.{version_info.minor}"
                f".{version_info.micro}\n"
            ),
        )
        self.file = None
        # mariadb host and credentials, by default we use localhost and root and
        # prefer to have a default my.cnf with mariadb credentials
        self.mariadbconnect = ""
        self.host = ""
        self.user = ""
        self.password = ""
        self.defaultsfile = ""
        self.databases = ""
        self.dumpbinary = "mariadb-dump"
        self.dumpoptions = " --events --single-transaction "

    def parse_plugin_definition(self, plugindef):
        """
        """
        bareosfd.DebugMessage(100, "parse_plugin_definition started\n")
        BareosFdPluginBaseclass.parse_plugin_definition(self, plugindef)

        if "dumpbinary" in self.options:
            self.dumpbinary = self.options["dumpbinary"]

        # if dumpoptions is set, we use it
        if "dumpoptions" in self.options:
            self.dumpoptions = self.options["dumpoptions"]
        # default is to add the drop statement
        if not "drop_and_recreate" in self.options or not self.options["drop_and_recreate"] == "false":
            self.dumpoptions += " --add-drop-database --databases "

        #add defaults-file if specified
        if "defaultsfile" in self.options:
            self.defaultsfile = self.options["defaultsfile"]
            self.mariadbconnect = f" --defaults-file={self.defaultsfile}"

        if "host" in self.options:
            self.host = self.options["host"]
            self.mariadbconnect += f" --host={self.host}"

        if "user" in self.options:
            self.user = self.options["user"]
            self.mariadbconnect += f" --user={self.user}"

        if "password" in self.options:
            self.password = self.options['password']
            self.mariadbconnect += f" --password={self.password}"

        bareosfd.DebugMessage(150, f"mariadbconnect={self.mariadbconnect}\n")
        # if plugin has db configured (a list of comma separated databases to backup
        # we use it here as list of databases to backup
        if "db" in self.options:
            self.databases = self.options['db'].split(',')
        # Otherwise we backup all existing databases
        else:
            showDbCommand = f"mariadb {self.mariadbconnect} -B -N -e 'show databases'"
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
                    f"Failed mariadb command: '{showDbCommand}'"
                )
                return bRC_Error
            if returnCode != 0:
                (stdOut, stdError) = showDb.communicate()
                JobMessage(
                    M_FATAL,
                    f"No databases specified and show databases failed. {stdError}\n"
                )
                DebugMessage(
                    10,
                    f"Failed mariadb command: '{showDbCommand}'"
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
        For each database to backup we create a mariadb-dump subprocess, wrting to
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

        sizeDbCommand = f"mariadb {self.mariadbconnect} -B -N -e 'SELECT (SUM(DATA_LENGTH + INDEX_LENGTH)) FROM information_schema.TABLES WHERE TABLE_SCHEMA = \"{db}\"'"
        sizeDb = Popen(sizeDbCommand, shell=True, stdout=PIPE, stderr=PIPE)
        size_curr_db = sizeDb.stdout.read()
        sizeDb.wait()
        sizereturnCode = sizeDb.poll()

        statp = StatPacket()
        savepkt.statp = statp
        savepkt.fname = "@mariadbbackup@/{}.sql".format(db)
        savepkt.type = FT_REG

        dumpcommand = f"{self.dumpbinary} {self.mariadbconnect} {db} {self.dumpoptions}"
        DebugMessage(
            100,
            f"Dumper: '{dumpcommand}'\n"
        )
        self.stream = Popen(dumpcommand, shell=True, stdout=PIPE, stderr=PIPE)

        JobMessage(
            M_INFO,
            f"Starting backup of '{savepkt.fname}'\n"
        )
        return bRC_OK


    def plugin_io(self, IOP):
        """
        Called for io operations. We read from pipe into buffers or on restore
        create a file for each database and write into it.
        """
        DebugMessage(
            100,
            f"plugin_io called with '{str(IOP.func)}'\n"
        )

        if IOP.func == IO_OPEN:
            try:
                if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                    self.file = open(IOP.fname, 'wb');
            except Exception as exception:
                IOP.status = -1;
                DebugMessage(
                    100,
                    f"Error opening file: '{IOP.fname}'\n"
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
                    f"Error writing data: '{str(msg)}'\n"
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
                f"plugin_io called with unsupported IOP: '{str(IOP.func)}'\n"
            )
            return bRC_OK

    def end_backup_file(self):
        """
        Check, if dump was successfull.
        """
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
                f"end_backup_file() entry point in Python called. Returncode: {self.stream.returncode}\n"
            )
            if returnCode != 0:
                (stdOut, stdError) = self.stream.communicate()
                if stdError == None:
                    stdError = ''
                JobMessage(
                    M_ERROR,
                    f"Dump command returned non-zero value: {returnCode}, message: {stdError}\n"
                )

        if self.databases:
                return bRC_More
        else:
            if returnCode == 0:
                return bRC_OK
            else:
                return bRC_Error


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
