#!/usr/bin/env python
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


class PythonBareosProtocol124Test(bareos_unittest.Base):
    """
    There are 4 cases to check:
    # console: notls, director: notls => login (notls)
    # console: notls, director: tls   => login (notls!)
    # console: tls, director: notls   => login (tls!)
    # console: tls, director: tls     => login (tls)
    """

    def test_login_notls_notls(self):
        """
        console: notls, director: notls => login
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=False)
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            protocolversion=ProtocolVersions.bareos_12_4,
            tls_psk_enable=False,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        protocolversion = director.get_protocol_version()
        self.assertEqual(protocolversion, ProtocolVersions.bareos_12_4)

        # As there is no encryption,
        # the socket should not contain a cipher() method.
        self.assertFalse(hasattr(director.socket, "cipher"))

    def test_login_notls_tls(self):
        """
        console: notls, director: tls => login

        Login will succeed.
        This happens only, because the old ProtocolVersions.bareos_12_4 is used.
        When using the current protocol, this login will fail.
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=True)
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            protocolversion=ProtocolVersions.bareos_12_4,
            tls_psk_enable=False,
            name=username,
            password=password,
            **self.director_extra_options
        )
        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        protocolversion = director.get_protocol_version()
        self.assertEqual(protocolversion, ProtocolVersions.bareos_12_4)

        # As there is no encryption,
        # the socket should not contain a cipher() method.
        self.assertFalse(hasattr(director.socket, "cipher"))

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_login_tls_notls(self):
        """
        console: tls, director: notls => login (TLS!)

        The behavior of the Bareos Director is to use TLS-PSK
        even if "TLS Enable = no" is set in the Console configuration of the Director.
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=False)
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            protocolversion=ProtocolVersions.bareos_12_4,
            tls_psk_enable=True,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        protocolversion = director.get_protocol_version()
        self.assertEqual(protocolversion, ProtocolVersions.bareos_12_4)

        self.assertTrue(hasattr(director.socket, "cipher"))
        cipher = director.socket.cipher()
        logger.debug(str(cipher))

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_login_tls_tls(self):
        """
        console: tls, director: tls     => login
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=True)
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            protocolversion=ProtocolVersions.bareos_12_4,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        protocolversion = director.get_protocol_version()
        self.assertEqual(protocolversion, ProtocolVersions.bareos_12_4)

        self.assertTrue(hasattr(director.socket, "cipher"))
        cipher = director.socket.cipher()
        logger.debug(str(cipher))
