#!/usr/bin/env python
#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2026 Bareos GmbH & Co. KG
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
    Bareos Director no longer supports pre-18.2 console protocols.
    """

    def assert_protocol_124_rejected(self, username, password, tls_psk_enable):
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                protocolversion=ProtocolVersions.bareos_12_4,
                tls_psk_enable=tls_psk_enable,
                name=username,
                password=password,
                **self.director_extra_options
            )

    def test_login_notls_notls(self):
        """
        console: notls, director: notls => nologin
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=False)
        password = self.get_operator_password(username)

        self.assert_protocol_124_rejected(username, password, tls_psk_enable=False)

    def test_login_notls_tls(self):
        """
        console: notls, director: tls => nologin
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=True)
        password = self.get_operator_password(username)

        self.assert_protocol_124_rejected(username, password, tls_psk_enable=False)

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_login_tls_notls(self):
        """
        console: tls, director: notls => nologin
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=False)
        password = self.get_operator_password(username)

        self.assert_protocol_124_rejected(username, password, tls_psk_enable=True)

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_login_tls_tls(self):
        """
        console: tls, director: tls => nologin
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=True)
        password = self.get_operator_password(username)

        self.assert_protocol_124_rejected(username, password, tls_psk_enable=True)
