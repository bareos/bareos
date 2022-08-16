#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2022 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

# -*- coding: utf-8 -*-

from __future__ import print_function
import json
import logging
import os
import re
import subprocess
from time import sleep
import unittest
import warnings

import bareos.bsock
from bareos.bsock.constants import Constants
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.bsock.lowlevel import LowLevel
import bareos.exceptions

import bareos_unittest


class PythonBareosJsonRunScriptTest(bareos_unittest.Json):
    def test_backup_runscript_client_defaults(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-client-defaults"
        level = None
        expected_log = ": ClientBeforeJob: jobname={}".format(jobname)

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_log
        )

    def test_backup_runscript_client(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-client"
        level = None
        expected_log = ": ClientBeforeJob: jobname={}".format(jobname)

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: ClientBeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_log
        )

    def test_backup_runscript_server(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-server"
        level = None
        expected_logs = [
            ": BeforeJob: jobname={}".format(jobname),
            ": BeforeJob: daemon=bareos-dir",
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_backup_runscript_console_client(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-console-client"
        level = None
        expected_logs = [
            ': console command: run BeforeJob "whoami"',
            ': console command: run BeforeJob "version"',
            ': console command: run BeforeJob "list jobid=',
            ": ClientBeforeJob: jobname={}".format(jobname),
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: ClientBeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_backup_runscript_console_server(self):
        """
        Run a job which contains a console runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-console-server"
        level = None
        expected_logs = [
            'console command: run BeforeJob "version"',
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_admin_runscript_server(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-server"
        level = None
        expected_logs = [
            ": BeforeJob: jobname={}".format(jobname),
            ": BeforeJob: daemon=bareos-dir",
            ": BeforeJob: jobtype=Admin",
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_admin_runscript_console_server_defaults(self):
        """
        Run a job which contains a console runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-console-server-defaults"
        level = None
        expected_logs = [
            ': console command: run BeforeJob "whoami"',
            ': console command: run BeforeJob "version"',
            ': console command: run BeforeJob "list jobid=',
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #   "time": "2022-08-10 13:14:20",
        #   "logtext": "bareos-dir JobId 3: console command: run BeforeJob \"whoami\"\n"
        # },
        # {
        #   "time": "2022-08-10 13:14:20",
        #   "logtext": "bareos-dir JobId 3: console command: run BeforeJob \"version\"\n"
        # },
        # {
        #   "time": "2022-08-10 13:14:20",
        #   "logtext": "bareos-dir JobId 3: console command: run BeforeJob \"list jobid=3\"\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_admin_runscript_console_server(self):
        """
        Run a job which contains a console runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-console-server"
        level = None
        expected_logs = [
            ': console command: run BeforeJob "whoami"',
            ': console command: run BeforeJob "version"',
            ': console command: run BeforeJob "list jobid=',
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #   "time": "2022-08-10 13:14:20",
        #   "logtext": "bareos-dir JobId 3: console command: run BeforeJob \"whoami\"\n"
        # },
        # {
        #   "time": "2022-08-10 13:14:20",
        #   "logtext": "bareos-dir JobId 3: console command: run BeforeJob \"version\"\n"
        # },
        # {
        #   "time": "2022-08-10 13:14:20",
        #   "logtext": "bareos-dir JobId 3: console command: run BeforeJob \"list jobid=3\"\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_admin_runscript_console_server_invalid(self):
        """
        Run a job which contains an invalid console runscript
        (containing the invalid command "INVALID").
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-console-server-invalid"
        level = None
        expected_logs = [
            ': console command: run BeforeJob "list jobid=',
            ': console command: run BeforeJob "INVALID"',
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #   "time": "2022-08-10 13:21:21",
        #   "logtext": "bareos-dir JobId 6: console command: run BeforeJob \"list jobid=6\"\n"
        # },
        # {
        #   "time": "2022-08-10 13:21:21",
        #   "logtext": "bareos-dir JobId 6: console command: run BeforeJob \"INVALID\"\n"
        # },

        jobId = self.run_job(director_root, jobname, level)
        self.wait_job(director_root, jobId, expected_status=u"Error")
        self.search_joblog(director_root, jobId, expected_logs)

    def test_admin_runscript_client(self):
        """
        RunScripts configured with "RunsOnClient = yes" (default)
        are not executed in Admin Jobs.
        Instead, a warning is written to the joblog.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-client"
        level = None
        expected_logs = [": Invalid runscript definition"]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        # "time": "2019-12-12 13:23:16",
        # "logtext": "bareos-dir JobId 7: Warning: Invalid runscript definition (command=...). Admin Jobs only support local runscripts.\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )
