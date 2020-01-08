#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2015-2016 Bareos GmbH & Co. KG
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
# Uses Percona's xtrabackup for backup and restore of MySQL / MariaDB databases

from bareosfd import *
from bareos_fd_consts import *
import os
from subprocess import *
from BareosFdPluginBaseclass import *
import BareosFdWrapper
import datetime
import time
import tempfile
import shutil
import json


class BareosFdPercona(BareosFdPluginBaseclass):
    """
        Plugin for backing up all mysql innodb databases found in a specific mysql server
        using the Percona xtrabackup tool.
    """

    def __init__(self, context, plugindef):
        # BareosFdPluginBaseclass.__init__(self, context, plugindef)
        super(BareosFdPercona, self).__init__(context, plugindef)
        # we first create and backup the stream and after that
        # the lsn file as restore-object
        self.files_to_backup = ["lsnfile", "stream"]
        self.tempdir = tempfile.mkdtemp()
        # self.logdir = GetValue(context, bVariable['bVarWorkingDir'])
        self.logdir = "/var/log/bareos/"
        self.log = "bareos-plugin-percona.log"
        self.rop_data = {}
        self.max_to_lsn = 0
        self.err_fd = None

    def parse_plugin_definition(self, context, plugindef):
        """
        We have default options that should work out of the box in the most  use cases
        that the mysql/mariadb is on the same host and can be accessed without user/password information,
        e.g. with a valid my.cnf for user root.
        """
        BareosFdPluginBaseclass.parse_plugin_definition(self, context, plugindef)

        if "dumpbinary" in self.options:
            self.dumpbinary = self.options["dumpbinary"]
        else:
            self.dumpbinary = "xtrabackup"

        if "restorecommand" not in self.options:
            self.restorecommand = "xbstream -x -C "
        else:
            self.restorecommand = self.options["restorecommand"]

        # Default is not to write an extra logfile
        if "log" not in self.options:
            self.log = False
        elif self.options["log"] == "false":
            self.log = False
        elif os.path.isabs(self.options["log"]):
            self.log = self.options["log"]
        else:
            self.log = os.path.join(self.logdir, self.options["log"])

        # By default, standard mysql-config files will be used, set
        # this option to use extra files
        self.connect_options = {"read_default_group": "client"}
        if "mycnf" in self.options:
            self.connect_options["read_default_file"] = self.options["mycnf"]
            self.mycnf = "--defaults-extra-file=%s " % self.options["mycnf"]
        else:
            self.mycnf = ""

        # If true, incremental jobs will only be performed, if LSN has increased
        # since last call.
        if (
            "strictIncremental" in self.options
            and self.options["strictIncremental"] == "true"
        ):
            self.strictIncremental = True
        else:
            self.strictIncremental = False

        self.dumpoptions = self.mycnf

        # if dumpoptions is set, we use that here, otherwise defaults
        if "dumpoptions" in self.options:
            self.dumpoptions += self.options["dumpoptions"]
        else:
            self.dumpoptions += " --backup --stream=xbstream"

        self.dumpoptions += " --extra-lsndir=%s" % self.tempdir

        if "extradumpoptions" in self.options:
            self.dumpoptions += " " + self.options["extradumpoptions"]

        # We need to call mysql to get the current Log Sequece Number (LSN)
        if "mysqlcmd" in self.options:
            self.mysqlcmd = self.options["mysqlcmd"]
        else:
            self.mysqlcmd = "mysql %s -r" % self.mycnf

        return bRCs["bRC_OK"]

    def check_plugin_options(self, context, mandatory_options=None):
        accurate_enabled = GetValue(context, bVariable["bVarAccurate"])
        if accurate_enabled is not None and accurate_enabled != 0:
            JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "start_backup_job: Accurate backup not allowed please disable in Job\n",
            )
            return bRCs["bRC_Error"]
        else:
            return bRCs["bRC_OK"]

    def create_file(self, context, restorepkt):
        """
        On restore we create a subdirectory for the first base backup and each incremental backup.
        Because percona expects an empty directory, we create a tree starting with jobId/ of restore job
        """
        FNAME = restorepkt.ofname
        DebugMessage(context, 100, "create file with %s called\n" % FNAME)
        self.writeDir = "%s/%d/" % (os.path.dirname(FNAME), self.jobId)
        # FNAME contains originating jobId after last .
        origJobId = int(FNAME.rpartition(".")[-1])
        if origJobId in self.rop_data:
            rop_from_lsn = int(self.rop_data[origJobId]["from_lsn"])
            rop_to_lsn = int(self.rop_data[origJobId]["to_lsn"])
            self.writeDir += "/%020d_" % rop_from_lsn
            self.writeDir += "%020d_" % rop_to_lsn
            self.writeDir += "%010d" % origJobId
        else:
            JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "No lsn information found in restore object for file %s from job %d\n"
                % (FNAME, origJobId),
            )

        # Create restore directory, if not existent
        if not os.path.exists(self.writeDir):
            bareosfd.DebugMessage(
                context,
                200,
                "Directory %s does not exist, creating it now\n" % self.writeDir,
            )
            os.makedirs(self.writeDir)
        # Percona requires empty directory
        if os.listdir(self.writeDir):
            JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "Restore with xbstream needs empty directory: %s\n" % self.writeDir,
            )
            return bRCs["bRC_Error"]
        self.restorecommand += self.writeDir
        DebugMessage(
            context,
            100,
            'Restore using xbstream to extract files with "%s"\n' % self.restorecommand,
        )
        restorepkt.create_status = bCFs["CF_EXTRACT"]
        return bRCs["bRC_OK"]

    def start_backup_job(self, context):
        """
        We will check, if database has changed since last backup
        in the incremental case
        """
        check_option_bRC = self.check_plugin_options(context)
        if check_option_bRC != bRCs["bRC_OK"]:
            return check_option_bRC
        bareosfd.DebugMessage(
            context, 100, "start_backup_job, level: %s\n" % chr(self.level)
        )
        if chr(self.level) == "I":
            # We check, if we have a LSN received by restore object from previous job
            if self.max_to_lsn == 0:
                JobMessage(
                    context,
                    bJobMessageType["M_FATAL"],
                    "No LSN received to be used with incremental backup\n",
                )
                return bRCs["bRC_Error"]
            # Try to load MySQLdb module
            hasMySQLdbModule = False
            try:
                import MySQLdb

                hasMySQLdbModule = True
                bareosfd.DebugMessage(context, 100, "Imported module MySQLdb\n")
            except ImportError:
                bareosfd.DebugMessage(
                    context,
                    100,
                    "Import of module MySQLdb failed. Using command pipe instead\n",
                )
            # contributed by https://github.com/kjetilho
            if hasMySQLdbModule:
                try:
                    conn = MySQLdb.connect(**self.connect_options)
                    cursor = conn.cursor()
                    cursor.execute("SHOW ENGINE INNODB STATUS")
                    result = cursor.fetchall()
                    if len(result) == 0:
                        JobMessage(
                            context,
                            bJobMessageType["M_FATAL"],
                            "Could not fetch SHOW ENGINE INNODB STATUS, unprivileged user?",
                        )
                        return bRCs["bRC_Error"]
                    info = result[0][2]
                    conn.close()
                    for line in info.split("\n"):
                        if line.startswith("Log sequence number"):
                            last_lsn = int(line.split(" ")[3])
                except Exception as e:
                    JobMessage(
                        context,
                        bJobMessageType["M_FATAL"],
                        "Could not get LSN, Error: %s" % e,
                    )
                    return bRCs["bRC_Error"]
            # use old method as fallback, if module MySQLdb not available
            else:
                get_lsn_command = (
                    "echo 'SHOW ENGINE INNODB STATUS' | %s | grep 'Log sequence number' | cut -d ' ' -f 4"
                    % self.mysqlcmd
                )
                last_lsn_proc = Popen(
                    get_lsn_command, shell=True, stdout=PIPE, stderr=PIPE
                )
                last_lsn_proc.wait()
                returnCode = last_lsn_proc.poll()
                (mysqlStdOut, mysqlStdErr) = last_lsn_proc.communicate()
                if returnCode != 0 or mysqlStdErr:
                    JobMessage(
                        context,
                        bJobMessageType["M_FATAL"],
                        'Could not get LSN with command "%s", Error: %s'
                        % (get_lsn_command, mysqlStdErr),
                    )
                    return bRCs["bRC_Error"]
                else:
                    try:
                        last_lsn = int(mysqlStdOut)
                    except:
                        JobMessage(
                            context,
                            bJobMessageType["M_FATAL"],
                            'Error reading LSN: "%s" not an integer' % mysqlStdOut,
                        )
                        return bRCs["bRC_Error"]
            JobMessage(
                context, bJobMessageType["M_INFO"], "Backup until LSN: %d\n" % last_lsn
            )
            if (
                self.max_to_lsn > 0
                and self.max_to_lsn >= last_lsn
                and self.strictIncremental
            ):
                bareosfd.DebugMessage(
                    context,
                    100,
                    "Last LSN of DB %d is not higher than LSN from previous job %d. Skipping this incremental backup\n"
                    % (last_lsn, self.max_to_lsn),
                )
                self.files_to_backup = ["lsn_only"]
                return bRCs["bRC_OK"]
        return bRCs["bRC_OK"]

    def start_backup_file(self, context, savepkt):
        """
        This method is called, when Bareos is ready to start backup a file
        """
        if not self.files_to_backup:
            self.file_to_backup = None
            DebugMessage(context, 100, "start_backup_file: None\n")
        else:
            self.file_to_backup = self.files_to_backup.pop()
            DebugMessage(context, 100, "start_backup_file: %s\n" % self.file_to_backup)

        statp = StatPacket()
        savepkt.statp = statp

        if self.file_to_backup == "stream":
            # This is the database backup as xbstream
            savepkt.fname = "/_percona/xbstream.%010d" % self.jobId
            savepkt.type = bFileType["FT_REG"]
            if self.max_to_lsn > 0:
                self.dumpoptions += " --incremental-lsn=%d" % self.max_to_lsn
            self.dumpcommand = "%s %s" % (self.dumpbinary, self.dumpoptions)
            DebugMessage(context, 100, "Dumper: '" + self.dumpcommand + "'\n")
        elif self.file_to_backup == "lsnfile":
            # The restore object containing the log sequence number (lsn)
            # Read checkpoints and create restore object
            checkpoints = {}
            # improve: Error handling
            with open("%s/xtrabackup_checkpoints" % self.tempdir) as lsnfile:
                for line in lsnfile:
                    key, value = line.partition("=")[::2]
                    checkpoints[key.strip()] = value.strip()
            savepkt.fname = "/_percona/xtrabackup_checkpoints"
            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(json.dumps(checkpoints))
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
            shutil.rmtree(self.tempdir)
        elif self.file_to_backup == "lsn_only":
            # We have nothing to backup incremental, so we just have to pass
            # the restore object from previous job
            savepkt.fname = "/_percona/xtrabackup_checkpoints"
            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(self.row_rop_raw)
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
        else:
            # should not happen
            JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "Unknown error. Don't know how to handle %s\n" % self.file_to_backup,
            )

        JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Starting backup of " + savepkt.fname + "\n",
        )
        return bRCs["bRC_OK"]

    def plugin_io(self, context, IOP):
        """
        Called for io operations. We read from pipe into buffers or on restore
        send to xbstream
        """
        DebugMessage(context, 200, "plugin_io called with " + str(IOP.func) + "\n")

        if IOP.func == bIOPS["IO_OPEN"]:
            DebugMessage(context, 100, "plugin_io called with IO_OPEN\n")
            if self.log:
                try:
                    self.err_fd = open(self.log, "a")
                except IOError as msg:
                    DebugMessage(
                        context,
                        100,
                        "Could not open log file (%s): %s\n"
                        % (self.log, format(str(msg))),
                    )
            if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                if self.log:
                    self.err_fd.write(
                        '%s Restore Job %s opens stream with "%s"\n'
                        % (datetime.datetime.now(), self.jobId, self.restorecommand)
                    )
                self.stream = Popen(
                    self.restorecommand, shell=True, stdin=PIPE, stderr=self.err_fd
                )
            else:
                if self.log:
                    self.err_fd.write(
                        '%s Backup Job %s opens stream with "%s"\n'
                        % (datetime.datetime.now(), self.jobId, self.dumpcommand)
                    )
                self.stream = Popen(
                    self.dumpcommand, shell=True, stdout=PIPE, stderr=self.err_fd
                )
            return bRCs["bRC_OK"]

        elif IOP.func == bIOPS["IO_READ"]:
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.stream.stdout.readinto(IOP.buf)
            IOP.io_errno = 0
            return bRCs["bRC_OK"]

        elif IOP.func == bIOPS["IO_WRITE"]:
            try:
                self.stream.stdin.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as msg:
                IOP.io_errno = -1
                DebugMessage(
                    context, 100, "Error writing data: " + format(str(msg)) + "\n"
                )
            return bRCs["bRC_OK"]

        elif IOP.func == bIOPS["IO_CLOSE"]:
            DebugMessage(context, 100, "plugin_io called with IO_CLOSE\n")
            self.subprocess_returnCode = self.stream.poll()
            if self.subprocess_returnCode is None:
                # Subprocess is open, we wait until it finishes and get results
                try:
                    self.stream.communicate()
                    self.subprocess_returnCode = self.stream.poll()
                except:
                    JobMessage(
                        context,
                        bJobMessageType["M_ERROR"],
                        "Dump / restore command not finished properly\n",
                    )
                    bRCs["bRC_Error"]
                return bRCs["bRC_OK"]
            else:
                DebugMessage(
                    context,
                    100,
                    "Subprocess has terminated with returncode: %d\n"
                    % self.subprocess_returnCode,
                )
                return bRCs["bRC_OK"]

        elif IOP.func == bIOPS["IO_SEEK"]:
            return bRCs["bRC_OK"]

        else:
            DebugMessage(
                context,
                100,
                "plugin_io called with unsupported IOP:" + str(IOP.func) + "\n",
            )
            return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        """
        Check if dump was successful.
        """
        # Usually the xtrabackup process should have terminated here, but on some servers
        # it has not always.
        if self.file_to_backup == "stream":
            returnCode = self.subprocess_returnCode
            if returnCode is None:
                JobMessage(
                    context,
                    bJobMessageType["M_ERROR"],
                    "Dump command not finished properly for unknown reason\n",
                )
                returnCode = -99
            else:
                DebugMessage(
                    context,
                    100,
                    "end_backup_file() entry point in Python called. Returncode: %d\n"
                    % self.stream.returncode,
                )
                if returnCode != 0:
                    msg = [
                        "Dump command returned non-zero value: %d" % returnCode,
                        'command: "%s"' % self.dumpcommand,
                    ]
                    if self.log:
                        msg += ['log file: "%s"' % self.log]
                    JobMessage(
                        context, bJobMessageType["M_FATAL"], ", ".join(msg) + "\n"
                    )
            if returnCode != 0:
                return bRCs["bRC_Error"]

            if self.log:
                self.err_fd.write(
                    "%s Backup Job %s closes stream\n"
                    % (datetime.datetime.now(), self.jobId)
                )
                self.err_fd.close()

        if self.files_to_backup:
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]

    def end_restore_file(self, context):
        """
        Check if writing to restore command was successful.
        """
        returnCode = self.subprocess_returnCode
        if returnCode is None:
            JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "Restore command not finished properly for unknown reason\n",
            )
            returnCode = -99
        else:
            DebugMessage(
                context,
                100,
                "end_restore_file() entry point in Python called. Returncode: %d\n"
                % self.stream.returncode,
            )
            if returnCode != 0:
                msg = ["Restore command returned non-zero value: %d" % return_code]
                if self.log:
                    msg += ['log file: "%s"' % self.log]
                JobMessage(context, bJobMessageType["M_ERROR"], ", ".join(msg) + "\n")
        if self.log:
            self.err_fd.write(
                "%s Restore Job %s closes stream\n"
                % (datetime.datetime.now(), self.jobId)
            )
            self.err_fd.close()

        if returnCode == 0:
            return bRCs["bRC_OK"]
        else:
            return bRCs["bRC_Error"]

    def restore_object_data(self, context, ROP):
        """
        Called on restore and on diff/inc jobs.
        """
        # Improve: sanity / consistence check of restore object
        self.row_rop_raw = ROP.object
        self.rop_data[ROP.jobid] = json.loads(str(self.row_rop_raw))
        if (
            "to_lsn" in self.rop_data[ROP.jobid]
            and self.rop_data[ROP.jobid]["to_lsn"] > self.max_to_lsn
        ):
            self.max_to_lsn = int(self.rop_data[ROP.jobid]["to_lsn"])
            JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "Got to_lsn %d from restore object of job %d\n"
                % (self.max_to_lsn, ROP.jobid),
            )
        return bRCs["bRC_OK"]


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
