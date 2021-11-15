#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2021 Bareos GmbH & Co. KG
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


class PythonBareosFiledaemonTest(bareos_unittest.Base):
    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_status(self):
        logger = logging.getLogger()

        name = self.director_name
        password = self.filedaemon_director_password

        bareos_password = bareos.bsock.Password(password)
        fd = bareos.bsock.FileDaemon(
            address=self.filedaemon_address,
            port=self.filedaemon_port,
            name=name,
            password=bareos_password,
            **self.director_extra_options
        )

        result = fd.call(u"status")

        expected_regex_str = r"{} \(director\) connected at: (.*)".format(name)
        expected_regex = bytes(bytearray(expected_regex_str, "utf-8"))

        # logger.debug(expected_regex)
        # logger.debug(result)

        # logger.debug(re.search(expected_regex, result).group(1).decode('utf-8'))

        self.assertRegex(result, expected_regex)

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_execute_external_command(self):
        logger = logging.getLogger()

        name = self.director_name
        password = self.filedaemon_director_password

        # let the shell calculate 6 * 7
        # and verify, that the output containts 42.
        command = u"bash -c \"let result='6 * 7'; echo result=$result\""
        expected_regex_str = r"ClientBeforeJob: result=(42)"
        expected_regex = bytes(bytearray(expected_regex_str, "utf-8"))

        bareos_password = bareos.bsock.Password(password)
        fd = bareos.bsock.FileDaemon(
            address=self.filedaemon_address,
            port=self.filedaemon_port,
            name=name,
            password=bareos_password,
            **self.director_extra_options
        )

        fd.call(
            [
                "Run",
                "OnSuccess=1",
                "OnFailure=1",
                "AbortOnError=1",
                "When=2",
                "Command={}".format(command),
            ]
        )
        result = fd.call([u"RunBeforeNow"])

        # logger.debug(expected_regex)
        logger.debug(result)

        logger.debug(
            u"result is {}".format(
                re.search(expected_regex, result).group(1).decode("utf-8")
            )
        )

        self.assertRegex(result, expected_regex)
