#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2024 Bareos GmbH & Co. KG
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


class PythonBareosListCommandTest(bareos_unittest.Json):
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
            **self.director_extra_options,
        )

        director.call("run job=backup-bareos-fd yes")
        director.call("wait")
        director.call("restore client=bareos-fd fileset=SelfTest select all done yes")
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
        resultkeys = list(result["jobs"][0].keys())
        resultkeys.sort()
        expected_list_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_list_keys,
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
        resultkeys = list(result["jobs"][0].keys())

        resultkeys.sort()
        expected_long_list_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_long_list_keys,
        )

        # Long list with options

        result = director.call("llist jobs current")
        self.assertTrue(result["jobs"])

        result = director.call("llist jobs enable")
        self.assertTrue(result["jobs"])

        result = director.call("llist jobs last")
        self.assertTrue(result["jobs"])

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
        resultkeys = list(result["jobs"][0].keys())

        resultkeys.sort()
        expected_long_list_last_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_long_list_last_keys,
        )

        result = director.call("llist jobs last current")
        self.assertTrue(result["jobs"])

        result = director.call("llist jobs last current enable")
        self.assertTrue(result["jobs"])

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

        # list jobs jobstatus=X
        result = director.call("list jobs jobstatus=T")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertTrue(job["jobstatus"], "T")

        # running a job a canceling
        director.call("run job=backup-bareos-fd yes")
        director.call("cancel job=backup-bareos-fd all yes")
        director.call("wait")

        # list jobs jobstatus=X,Y,z
        result = director.call("list jobs jobstatus=T,A")
        self.assertTrue(result["jobs"])

        terminated_jobs = 0
        canceled_jobs = 0
        for job in result["jobs"]:
            if job["jobstatus"] == "T":
                terminated_jobs += 1
            if job["jobstatus"] == "A":
                canceled_jobs += 1

        self.assertTrue(terminated_jobs >= 1)
        self.assertTrue(canceled_jobs >= 1)

        result = director.call("list jobs jobstatus=R")
        self.assertFalse(result["jobs"])

        # list jobs jobtype=X
        result = director.call("list jobs jobtype=B")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["type"], "B")

        result = director.call("list jobs jobtype=Restore")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["type"], "R")

        result = director.call("list jobs jobtype=B,R")
        self.assertTrue(result["jobs"])

        backup_jobs = 0
        restore_jobs = 0
        for job in result["jobs"]:
            if job["type"] == "B":
                backup_jobs += 1
            if job["type"] == "R":
                restore_jobs += 1

        self.assertTrue(backup_jobs >= 1)
        self.assertTrue(restore_jobs >= 1)

        # list jobs joblevel/level=X
        result = director.call("list jobs joblevel=F")
        result2 = director.call("list jobs level=F")
        self.assertTrue(result == result2)
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["level"], "F")

        result = director.call("list jobs level=I")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["level"], "I")

        result = director.call("list jobs joblevel=F,I")
        self.assertTrue(result["jobs"])
        full_jobs = 0
        incremental_jobs = 0
        for job in result["jobs"]:
            if job["level"] == "F":
                full_jobs += 1
            if job["level"] == "I":
                incremental_jobs += 1

        self.assertTrue(full_jobs >= 1)
        self.assertTrue(incremental_jobs >= 1)

    def test_list_media(self):
        """
        verifying `list media` and `llist media ...` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        director.call("run job=backup-bareos-fd yes")
        director.call("wait")

        # check for expected keys
        result = director.call("list media")
        expected_list_media_keys = [
            "mediaid",
            "volumename",
            "volstatus",
            "enabled",
            "volbytes",
            "volfiles",
            "volretention",
            "recycle",
            "slot",
            "inchanger",
            "mediatype",
            "lastwritten",
            "storage",
        ]
        resultkeys = list(result["volumes"]["full"][0].keys())

        resultkeys.sort()
        expected_list_media_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_list_media_keys,
        )

        # check expected behavior when asking for specific volume by name
        test_volume = "test_volume0001"
        director.call("label volume={} pool=Full".format(test_volume))
        director.call("wait")
        result = director.call("list media=test_volume0001")
        self.assertEqual(
            result["volume"]["volumename"],
            test_volume,
        )
        result = director.call("list volume={}".format(test_volume))
        self.assertEqual(
            result["volume"]["volumename"],
            test_volume,
        )

        # check expected behavior when asking for specific volume by mediaid
        result = director.call("list mediaid=2")
        self.assertEqual(
            result["volume"]["mediaid"],
            "2",
        )
        result = director.call("list volumeid=2")
        self.assertEqual(
            result["volume"]["mediaid"],
            "2",
        )

        result = director.call("llist mediaid=2")
        self.assertEqual(
            result["volume"]["mediaid"],
            "2",
        )
        result = director.call("llist volumeid=2")
        self.assertEqual(
            result["volume"]["mediaid"],
            "2",
        )
        director.call("delete volume=test_volume0001 yes")
        os.remove("storage/{}".format(test_volume))

    def test_list_pool(self):
        """
        verifying `list pool` and `llist pool ...` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        director.call("run job=backup-bareos-fd yes")
        director.call("wait")

        result = director.call("list pool")
        expected_list_pool_keys = [
            "poolid",
            "name",
            "numvols",
            "maxvols",
            "pooltype",
            "labelformat",
        ]

        resultkeys = list(result["pools"][0].keys())

        resultkeys.sort()
        expected_list_pool_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_list_pool_keys,
        )

        # check expected behavior when asking for specific volume by name
        result = director.call("list pool=Incremental")
        self.assertEqual(
            result["pools"][0]["name"],
            "Incremental",
        )

        result = director.call("llist pool")
        expected_long_list_pool_keys = [
            "poolid",
            "name",
            "numvols",
            "maxvols",
            "useonce",
            "usecatalog",
            "acceptanyvolume",
            "volretention",
            "voluseduration",
            "maxvoljobs",
            "maxvolbytes",
            "autoprune",
            "recycle",
            "pooltype",
            "labelformat",
            "enabled",
            "scratchpoolid",
            "recyclepoolid",
            "labeltype",
        ]
        resultkeys = list(result["pools"][0].keys())

        resultkeys.sort()
        expected_long_list_pool_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_long_list_pool_keys,
        )

        # check expected behavior when asking for specific volume by name
        result = director.call("llist pool=Incremental")
        self.assertEqual(
            result["pools"][0]["name"],
            "Incremental",
        )

        result = director.call("list poolid=3")
        self.assertEqual(
            result["pools"][0]["name"],
            "Full",
        )

        self.assertEqual(
            result["pools"][0]["poolid"],
            "3",
        )

        result = director.call("llist poolid=3")
        self.assertEqual(
            result["pools"][0]["name"],
            "Full",
        )

        self.assertEqual(
            result["pools"][0]["poolid"],
            "3",
        )

    def test_list_files(self):
        """
        verifying `list files` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director_json = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.run_job(
            director_json,
            jobname="backup-bareos-fd",
            level="Full",
            extra="fileset=NumberedFiles",
            wait=True,
        )
        result = director_json.call(f"list files jobid={jobid}")
        # Before fixing https://bugs.bareos.org/view.php?id=1007 the keys could be scrambled after 100 entries.
        filenames = result["filenames"]
        min_files = 100
        self.assertGreater(
            len(filenames),
            min_files,
            f"More than {min_files} files are required for this test, only {len(filenames)} given.",
        )

        bad_headers = [f for f in filenames if "filename" not in f]
        self.assertEqual(
            bad_headers,
            [],
            f"Some files were not listed with the correct 'filename' header.",
        )

        filenames_json = set(f["filename"] for f in filenames)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )
        result = (
            director.call(f"list files jobid={jobid}")
            .decode("utf-8")
            .replace("\n ", "\n")
        )
        # start at the first path,
        # skipping lines like 'Automatically selected Catalog: MyCatalog\nUsing Catalog "MyCatalog"\n'
        start = result.find("\n/") + 1
        filenames_plain = set(result[start:].splitlines())
        self.assertEqual(
            filenames_json,
            filenames_plain,
            "JSON result is not identical to plain (api 0) result.",
        )
