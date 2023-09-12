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


class PythonBareosPlainTest(bareos_unittest.Base):
    def test_login_to_noexisting_host(self):
        logger = logging.getLogger()

        # try to connect to invalid host:port. Use port 9 (discard).
        port = 9

        bareos_password = bareos.bsock.Password(self.director_root_password)
        with self.assertRaises(bareos.exceptions.ConnectionError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=port,
                password=bareos_password,
                **self.director_extra_options
            )

    def test_login_as_root(self):
        logger = logging.getLogger()

        bareos_password = bareos.bsock.Password(self.director_root_password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            password=bareos_password,
            **self.director_extra_options
        )
        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual("root", whoami.rstrip())

    def test_login_as_user(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )
        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

    def test_login_with_not_existing_username(self):
        """
        Verify bareos.bsock.DirectorConsole raises an AuthenticationError exception.
        """
        logger = logging.getLogger()

        username = "nonexistinguser"
        password = "secret"

        bareos_password = bareos.bsock.Password(password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                director = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name=username,
                    password=bareos_password,
                    **self.director_extra_options
                )

    def test_login_with_wrong_password(self):
        """
        Verify bareos.bsock.DirectorConsole raises an AuthenticationError exception.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = "WRONGPASSWORD"

        bareos_password = bareos.bsock.Password(password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                director = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name=username,
                    password=bareos_password,
                    **self.director_extra_options
                )

    def test_no_autodisplay_command(self):
        """
        The console has no access to the "autodisplay" command.
        However, the initialization of DirectorConsole calls this command.
        However, the error should not be visible to the console.
        """
        logger = logging.getLogger()

        username = u"noautodisplaycommand"
        password = u"secret"

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password,
            **self.director_extra_options
        )

        # get the list of all command
        result = director.call(".help all")
        # logger.debug(str(result))

        # verify, the result contains command. We test for the list command.
        self.assertIn(u"list", str(result))
        # verify, the result does not contain the autodisplay command.
        self.assertNotIn(u"autodisplay", str(result))

        # check if the result of 'whoami' only contains the expected result.
        result = director.call("whoami").decode("utf-8")
        logger.debug(str(result))

        self.assertEqual(username, result.rstrip())

    def test_json_without_json_backend(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )
        result = director.call(".api json").decode("utf-8")
        result = director.call("whoami").decode("utf-8")
        logger.debug(str(result))
        content = json.loads(str(result))
        logger.debug(str(content))
        self.assertEqual(content["whoami"].rstrip(), username)
