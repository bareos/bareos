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
from sys import version
import stat
import time
from collections import deque
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

        # The contents of the directories pg_dynshmem/, pg_notify/, pg_serial/, pg_snapshots/,
        # pg_stat_tmp/, and pg_subtrans/ can be omitted from the backup as they will be initialized
        # on postmaster startup. (but not the directories themselves)
        # They are mandatory to make pg cluster start successful.
        # By default we will exclude them so os.walk will not go into it,
        # but we re-add the dir afterwards in case of Full backup.
        self.mandatory_subdirs = [
            "pg_dynshmem",
            "pg_notify",
            "pg_serial",
            "pg_snapshots",
            "pg_stat_tmp",
            "pg_subtrans",
            "pg_wal",
        ]
        # log is not exclude by default but can be a good candidate.
        # pgsql_tmp can have several locations.
        self.ignore_subdirs = [
            # "log",
            "pgsql_tmp",
        ]
        self.ignore_subdirs.extend(self.mandatory_subdirs)
        # pg_internal.init files can be omitted from the backup whenever a file of that name is
        # found. These files contain relation cache data that is always rebuilt when recovering.
        self.ignore_files = ["pg_internal.init"]
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
        self.fname = ""
        self.file_type = ""
        self.paths_to_backup = deque()
        self.file_to_backup = ""
        # Store os.stat of PG_VERSION as reference for our virtual files
        self.ref_statp = None
        # We need to get the stat-packet in set_file_attributes and use it again
        # in end_restore_file, and this may be mixed up with different files
        self.stat_packets = {}
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

    def __build_paths_to_backup(self, start_dir):
        """
        Build the tree of paths to be backed up by recursing from top directory.
        To be able to exclude the ignore_subdirs we use os.walk with TopDown True.
        The deque is reversed, so file appear first, and directories at the end.
        self.paths_to_backup is extended with it.

        That function be used to parse `postgresql_data_dir`, `wal_archive_dir`, `tablespace`
        """
        if not start_dir.endswith("/"):
            start_dir += "/"
        self.paths_to_backup.append(start_dir)
        # local store for searching tree
        _paths = deque()
        for topdir, dirnames, filenames in os.walk(start_dir, topdown=True):
            # TODO check that assumption
            # Usually Bareos takes care about timestamps when doing
            # incremental backups but here we have to compare against
            # last BackupPostgreSQL timestamp
            # 20230926 Why should we, or if yes isn't that needed only for wal ?

            # Filter out any ignored subdirectory
            dirnames[:] = [
                dirname for dirname in dirnames if not dirname in self.ignore_subdirs
            ]

            for filename in filenames:
                if (
                    not os.path.dirname(os.path.normpath(filename))
                    in self.ignore_subdirs
                    and not filename in self.ignore_files
                ):
                    fullname = os.path.join(topdir, filename)
                    try:
                        # Create the reference file
                        if filename == "PG_VERSION":
                            self.ref_statp = os.stat(fullname)
                        mtime = os.stat(fullname).st_mtime
                        if mtime > self.last_backup_stop_time + 1:
                            bareosfd.DebugMessage(
                                150,
                                (
                                    f"file:{fullname}"
                                    f" fullTime: {self.last_backup_stop_time}"
                                    f" os.st_mtime: {mtime}\n"
                                ),
                            )
                            _paths.append(fullname)
                    except os.error as os_err:
                        # if can't stat the file emit a warning instead error
                        # like in traditional backup
                        bareosfd.JobMessage(
                            bareosfd.M_WARNING,
                            f"Could not stat reference file {fullname}: {os_err}\n",
                        )
                        continue
            # TODO check if we should also check mtime for dirs.
            for dirname in dirnames:
                if not dirname in self.ignore_subdirs:
                    fulldirname = os.path.join(topdir, dirname) + "/"
                    _paths.append(fulldirname)

        _paths.reverse()
        # Now re-add excluded mandatory_subdirs as directory only.
        # But only for Full and pg_working_dir
        if (
            self.full_backup_running
            and start_dir == self.options["postgresql_data_dir"]
        ):
            for dirname in self.mandatory_subdirs:
                _paths.append(os.path.join(start_dir, dirname) + "/")

        self.paths_to_backup.extend(_paths)

    def __check_for_wal_files(self):
        """
        Look for new WAL files and backup
        Backup start time is timezone aware, we need to add timezone
        to files' mtime to make them comparable
        """
        # We have to add local timezone to the file's timestamp in order
        # to compare them with the backup starttime, which has a timezone
        # TODO recheck after PR1536 is merged
        wal_archive_dir = self.options["wal_archive_dir"]
        self.paths_to_backup.append(wal_archive_dir)
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
                self.paths_to_backup.append(full_path)

        if self.paths_to_backup:
            return bareosfd.bRC_More

        return bareosfd.bRC_OK

    def __check_lsn_diff(self, current_lsn, last_lsn):
        """
        Verify byte difference between 2 lsn using pg_wal_lsn_diff()
        Return True if any.
        """
        diff_exist = False
        try:
            result = self.db_con.run(
                f"SELECT pg_wal_lsn_diff('{current_lsn}','{last_lsn}')"
            )[0][0]
            if not int(result):
                raise ValueError

            if result > 0:
                diff_exist = True

            bareosfd.JobMessage(
                bareosfd.M_INFO,
                (
                    f"A difference was found {(diff_exist)}, "
                    f" between current_lsn {current_lsn} and last LSN: {last_lsn}\n"
                ),
            )
        except pg8000.Error as pg_err:
            current_lsn = 0
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                (f"Error checking lsn difference was: {result}" f": {pg_err} \n"),
            )
        except ValueError as val_err:
            current_lsn = 0
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                (
                    f"Error checking lsn difference was: {repr(result)}"
                    f": {val_err} \n"
                    f" between current_lsn {current_lsn} and last LSN: {last_lsn}\n"
                ),
            )

        return diff_exist

    def __check_pg_major_version(self):
        """
        For incremental we check if the same PG Major version as previous backup is used.
        Otherwise fail requesting a new full.
        """

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
                    f"PostgreSQL version of previous backup {self.last_pg_major}"
                    f" does not match current version {self.pg_major_version}."
                    f" Please run a new Full.\n"
                ),
            )
            return bareosfd.bRC_Error

    def __check_tablespace_in_cluster(self):
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
                self.__build_paths_to_backup(spacelink)
        except pg8000.Error as pg_err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"tablespacemap checking statement failed: {pg_err}"
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_Error

    def __close_db_connection(self):
        """
        Make sure the DB connection is closed properly.
        Will not throw if the connection was already closed.
        """
        if self.db_con is None:
            return

        try:
            self.db_con.close()
        except pg8000.exceptions.InterfaceError:
            pass

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            "Database connection closed.\n",
        )

    def __complete_backup_job(self):
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
            ("__complete_backup_job_and_close_db(), entry point in Python called\n"),
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
            # Split the 3 columns
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

        except pg8000.Error as pg_err:
            bareosfd.JobMessage(
                bareosfd.M_ERROR, f"pg_backup_stop statement failed: {pg_err}\n"
            )
        except Exception as err:
            bareosfd.JobMessage(
                bareosfd.M_ERROR, f"__complete_backup_job unknown failure: {err}\n"
            )

        self.full_backup_running = False

    def __create_db_connection(self):
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
        except pg8000.Error as pg_err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    f"Could not connect to database {self.db_name},"
                    f" user {self.db_user}, host: {self.db_host} : '{pg_err}'\n"
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

    def __do_switch_wal(self):
        """
        Let PostgreSQL write latest transaction into a new WAL file now
        """
        try:
            result = self.db_con.run("SELECT pg_switch_wal()")
        except pg8000.Error as pg_err:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                f"Could not switch to next WAL segment: {pg_err}\n",
            )

    def __get_current_lsn(self):
        """
        Get and return current Log Sequence Number (LSN)
        """
        try:
            current_lsn = self.db_con.run("SELECT pg_current_wal_lsn()")[0][0]
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                f"Current LSN {current_lsn}, last LSN: {self.last_lsn}\n",
            )
        except pg8000.Error as pg_err:
            current_lsn = 0
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                (
                    f"Could not get current LSN, last LSN was: {self.last_lsn}"
                    f": {pg_err} \n"
                ),
            )
        return current_lsn

    def __pg_backup_start(self):
        """
        Run pg_backup_start depending on pg_major version
        Return bRC_OK if success
        """
        # Add PG major version to the label that can help operator to distinguish backup.
        self.backup_label_string = (
            f"Bareos.plugin.postgresql.jobid.{self.jobId}.PG{self.pg_major_version}"
        )
        if self.pg_major_version >= 15:
            start_stmt = (
                f"pg_backup_start('{self.backup_label_string}',"
                f" fast => {self.start_fast})"
            )
        else:
            start_stmt = (
                f"pg_start_backup('{self.backup_label_string}',"
                f" fast => {self.start_fast},"
                f" exclusive => False)"
            )
        bareosfd.DebugMessage(100, f"Send 'SELECT {start_stmt}' to PostgreSQL\n")
        bareosfd.DebugMessage(100, f"backup label = {self.backup_label_string}\n")
        # We tell PostgreSQL that we want to start to backup file now
        self.backup_start_time = datetime.datetime.now(
            tz=dateutil.tz.tzoffset(None, self.tz_offset)
        )
        try:
            result = self.db_con.run(f"SELECT {start_stmt};")
        except pg8000.Error as pg_err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"{start_stmt} statement failed: {pg_err}"
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(150, f"Start response LSN: {str(result)}\n")
        return bareosfd.bRC_OK

    def __wait_for_wal_archiving(self, lsn):
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
                f"__wait_for_wal_archiving({lsn}): wal filename={wal_filename}\n",
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
            self.ignore_subdirs.extend(self.options["ignore_subdirs"])

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

        # TODO handle ssl_context support in connection
        # Normally not needed as the bareos-fd has to be located on host within
        # the pg cluster: as such using socket is preferably.

        return bareosfd.bRC_OK

    def plugin_io_read(self, IOP):
        """
        plugin_io_read function specific for our virtual files and ROP
        """
        if self.file_type == "FT_REG":
            bareosfd.DebugMessage(
                200,
                (
                    f"BareosFdPluginPostgreSQL:plugin_io_read reading {IOP.count}"
                    f" from file {self.fname}\n"
                ),
            )
            IOP.buf = bytearray(IOP.count)
            if self.fname == "ROP":
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
            if self.fname == self.backup_label_filename:
                bdata = bytearray(self.backup_label_data, "utf-8")
            elif self.fname == self.recovery_filename:
                bdata = self.recovery_data
            elif self.fname == self.tablespace_map_filename:
                bdata = bytearray(self.tablespace_map_data, "utf-8")
            else:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL, f'Unexpected file name "{self.fname}"'
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
                    f" file {self.fname} (type {self.file_type})\n"
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
                self.fname = IOP.fname
                # Force it so upper plugin will not do it wrong.
                if self.fname == "ROP":
                    self.file_type = "FT_RESTORE_FIRST"
                else:
                    self.file_type = "FT_REG"
                # Force io in plugin
                IOP.status = bareosfd.iostat_do_in_plugin
                bareosfd.DebugMessage(
                    250,
                    (
                        f"BareosFdPluginPostgreSQL:plugin_io open find {self.fname}"
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
            if self.fname in self.virtual_files:
                bareosfd.DebugMessage(
                    250,
                    (
                        f"BareosFdPluginPostgreSQL:plugin_io io_read calling"
                        f" self.plugin_io_read for {self.fname}\n"
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

        self.__create_db_connection()
        bareosfd.JobMessage(
            bareosfd.M_INFO,
            f"python: {version} | pg8000: {pg8000.__version__}\n",
        )

        if chr(self.level) == "F":
            # For Full we backup the PostgreSQL data directory
            start_dir = self.options["postgresql_data_dir"]
            bareosfd.DebugMessage(100, f"data_dir: {start_dir}\n")
            bareosfd.JobMessage(bareosfd.M_INFO, f"data_dir: {start_dir}\n")
            self.full_backup_running = True
        else:
            # If level is not Full, we only backup WAL files
            # and create a restore object ROP with timestamp information.

            self.__check_pg_major_version()

            start_dir = self.options["wal_archive_dir"]
            bareosfd.JobMessage(bareosfd.M_INFO, f"data_dir: {start_dir}\n")

            self.paths_to_backup.append("ROP")
            self.virtual_files.append("ROP")

            current_lsn = self.__get_current_lsn()
            lsn_diff = self.__check_lsn_diff(current_lsn, self.last_lsn)
            if self.switch_wal and lsn_diff:
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    f"Difference found in LSN sequence switching wal\n",
                )
                # Ask pg to switch wal
                self.__do_switch_wal()
                # Pick the new lsn and update our last_lsn
                current_lsn = self.__get_current_lsn()
                bareosfd.DebugMessage(
                    150,
                    (
                        f"after pg_switch_wal(): current_lsn: {current_lsn}"
                        f" last_lsn: {self.last_lsn}\n"
                    ),
                )
                self.last_lsn = current_lsn

                if not self.__wait_for_wal_archiving(current_lsn):
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

        self.__build_paths_to_backup(start_dir)

        # If level is not Full, we are done here and set the new
        # last_backup_stop_time as reference for future jobs
        # Will be written into the Restore Object
        if not chr(self.level) == "F":
            self.last_backup_stop_time = int(time.time())
            return bareosfd.bRC_OK

        # For Full in non exclusive mode we can't know if there's
        # already a running job. Document how to use
        # Allow Duplicate Job = no to exclude that case in job config.

        self.__check_tablespace_in_cluster()

        return self.__pg_backup_start()

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        In this example only files (no directories) are allowed.
        We distinguish normal files from the virtuals and ROP.
        """
        bareosfd.DebugMessage(100, "start_backup_file called\n")
        if not self.paths_to_backup:
            bareosfd.DebugMessage(100, "No files to backup\n")
            return bareosfd.bRC_Skip

        try:
            self.file_to_backup = self.paths_to_backup.popleft()
        except UnicodeEncodeError:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                f"name {repr(self.file_to_backup)} cannot be encoded in utf-8",
            )
            return bareosfd.bRC_Error
        bareosfd.DebugMessage(100, f"file: {self.file_to_backup}\n")

        my_statp = bareosfd.StatPacket()
        if self.file_to_backup == "ROP":
            self.rop_data["plugin_version"] = 2
            self.rop_data["pg_major_version"] = self.pg_major_version
            self.rop_data["last_backup_stop_time"] = self.last_backup_stop_time
            self.rop_data["last_lsn"] = self.last_lsn
            savepkt.fname = "/_bareos_postgresql_plugin/metadata"
            savepkt.type = bareosfd.FT_RESTORE_FIRST
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(json.dumps(self.rop_data), "utf-8")
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())
            savepkt.statp = my_statp
            savepkt.no_read = True
            bareosfd.DebugMessage(150, f"rop data: {str(self.rop_data)}\n")
        else:
            if self.file_to_backup in self.virtual_files:
                # We affect owner, group, mode from previously saved PG_VERSION
                # Time is the backup time
                my_statp.st_mode = self.ref_statp.st_mode
                my_statp.st_uid = self.ref_statp.st_uid
                my_statp.st_gid = self.ref_statp.st_gid
                my_statp.st_atime = int(time.time())
                my_statp.st_mtime = int(time.time())
                my_statp.st_ctime = int(time.time())
                savepkt.type = bareosfd.FT_REG
                if self.file_to_backup == self.backup_label_filename:
                    my_statp.st_size = len(self.backup_label_data)
                    savepkt.fname = self.backup_label_filename
                elif self.file_to_backup == self.recovery_filename:
                    my_statp.st_size = len(self.recovery_data)
                    savepkt.fname = self.recovery_filename
                elif (
                    self.file_to_backup == self.tablespace_map_filename
                    and self.tablespace_map_filename is not None
                ):
                    my_statp.st_size = len(self.tablespace_map_data)
                    savepkt.fname = self.tablespace_map_filename
            else:
                # Rest of our dirs/files
                savepkt.fname = self.file_to_backup
                # emit a warning and skip if file can't be read like in normal backup
                try:
                    # os.islink will detect links to directories only when
                    # there is no trailing slash - we need to perform checks
                    # on the stripped name but use it with trailing / for the backup itself
                    if os.path.islink(self.file_to_backup.rstrip("/")):
                        statp = os.lstat(self.file_to_backup)
                        savepkt.type = bareosfd.FT_LNK
                        savepkt.link = os.readlink(self.file_to_backup.rstrip("/"))
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

                    my_statp.st_mode = statp.st_mode
                    my_statp.st_ino = statp.st_ino
                    my_statp.st_dev = statp.st_dev
                    my_statp.st_nlink = statp.st_nlink
                    my_statp.st_uid = statp.st_uid
                    my_statp.st_gid = statp.st_gid
                    my_statp.st_size = statp.st_size
                    my_statp.st_atime = int(statp.st_atime)
                    my_statp.st_mtime = int(statp.st_mtime)
                    my_statp.st_ctime = int(statp.st_ctime)

                except os.error as os_err:
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        f'Skip "{os_err}"\n',
                    )
                    return bareosfd.bRC_Skip

            # for both virtual and normal file
            savepkt.statp = my_statp
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

    def set_file_attributes(self, restorepkt):
        """
        Need to verify: we set attributes here but on plugin_io close
        the mtime will be modified again.
        approach: save stat-packet here and set it on end_restore_file
        """
        # restorepkt.create_status = bareosfd.CF_CORE
        # return bareosfd.bRC_OK

        # Python attribute setting does not work properly with links
        #    if restorepkt.type in [bareosfd.FT_LNK,bareosfd.FT_LNKSAVED]:
        #        return bareosfd.bRC_OK
        file_name = restorepkt.ofname
        file_attr = restorepkt.statp
        self.stat_packets[file_name] = file_attr

        bareosfd.DebugMessage(
            150,
            f"Set file attributes {file_name} with stat {str(file_attr)}\n",
        )
        try:
            os.chown(file_name, file_attr.st_uid, file_attr.st_gid)
            os.chmod(file_name, file_attr.st_mode)
            os.utime(file_name, (file_attr.st_atime, file_attr.st_mtime))
            new_stat = os.stat(file_name)
            bareosfd.DebugMessage(
                150,
                f"Verified file attributes {file_name} with stat {str(new_stat)}n",
            )
        except os.error as os_err:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                f'Could net set attributes for file {file_name} "{os_err}"',
            )

        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """
        Creates the file to be restored and directory structure, if needed.
        """
        bareosfd.DebugMessage(
            100,
            f"create_file() entry point in Python called with {restorepkt}\n",
        )
        self.fname = restorepkt.ofname
        if not self.fname:
            return bareosfd.bRC_Error

        dirname = os.path.dirname(self.fname.rstrip("/"))
        if not os.path.exists(dirname):
            bareosfd.DebugMessage(
                200, f"Directory {dirname} does not exist, creating it now\n"
            )
            try:
                os.makedirs(dirname)
            except OSError as os_err:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    f'os.makedirs error on {dirname}: "{os_err}"',
                )

        # open creates the file, if not yet existing, we close it again right away
        # it will be opened again in plugin_io.
        if restorepkt.type == bareosfd.FT_REG:
            try:
                with open(self.fname, "wb") as file:
                    file.close()
            except OSError as file_err:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    f'open/close error on {self.fname}: "{file_err}"',
                )
            restorepkt.create_status = bareosfd.CF_EXTRACT
        elif restorepkt.type == bareosfd.FT_LNK:
            link_name = restorepkt.olname
            if not os.path.islink(self.fname.rstrip("/")):
                try:
                    os.symlink(link_name, self.fname.rstrip("/"))
                except OSError as os_err:
                    bareosfd.JobMessage(
                        bareosfd.M_ERROR,
                        f'os.symlink error on {self.fname}: "{os_err}"',
                    )
            restorepkt.create_status = bareosfd.CF_CREATED
        elif restorepkt.type == bareosfd.FT_LNKSAVED:
            link_name = restorepkt.olname
            if not os.path.exists(link_name):
                try:
                    os.link(link_name, self.fname.rstrip("/"))
                except OSError as os_err:
                    bareosfd.JobMessage(
                        bareosfd.M_ERROR,
                        f'os.link error on {self.fname}: "{os_err}"',
                    )

            restorepkt.create_status = bareosfd.CF_CREATED
        elif restorepkt.type == bareosfd.FT_DIREND:
            if not os.path.exists(self.fname):
                try:
                    os.makedirs(self.fname)
                except OSError as os_err:
                    bareosfd.JobMessage(
                        bareosfd.M_ERROR,
                        f'os.makedirs error on {self.fname}: "{os_err}"',
                    )
            restorepkt.create_status = bareosfd.CF_CREATED
        elif restorepkt.type == bareosfd.FT_FIFO:
            if not os.path.exists(self.fname):
                try:
                    os.mkfifo(self.fname, 0o600)
                except OSError as os_err:
                    bareosfd.JobMessage(
                        bareosfd.M_ERROR,
                        f'os.mkfifo error on {self.fname}: "{os_err}"',
                    )
            restorepkt.create_status = bareosfd.CF_CREATED
        else:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                f"Unknown type {restorepkt.type} for file {self.fname}",
            )
        return bareosfd.bRC_OK

    def end_restore_file(self):
        """
        all actions done at the end of file restore.
        """
        bareosfd.DebugMessage(
            100,
            f"end_restore_file() entry point in Python called fname: {self.fname}\n",
        )

        bareosfd.DebugMessage(
            150,
            (
                f"end_restore_file set {self.fname} attributes"
                f" with stat {str(self.stat_packets[self.fname])}\n"
            ),
        )
        try:
            os.chown(
                self.fname,
                self.stat_packets[self.fname].st_uid,
                self.stat_packets[self.fname].st_gid,
            )
            os.chmod(self.fname, self.stat_packets[self.fname].st_mode)
            os.utime(
                self.fname,
                (
                    self.stat_packets[self.fname].st_atime,
                    self.stat_packets[self.fname].st_mtime,
                ),
            )
            # del sometimes leads to no-key errors, like if end_restore_file is called
            # sometimes multiple times. so we don't purge.
            # del self.statp[self.fname]
        except os.error as os_err:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                f'Could net set attributes for file {self.fname}: "{os_err}"',
            )
        return bareosfd.bRC_OK

    def end_backup_file(self):
        """
        Here we return 'bareosfd.bRC_More' as long as our list files_to_backup is not
        empty and bareosfd.bRC_OK when we are done
        """
        bareosfd.DebugMessage(100, "end_backup_file() entry point in Python called\n")
        if self.paths_to_backup:
            return bareosfd.bRC_More

        if self.full_backup_running:
            self.__complete_backup_job()
            # Now we can also create the Restore object with the right timestamp
            # We start by ROP so it is the last object in backup and first in restore
            self.virtual_files = [
                "ROP",
                self.backup_label_filename,
                self.recovery_filename,
            ]
            if self.tablespace_map_filename is not None:
                self.virtual_files.append(self.tablespace_map_filename)
            self.paths_to_backup += self.virtual_files
            return self.__check_for_wal_files()

        self.__close_db_connection()

        return bareosfd.bRC_OK

    def end_backup_job(self):
        """
        Called if backup job ends, before ClientAfterJob
        Make sure that db connection was closed in any way,
        especially when job was canceled
        """
        if self.full_backup_running:
            self.__complete_backup_job()
            self.full_backup_running = False
        else:
            self.__close_db_connection()
        return bareosfd.bRC_OK


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
