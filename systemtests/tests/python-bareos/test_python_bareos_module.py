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


class PythonBareosModuleTest(bareos_unittest.Base):
    def versiontuple(self, versionstring):
        version, separator, suffix = versionstring.partition("~")
        return tuple(map(int, (version.split("."))))

    def test_exception_connection_error(self):
        """
        Test if the ConnectionError exception deliveres the expected result.
        """
        logger = logging.getLogger()

        message = "my test message"
        try:
            raise bareos.exceptions.ConnectionError("my test message")
        except bareos.exceptions.ConnectionError as e:
            logger.debug(
                "signal: str={}, repr={}, args={}".format(str(e), repr(e), e.args)
            )
            self.assertEqual(message, str(e))

    def test_exception_signal_received(self):
        """
        Test if the SignalReceivedException deliveres the expected result.
        """
        logger = logging.getLogger()

        try:
            raise bareos.exceptions.SignalReceivedException(Constants.BNET_TERMINATE)
        except bareos.exceptions.SignalReceivedException as e:
            logger.debug(
                "signal: str={}, repr={}, args={}".format(str(e), repr(e), e.args)
            )
            self.assertIn("terminate", str(e))

    def test_protocol_message(self):
        logger = logging.getLogger()

        name = u"Testname"
        expected_regex_str = r"Hello {} calling version (.*)".format(name)
        expected_regex = bytes(bytearray(expected_regex_str, "utf-8"))

        hello_message = ProtocolMessages().hello(name)
        logger.debug(hello_message)

        self.assertRegex(hello_message, expected_regex)

        version = re.search(expected_regex, hello_message).group(1).decode("utf-8")
        logger.debug(u"version: {} ({})".format(version, type(version)))

        self.assertGreaterEqual(
            self.versiontuple(version), self.versiontuple(u"18.2.5")
        )

    def test_password(self):
        password = bareos.bsock.Password("secret")
        md5 = password.md5()
        self.assertTrue(isinstance(md5, bytes))
        self.assertEqual(md5, b"5ebe2294ecd0e0f08eab7690d2a6ee69")
