#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2024 Bareos GmbH & Co. KG
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


class PythonBareosDeleteTest(bareos_unittest.Json):
    def test_delete_jobid(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd"

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        jobid = self.get_backup_jobid(director, jobname)
        job = self.list_jobid(director, jobid)
        result = director.call("delete jobid={}".format(jobid))
        logger.debug(str(result))
        self.assertIn(jobid, result["deleted"]["jobids"])
        with self.assertRaises(ValueError):
            job = self.list_jobid(director, jobid)

    def test_delete_jobids(self):
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

        # Note:
        # If delete is called on a non existing jobid,
        # this jobid will neithertheless be returned as deleted.
        jobids = ["1001", "1002", "1003"]
        result = director.call("delete jobid={}".format(",".join(jobids)))
        logger.debug(str(result))
        for jobid in jobids:
            self.assertIn(jobid, result["deleted"]["jobids"])
            with self.assertRaises(ValueError):
                job = self.list_jobid(director, jobid)

    def test_delete_jobid_paramter(self):
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

        # Note:
        # If delete is called on a non existing jobid,
        # this jobid will neithertheless be returned as deleted.
        jobids = "jobid=1001,1002,1101-1110,1201 jobid=2001,2002,2101-2110,2201"
        #                 1 +  1 +      10  + 1       +  1 +  1 +      10 +  1
        number = 26
        result = director.call("delete {}".format(jobids))
        logger.debug(str(result))
        deleted_jobids = result["deleted"]["jobids"]
        self.assertEqual(
            len(deleted_jobids),
            number,
            "Failed: expecting {} jobids, found {} ({})".format(
                number, len(deleted_jobids), deleted_jobids
            ),
        )

    def test_delete_invalid_jobids(self):
        """"""
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

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = director.call("delete jobid={}".format("1000_INVALID"))

    def test_delete_volume(self):
        """"""
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd"

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        jobid = self.get_backup_jobid(director, jobname, "Full")
        job = self.list_jobid(director, jobid)
        result = director.call("list volume jobid={}".format(jobid))
        volume = result["volumes"][0]["volumename"]
        result = director.call("delete volume={} yes".format(volume))
        logger.debug(str(result))
        self.assertIn(jobid, result["deleted"]["jobids"])
        self.assertIn(volume, result["deleted"]["volumes"])
        with self.assertRaises(ValueError):
            job = self.list_jobid(director, jobid)
