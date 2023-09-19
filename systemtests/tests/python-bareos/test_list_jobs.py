#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2023 Bareos GmbH & Co. KG
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


class PythonBareosListJobsTest(bareos_unittest.Base):
    def test_list_jobs(self):
        """
        verifying `list jobs` and `llist jobs ...` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        director.call("run job=backup-bareos-fd yes")
        director.call("wait")

        # Regular list jobs
        result = director.call("list jobs")

        expected_list_keys = [
            "jobid",
            "name",
            "client",
            "starttime",
            "duration",
            "type",
            "level",
            "jobfiles",
            "jobbytes",
            "jobstatus",
        ]
        self.assertEqual(
            list(result["jobs"][0].keys()).sort(),
            expected_list_keys.sort(),
        )
        reg = re.compile("..:..:..")
        self.assertTrue(re.match(reg, result["jobs"][0]["duration"]))

        # Long list jobs
        result = director.call("llist jobs")

        expected_long_list_keys = [
            "jobid",
            "job",
            "name",
            "purgedfiles",
            "type",
            "level",
            "clientid",
            "client",
            "jobstatus",
            "schedtime",
            "starttime",
            "endtime",
            "realendtime",
            "duration",
            "jobtdate",
            "volsessionid",
            "volsessiontime",
            "jobfiles",
            "jobbytes",
            "joberrors",
            "jobmissingfiles",
            "poolid",
            "poolname",
            "priorjobid",
            "filesetid",
            "fileset",
        ]
        self.assertEqual(
            list(result["jobs"][0].keys()).sort(),
            expected_long_list_keys.sort(),
        )

        # Long list with options

        result = director.call("llist jobs current")
        self.assertNotEqual(
            result["jobs"][0],
            "",
        )

        result = director.call("llist jobs enable")
        self.assertNotEqual(
            result["jobs"][0],
            "",
        )

        result = director.call("llist jobs last")
        self.assertNotEqual(
            result["jobs"][0],
            "",
        )

        expected_long_list_last_keys = [
            "jobid",
            "job",
            "name",
            "purgedfiles",
            "type",
            "level",
            "clientid",
            "client",
            "jobstatus",
            "schedtime",
            "starttime",
            "endtime",
            "realendtime",
            "duration",
            "jobtdate",
            "volsessionid",
            "volsessiontime",
            "jobfiles",
            "jobbytes",
            "joberrors",
            "jobmissingfiles",
            "poolid",
            "poolname",
            "priorjobid",
            "filesetid",
            "fileset",
        ]
        self.assertEqual(
            list(result["jobs"][0].keys()).sort(),
            expected_long_list_last_keys.sort(),
        )

        result = director.call("llist jobs last current")
        self.assertNotEqual(
            result["jobs"][0],
            "",
        )

        result = director.call("llist jobs last current enable")
        self.assertNotEqual(
            result["jobs"][0],
            "",
        )

        # Counting jobs
        result = director.call("list jobs count")

        expected_list_count_keys = ["count"]
        self.assertEqual(
            list(result["jobs"][0].keys()),
            expected_list_count_keys,
        )

        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("list jobs count last")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("list jobs count current")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        # Long list counting

        result = director.call("llist jobs count")
        expected_long_list_count_keys = ["count"]
        self.assertEqual(
            list(result["jobs"][0].keys()),
            expected_long_list_count_keys,
        )
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("llist jobs count last")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("llist jobs count current")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call(
            "llist jobs limit=99999999999999999999999999999999999999 offset=99999999999999999999999999999999999999999999999"
        )
        self.assertEqual(len(result["jobs"]), 0)
