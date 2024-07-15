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


class PythonBareosJsonConfigTest(bareos_unittest.Json):
    def test_show_command(self):
        """
        Verify, that the "show" command delivers valid JSON.
        If the JSON is not valid, the "call" command would raise an exception.
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

        resourcesname = "clients"
        newclient = "test-client-fd"
        newpassword = "secret"

        director.call("show all")

        try:
            os.remove("etc/bareos/bareos-dir.d/client/{}.conf".format(newclient))
            director.call("reload")
        except OSError:
            pass

        self.assertFalse(
            self.check_resource(director, resourcesname, newclient),
            "Resource {} in {} already exists.".format(newclient, resourcesname),
        )

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            director.call("show client={}".format(newclient))

        self.configure_add(
            director,
            resourcesname,
            newclient,
            "client={} password={} address=127.0.0.1".format(newclient, newpassword),
        )

        director.call("show all")
        director.call("show all verbose")
        result = director.call("show client={}".format(newclient))
        logger.debug(str(result))
        director.call("show client={} verbose".format(newclient))

    def test_configure_add_with_quotes(self):
        """
        verifying that configure add handles quoted strings correctly
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

        resourcesname = "jobs"
        testname = "testjob"
        stringwithwhitespace = "strings with whitespace"
        testscript = '/bin/bash -c \\"echo Hallo\\"'
        priority = 11

        try:
            os.remove("etc/bareos/bareos-dir.d/job/{}.conf".format(testname))
            director.call("reload")
        except OSError:
            pass

        self.assertFalse(
            self.check_resource(director, resourcesname, testname),
            "Resource {} in {} already exists.".format(testname, resourcesname),
        )

        self.configure_add(
            director,
            resourcesname,
            testname,
            'job jobdefs=DefaultJob name={} description="{}" runafterjob="{}" priority={}'.format(
                testname, stringwithwhitespace, testscript, priority
            ),
        )

        result = director.call("show jobs={}".format(testname))

        self.assertEqual(
            result["jobs"][testname]["description"],
            stringwithwhitespace,
        )

        self.assertEqual(
            result["jobs"][testname]["runscript"][0]["runafterjob"],
            testscript,
        )

        self.assertEqual(
            result["jobs"][testname]["priority"],
            priority,
        )

    def _test_configure_add_conole_with_where_acl(self, name, where_acl):
        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        resourcetype = "console"

        try:
            os.remove("etc/bareos/bareos-dir.d/{}/{}.conf".format(resourcetype, name))
            director.call("reload")
        except OSError:
            pass

        self.assertFalse(
            self.check_resource(director, resourcetype, name),
            "Resource {} in {} already exists.".format(name, resourcetype),
        )

        self.configure_add(
            director,
            resourcetype,
            name,
            '{} name={} password=secret profile=operator WhereACL="{}"'.format(
                resourcetype, name, where_acl
            ),
        )

        self.assertTrue(self.check_resource(director, resourcetype, name))

    def test_configure_add_valid_where_acl(self):
        self._test_configure_add_conole_with_where_acl(
            "valid-WhereAcl", "/tmp/validpath"
        )

    def test_configure_add_invalid_where_acl(self):
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            self._test_configure_add_conole_with_where_acl("invalid-WhereAcl", "|!(){}")
