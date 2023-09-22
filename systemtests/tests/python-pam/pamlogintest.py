#!/usr/bin/env python
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2023 Bareos GmbH & Co. KG
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
import logging
import os
import unittest

import bareos.bsock
from bareos.bsock.protocolversions import ProtocolVersions
import bareos.exceptions

import bareos_unittest


class PamLoginTest(bareos_unittest.Base):
    """
    Requires Bareos Console Protocol >= 18.2.
    """

    @classmethod
    def setUpClass(cls):
        super(PamLoginTest, cls).setUpClass()
        cls.console_pam_username = "PamConsole-notls"
        cls.console_pam_password = "secret"

    def test_pam_login_notls(self):
        pam_username = "user1"
        pam_password = "user1"

        #
        # login as console_pam_username
        #
        bareos_password = bareos.bsock.Password(self.console_pam_password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=self.console_pam_username,
            password=bareos_password,
            pam_username=pam_username,
            pam_password=pam_password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(pam_username, whoami.rstrip())

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_pam_login_tls(self):
        pam_username = "user1"
        pam_password = "user1"

        #
        # login as console_pam_username
        #
        bareos_password = bareos.bsock.Password(self.console_pam_password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=self.console_pam_username,
            password=self.console_pam_password,
            pam_username=pam_username,
            pam_password=pam_password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(pam_username, whoami.rstrip())

    def test_pam_login_with_not_existing_username(self):
        """
        Verify bareos.bsock.DirectorConsole raises an AuthenticationError exception.
        """
        pam_username = "nonexistinguser"
        pam_password = "secret"

        bareos_password = bareos.bsock.Password(self.console_pam_password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                name=self.console_pam_username,
                password=bareos_password,
                pam_username=pam_username,
                pam_password=pam_password,
                **self.director_extra_options
            )

    def test_login_with_not_wrong_password(self):
        """
        Verify bareos.bsock.DirectorConsole raises an AuthenticationError exception.
        """
        pam_username = "user1"
        pam_password = "WRONGPASSWORD"

        bareos_password = bareos.bsock.Password(self.console_pam_password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                name=self.console_pam_username,
                password=bareos_password,
                pam_username=pam_username,
                pam_password=pam_password,
                **self.director_extra_options
            )

    def test_login_with_director_requires_pam_but_credentials_not_given(self):
        """
        When the Director tries to verify the PAM credentials,
        but no credentials where submitted,
        the login must fail.
        """

        bareos_password = bareos.bsock.Password(self.console_pam_password)
        with self.assertRaises(bareos.exceptions.AuthenticationError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                name=self.console_pam_username,
                password=bareos_password,
                **self.director_extra_options
            )

    def test_login_without_director_pam_console_but_with_pam_credentials(self):
        """
        When PAM credentials are given,
        but the Director don't authenticate using them,
        the login should fail.
        """

        pam_username = "user1"
        pam_password = "user1"

        with self.assertRaises(bareos.exceptions.AuthenticationError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                name=self.get_operator_username(),
                password=self.get_operator_password(),
                pam_username=pam_username,
                pam_password=pam_password,
                **self.director_extra_options
            )

    def test_login_with_director_requires_pam_but_protocol_124(self):
        """
        When the Director tries to verify the PAM credentials,
        but the console connects via protocol bareos_12.4,
        the console first retrieves a "1000 OK",
        but further communication fails.
        In the end, a ConnectionLostError exception is raised.
        Sometimes this occurs during initialization,
        sometimes first the call command fails.
        """

        bareos_password = bareos.bsock.Password(self.console_pam_password)
        with self.assertRaises(bareos.exceptions.ConnectionLostError):
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                protocolversion=ProtocolVersions.bareos_12_4,
                name=self.console_pam_username,
                password=bareos_password,
                **self.director_extra_options
            )
            result = director.call("whoami").decode("utf-8")
