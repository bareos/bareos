#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2021 Bareos GmbH & Co. KG
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
# Bareos python plugins class that performs online backups (full and incremental)
# from Postgres databases. Supports point-in-time (PIT) restores.

import os
import pg8000  # minimum required version is 1.16
import time
import datetime
import dateutil.parser
import dateutil
import json
import getpass
from BareosFdPluginLocalFilesBaseclass import BareosFdPluginLocalFilesBaseclass
import bareosfd


class BareosFdPluginPostgres(BareosFdPluginLocalFilesBaseclass):  # noqa
    """
    Bareos-FD-Plugin-Class for PostgreSQL online (Hot) backup of database
    files and database transaction logs (WAL) archiving to allow incrmental
    backups and point-in-time recovery.
    """

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )

        # check if pg8000 module is new enough
        major, minor, iter = pg8000.__version__.split(".")
        if int(major) < 1 or (int(major) == 1 and int(minor) < 16):
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "FATAL ERROR: pg8000 module is too old({}), required is >=1.16\n".format(
                    pg8000.__version__
                ),
            )

        # Last argument of super constructor is a list of mandatory arguments
        super(BareosFdPluginPostgres, self).__init__(
            plugindef, ["postgresDataDir", "walArchive"]
        )
        self.ignoreSubdirs = ["pg_wal", "pg_log", "pg_xlog"]

        self.dbCon = None
        # This will be set to True between SELECT pg_start_backup
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

    def check_options(self, mandatory_options=None):
        """
        Check for mandatory options and verify database connection
        """
        result = super(BareosFdPluginPostgres, self).check_options(mandatory_options)
        if not result == bareosfd.bRC_OK:
            return result
        # Accurate may cause problems with plugins
        accurate_enabled = bareosfd.GetValue(bareosfd.bVarAccurate)
        if accurate_enabled is not None and accurate_enabled != 0:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "start_backup_job: Accurate backup not allowed please disable in Job\n",
            )
            return bareosfd.bRC_Error
        if not self.options["postgresDataDir"].endswith("/"):
            self.options["postgresDataDir"] += "/"
        self.labelFileName = self.options["postgresDataDir"] + "backup_label"
        if not self.options["walArchive"].endswith("/"):
            self.options["walArchive"] += "/"

        if "ignoreSubdirs" in self.options:
            self.ignoreSubdirs = self.options["ignoreSubdirs"]

        # get postgresql connection settings from environment as libpq also uses them
        self.dbuser = os.environ.get("PGUSER", getpass.getuser())
        self.dbpassword = os.environ.get("PGPASSWORD", "")
        self.dbHost = os.environ.get("PGHOST", "localhost")
        self.dbport = os.environ.get("PGPORT", "5432")
        self.dbname = os.environ.get("PGDATABASE", "postgres")

        if "dbname" in self.options:
            self.dbname = self.options.get("dbname", "postgres")
        if "dbuser" in self.options:
            self.dbuser = self.options.get("dbuser")

        # emulate behaviour of libpq handling of unix domain sockets
        # i.e. there only the socket directory is set and ".s.PGSQL.<dbport>"
        # is appended before opening
        if "dbHost" in self.options:
            if self.options["dbHost"].startswith("/"):
                if not self.options["dbHost"].endswith("/"):
                    self.options["dbHost"] += "/"
                self.dbHost = self.options["dbHost"] + ".s.PGSQL." + self.dbport
            else:
                self.dbHost = self.options["dbHost"]

        self.switchWal = True
        if "switchWal" in self.options:
            self.switchWal = self.options["switchWal"].lower() == "true"

        self.switchWalTimeout = 60
        if "switchWalTimeout" in self.options:
            try:
                self.switchWalTimeout = int(self.options["switchWalTimeout"])
            except ValueError as e:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "start_backup_job: Plugin option switchWalTimeout %s is not an integer value\n"
                    % (self.options["switchWalTimeout"]),
                )
                return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def formatLSN(self, rawLSN):
        """
        Postgres returns LSN in a non-comparable format with varying length, e.g.
        0/3A6A710
        0/F00000
        We fill the part before the / and after it with leading 0 to get strings with equal length
        """
        lsnPre, lsnPost = rawLSN.split("/")
        return lsnPre.zfill(8) + "/" + lsnPost.zfill(8)

    def start_backup_job(self):
        """
        Make filelist in super class and tell Postgres
        that we start a backup now
        """
        bareosfd.DebugMessage(100, "start_backup_job in PostgresPlugin called")
        try:
            if self.options["dbHost"].startswith("/"):
                self.dbCon = pg8000.Connection(
                    self.dbuser, database=self.dbname, unix_sock=self.dbHost
                )
            else:
                self.dbCon = pg8000.Connection(
                    self.dbuser, database=self.dbname, host=self.dbHost
                )

            result = self.dbCon.run("SELECT current_setting('server_version_num')")
            self.pgVersion = int(result[0][0])
            # WARNING: JobMessages cause fatal errors at this stage
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "Connected to Postgres version %d\n" % self.pgVersion,
            )
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Could not connect to database %s, user %s, host: %s: %s\n"
                % (self.dbname, self.dbuser, self.dbHost, e),
            )
            return bareosfd.bRC_Error
        if chr(self.level) == "F":
            # For Full we backup the Postgres data directory
            # Restore object ROP comes later, after file backup
            # is done.
            startDir = self.options["postgresDataDir"]
            self.files_to_backup.append(startDir)
            bareosfd.DebugMessage(
                100, "dataDir: %s\n" % self.options["postgresDataDir"]
            )
            bareosfd.JobMessage(
                bareosfd.M_INFO, "dataDir: %s\n" % self.options["postgresDataDir"]
            )
        else:
            # If level is not Full, we only backup WAL files
            # and create a restore object ROP with timestamp information.
            startDir = self.options["walArchive"]
            self.files_to_backup.append("ROP")
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
                    bareosfd.M_INFO,
                    "WAL switching not supported on Postgres Version < 9\n",
                )
            else:
                try:
                    currentLSN = self.formatLSN(self.dbCon.run(getLsnStmt)[0][0])
                    bareosfd.JobMessage(
                        bareosfd.M_INFO,
                        "Current LSN %s, last LSN: %s\n" % (currentLSN, self.lastLSN),
                    )
                except Exception as e:
                    currentLSN = 0
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        "Could not get current LSN, last LSN was: %s : %s \n"
                        % (self.lastLSN, e),
                    )
                if currentLSN > self.lastLSN and self.switchWal:
                    # Let Postgres write latest transaction into a new WAL file now
                    try:
                        result = self.dbCon.run(switchLsnStmt)
                    except Exception as e:
                        bareosfd.JobMessage(
                            bareosfd.M_WARNING,
                            "Could not switch to next WAL segment: %s\n" % e,
                        )
                    try:
                        result = self.dbCon.run(getLsnStmt)
                        currentLSNraw = result[0][0]
                        currentLSN = self.formatLSN(currentLSNraw)

                        bareosfd.DebugMessage(
                            150,
                            "after pg_switch_wal(): currentLSN: %s lastLSN: %s\n"
                            % (currentLSN, self.lastLSN),
                        )

                        self.lastLSN = currentLSN

                    except Exception as e:
                        bareosfd.JobMessage(
                            bareosfd.M_WARNING,
                            "Could not read LSN after switching to new WAL segment: %s\n"
                            % e,
                        )

                    if not self.wait_for_wal_archiving(currentLSNraw):
                        return bareosfd.bRC_Error

                else:
                    # Nothing has changed since last backup - only send ROP this time
                    bareosfd.JobMessage(
                        bareosfd.M_INFO,
                        "Same LSN %s as last time - nothing to do\n" % currentLSN,
                    )
                    return bareosfd.bRC_OK

        # Gather files from startDir (Postgres data dir or walArchive for incr/diff jobs)
        for fileName in os.listdir(startDir):
            fullName = os.path.join(startDir, fileName)
            # We need a trailing '/' for directories
            if os.path.isdir(fullName) and not fullName.endswith("/"):
                fullName += "/"
                bareosfd.DebugMessage(100, "fullName: %s\n" % fullName)
            # Usually Bareos takes care about timestamps when doing incremental backups
            # but here we have to compare against last BackupPostgres timestamp
            try:
                mTime = os.stat(fullName).st_mtime
            except Exception as e:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    "Could net get stat-info for file %s: %s\n" % (fullName, e),
                )
                continue
            bareosfd.DebugMessage(
                150,
                "%s fullTime: %d mtime: %d\n"
                % (fullName, self.lastBackupStopTime, mTime),
            )
            if mTime > self.lastBackupStopTime + 1:
                bareosfd.DebugMessage(
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
            return bareosfd.bRC_OK

        # For Full we check for a running job and tell Postgres that
        # we want to backup the DB files now.
        if os.path.exists(self.labelFileName):
            self.parseBackupLabelFile()
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                'Another Postgres Backup Operation is in progress ("{}" file exists). You may stop it using SELECT pg_stop_backup()\n'.format(
                    self.labelFileName
                ),
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, "Send 'SELECT pg_start_backup' to Postgres\n")
        # We tell Postgres that we want to start to backup file now
        self.backupStartTime = datetime.datetime.now(
            tz=dateutil.tz.tzoffset(None, self.tzOffset)
        )
        try:
            result = self.dbCon.run(
                "SELECT pg_start_backup('%s');" % self.backupLabelString
            )
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, "pg_start_backup statement failed: %s" % (e)
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(150, "Start response: %s\n" % str(result))
        bareosfd.DebugMessage(
            150, "Adding label file %s to fileset\n" % self.labelFileName
        )
        self.files_to_backup.append(self.labelFileName)
        bareosfd.DebugMessage(150, "Filelist: %s\n" % (self.files_to_backup))
        self.PostgressFullBackupRunning = True
        return bareosfd.bRC_OK

    def parseBackupLabelFile(self):
        try:
            with open(self.labelFileName, "r") as labelFile:
                for labelItem in labelFile.read().splitlines():
                    k, v = labelItem.split(":", 1)
                    self.labelItems.update({k.strip(): v.strip()})
            bareosfd.DebugMessage(150, "Labels read: %s\n" % str(self.labelItems))
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "Could not read Label File %s: %s\n" % (self.labelFileName, e),
            )

    def start_backup_file(self, savepkt):
        """
        For normal files we call the super method
        Special objects are treated here
        """
        if not self.files_to_backup:
            bareosfd.DebugMessage(100, "No files to backup\n")
            return bareosfd.bRC_Skip

        # Plain files are handled by super class
        if self.files_to_backup[-1] not in ["ROP"]:
            return super(BareosFdPluginPostgres, self).start_backup_file(savepkt)

        # Here we create the restore object
        self.file_to_backup = self.files_to_backup.pop()
        bareosfd.DebugMessage(100, "file: " + self.file_to_backup + "\n")
        savepkt.statp = bareosfd.StatPacket()
        if self.file_to_backup == "ROP":
            self.rop_data["lastBackupStopTime"] = self.lastBackupStopTime
            self.rop_data["lastLSN"] = self.lastLSN
            savepkt.fname = "/_bareos_postgres_plugin/metadata"
            savepkt.type = bareosfd.FT_RESTORE_FIRST
            savepkt.object_name = savepkt.fname
            bareosfd.DebugMessage(150, "fname: " + savepkt.fname + "\n")
            bareosfd.DebugMessage(150, "rop " + str(self.rop_data) + "\n")
            savepkt.object = bytearray(json.dumps(self.rop_data), "utf-8")
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
        else:
            # should not happen
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Unknown error. Don't know how to handle %s\n" % self.file_to_backup,
            )
        return bareosfd.bRC_OK

    def restore_object_data(self, ROP):
        """
        Called on restore and on diff/inc jobs.
        """
        # Improve: sanity / consistence check of restore object
        # ROP.object is of type bytearray.
        self.row_rop_raw = ROP.object.decode("UTF-8")
        try:
            self.rop_data[ROP.jobid] = json.loads(self.row_rop_raw)
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                'Could not parse restore object json-data "%s" / "%s"\n'
                % (self.row_rop_raw, e),
            )

        if "lastBackupStopTime" in self.rop_data[ROP.jobid]:
            self.lastBackupStopTime = int(
                self.rop_data[ROP.jobid]["lastBackupStopTime"]
            )
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "Got lastBackupStopTime %d from restore object of job %d\n"
                % (self.lastBackupStopTime, ROP.jobid),
            )
        if "lastLSN" in self.rop_data[ROP.jobid]:
            self.lastLSN = self.rop_data[ROP.jobid]["lastLSN"]
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "Got lastLSN %s from restore object of job %d\n"
                % (self.lastLSN, ROP.jobid),
            )
        return bareosfd.bRC_OK

    def closeDbConnection(self):
        # TODO Error Handling
        # Get Backup Start Date
        self.parseBackupLabelFile()
        # self.execute_SQL("SELECT pg_backup_start_time()")
        # self.backupStartTime = self.dbCursor.fetchone()[0]
        # Tell Postgres we are done
        try:
            results = self.dbCon.run("SELECT pg_stop_backup();")
            self.lastLSN = self.formatLSN(results[0][0])
            self.lastBackupStopTime = int(time.time())
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "Database connection closed. "
                + "CHECKPOINT LOCATION: %s, " % self.labelItems["CHECKPOINT LOCATION"]
                + "START WAL LOCATION: %s\n" % self.labelItems["START WAL LOCATION"],
            )
            self.PostgressFullBackupRunning = False
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_ERROR, "pg_stop_backup statement failed: %s\n" % (e)
            )

    def checkForWalFiles(self):
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
            try:
                st = os.stat(fullPath)
            except Exception as e:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    "Could net get stat-info for file %s: %s\n" % (fullPath, e),
                )
                continue
            fileMtime = datetime.datetime.fromtimestamp(st.st_mtime)
            if (
                fileMtime.replace(tzinfo=dateutil.tz.tzoffset(None, self.tzOffset))
                > self.backupStartTime
            ):
                bareosfd.DebugMessage(150, "Adding WAL file %s for backup\n" % fileName)
                self.files_to_backup.append(fullPath)

        if self.files_to_backup:
            return bareosfd.bRC_More
        else:
            return bareosfd.bRC_OK

    def end_backup_file(self):
        """
        Here we return 'bareosfd.bRC_More' as long as our list files_to_backup is not
        empty and bareosfd.bRC_OK when we are done
        """
        bareosfd.DebugMessage(100, "end_backup_file() entry point in Python called\n")
        if self.files_to_backup:
            return bareosfd.bRC_More
        else:
            if self.PostgressFullBackupRunning:
                self.closeDbConnection()
                # Now we can also create the Restore object with the right timestamp
                self.files_to_backup.append("ROP")
                return self.checkForWalFiles()
            else:
                return bareosfd.bRC_OK

    def end_backup_job(self):
        """
        Called if backup job ends, before ClientAfterJob
        Make sure that dbconnection was closed in any way,
        especially when job was cancelled
        """
        if self.PostgressFullBackupRunning:
            self.closeDbConnection()
            self.PostgressFullBackupRunning = False
        return bareosfd.bRC_OK

    def wait_for_wal_archiving(self, LSN):
        """
        Wait for wal archiving to be finished by checking if the wal file
        for the given LSN is present in the filesystem.
        """

        pgMajorVersion = self.pgVersion // 10000
        if pgMajorVersion >= 10:
            wal_filename_func = "pg_walfile_name"
        else:
            wal_filename_func = "pg_xlogfile_name"

        walfile_stmt = "SELECT %s('%s')" % (wal_filename_func, LSN)

        try:
            result = self.dbCon.run(walfile_stmt)
            wal_filename = result[0][0]

            bareosfd.DebugMessage(
                100,
                "wait_for_wal_archiving(%s): wal filename=%s\n" % (LSN, wal_filename),
            )

        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error getting WAL filename for LSN %s\n" % (LSN, e),
            )
            return False

        wal_file_path = self.options["walArchive"] + wal_filename

        # To finish as quick as possible but with low impact on a heavy loaded
        # system, we use increasing sleep times here, starting with a small value
        sleep_time = 0.01
        slept_sum = 0.0
        while slept_sum <= self.switchWalTimeout:
            if os.path.exists(wal_file_path):
                return True
            time.sleep(sleep_time)
            slept_sum += sleep_time
            sleep_time *= 1.2

        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            "Timeout waiting %s s for wal file %s to be archived\n"
            % (self.switchWalTimeout, wal_filename),
        )
        return False


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
