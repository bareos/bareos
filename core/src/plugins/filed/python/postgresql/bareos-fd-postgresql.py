#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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
bareos-fd-postgresql is a plugin to backup PostgreSQL clusters via non-exclusive backup mode
"""
import os
import io
import json
import getpass
from sys import version_info
import stat
import time
import datetime
import dateutil
import dateutil.parser
import bareosfd
from bareosfd import *

from BareosFdPluginBaseclass import BareosFdPluginBaseclass
from BareosFdWrapper import *  # noqa


try:
    import pg8000

    # check if pg8000 module is new enough
    major, minor, patch = pg8000.__version__.split(".")
    if int(major) < 1 or (int(major) == 1 and int(minor) < 16):
        raise ValueError(
            (
                f"pg8000 module version ({pg8000.__version__}) is lower"
                f" than required version 1.16\n"
            )
        )

except ValueError as err_version:
    bareosfd.JobMessage(bareosfd.M_FATAL, f"FATAL ERROR: {err_version}\n")

except ImportError as err_import:
    bareosfd.JobMessage(
        bareosfd.M_FATAL, f"could not import Python module: pg8000 {err_import}\n"
    )

except Exception as err_unknown:
    bareosfd.JobMessage(bareosfd.M_FATAL, f"Unknown error {err_unknown}\n")


def parse_row(row):
    """
    This function exists to fix bug in pg8000 >= 1.26 < 1.30.0
    Returned results of pg_backup_stop are string representation
    of tuple
    """
    remove_parens = row[1:-1]
    [lsn, backup_label, tablespace] = remove_parens.split(",")
    # lsn does not have quotes
    # remove quotes
    backup_label = backup_label[1:-1]
    tablespace = tablespace[1:-1]
    return [lsn, backup_label, tablespace]


@BareosPlugin
class BareosFdPluginPostgreSQL(BareosFdPluginBaseclass):  # noqa
    """
    Bareos-FD-Plugin-Class for PostgreSQL online (Hot) backup of cluster
    files and database transaction logs (WAL) archiving to allow incremental
    backups and point-in-time (pitr) recovery.
    If the cluster use tablespace, the backup will also backup and restore those
    (symlinks in pg_tblspc, and real external location data)
    The plugin job will fail if previous job was not done on same PG major version
    """

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100, f"Constructor called in module {__name__} with plugindef={plugindef}\n"
        )

        bareosfd.DebugMessage(
            100,
            (
                f"Python Version: {version_info.major}.{version_info.minor}"
                f".{version_info.micro}\n"
            ),
        )

        # Last argument of super constructor is a list of mandatory arguments
        super().__init__(plugindef, ["postgresql_data_dir", "wal_archive_dir"])

        self.ignore_subdirs = [
            "log",
            "pg_wal",
            "pg_log",
            "pg_xlog",
            "pgsql_tmp",
            "pg_dynshmem",
            "pg_notify",
            "pg_serial",
            "pg_snapshots",
            "pg_stat_tmp",
            "pg_subtrans",
        ]
        self.options = {}
        self.db_con = None
        self.db_user = None
        self.db_password = None
        self.db_host = None
        self.db_port = None
        self.db_name = None
        self.start_fast = False
        self.stop_wait_wal_archive = True
        self.switch_wal = True
        self.switch_wal_timeout = 60

        self.virtual_files = []
        self.backup_label_filename = None
        self.backup_label_data = None
        self.tablespace_map_filename = None
        self.tablespace_map_data = None
        self.recovery_filename = None
        self.recovery_data = None

        # Needed to transfer Data to virtual files
        self.data_stream = None
        self.FNAME = ""
        self.file_type = ""
        self.files_to_backup = []
        self.file_to_backup = ""
        self.ref_statp = None
        self.last_lsn = 0
        # This will be set to True between SELECT pg_backup_start and pg_backup_stop.
        # We backup the cluster files during that time
        self.full_backup_running = False
        # Store items given by `pg_backup_stop()` for virtual backup_label file
        self.label_items = {}
        # We will store the starttime from backup_label here
        self.backup_start_time = None
        # PostgreSQL last backup stop time
        self.last_backup_stop_time = 0
        # Our label, will be used as `backup label` in `SELECT pg_start_backup`
        self.backup_label_string = ""
        # Raw restore object data (json-string)
        self.row_rop_raw = None
        # Dictionary to store passed restore object data
        self.rop_data = {}
        # We need our timezone information for some timestamp comparisons
        # this one respects daylight saving timezones
        self.tz_offset = -(
            time.altzone
            if (time.daylight and time.localtime().tm_isdst)
            else time.timezone
        )
        self.pg_version = 0
        self.pg_major_version = None
        self.last_pg_major = None
        # TODO create support for external_config file

        self.statp = {}

    def build_files_to_backup(self, start_dir):
        """
        Build the list of files to be backed up by recursing from top directory.
        The parsing has to be in reverse mode so file first, directory at the end

        Will be use to parse `postgresql_data_dir`, `wal_archive_dir`, `tablespace`
        """
        if not start_dir.endswith("/"):
            start_dir += "/"
        self.files_to_backup.append(start_dir)
        for filename in os.listdir(start_dir):
            fullname = os.path.join(start_dir, filename)
            # We need a trailing '/' for directories
            if os.path.isdir(fullname) and not fullname.endswith("/"):
                fullname += "/"
                bareosfd.DebugMessage(100, f"fullname: {fullname}\n")

            # TODO check that assumption
            # Usually Bareos takes care about timestamps when doing
            # incremental backups but here we have to compare against
            # last BackupPostgreSQL timestamp
            try:
                # Create the reference file
                if filename == "PG_VERSION":
                    self.ref_statp = os.stat(fullname)

                mtime = os.stat(fullname).st_mtime
            except os.error as os_err:
                # if can't stat the file emit a warning instead error
                # like in traditional backup
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    f"Could net get stat-info for reference file {fullname}: {os_err}\n",
                )
                continue
            bareosfd.DebugMessage(
                150,
                (
                    f"file:{fullname}"
                    f" fullTime: {self.last_backup_stop_time}"
                    f" mtime: {mtime}\n"
                ),
            )
            if mtime > self.last_backup_stop_time + 1:
                bareosfd.DebugMessage(
                    150,
                    (
                        f"file:{fullname}"
                        f" fullTime: {self.last_backup_stop_time}"
                        f" mtime: {mtime}\n"
                    ),
                )
                self.files_to_backup.append(fullname)
                if os.path.isdir(fullname) and filename not in self.ignore_subdirs:
                    for topdir, dirnames, filenames in os.walk(fullname):
                        for filename in filenames:
                            self.files_to_backup.append(os.path.join(topdir, filename))
                        for dirname in dirnames:
                            fulldirname = os.path.join(topdir, dirname) + "/"
                            self.files_to_backup.append(fulldirname)

    def check_tablespace_in_cluster(self):
        """
        We will check if any tablespace exist in the cluster.
        Collect the oid and parse the real content location.
        """
        try:
            tablespace_stmt = (
                    "select oid,spcname from pg_tablespace"
                    " where spcname not in ('pg_default','pg_global');"
                )
            results = self.db_con.run(tablespace_stmt)
            for row in results:
                oid, tablespace_name = row
                spacelink = os.path.realpath(
                    os.path.join(
                        self.options["postgresql_data_dir"], "pg_tblspc", str(oid)
                    )
                )
                bareosfd.DebugMessage(
                    100,
                    (
                        f"tablespacemap found: oid={oid} name={tablespace_name}\n"
                        f" adding {spacelink} to build_file_to_backup\n"
                    ),
                )
                self.build_files_to_backup(spacelink)
        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"tablespacemap checking statement failed: {err}"
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_Error

    def check_options(self, mandatory_options=None):
        """
        Check for mandatory options and verify database connection
        """
        result = super().check_options(mandatory_options)
        if not result == bareosfd.bRC_OK:
            return result
        # Accurate will cause problems with plugins
        accurate_enabled = bareosfd.GetValue(bareosfd.bVarAccurate)
        if accurate_enabled is not None and accurate_enabled != 0:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    "start_backup_job: Accurate backup not allowed."
                    " Please disable in Job\n"
                ),
            )
            return bareosfd.bRC_Error

        if not self.options["postgresql_data_dir"].endswith("/"):
            self.options["postgresql_data_dir"] += "/"

        if not self.options["wal_archive_dir"].endswith("/"):
            self.options["wal_archive_dir"] += "/"

        if "ignore_subdirs" in self.options:
            self.ignore_subdirs = self.options["ignore_subdirs"]

        self.backup_label_filename = (
            self.options["postgresql_data_dir"] + "backup_label"
        )

        # get PostgreSQL connection settings from environment like libpq
        self.db_user = os.environ.get("PGUSER", getpass.getuser())
        self.db_password = os.environ.get("PGPASSWORD", "")
        self.db_host = os.environ.get("PGHOST", "localhost")
        self.db_port = os.environ.get("PGPORT", "5432")
        self.db_name = os.environ.get("PGDATABASE", "postgres")

        if "db_user" in self.options:
            self.db_user = self.options.get("db_user")
        if "db_password" in self.options:
            self.db_password = self.options.get("db_password")
        if "db_port" in self.options:
            self.db_port = self.options.get("db_port", "5432")
        # emulate behavior of libpq handling of unix domain sockets
        # i.e. there only the socket directory is set and ".s.PGSQL.<db_port>"
        # is appended before opening
        if "db_host" in self.options:
            if self.options["db_host"].startswith("/"):
                if not self.options["db_host"].endswith("/"):
                    self.options["db_host"] += "/"
                self.db_host = self.options["db_host"] + ".s.PGSQL." + self.db_port
            else:
                self.db_host = self.options["db_host"]
        if "db_name" in self.options:
            self.db_name = self.options.get("db_name", "postgres")

        if "switch_wal" in self.options:
            self.switch_wal = self.options["switch_wal"].lower() == "true"

        if "switch_wal_timeout" in self.options:
            try:
                self.switch_wal_timeout = int(self.options["switch_wal_timeout"])
            except ValueError as err:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    (
                        f"start_backup_job: Plugin option switch_wal_timeout"
                        f" {self.options['switch_wal_timeout']} is not an integer.\n"
                        f"{err}\n"
                    ),
                )
                return bareosfd.bRC_Error

        if "start_fast" in self.options:
            self.start_fast = bool(self.options["start_fast"])

        if "stop_wait_wal_archive" in self.options:
            self.stop_wait_wal_archive = bool(self.options["stop_wait_wal_archive"])

        # TODO
        # handle ssl_context support in connection
        # Normally not needed as the bareos-fd has to be located on host within
        # the pg cluster: as such using socket is preferably.

        return bareosfd.bRC_OK

    def create_db_connection(self):
        """
        Setup the db connection, and check pg cluster version.
        """
        bareosfd.DebugMessage(
            100, "create_check_db_connection in Postgresql Plugin called\n"
        )
        try:
            if self.options["db_host"].startswith("/"):
                self.db_con = pg8000.Connection(
                    self.db_user,
                    password=self.db_password,
                    database=self.db_name,
                    unix_sock=self.db_host,
                )
            else:
                self.db_con = pg8000.Connection(
                    self.db_user,
                    password=self.db_password,
                    database=self.db_name,
                    host=self.db_host,
                    port=self.db_port,
                )

            if "role" in self.options:
                self.db_con.run(f"SET ROLE {self.options['role']}")
                bareosfd.DebugMessage(
                    100, f"SQL role switched to {self.options['role']}\n"
                )
            self.pg_version = int(
                self.db_con.run("SELECT current_setting('server_version_num')")[0][0]
            )
            self.pg_major_version = self.pg_version // 10000
            bareosfd.JobMessage(
                bareosfd.M_INFO, f"Connected to PostgreSQL version {self.pg_version}\n"
            )
        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    f"Could not connect to database {self.db_name},"
                    f" user {self.db_user}, host: {self.db_host} : '{err}'\n"
                ),
            )
            return bareosfd.bRC_Error

        if self.pg_major_version < 10:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    f"Only PostgreSQL server version >= 10 is supported."
                    f" Version detected {self.pg_major_version}\n"
                ),
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def format_lsn(self, raw_lsn):
        """
        PostgreSQL returns LSN in a non-comparable format with varying length, e.g.
        0/3A6A710
        0/F00000
        We fill the part before the / and after it with leading 0 to get strings
        with equal length
        """
        lsn_pre, lsn_post = raw_lsn.split("/")
        return lsn_pre.zfill(8) + "/" + lsn_post.zfill(8)

    def lsn_to_int(self, raw_lsn):
        """
        Return LSN converted to integer safer to compare than string
        """
        return int("".join(map(lambda x: x.zfill(8), raw_lsn.split("/"))), base=16)

    def plugin_io_read(self, IOP):
        """
        plugin_io_read function specific for our virtual files and ROP
        """
        if self.file_type == "FT_REG":
            bareosfd.DebugMessage(
                200,
                (
                    f"BareosFdPluginPostgreSQL:plugin_io_read reading {IOP.count}"
                    f" from file {self.FNAME}\n"
                ),
            )
            IOP.buf = bytearray(IOP.count)
            if self.FNAME == "ROP":
                # should never be the case with no_read = True
                IOP.buf = bytearray()
                IOP.status = 0
                IOP.io_errno = 0
                bareosfd.DebugMessage(
                    100,
                    (
                        "BareosFdPluginPostgreSQL:plugin_io_read for ROP\n"
                        "That can't be true with no_read = True\n"
                    ),
                )
            if self.FNAME == self.backup_label_filename:
                bdata = bytearray(self.backup_label_data, "utf-8")
            elif self.FNAME == self.recovery_filename:
                bdata = self.recovery_data
            elif self.FNAME == self.tablespace_map_filename:
                bdata = bytearray(self.tablespace_map_data, "utf-8")
            else:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL, f'Unexpected file name "{self.FNAME}"'
                )
                return bareosfd.bRC_Error

            if self.data_stream is None:
                bareosfd.DebugMessage(
                    250,
                    (
                        f"BareosFdPluginPostgreSQL:plugin_io_read type of"
                        f" bdata {type(bdata)}\n"
                    ),
                )
                self.data_stream = io.BytesIO(bdata)

            try:
                IOP.status = self.data_stream.readinto(IOP.buf)
                bareosfd.DebugMessage(
                    250,
                    (
                        f"BareosFdPluginPostgreSQL:plugin_io_read"
                        f" IOP.status {IOP.status}\n{self.data_stream}\n"
                    ),
                )

                if IOP.status == 0:
                    self.data_stream = None
                IOP.io_errno = 0
            except Exception as err:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    f'Could net read {IOP.count} bytes from "{str(bdata)}". "{err}"',
                )
                # IOP.io_errno = err.errno
                return bareosfd.bRC_Error
        else:
            bareosfd.DebugMessage(
                100,
                (
                    f"BareosFdPluginPostgreSQL:plugin_io_read Did not read from"
                    f" file {self.FNAME} (type {self.file_type})\n"
                ),
            )
            IOP.buf = bytearray()
            IOP.status = 0
            IOP.io_errno = 0

        return bareosfd.bRC_OK

    def plugin_io(self, IOP):
        """
        IO_OPEN = 1,
        IO_READ = 2,
        IO_WRITE = 3,
        IO_CLOSE = 4,
        IO_SEEK = 5
        """
        bareosfd.DebugMessage(
            250, f"BareosFdPluginPostgreSQL:plugin_io called with function {IOP.func}\n"
        )
        if IOP.func == bareosfd.IO_OPEN:
            if IOP.fname in self.virtual_files:
                self.FNAME = IOP.fname
                # Force it so upper plugin will not do it wrong.
                if self.FNAME == "ROP":
                    self.file_type = "FT_RESTORE_FIRST"
                else:
                    self.file_type = "FT_REG"
                # Force io in plugin
                IOP.status = bareosfd.iostat_do_in_plugin
                bareosfd.DebugMessage(
                    250,
                    (
                        f"BareosFdPluginPostgreSQL:plugin_io open find {self.FNAME}"
                        f" in {self.virtual_files}\n"
                    ),
                )
                return bareosfd.bRC_OK

            result = super().plugin_io_open(IOP)
            if self.file is not None:
                if result == bareosfd.bRC_OK and not self.file.closed:
                    IOP.filedes = self.file.fileno()
                    IOP.status = bareosfd.iostat_do_in_core
            return result
        elif IOP.func == bareosfd.IO_READ:
            if self.FNAME in self.virtual_files:
                bareosfd.DebugMessage(
                    250,
                    (
                        f"BareosFdPluginPostgreSQL:plugin_io io_read calling"
                        f" self.plugin_io_read for {self.FNAME}\n"
                    ),
                )
                return self.plugin_io_read(IOP)

            return super().plugin_io_read(IOP)
        elif IOP.func == bareosfd.IO_WRITE:
            return super().plugin_io_write(IOP)
        elif IOP.func == bareosfd.IO_CLOSE:
            return super().plugin_io_close(IOP)
        elif IOP.func == bareosfd.IO_SEEK:
            return super().plugin_io_seek(IOP)

    def start_backup_job(self):
        """
        Create the file list and tell PostgreSQL we start a backup now.
        """
        bareosfd.DebugMessage(100, "start_backup_job in PostgresqlPlugin called\n")

        self.create_db_connection()

        if chr(self.level) == "F":
            # For Full we backup the PostgreSQL data directory
            start_dir = self.options["postgresql_data_dir"]
            bareosfd.DebugMessage(100, f"data_dir: {start_dir}\n")
            bareosfd.JobMessage(bareosfd.M_INFO, f"data_dir: {start_dir}\n")
        else:
            # If level is not Full, we only backup WAL files
            # and create a restore object ROP with timestamp information.

            # Check if we are running the same PG Major version for incremental.
            # Otherwise fail requesting a new full.
            # Note: if we want to support creating incrementals on top
            #       of fulls from the old plugin we would need to handle
            #       this case differently
            if self.last_pg_major is None:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    (
                        "no pg version from previous backup recorded."
                        " Is the previous backup from an old plugin?\n"
                    ),
                )
                return bareosfd.bRC_Error

            if self.pg_major_version != self.last_pg_major:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    (
                        f"pg version of previous backup {self.last_pg_major} does not"
                        f" match current pg version {self.pg_major_version}."
                        f" Please run a new Full.\n"
                    ),
                )
                return bareosfd.bRC_Error

            start_dir = self.options["wal_archive_dir"]
            self.files_to_backup.append("ROP")
            self.virtual_files.append("ROP")
            # get current Log Sequence Number (LSN)
            # PG10: pg_current_wal_lsn
            get_lsn_stmt = "SELECT pg_current_wal_lsn()"
            switch_lsn_stmt = "SELECT pg_switch_wal()"
            try:
                current_lsn = self.db_con.run(get_lsn_stmt)[0][0]
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    f"Current LSN {current_lsn}, last LSN: {self.last_lsn}\n",
                )
            except Exception as err:
                current_lsn = 0
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    (
                        f"Could not get current LSN, last LSN was: {self.last_lsn}"
                        f": {err} \n"
                    ),
                )
            # TODO maybe we want to ensure that pg_walfile_name_offset()
            # return 0 bytes between lsn.
            current_lsn_formated = self.lsn_to_int(current_lsn)
            last_lsn_formated = self.lsn_to_int(self.last_lsn)
            if self.switch_wal and current_lsn_formated > last_lsn_formated:
                # Let PostgreSQL write latest transaction into a new WAL file now
                try:
                    result = self.db_con.run(switch_lsn_stmt)
                except Exception as err:
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        f"Could not switch to next WAL segment: {err}\n",
                    )
                # Pick the new raw lsn and update our last_lsn
                try:
                    current_lsn_raw = self.db_con.run(get_lsn_stmt)[0][0]
                    current_lsn = current_lsn_raw
                    bareosfd.DebugMessage(
                        150,
                        (
                            f"after pg_switch_wal(): current_lsn: {current_lsn}"
                            f" last_lsn: {self.last_lsn}\n"
                        ),
                    )
                    self.last_lsn = current_lsn
                except Exception as err:
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        (
                            f"Could not read LSN after switching to"
                            f" new WAL segment: {err}\n"
                        ),
                    )

                if not self.wait_for_wal_archiving(current_lsn):
                    return bareosfd.bRC_Error

            else:
                # Nothing has changed since last backup - only send ROP this time
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    f"Same LSN {current_lsn} as last time - nothing to do\n",
                )
                return bareosfd.bRC_OK

        # Plugins needs to build by themselves the list of file/dir to backup
        # Gather files from start_dir:
        #     - postgresql_data_dir for full
        #         ask PG if there's tablespace in use, if yes decode
        #         and add the real location too.
        #     - wal_archive_dir for incr/diff jobs

        self.build_files_to_backup(start_dir)

        # If level is not Full, we are done here and set the new
        # last_backup_stop_time as reference for future jobs
        # Will be written into the Restore Object
        if not chr(self.level) == "F":
            self.last_backup_stop_time = int(time.time())
            return bareosfd.bRC_OK

        # For Full in non exclusive mode we can't know if there's
        # already a running job. Document how to use
        # Allow Duplicate Job = no to exclude that case in job config.

        self.check_tablespace_in_cluster()
        # Add PG major version to the label that can help Ops to distinguish backup.
        self.backup_label_string = (
            f"Bareos.plugin.postgresql.jobid.{self.jobId}.PG{self.pg_major_version}"
        )
        if self.pg_major_version < 15:
            start_stmt = (
                f"pg_start_backup('{self.backup_label_string}',"
                f"fast => {self.start_fast},"
                f"exclusive => False)"
            )
        else:
            start_stmt = (
                f"pg_backup_start('{self.backup_label_string}',"
                f"fast => {self.start_fast})"
            )
        bareosfd.DebugMessage(100, f"Send 'SELECT {start_stmt}' to PostgreSQL\n")
        bareosfd.DebugMessage(100, f"backup label = {self.backup_label_string}\n")
        # We tell PostgreSQL that we want to start to backup file now
        self.backup_start_time = datetime.datetime.now(
            tz=dateutil.tz.tzoffset(None, self.tz_offset)
        )
        try:
            result = self.db_con.run(f"SELECT {start_stmt};")
        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"{start_stmt} statement failed: {err}"
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(150, f"Start response LSN: {str(result)}\n")
        self.full_backup_running = True

        return bareosfd.bRC_OK

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        In this example only files (no directories) are allowed.
        We distinguish normal files from the virtuals and ROP.
        """
        bareosfd.DebugMessage(100, "start_backup_file called\n")
        if not self.files_to_backup:
            bareosfd.DebugMessage(100, "No files to backup\n")
            return bareosfd.bRC_Skip

        try:
            self.file_to_backup = self.files_to_backup.pop().encode("utf-8")
        except UnicodeEncodeError:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                f"name {repr(self.file_to_backup)} cannot be encoded in utf-8",
                )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, f"file: {self.file_to_backup}\n")
        statp = bareosfd.StatPacket()
        if self.file_to_backup == "ROP":
            self.rop_data["last_backup_stop_time"] = self.last_backup_stop_time
            self.rop_data["last_lsn"] = self.last_lsn
            self.rop_data["plugin_version"] = 2
            self.rop_data["pg_major_version"] = self.pg_major_version
            savepkt.fname = "/_bareos_postgresql_plugin/metadata"
            savepkt.type = bareosfd.FT_RESTORE_FIRST
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(json.dumps(self.rop_data), "utf-8")
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
            savepkt.statp = statp
            savepkt.no_read = True
            bareosfd.DebugMessage(150, f"rop data: {str(self.rop_data)}\n")
        else:
            if self.file_to_backup in self.virtual_files:
                # We affect owner, group, mode from previously saved PG_VERSION
                # Time is the backup time
                statp.st_mode = self.ref_statp.st_mode
                statp.st_uid = self.ref_statp.st_uid
                statp.st_gid = self.ref_statp.st_gid
                statp.st_atime = int(time.time())
                statp.st_mtime = int(time.time())
                statp.st_ctime = int(time.time())
                savepkt.type = bareosfd.FT_REG
                if self.file_to_backup == self.backup_label_filename:
                    statp.st_size = len(self.backup_label_data)
                    savepkt.fname = self.backup_label_filename
                elif self.file_to_backup == self.recovery_filename:
                    statp.st_size = len(self.recovery_data)
                    savepkt.fname = self.recovery_filename
                elif (
                    self.file_to_backup == self.tablespace_map_filename
                    and self.tablespace_map_filename is not None
                ):
                    statp.st_size = len(self.tablespace_map_data)
                    savepkt.fname = self.tablespace_map_filename
            else:
                # Rest of our dirs/files
                savepkt.fname = self.file_to_backup
                # emit a warning and skip if file can't be read like in normal backup
                try:
                # os.islink will detect links to directories only when
                # there is no trailing slash - we need to perform checks
                # on the stripped name but use it with trailing / for the backup itself
                    if os.path.islink(self.file_to_backup.rstrip(b"/")):
                        statp = os.lstat(self.file_to_backup)
                        savepkt.type = bareosfd.FT_LNK
                        savepkt.link = os.readlink(self.file_to_backup.rstrip(b"/"))
                    else:
                        statp = os.stat(self.file_to_backup)
                        if os.path.isfile(self.file_to_backup):
                            savepkt.type = bareosfd.FT_REG
                        elif os.path.isdir(self.file_to_backup):
                            savepkt.type = bareosfd.FT_DIREND
                            savepkt.link = self.file_to_backup
                        elif stat.S_ISFIFO(os.stat(self.file_to_backup).st_mode):
                            savepkt.type = bareosfd.FT_FIFO
                        else:
                            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                                (
                                    f"Unknown error."
                                    f"Don't know how to handle {self.file_to_backup}\n"
                                ),
                            )
                            return bareosfd.bRC_Error
                except os.error as os_err:
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        f"Skipping disappeared file {self.file_to_backup}: \"{os_err}\"",
                    )
                    return bareosfd.bRC_Skip


            # for both virtual and normal file
            savepkt.statp = statp
            savepkt.no_read = False

        bareosfd.DebugMessage(150, f"file name: {str(savepkt.fname)}\n")
        bareosfd.DebugMessage(150, f"file type: {savepkt.type}\n")
        bareosfd.DebugMessage(150, f"file statpx: {str(savepkt.statp)}\n")

        return bareosfd.bRC_OK

    def restore_object_data(self, ROP):
        """
        Called on restore and on diff/inc jobs.
        """
        # ROP.object is of type bytearray.
        self.row_rop_raw = ROP.object.decode("UTF-8")
        try:
            self.rop_data[ROP.jobid] = json.loads(self.row_rop_raw)
        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    f"Could not parse restore object json-data"
                    f" '{self.row_rop_raw}' / '{err}'\n"
                ),
            )
            return bareosfd.bRC_Error

        if "last_backup_stop_time" in self.rop_data[ROP.jobid]:
            self.last_backup_stop_time = int(
                self.rop_data[ROP.jobid]["last_backup_stop_time"]
            )
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                (
                    f"Got last_backup_stop_time {self.last_backup_stop_time}"
                    f" from restore object of job {ROP.jobid}\n"
                ),
            )
        if (
            "last_lsn" in self.rop_data[ROP.jobid]
            and self.rop_data[ROP.jobid]["last_lsn"] is not None
        ):
            self.last_lsn = self.rop_data[ROP.jobid]["last_lsn"]
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                f"Got last_lsn {self.last_lsn} from restore object of job {ROP.jobid}\n",
            )

        if (
            "pg_major_version" in self.rop_data[ROP.jobid]
            and self.rop_data[ROP.jobid]["pg_major_version"] is not None
        ):
            self.last_pg_major = self.rop_data[ROP.jobid]["pg_major_version"]
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                (
                    f"Got pg major version {self.last_pg_major} from restore object"
                    f" of job {ROP.jobid}\n"
                ),
            )

        return bareosfd.bRC_OK

    def close_db_connection(self):
        """
        Make sure the DB connection is closed properly.
        Will not throw if the connection was already closed.
        """
        message = "Database connection closed.\n"
        if self.db_con is None:
            return

        try:
            self.db_con.close()
        except pg8000.exceptions.InterfaceError:
            pass

        bareosfd.JobMessage(bareosfd.M_INFO, message)

    def complete_backup_job(self):
        """
        Call pg_backup_stop() on PostgreSQL DB to mark the backup job as completed.

        https://www.postgresql.org/docs/current/continuous-archiving.html#BACKUP-LOWLEVEL-BASE-BACKUP
        pg_backup_stop will return one row with three values.
        The second of these fields should be written to a file named
        backup_label in the root directory of the backup.
        The third field should be written to a file named
        tablespace_map unless the field is empty.
        """
        bareosfd.DebugMessage(
            100,
            ("complete_backup_job_and_close_db(), entry point in Python called\n"),
        )

        if self.pg_major_version < 15:
            stop_stmt = (
                f"pg_stop_backup(exclusive => False,"
                f" wait_for_archive => {self.stop_wait_wal_archive})"
            )
        else:
            stop_stmt = f"pg_backup_stop({self.stop_wait_wal_archive})"
        try:
            bareosfd.DebugMessage(100, f"Send '{stop_stmt}' to PostgreSQL\n")
            first_row = self.db_con.run(f"SELECT {stop_stmt};")[0][0]
            bareosfd.DebugMessage(
                100,
                (
                    f"row returned type is {type(first_row)}\n"
                    f"isstr:{isinstance(first_row, str)}\n"
                    f"content:'{first_row}'\n"
                ),
            )
            if isinstance(first_row, str):
                first_row = parse_row(first_row)

            bareosfd.DebugMessage(
                150,
                (
                    f"pg_backup_stop return with: '{first_row}'\n"
                    f"Being a type of '{type(first_row)}'\n"
                    f"With a length of '{len(first_row)}'\n"
                ),
            )

            self.last_lsn, self.backup_label_data, self.tablespace_map_data = first_row

            self.backup_label_filename = (
                self.options["postgresql_data_dir"] + "backup_label"
            )
            bareosfd.DebugMessage(
                100,
                (
                    f"backup_label file created in"
                    f" '{self.backup_label_filename}'\n"
                    f"LSN='{self.last_lsn}'\n"
                    f"Label='{self.backup_label_data}'\n"
                ),
            )

            if len(self.tablespace_map_data) > 0 and self.tablespace_map_data != "":
                self.tablespace_map_filename = (
                    self.options["postgresql_data_dir"] + "tablespace_map"
                )
                bareosfd.DebugMessage(
                    100,
                    (
                        f"tablespace_map create in {self.tablespace_map_filename}\n"
                        f"Content='{self.tablespace_map_data}'\n"
                    ),
                )

            bareosfd.DebugMessage(100, f"pg_major_version {self.pg_major_version}\n")
            recovery_string = (
                f"# Created by bareos {bareosfd.GetValue(bareosfd.bVarJobName)}\n"
                f"# on PostgreSQL cluster version {str(self.pg_version)}\n"
            )
            if self.pg_major_version >= 10 and self.pg_major_version < 12:
                self.recovery_filename = (
                    self.options["postgresql_data_dir"] + "recovery.conf"
                )
                self.recovery_data = bytes(
                    f"{recovery_string}\n# Place here your restore command\n", "utf-8"
                )
            if self.pg_major_version >= 12:
                self.recovery_filename = (
                    self.options["postgresql_data_dir"] + "recovery.signal"
                )
                self.recovery_data = bytes(recovery_string, "utf-8")

            bareosfd.DebugMessage(100, f"len recovery_data {len(self.recovery_data)}\n")
            bareosfd.DebugMessage(
                100,
                (
                    f"recovery file create in {self.recovery_filename}\n"
                    f"with data {self.recovery_data}\n"
                ),
            )
            self.last_backup_stop_time = int(time.time())

        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_ERROR, f"pg_backup_stop statement failed: {err}\n"
            )

        self.full_backup_running = False

    def check_for_wal_files(self):
        """
        Look for new WAL files and backup
        Backup start time is timezone aware, we need to add timezone
        to files' mtime to make them comparable
        """
        # We have to add local timezone to the file's timestamp in order
        # to compare them with the backup starttime, which has a timezone
        # TODO check after PR1536 is merged
        wal_archive_dir = self.options["wal_archive_dir"]
        self.files_to_backup.append(wal_archive_dir)
        for filename in os.listdir(wal_archive_dir):
            full_path = os.path.join(wal_archive_dir, filename)
            try:
                wal_st = os.stat(full_path)
            except os.error as os_err:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    f"Could not get stat-info for file {full_path}: {os_err}\n",
                )
                continue
            file_mtime = datetime.datetime.fromtimestamp(wal_st.st_mtime)
            if (
                file_mtime.replace(tzinfo=dateutil.tz.tzoffset(None, self.tz_offset))
                > self.backup_start_time
            ):
                bareosfd.DebugMessage(150, f"Adding WAL file {filename} for backup\n")
                self.files_to_backup.append(full_path)

        if self.files_to_backup:
            return bareosfd.bRC_More

        return bareosfd.bRC_OK

    def end_backup_file(self):
        """
        Here we return 'bareosfd.bRC_More' as long as our list files_to_backup is not
        empty and bareosfd.bRC_OK when we are done
        """
        bareosfd.DebugMessage(100, "end_backup_file() entry point in Python called\n")
        if self.files_to_backup:
            return bareosfd.bRC_More

        if self.full_backup_running:
            self.complete_backup_job()
            # Now we can also create the Restore object with the right timestamp
            # We start by ROP so it is the last object in backup and first in restore
            self.virtual_files = [
                "ROP",
                self.backup_label_filename,
                self.recovery_filename,
            ]
            if self.tablespace_map_filename is not None:
                self.virtual_files.append(self.tablespace_map_filename)
            self.files_to_backup += self.virtual_files
            return self.check_for_wal_files()

        self.close_db_connection()

        return bareosfd.bRC_OK

    def end_backup_job(self):
        """
        Called if backup job ends, before ClientAfterJob
        Make sure that db connection was closed in any way,
        especially when job was canceled
        """
        if self.full_backup_running:
            self.complete_backup_job()
            self.full_backup_running = False
        else:
            self.close_db_connection()
        return bareosfd.bRC_OK

    def wait_for_wal_archiving(self, lsn):
        """
        Wait for wal archiving to be finished by checking if the wal file
        for the given LSN is present in the filesystem.
        """

        if self.pg_major_version >= 10:
            wal_filename_func = "pg_walfile_name"
        else:
            wal_filename_func = "pg_xlogfile_name"

        walfile_stmt = f"SELECT {wal_filename_func}('{lsn}')"

        try:
            wal_filename = self.db_con.run(walfile_stmt)[0][0]

            bareosfd.DebugMessage(
                100,
                f"wait_for_wal_archiving({lsn}): wal filename={wal_filename}\n",
            )

        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"Error getting WAL filename for LSN {lsn}\n{err}\n"
            )
            return False

        wal_file_path = self.options["wal_archive_dir"] + wal_filename

        # To finish as quick as possible but with low impact on a heavy loaded
        # system, we use increasing sleep times here, starting with a small value
        sleep_time = 0.01
        slept_sum = 0.0
        while slept_sum <= self.switch_wal_timeout:
            if os.path.exists(wal_file_path):
                return True
            time.sleep(sleep_time)
            slept_sum += sleep_time
            sleep_time *= 1.2

        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            (
                f"Timeout waiting {self.switch_wal_timeout} sec."
                f" for wal file {wal_filename} to be archived\n"
            ),
        )
        return False


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
