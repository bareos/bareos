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


class PythonBareosShowTest(bareos_unittest.Json):
    def test_fileset(self):
        """
        Filesets are stored in the database,
        as soon as a job using them did run.
        We want to verify, that the catalog content is correct.
        As comparing the full content is difficult,
        we only check if the "Description" is stored in the catalog.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = u"backup-bareos-fd"

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        jobid = self.run_job(director, jobname, wait=True)
        result = director.call("llist jobid={}".format(jobid))
        fileset = result["jobs"][0]["fileset"]
        result = director.call("list fileset jobid={} limit=1".format(jobid))
        fileset_content_list = result["filesets"][0]["filesettext"]

        result = director.call("show fileset={}".format(fileset))
        fileset_show_description = result["filesets"][fileset]["description"]

        self.assertIn(fileset_show_description, fileset_content_list)

    def test_show_resources(self):
        """
        show resources in bconsole
        """

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )
        list_of_resources = {
            "directors",
            "clients",
            "jobs",
            "jobdefs",
            "storages",
            "schedules",
            "catalogs",
            "filesets",
            "pools",
            "messages",
            "profiles",
            "consoles",
            "counters",
            "users",
        }
        for resource in list_of_resources:
            call = director.call("show {}".format(resource))
            result = call[resource]
            self.assertTrue(len(result) > 0)
