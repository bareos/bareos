#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
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
# Author: Maik Aussendorf
#
# Bareos python plugins class that adds files from a local list to
# the backup fileset

from bareosfd import *
from bareos_fd_consts import bJobMessageType, bFileType, bRCs
import os
import sys
import re
import psycopg2
import time
import datetime
from dateutil import parser
import dateutil
import json
import BareosFdPluginLocalFileset
from BareosFdPluginBaseclass import *


class BareosFdPluginPostgres(
    BareosFdPluginLocalFileset.BareosFdPluginLocalFileset
):  # noqa
    """
    Simple Bareos-FD-Plugin-Class that parses a file and backups all files
    listed there Filename is taken from plugin argument 'filename'
    """

    def __init__(self, context, plugindef):
        bareosfd.DebugMessage(
            context,
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        # Last argument of super constructor is a list of mandatory arguments
        super(BareosFdPluginPostgres, self).__init__(
            context, plugindef, ["walArchive"]
        )
        self.ignoreSubdirs = ["pg_wal", "pg_log", "pg_xlog"]

        self.dbCon = None
        self.dbCursor = None
        # This will be set to True between SELCET pg_start_backup
        # and SELECT pg_stop_backup. We backup database file during
        # this time
        self.PostgressFullBackupRunning = False
        # Here we store items found in file backup_label, produced by Postgres
        self.labelItems = dict()
        # We will store the starttime from backup_label here
        self.backupStartTime = None
        # Postgresql backup stop time
        self.lastBackupStopTime = 0
        # Our label, will be used for SELECT pg_start_backup and there
        # be used as backup_label
        self.backupLabelString = "Bareos.pgplugin.jobid.%d" % self.jobId
        # Raw restore object data (json-string)
        self.row_rop_raw = None
        # Dictionary to store passed restore object data
        self.rop_data = {}
        # we need our timezone information for some timestamp comparisons
        # this one respects daylight saving timezones
        self.tzOffset = -(
            time.altzone
            if (time.daylight and time.localtime().tm_isdst)
            else time.timezone
        )

    def check_options(self, context, mandatory_options=None):
        """
        Check for mandatory options and verify database connection
        """
        result = super(BareosFdPluginPostgres, self).check_options(
            context, mandatory_options
        )
        if not result == bRCs["bRC_OK"]:
            return result
        if not self.options["postgresDataDir"].endswith("/"):
            self.options["postgresDataDir"] += "/"
        self.labelFileName = self.options["postgresDataDir"] + "/backup_label"
        if not self.options["walArchive"].endswith("/"):
            self.options["walArchive"] += "/"
        if "ignoreSubdirs" in self.options:
            self.ignoreSubdirs = self.options["ignoreSubdirs"]
        if "dbname" in self.options:
            self.dbname = self.options["dbname"]
        else:
            self.dbname = "postgres"
        if "dbuser" in self.options:
            self.dbuser = self.options["dbuser"]
        else:
            self.dbuser = "root"
        if not "switchWal" in self.options:
            self.switchWal = True
        else:
            self.switchWal = self.options["switchWal"].lower() == "true"
        return bRCs["bRC_OK"]

    def start_backup_job(self, context):
        """
        Make filelist in super class and tell Postgres
        that we start a backup now
        """
        bareosfd.DebugMessage(
            context, 100, "start_backup_job in PostgresPlugin called",
        )
        try:
            self.dbCon = psycopg2.connect(
                "dbname=%s user=%s" % (self.dbname, self.dbuser)
            )
            self.dbCursor = self.dbCon.cursor()
            self.dbCursor.execute("SELECT current_setting('server_version_num')")
            self.pgVersion = int(self.dbCursor.fetchone()[0])
            #bareosfd.DebugMessage(
            #    context, 1, "Connected to Postgres version %d\n" % self.pgVersion,
            #)
            ## WARNING: JobMessages cause fatal errors at this stage
            JobMessage(
               context,
               bJobMessageType["M_INFO"],
               "Connected to Postgres version %d\n"
               % (self.pgVersion),
            )
        except:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "Could not connect to database %s, user %s\n"
                % (self.dbname, self.dbuser),
            )
            return bRCs["bRC_Error"]
        # If level is not Full, we only backup newer WAL files
        if chr(self.level) == "F":
            startDir = self.options["postgresDataDir"]
            self.files_to_backup.append(startDir)
            bareosfd.DebugMessage(
                context, 100, "dataDir: %s\n" % self.options["postgresDataDir"],
            )
        else:
            # For Full the Restore object comes after DB connection close
            self.files_to_backup.append("ROP")
            # With incremental / diff jobs we just backup new WAL files
            startDir = self.options["walArchive"]
            # get current Log Sequence Number (LSN)
            # PG8: not supported
            # PG9: pg_get_current_xlog_location
            # PG10: pg_current_wal_lsn
            pgMajorVersion = self.pgVersion // 10000
            if pgMajorVersion >= 10:
                getLsnStmt = "SELECT pg_current_wal_lsn()"
                switchLsnStmt = "SELECT pg_switch_wal()"
            elif pgMajorVersion >= 9:
                getLsnStmt = "SELECT pg_current_xlog_location()"
                switchLsnStmt = "SELECT pg_switch_xlog()"
            if pgMajorVersion < 9:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_INFO"],
                    "WAL switching not supported on Postgres Version < 9\n",
                )
            else:
                self.dbCursor.execute(getLsnStmt)
                currentLSN = self.dbCursor.fetchone()[0].zfill(17)
                bareosfd.DebugMessage(
                    context,
                    100,
                    "Current LSN %s, last LSN: %s\n" % (currentLSN, self.lastLSN),
                )
                if currentLSN > self.lastLSN and self.switchWal:
                    # Let Postgres write latest transaction into a new WAL file now
                    self.dbCursor.execute(switchLsnStmt)
                    # reread LSN as it changes after WAL switching
                    self.dbCursor.execute(getLsnStmt)
                    currentLSN = self.dbCursor.fetchone()[0].zfill(17)
                    self.lastLSN = currentLSN
                    # wait some seconds to make sure WAL file gets written
                    time.sleep(10)
                else:
                    # Nothing has changed since last backup - only send ROP this time
                    bareosfd.JobMessage(
                        context,
                        bJobMessageType["M_INFO"],
                        "Same LSN %s as last time - nothing to do\n" % currentLSN,
                    )
                    return bRCs["bRC_OK"]

        # Gather files from startDir (Postgres data dir or walArchive for incr/diff jobs)
        for fileName in os.listdir(startDir):
            fullName = os.path.join(startDir, fileName)
            # We need a trailing '/' for directories
            if os.path.isdir(fullName) and not fullName.endswith("/"):
                fullName += "/"
                bareosfd.DebugMessage(
                    context, 100, "fullName: %s\n" % fullName,
                )
            # Usually Bareos takes care about timestamps when doing incremental backups
            # but here we have to compare against last BackupPostgres timestamp
            mTime = os.stat(fullName).st_mtime
            bareosfd.DebugMessage(
                context,
                150,
                "%s fullTime: %d mtime: %d\n"
                % (fullName, self.lastBackupStopTime, mTime),
            )
            if mTime > self.lastBackupStopTime + 1:
                bareosfd.DebugMessage(
                    context,
                    150,
                    "file: %s, fullTime: %d mtime: %d\n"
                    % (fullName, self.lastBackupStopTime, mTime),
                )
                self.files_to_backup.append(fullName)
                if os.path.isdir(fullName) and fileName not in self.ignoreSubdirs:
                    for topdir, dirNames, fileNames in os.walk(fullName):
                        for fileName in fileNames:
                            self.files_to_backup.append(os.path.join(topdir, fileName))
                        for dirName in dirNames:
                            fullDirName = os.path.join(topdir, dirName) + "/"
                            self.files_to_backup.append(fullDirName)

        # If level is not Full, we are done here and set the new
        # lastBackupStopTime as reference for future jobs
        # Will be written into the Restore Object
        if not chr(self.level) == "F":
            self.lastBackupStopTime = int(time.time())
            return bRCs["bRC_OK"]

        # For Full we check for a running job and tell Postgres that
        # we want to backup the DB files now.
        if os.path.exists(self.labelFileName):
            self.parseBackupLabelFile(context)
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                'Another Postgres Backup Operation "%s" is in progress. '
                % self.labelItems["LABEL"]
                + "You may stop it using SELECT pg_stop_backup()",
            )
            return bRCs["bRC_Error"]

        bareosfd.DebugMessage(
            context, 100, "Send 'SELECT pg_start_backup' to Postgres\n"
        )
        # We tell Postgres that we want to start to backup file now
        self.backupStartTime = datetime.datetime.now(
            tz=dateutil.tz.tzoffset(None, self.tzOffset)
        )
        self.dbCursor.execute("SELECT pg_start_backup('%s');" % self.backupLabelString)
        results = self.dbCursor.fetchall()
        bareosfd.DebugMessage(context, 150, "Start response: %s\n" % str(results))
        bareosfd.DebugMessage(
            context, 150, "Adding label file %s to fileset\n" % self.labelFileName
        )
        self.files_to_backup.append(self.labelFileName)
        bareosfd.DebugMessage(
            context, 150, "Filelist: %s\n" % (self.files_to_backup),
        )
        self.PostgressFullBackupRunning = True
        return bRCs["bRC_OK"]

    def parseBackupLabelFile(self, context):
        try:
            labelFile = open(self.labelFileName, "rb")
        except:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_WARNING"],
                "Could not open Label File %s" % (self.labelFileName),
            )

        for labelItem in labelFile.read().splitlines():
            print labelItem
            k, v = labelItem.split(":", 1)
            self.labelItems.update({k.strip(): v.strip()})
        labelFile.close()
        bareosfd.DebugMessage(context, 150, "Labels read: %s\n" % str(self.labelItems))

    def start_backup_file(self, context, savepkt):
        """
        For normal files we call the super method
        Special objects are treated here
        """
        if not self.files_to_backup:
            bareosfd.DebugMessage(context, 100, "No files to backup\n")
            return bRCs["bRC_Skip"]

        # Plain files are handled by super class
        if self.files_to_backup[-1] not in ["ROP"]:
            return super(BareosFdPluginPostgres, self).start_backup_file(
                context, savepkt
            )

        # Here we create the restore object
        self.file_to_backup = self.files_to_backup.pop()
        bareosfd.DebugMessage(context, 100, "file: " + self.file_to_backup + "\n")
        savepkt.statp = StatPacket()
        if self.file_to_backup == "ROP":
            self.rop_data["lastBackupStopTime"] = self.lastBackupStopTime
            self.rop_data["lastLSN"] = self.lastLSN
            savepkt.fname = "/_bareos_postgres_plugin/metadata"
            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.object_name = savepkt.fname
            bareosfd.DebugMessage(context, 150, "fname: " + savepkt.fname + "\n")
            bareosfd.DebugMessage(context, 150, "rop " + str(self.rop_data) + "\n")
            savepkt.object = bytearray(json.dumps(self.rop_data))
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
        else:
            # should not happen
            JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "Unknown error. Don't know how to handle %s\n" % self.file_to_backup,
            )
        return bRCs["bRC_OK"]

    def restore_object_data(self, context, ROP):
        """
        Called on restore and on diff/inc jobs.
        """
        # Improve: sanity / consistence check of restore object
        self.row_rop_raw = ROP.object
        self.rop_data[ROP.jobid] = json.loads(str(self.row_rop_raw))
        if "lastBackupStopTime" in self.rop_data[ROP.jobid]:
            self.lastBackupStopTime = int(
                self.rop_data[ROP.jobid]["lastBackupStopTime"]
            )
            JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "Got lastBackupStopTime %d from restore object of job %d\n"
                % (self.lastBackupStopTime, ROP.jobid),
            )
        if "lastLSN" in self.rop_data[ROP.jobid]:
            self.lastLSN = self.rop_data[ROP.jobid]["lastLSN"]
            JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "Got lastLSN %s from restore object of job %d\n"
                % (self.lastLSN, ROP.jobid),
            )
        return bRCs["bRC_OK"]

    def closeDbConnection(self, context):
        # TODO Error Handling
        # Get Backup Start Date
        self.parseBackupLabelFile(context)
        # self.dbCursor.execute("SELECT pg_backup_start_time()")
        # self.backupStartTime = self.dbCursor.fetchone()[0]
        # Tell Postgres we are done
        self.dbCursor.execute("SELECT pg_stop_backup();")
        self.lastLSN = self.dbCursor.fetchone()[0].zfill(17)
        self.lastBackupStopTime = int(time.time())
        results = self.dbCursor.fetchall()
        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Database connection closed. "
            + "CHECKPOINT LOCATION: %s, " % self.labelItems["CHECKPOINT LOCATION"]
            + "START WAL LOCATION: %s\n" % self.labelItems["START WAL LOCATION"],
        )
        self.PostgressFullBackupRunning = False

    def checkForWalFiles(self, context):
        """
        Look for new WAL files and backup
        Backup start time is timezone aware, we need to add timezone
        to files' mtime to make them comparable
        """
        # We have to add local timezone to the file's timestamp in order
        # to compare them with the backup starttime, which has a timezone
        walArchive = self.options["walArchive"]
        self.files_to_backup.append(walArchive)
        for fileName in os.listdir(walArchive):
            fullPath = os.path.join(walArchive, fileName)
            st = os.stat(fullPath)
            fileMtime = datetime.datetime.fromtimestamp(st.st_mtime)
            if (
                fileMtime.replace(tzinfo=dateutil.tz.tzoffset(None, self.tzOffset))
                > self.backupStartTime
            ):
                bareosfd.DebugMessage(
                    context, 150, "Adding WAL file %s for backup\n" % fileName
                )
                self.files_to_backup.append(fullPath)

        if self.files_to_backup:
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        """
        Here we return 'bRC_More' as long as our list files_to_backup is not
        empty and bRC_OK when we are done
        """
        bareosfd.DebugMessage(
            context, 100, "end_backup_file() entry point in Python called\n"
        )
        if self.files_to_backup:
            return bRCs["bRC_More"]
        else:
            if self.PostgressFullBackupRunning:
                self.closeDbConnection(context)
                # Now we can also create the Restore object with the right timestamp
                self.files_to_backup.append("ROP")
                return self.checkForWalFiles(context)
            else:
                return bRCs["bRC_OK"]

    def end_backup_job(self, context):
        """
        Called if backup job ends, before ClientAfterJob 
        Make sure that dbconnection was closed in any way,
        especially when job was cancelled
        """
        if self.PostgressFullBackupRunning:
            self.closeDbConnection(context)
            self.PostgressFullBackupRunning = False
        return bRCs["bRC_OK"]


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
