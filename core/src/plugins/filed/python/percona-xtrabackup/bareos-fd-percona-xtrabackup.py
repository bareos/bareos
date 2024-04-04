#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2015-2024 Bareos GmbH & Co. KG
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

# This module contains the wrapper functions called by the Bareos-FD, the functions call the corresponding
# methods from your plugin class
from BareosFdWrapper import *


from bareosfd import *
import os
from subprocess import *
from BareosFdPluginBaseclass import *
import BareosFdWrapper
import datetime
import time
import tempfile
import shutil
import json
import signal


@BareosPlugin
class BareosFdPercona(BareosFdPluginBaseclass):
    """
    Plugin for backing up all mysql innodb databases found in a specific mysql server
    using the Percona xtrabackup tool.
    """

    def __init__(self, plugindef):
        # BareosFdPluginBaseclass.__init__(self, plugindef)
        super(BareosFdPercona, self).__init__(plugindef)
        self.events = []
        self.events.append(bEventStartBackupJob)
        self.events.append(bEventStartRestoreJob)
        self.events.append(bEventEndRestoreJob)
        RegisterEvents(self.events)
        # we first create and backup the stream and after that
        # the lsn file as restore-object
        self.files_to_backup = ["lsnfile", "stream"]
        self.tempdir = tempfile.mkdtemp()
        # self.logdir = GetValue(bVarWorkingDir)
        self.logdir = "/var/log/bareos/"
        self.log = "bareos-plugin-percona.log"
        self.rop_data = {}
        self.max_to_lsn = 0
        self.err_fd = None

    def parse_plugin_definition(self, plugindef):
        """
        We have default options that should work out of the box in the most  use cases
        that the mysql/mariadb is on the same host and can be accessed without user/password information,
        e.g. with a valid my.cnf for user root.
        """
        BareosFdPluginBaseclass.parse_plugin_definition(self, plugindef)

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

        return bRC_OK

    def check_plugin_options(self, mandatory_options=None):
        accurate_enabled = GetValue(bVarAccurate)
        if accurate_enabled is not None and accurate_enabled != 0:
            JobMessage(
                M_FATAL,
                "start_backup_job: Accurate backup not allowed please disable in Job\n",
            )
            return bRC_Error
        else:
            return bRC_OK

    def create_file(self, restorepkt):
        """
        On restore we create a subdirectory for the first base backup and each incremental backup.
        Because percona expects an empty directory, we create a tree starting with jobId/ of restore job
        """
        FNAME = restorepkt.ofname
        DebugMessage(100, "create file with %s called\n" % FNAME)
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
                M_ERROR,
                "No lsn information found in restore object for file %s from job %d\n"
                % (FNAME, origJobId),
            )

        # Create restore directory, if not existent
        if not os.path.exists(self.writeDir):
            bareosfd.DebugMessage(
                200,
                "Directory %s does not exist, creating it now\n" % self.writeDir,
            )
            os.makedirs(self.writeDir)
        # Percona requires empty directory
        if os.listdir(self.writeDir):
            JobMessage(
                M_FATAL,
                "Restore with xbstream needs empty directory: %s\n" % self.writeDir,
            )
            return bRC_Error
        self.restorecommand += self.writeDir
        DebugMessage(
            100,
            'Restore using xbstream to extract files with "%s"\n' % self.restorecommand,
        )
        restorepkt.create_status = CF_EXTRACT
        return bRC_OK

    def start_backup_job(self):
        """
        We will check, if database has changed since last backup
        in the incremental case
        """
        check_option_bRC = self.check_plugin_options()
        if check_option_bRC != bRC_OK:
            return check_option_bRC
        bareosfd.DebugMessage(100, "start_backup_job, level: %s\n" % chr(self.level))
        if chr(self.level) == "I":
            # We check, if we have a LSN received by restore object from previous job
            if self.max_to_lsn == 0:
                JobMessage(
                    M_FATAL,
                    "No LSN received to be used with incremental backup\n",
                )
                return bRC_Error
            # Try to load MySQLdb module
            hasMySQLdbModule = False
            try:
                import MySQLdb

                hasMySQLdbModule = True
                bareosfd.DebugMessage(100, "Imported module MySQLdb\n")
            except ImportError:
                bareosfd.DebugMessage(
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
                            M_FATAL,
                            "Could not fetch SHOW ENGINE INNODB STATUS, unprivileged user?",
                        )
                        return bRC_Error
                    info = result[0][2]
                    conn.close()
                    for line in info.split("\n"):
                        if line.startswith("Log sequence number"):
                            last_lsn = int(line.split(" ")[-1])
                except Exception as e:
                    JobMessage(
                        M_FATAL,
                        "Could not get LSN, Error: %s" % e,
                    )
                    return bRC_Error
            # use old method as fallback, if module MySQLdb not available
            else:
                get_lsn_command = (
                    "echo 'SHOW ENGINE INNODB STATUS' | %s | grep 'Log sequence number' | awk '{ print $4 }'"
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
                        M_FATAL,
                        'Could not get LSN with command "%s", Error: %s'
                        % (get_lsn_command, mysqlStdErr),
                    )
                    return bRC_Error
                else:
                    try:
                        last_lsn = int(mysqlStdOut)
                    except:
                        JobMessage(
                            M_FATAL,
                            'Error reading LSN: "%s" not an integer' % mysqlStdOut,
                        )
                        return bRC_Error
            JobMessage(M_INFO, "Backup until LSN: %d\n" % last_lsn)
            if (
                self.max_to_lsn > 0
                and self.max_to_lsn >= last_lsn
                and self.strictIncremental
            ):
                bareosfd.DebugMessage(
                    100,
                    "Last LSN of DB %d is not higher than LSN from previous job %d. Skipping this incremental backup\n"
                    % (last_lsn, self.max_to_lsn),
                )
                self.files_to_backup = ["lsn_only"]
                return bRC_OK
        return bRC_OK

    def start_backup_file(self, savepkt):
        """
        This method is called, when Bareos is ready to start backup a file
        """
        if not self.files_to_backup:
            self.file_to_backup = None
            DebugMessage(100, "start_backup_file: None\n")
        else:
            self.file_to_backup = self.files_to_backup.pop()
            DebugMessage(100, "start_backup_file: %s\n" % self.file_to_backup)

        statp = StatPacket()
        savepkt.statp = statp

        if self.file_to_backup == "stream":
            # This is the database backup as xbstream
            savepkt.fname = "/_percona/xbstream.%010d" % self.jobId
            savepkt.type = FT_REG
            if self.max_to_lsn > 0:
                self.dumpoptions += " --incremental-lsn=%d" % self.max_to_lsn
            self.dumpcommand = "%s %s" % (self.dumpbinary, self.dumpoptions)
            DebugMessage(100, "Dumper: '" + self.dumpcommand + "'\n")
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
            savepkt.type = FT_RESTORE_FIRST
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(json.dumps(checkpoints), encoding="utf8")
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
            shutil.rmtree(self.tempdir)
        elif self.file_to_backup == "lsn_only":
            # We have nothing to backup incremental, so we just have to pass
            # the restore object from previous job
            savepkt.fname = "/_percona/xtrabackup_checkpoints"
            savepkt.type = FT_RESTORE_FIRST
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(self.row_rop_raw)
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
        else:
            # should not happen
            JobMessage(
                M_FATAL,
                "Unknown error. Don't know how to handle %s\n" % self.file_to_backup,
            )

        JobMessage(
            M_INFO,
            "Starting backup of " + savepkt.fname + "\n",
        )
        return bRC_OK

    def plugin_io(self, IOP):
        """
        Called for io operations. We read from pipe into buffers or on restore
        send to xbstream
        """
        DebugMessage(200, "plugin_io called with " + str(IOP.func) + "\n")

        if IOP.func == IO_OPEN:
            DebugMessage(100, "plugin_io called with IO_OPEN\n")
            if self.log:
                try:
                    self.err_fd = open(self.log, "a")
                except IOError as msg:
                    DebugMessage(
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
            return bRC_OK

        elif IOP.func == IO_READ:
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.stream.stdout.readinto(IOP.buf)
            IOP.io_errno = 0
            return bRC_OK

        elif IOP.func == IO_WRITE:
            try:
                self.stream.stdin.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as msg:
                IOP.io_errno = -1
                DebugMessage(100, "Error writing data: " + format(str(msg)) + "\n")
            return bRC_OK

        elif IOP.func == IO_CLOSE:
            DebugMessage(100, "plugin_io: called with IO_CLOSE\n")

            if self.jobType == "B":
                DebugMessage(
                    100,
                    "plugin_io: calling end_dumper() to wait for PID %s to terminate\n"
                    % (self.stream.pid),
                )
                bareos_xtrabackup_dumper_returncode = self.end_dumper()
                if bareos_xtrabackup_dumper_returncode != 0:
                    JobMessage(
                        M_FATAL,
                        (
                            "plugin_io[IO_CLOSE]: bareos_xtrabackup_dumper returncode:"
                            " %s\n"
                        )
                        % (bareos_xtrabackup_dumper_returncode),
                    )

                    # Cleanup tmpdir
                    shutil.rmtree(self.tempdir)
                    self.subprocess_returnCode = bareos_xtrabackup_dumper_returncode

                    return bRC_Error
                else:
                    self.subprocess_returnCode = bareos_xtrabackup_dumper_returncode

            elif self.jobType == "R":
                self.subprocess_returnCode = self.stream.poll()
                if self.subprocess_returnCode is None:
                    # Subprocess is open, we wait until it finishes and get results
                    try:
                        self.stream.communicate()
                        self.subprocess_returnCode = self.stream.poll()
                    except:
                        JobMessage(
                            M_ERROR,
                            "Dump / restore command not finished properly\n",
                        )
                        return bRC_Error
                    return bRC_OK
                else:
                    DebugMessage(
                        100,
                        "Subprocess has terminated with returncode: %d\n"
                        % self.subprocess_returnCode,
                    )

            return bRC_OK

        elif IOP.func == IO_SEEK:
            return bRC_OK

        else:
            DebugMessage(
                100,
                "plugin_io called with unsupported IOP:" + str(IOP.func) + "\n",
            )
            return bRC_OK

    def handle_plugin_event(self, event):
        if event in self.events:
            self.jobType = chr(bareosfd.GetValue(bareosfd.bVarType))
            DebugMessage(100, "jobType: %s\n" % (self.jobType))
        return bRC_OK

    def end_backup_file(self):
        """
        Check if dump was successful.
        """
        # Usually the xtrabackup process should have terminated here, but on some servers
        # it has not always.
        if self.file_to_backup == "stream":
            returnCode = self.subprocess_returnCode
            if returnCode is None:
                JobMessage(
                    M_ERROR,
                    "Dump command not finished properly for unknown reason\n",
                )
                returnCode = -99
            else:
                DebugMessage(
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
                    JobMessage(M_FATAL, ", ".join(msg) + "\n")
            if returnCode != 0:
                return bRC_Error

            if self.log:
                self.err_fd.write(
                    "%s Backup Job %s closes stream\n"
                    % (datetime.datetime.now(), self.jobId)
                )
                self.err_fd.close()

        if self.files_to_backup:
            return bRC_More
        else:
            return bRC_OK

    def end_restore_file(self):
        """
        Check if writing to restore command was successful.
        """
        returnCode = self.subprocess_returnCode
        if returnCode is None:
            JobMessage(
                M_ERROR,
                "Restore command not finished properly for unknown reason\n",
            )
            returnCode = -99
        else:
            DebugMessage(
                100,
                "end_restore_file() entry point in Python called. Returncode: %d\n"
                % self.stream.returncode,
            )
            if returnCode != 0:
                msg = ["Restore command returned non-zero value: %d" % returnCode]
                if self.log:
                    msg += ['log file: "%s"' % self.log]
                JobMessage(M_ERROR, ", ".join(msg) + "\n")
        if self.log:
            self.err_fd.write(
                "%s Restore Job %s closes stream\n"
                % (datetime.datetime.now(), self.jobId)
            )
            self.err_fd.close()

        if returnCode == 0:
            return bRC_OK
        else:
            return bRC_Error

    def restore_object_data(self, ROP):
        """
        Called on restore and on diff/inc jobs.
        """
        # Improve: sanity / consistence check of restore object
        self.row_rop_raw = ROP.object
        self.rop_data[ROP.jobid] = json.loads((self.row_rop_raw.decode("utf-8")))
        if (
            "to_lsn" in self.rop_data[ROP.jobid]
            and int(self.rop_data[ROP.jobid]["to_lsn"]) > self.max_to_lsn
        ):
            self.max_to_lsn = int(self.rop_data[ROP.jobid]["to_lsn"])
            JobMessage(
                M_INFO,
                "Got to_lsn %d from restore object of job %d\n"
                % (self.max_to_lsn, ROP.jobid),
            )
        return bRC_OK

    def end_dumper(self):
        """
        Wait for bareos_xtrabackup_dumper to terminate
        """
        bareos_xtrabackup_dumper_returncode = None
        # Wait timeout before sending TERM to the process
        timeout = 30
        start_time = int(time.time())
        sent_sigterm = False
        while self.stream.poll() is None:
            if int(time.time()) - start_time > timeout:
                DebugMessage(
                    100,
                    "Timeout wait for bareos_xtrabackup_dumper PID %s to terminate\n"
                    % (self.stream.pid),
                )
                if not sent_sigterm:
                    DebugMessage(
                        100,
                        "sending SIGTERM to bareos_xtrabackup_dumper PID %s\n"
                        % (self.stream.pid),
                    )
                    os.kill(self.stream.pid, signal.SIGTERM)
                    sent_sigterm = True
                    # Wait timeout after sending TERM to the process
                    timeout = 10
                    start_time = int(time.time())
                    continue
                else:
                    DebugMessage(
                        100,
                        "Giving up to wait for bareos_xtrabackup_dumper PID %s to terminate\n"
                        % (self.stream.pid),
                    )
                    # Set return code to 9 because we've tried to terminate subprocess earlier
                    self.stream.returncode = 9
                    break
            DebugMessage(
                100,
                "Waiting for bareos_xtrabackup_dumper PID %s to terminate\n"
                % (self.stream.pid),
            )
            time.sleep(1)

        bareos_xtrabackup_dumper_returncode = self.stream.returncode
        DebugMessage(
            100,
            "end_dumper() bareos_xtrabackup_dumper returncode: %s\n"
            % (bareos_xtrabackup_dumper_returncode),
        )

        return bareos_xtrabackup_dumper_returncode


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
