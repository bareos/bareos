#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2015-2025 Bareos GmbH & Co. KG
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
# Author: Philipp Storz
#
"""
bareos-fd-mariabackup is a plugin to backup and restore MariaDB with mariadb-backup and mbstream
"""
import os
import json
import time
import datetime
import tempfile
import shutil
from subprocess import *
import bareosfd
from bareosfd import *

from BareosFdPluginBaseclass import BareosFdPluginBaseclass
from BareosFdWrapper import *  # noqa


@BareosPlugin
class BareosFdMariabackup(BareosFdPluginBaseclass):
    """
    Plugin for backing up all innodb databases found in a specific mariadb server
    using the native mariadb-backup tool.
    """

    def __init__(self, plugindef):
        # BareosFdPluginBaseclass.__init__(self, plugindef)
        super(BareosFdMariabackup, self).__init__(plugindef)
        # we first create and backup the stream and after that
        # the lsn file as restore-object
        self.files_to_backup = ["lsnfile", "stream"]
        self.tempdir = tempfile.mkdtemp()
        # self.logdir = GetValue(bVarWorkingDir)
        self.logdir = "/var/log/bareos/"
        self.log = "bareos-plugin-mariadb-backup.log"
        self.rop_data = {}
        self.max_to_lsn = 0
        self.err_fd = None
        self.dumpbinary = "mariadb-backup"
        self.restorecommand = None
        self.mysqlcmd = None
        self.mycnf = ""
        self.strictIncremental = False
        self.dumpoptions = ""
        # xtrabackup_checkpoints for <11 and mariadb_backup_checkpoints for >=11
        # version needs to be retrieved with
        # mariadb --batch --skip-column-names --execute "select version();"
        self.checkpoints_filename = None

    def parse_plugin_definition(self, plugindef):
        """
        We have default options that should work out of the box in the most use cases
        when the mysql/mariadb is on the same host and can be accessed without user/password
        information,
        e.g. with a valid my.cnf for user root.
        """
        BareosFdPluginBaseclass.parse_plugin_definition(self, plugindef)

        if "dumpbinary" in self.options:
            self.dumpbinary = self.options["dumpbinary"]

        # The reset to default is needed for each file.
        self.restorecommand = "mbstream -x -C "
        if "restorecommand" in self.options:
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

        # By default, standard mysql-config files will be used,
        # set this option to use extra files
        self.connect_options = {"read_default_group": "client"}
        if "mycnf" in self.options:
            self.connect_options["read_default_file"] = self.options["mycnf"]
            self.mycnf = f"--defaults-file={self.options['mycnf']}"

        # If true, incremental jobs will only be performed, if LSN has increased
        # since last call.
        if (
            "strictIncremental" in self.options
            and self.options["strictIncremental"] == "true"
        ):
            self.strictIncremental = True

        self.dumpoptions = self.mycnf

        # if dumpoptions is set, we use that here, otherwise defaults
        if "dumpoptions" in self.options:
            self.dumpoptions += self.options["dumpoptions"]
        else:
            self.dumpoptions += " --backup --stream=xbstream"

        self.dumpoptions += f" --extra-lsndir={self.tempdir}"

        if "extradumpoptions" in self.options:
            self.dumpoptions += " " + self.options["extradumpoptions"]

        # We need to call mariadb to get the current Log Sequece Number (LSN)
        if "mysqlcmd" in self.options:
            DebugMessage(
                100, f"self option mysqlcmd detected {self.options['mysqlcmd']}\n"
            )
            self.mysqlcmd = self.options["mysqlcmd"]
        else:
            self.mysqlcmd = f"mariadb {self.mycnf} --raw"
            DebugMessage(100, f"Default in use self.mysqlcmd='{self.mysqlcmd}'\n")

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

    def check_plugin_mariadb_version(self):
        get_version_command = self.mysqlcmd.split()
        get_version_command.extend(
            ["--batch", "--skip-column-names", "--execute", "select version();"]
        )
        DebugMessage(100, f'mariadb version check: "{get_version_command}"\n')
        try:
            ret = check_output(get_version_command).decode().split(".")[0]
            mariadb_major_version = int(ret)
            if mariadb_major_version is not None and mariadb_major_version != 0:
                if mariadb_major_version < 11:
                    self.checkpoints_filename = "xtrabackup_checkpoints"
                elif mariadb_major_version >= 11:
                    self.checkpoints_filename = "mariadb_backup_checkpoints"
                DebugMessage(
                    100,
                    (
                        f"Detected mariadb version {mariadb_major_version} "
                        f": {self.checkpoints_filename}\n"
                    ),
                )
                return bRC_OK
        except CalledProcessError as run_err:
            DebugMessage(
                100,
                f"No mariadb version detected: {run_err}\n",
            )
            return bRC_Error
        except ValueError as val_err:
            DebugMessage(
                100,
                f"mariadb version detected seems wrong: {val_err}\n",
            )
            return bRC_Error

    def get_lsn_by_command(self):
        get_lsn_cmd = self.mysqlcmd.split()
        get_lsn_cmd.extend(
            [
                "--batch",
                "--skip-column-names",
                "--execute",
                "SHOW ENGINE INNODB STATUS;",
            ]
        )
        DebugMessage(100, f'get_lsn_by_command: "{get_lsn_cmd}"\n')
        try:
            ret = check_output(get_lsn_cmd).decode(errors='ignore')
            return ret
        except CalledProcessError as run_err:
            JobMessage(
                M_FATAL,
                f"Error while looking for LSN: {run_err}\n",
            )
            return bRC_Error

    def parse_innodb_status(self, res):
        last_lsn = None
        try:
            for line in res.split("\n"):
                if line.startswith("Log sequence number"):
                    last_lsn = int(line.split(" ")[-1])
        except ValueError as val_err:
            JobMessage(
                M_FATAL,
                f"Returned LSN seems wrong: {val_err}\n",
            )
            return bRC_Error
        if last_lsn is None:
            JobMessage(
                M_FATAL,
                f"Could not get LSN from {res}\n",
            )
            return bRC_Error
        return last_lsn

    def create_file(self, restorepkt):
        """
        On restore we create a subdirectory for the first base backup and each incremental backup.
        Because mariadb-backup expects an empty directory, we create a tree starting with jobId/
        of restore job
        """
        FNAME = restorepkt.ofname
        DebugMessage(100, "create file with %s called\n" % FNAME)
        self.writeDir = "%s/%d/" % (os.path.dirname(FNAME), self.jobId)
        # FNAME contains originating jobId after last .
        origJobId = int(FNAME.rpartition(".")[-1])
        if origJobId in self.rop_data:
            rop_from_lsn = int(self.rop_data[origJobId]["from_lsn"])
            rop_to_lsn = int(self.rop_data[origJobId]["to_lsn"])
            self.writeDir += "%020d_" % rop_from_lsn
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
        # mbstream requires empty directory
        if os.listdir(self.writeDir):
            JobMessage(
                M_FATAL,
                "Restore with mbstream needs empty directory: %s\n" % self.writeDir,
            )
            return bRC_Error
        self.restorecommand += self.writeDir
        DebugMessage(
            100,
            'Restore using mbstream to extract files with "%s"\n' % self.restorecommand,
        )
        restorepkt.create_status = CF_EXTRACT
        return bRC_OK

    def start_backup_job(self):
        """
        We will check, if database has changed since last backup
        in the incremental case
        """
        bareosfd.DebugMessage(100, "start_backup_job, check_plugin_option\n")
        check_option_bRC = self.check_plugin_options()
        if check_option_bRC != bRC_OK:
            return check_option_bRC
        bareosfd.DebugMessage(100, "start_backup_job, check_mariadb_version\n")
        check_version_bRC = self.check_plugin_mariadb_version()
        if check_version_bRC != bRC_OK:
            JobMessage(
                M_FATAL,
                "Unable to determine mariadb version\n",
            )
            return check_version_bRC
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
            # TODO Fix as this doesn't use the mysqldefault passed file and always try to connect
            # to default instance.
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
                    innodb_status = result[0][2]
                    conn.close()
                except Exception as e:
                    JobMessage(
                        M_FATAL,
                        "Could not get LSN, Error: %s" % e,
                    )
                    return bRC_Error
            # use old method as fallback, if module MySQLdb not available
            else:
                innodb_status = self.get_lsn_by_command()

            last_lsn = self.parse_innodb_status(innodb_status)
            if last_lsn == bRC_Error:
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
            savepkt.fname = "/_mariadb-backup/xbstream.%010d" % self.jobId
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
            with open(f"{self.tempdir}/{self.checkpoints_filename}") as lsnfile:
                for line in lsnfile:
                    key, value = line.partition("=")[::2]
                    checkpoints[key.strip()] = value.strip()
            savepkt.fname = f"/_mariadb-backup/{self.checkpoints_filename}"
            savepkt.type = FT_RESTORE_FIRST
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(json.dumps(checkpoints), encoding="utf8")
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
            shutil.rmtree(self.tempdir)
        elif self.file_to_backup == "lsn_only":
            # We have nothing to backup incremental, so we just have to pass
            # the restore object from previous job
            savepkt.fname = f"/_mariadb-backup/{self.checkpoints_filename}"
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
        send to mbstream
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
            DebugMessage(100, "plugin_io called with IO_CLOSE\n")
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

    def end_backup_file(self):
        """
        Check if dump was successful.
        """
        # Usually the mariadb-backup process should have terminated here,
        # but on some servers it has not always.
        DebugMessage(100, "end_backup_file() entry point in Python called\n")
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
                    "end_backup_file() Returncode: %d\n" % self.stream.returncode,
                )
                if returnCode != 0:
                    msg = [
                        "Dump command returned non-zero value: %d" % returnCode,
                        'command: "%s"' % self.dumpcommand,
                    ]
                    if self.log:
                        msg += ['log file: "%s"' % self.log]
                    JobMessage(M_FATAL, ", ".join(msg) + "\n")
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


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
