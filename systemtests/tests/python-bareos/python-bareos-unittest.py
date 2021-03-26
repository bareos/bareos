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


class PythonBareosBase(unittest.TestCase):

    config_directory = "/etc/bareos"

    director_name = u"bareos-dir"

    director_address = "localhost"
    director_port = 9101
    director_root_password = "secret"
    director_extra_options = {}
    client = "bareos-fd"

    filedaemon_address = "localhost"
    filedaemon_port = 9102
    filedaemon_director_password = u"secret"

    dbcheck_binary = "dbcheck"

    # restorefile = '/usr/sbin/bconsole'
    # path to store logging files
    backup_directory = "tmp/data/"
    debug = False
    logpath = "{}/PythonBareosTest.log".format(os.getcwd())

    def setUp(self):
        # Configure the logger, for information about the timings set it to INFO
        logging.basicConfig(
            filename=self.logpath,
            format="%(levelname)s %(module)s.%(funcName)s: %(message)s",
            level=logging.INFO,
        )
        logger = logging.getLogger()
        if self.debug:
            logger.setLevel(logging.DEBUG)

        # assertRegexpMatches has been renamed
        # to assertRegex in Python 3.2
        # and is deprecated now.
        # This prevents a deprecation warning.
        if hasattr(self, "assertRegexpMatches") and not hasattr(self, "assertRegex"):
            self.assertRegex = self.assertRegexpMatches

        logger.debug("setUp")

    def tearDown(self):
        logger = logging.getLogger()
        logger.debug("tearDown\n\n\n")

    def get_name_of_test(self):
        return self.id().split(".", 1)[1]

    def get_operator_username(self, tls=None):
        if tls is None:
            if bareos.bsock.DirectorConsole.is_tls_psk_available():
                tls = True
            else:
                tls = False
        if tls:
            return u"admin-tls"
        else:
            return u"admin-notls"

    def get_operator_password(self, username=None):
        return bareos.bsock.Password(u"secret")

    @staticmethod
    def append_to_file(filename, data):
        with open(filename, "a") as writer:
            writer.write(data)
            writer.flush()

    def run_dbcheck(self):
        logger = logging.getLogger()
        cmd = [self.dbcheck_binary, "-c", self.config_directory, "-vvv", "-b", "-f"]
        logger.debug("calling: {}".format(" ".join(cmd)))
        output = subprocess.check_output(cmd, stderr=subprocess.PIPE)
        logger.debug("result: {}".format(output))


class PythonBareosModuleTest(PythonBareosBase):
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


class PythonBareosPlainTest(PythonBareosBase):
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
        self.assertEqual(content["result"]["whoami"].rstrip(), username)


class PythonBareosProtocol124Test(PythonBareosBase):
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


class PythonBareosTlsPskTest(PythonBareosBase):
    """
    This test used the default protocol
    (opposite to the PythonBareosProtocol124Test class,
    which uses ProtocolVersions.bareos_12_4).
    However, on authentication failures,
    DirectorConsole falls back to ProtocolVersions.bareos_12_4.

    There are 4 cases to check:
    # console: notls, director: notls => login  (notls)
    # console: notls, director: tls   => login! (notls)
    # console: tls, director: notls   => login  (tls!)
    # console: tls, director: tls     => login  (tls)
    """

    def test_tls_psk_identity(self):
        """
        Check if tls_psk_identity is in the expected format
        and is of type "bytes".
        """
        core = LowLevel()
        core.identity_prefix = "R_TEST"
        core.name = "Test"
        identity = core.get_tls_psk_identity()
        self.assertTrue(isinstance(identity, bytes))
        self.assertEqual(identity, b"R_TEST\x1eTest")

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
            tls_psk_enable=False,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        # As there is no encryption,
        # the socket should not contain a cipher() method.
        self.assertFalse(hasattr(director.socket, "cipher"))

    def test_login_notls_tls(self):
        """
        console: notls, director: tls => login

        This works, because DirectorConsole falls back to the old protocol.
        Set tls_psk_require=True, if this should not happen.
        """

        logger = logging.getLogger()

        username = u"admin-tls"
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            tls_psk_enable=False,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        # As there is no encryption,
        # the socket should not contain a cipher() method.
        self.assertFalse(hasattr(director.socket, "cipher"))

    def test_login_notls_tls_fixedprotocolversion(self):
        """
        console: notls, director: tls => nologin

        As the protocolversion is set to a fixed value,
        there will be no fallback to the old protocol.
        """

        logger = logging.getLogger()

        username = u"admin-tls"
        password = self.get_operator_password(username)

        with self.assertRaises(bareos.exceptions.AuthenticationError):
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                director = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    tls_psk_enable=False,
                    protocolversion=ProtocolVersions.last,
                    name=username,
                    password=password,
                    **self.director_extra_options
                )

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
            tls_psk_enable=True,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

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
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        self.assertTrue(hasattr(director.socket, "cipher"))
        cipher = director.socket.cipher()
        logger.debug(str(cipher))

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_login_tls_tls_require(self):
        """
        console: tls, director: tls     => login
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=True)
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            tls_psk_require=True,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        self.assertTrue(hasattr(director.socket, "cipher"))
        cipher = director.socket.cipher()
        logger.debug(str(cipher))

    @unittest.skipUnless(
        bareos.bsock.DirectorConsole.is_tls_psk_available(), "TLS-PSK is not available."
    )
    def test_login_tls_tls_fixedprotocolversion(self):
        """
        console: tls, director: tls     => login
        """

        logger = logging.getLogger()

        username = self.get_operator_username(tls=True)
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            protocolversion=ProtocolVersions.last,
            tls_psk_require=True,
            name=username,
            password=password,
            **self.director_extra_options
        )

        whoami = director.call("whoami").decode("utf-8")
        self.assertEqual(username, whoami.rstrip())

        self.assertTrue(hasattr(director.socket, "cipher"))
        cipher = director.socket.cipher()
        logger.debug(str(cipher))


#
# Test with JSON backend
#


class PythonBareosJsonBase(PythonBareosBase):

    #
    # Util
    #

    @staticmethod
    def check_resource(director, resourcesname, name):
        logger = logging.getLogger()
        rc = False
        try:
            result = director.call(u".{}".format(resourcesname))
            for i in result[resourcesname]:
                if i["name"] == name:
                    rc = True
        except Exception as e:
            logger.warn(str(e))
        return rc

    @staticmethod
    def check_console(director, name):
        return PythonBareosJsonBase.check_resource(director, "consoles", name)

    @staticmethod
    def check_jobname(director, name):
        return PythonBareosJsonBase.check_resource(director, "jobs", name)

    def configure_add(self, director, resourcesname, resourcename, cmd):
        if not self.check_resource(director, resourcesname, resourcename):
            result = director.call("configure add {}".format(cmd))
            self.assertEqual(result["configure"]["add"]["name"], resourcename)
            self.assertTrue(
                self.check_resource(director, resourcesname, resourcename),
                u"Failed to find resource {} in {}.".format(
                    resourcename, resourcesname
                ),
            )

    def wait_job(self, director, jobId):
        result = director.call("wait jobid={}".format(jobId))
        # "result": {
        #    "job": {
        #    "jobid": 1,
        #    "jobstatuslong": "OK",
        #    "jobstatus": "T",
        #    "exitstatus": 0
        #    }
        # }
        self.assertEqual(result["job"]["jobstatuslong"], u"OK")

    def run_job(self, director, jobname, level=None, wait=False):
        logger = logging.getLogger()
        run_parameter = ["job={}".format(jobname), "yes"]
        if level:
            run_parameter.append(u"level={}".format(level))
        result = director.call("run {}".format(u" ".join(run_parameter)))
        jobId = result["run"]["jobid"]
        if wait:
            self.wait_job(director, jobId)

        return jobId

    def _test_job_result(self, jobs, jobid):
        logger = logging.getLogger()
        for job in jobs:
            if job["jobid"] == jobid:
                files = int(job["jobfiles"])
                logger.debug(u"Job {} contains {} files.".format(jobid, files))
                self.assertTrue(files >= 1, u"Job {} contains no files.".format(jobid))
                return True
        self.fail("Failed to find job {}".format(jobid))
        # add return to prevent pylint warning
        return False

    def _test_no_volume_in_pool(self, console, password, pool):
        logger = logging.getLogger()
        bareos_password = bareos.bsock.Password(password)
        console_poolbotfull = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=console,
            password=bareos_password,
            **self.director_extra_options
        )

        result = console_poolbotfull.call("llist media all")
        logger.debug(str(result))

        self.assertGreaterEqual(len(result["volumes"]), 1)

        for volume in result["volumes"]:
            self.assertNotEqual(volume["pool"], pool)

        return True

    def _test_list_with_valid_jobid(self, director, jobid):
        for cmd in ["list", "llist"]:
            result = director.call("{} jobs".format(cmd))
            self._test_job_result(result["jobs"], jobid)

            listcmd = "{} jobid={}".format(cmd, jobid)
            result = director.call(listcmd)
            self._test_job_result(result["jobs"], jobid)

    def _test_list_with_invalid_jobid(self, director, jobid):
        for cmd in ["list", "llist"]:
            result = director.call("{} jobs".format(cmd))
            with self.assertRaises(AssertionError):
                self._test_job_result(result["jobs"], jobid)

            listcmd = "{} jobid={}".format(cmd, jobid)
            result = director.call(listcmd)
            self.assertEqual(
                len(result),
                0,
                u"Command {} should not return results. Current result: {} visible".format(
                    listcmd, str(result)
                ),
            )

    def search_joblog(self, director, jobId, patterns):

        if isinstance(patterns, list):
            pattern_dict = {i: False for i in patterns}
        else:
            pattern_dict = {patterns: False}

        result = director.call("list joblog jobid={}".format(jobId))
        joblog_list = result["joblog"]

        found = False
        for pattern in pattern_dict:
            for logentry in joblog_list:
                if re.search(pattern, logentry["logtext"]):
                    pattern_dict[pattern] = True
            self.assertTrue(
                pattern_dict[pattern],
                'Failed to find pattern "{}" in Job Log of Job {}.'.format(
                    pattern, jobId
                ),
            )

    def run_job_and_search_joblog(self, director, jobname, level, patterns):

        jobId = self.run_job(director, jobname, level, wait=True)
        self.search_joblog(director, jobId, patterns)
        return jobId

    def list_jobid(self, director, jobid):
        result = director.call("llist jobid={}".format(jobid))
        try:
            return result["jobs"][0]
        except KeyError:
            raise ValueError(u"jobid {} does not exist".format(jobid))


class PythonBareosJsonBackendTest(PythonBareosJsonBase):
    def test_json_backend(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)
        client = self.client

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )
        result = director.call("list clients")
        logger.debug(str(result))
        # test if self.client is in the result of "list clients"
        for i in result["clients"]:
            logger.debug(str(i))
            if i["name"] == client:
                return
        self.fail('Failed to retrieve client {} from "list clients"'.format(client))

    def test_json_with_invalid_command(self):
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
            result = director.call("invalidcommand")

    def test_json_whoami(self):
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
        result = director.call("whoami")
        logger.debug(str(result))
        self.assertEqual(username, result["whoami"])

    @unittest.skip("Most commands do return valid JSON")
    def test_json_backend_with_invalid_json_output(self):
        logger = logging.getLogger()

        # This command sends additional plain (none JSON) output.
        # Therefore the result is not valid JSON.
        # Used "show clients" earlier,
        # however, this now produces valid output.
        # Commands like 'status storage' (and 'status client') only produces empty output.
        # The "messages" command shows plain outout in JSON mode,
        # but only if there are pending messages.
        bcmd = "show clients"

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director_plain = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        result = director_plain.call(bcmd)
        logger.debug(str(result))

        director_json = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # The JsonRpcInvalidJsonReceivedException
        # is inherited from JsonRpcErrorReceivedException,
        # so both exceptions could by tried.
        with self.assertRaises(bareos.exceptions.JsonRpcInvalidJsonReceivedException):
            result = director_json.call(bcmd)

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = director_json.call(bcmd)

    def test_json_no_api_command(self):
        """
        The bareos.bsock.DirectorConsoleJson calls .api on initialization.
        This test verifies, that a exception is raised,
        when it is not available.
        """
        logger = logging.getLogger()

        username = "noapicommand"
        password = "secret"

        bareos_password = bareos.bsock.Password(password)
        with self.assertRaises(bareos.exceptions.JsonRpcInvalidJsonReceivedException):
            # We expect, that an exception is raised,
            # as con class initialization,
            # the ".api json" command is called
            # and the "noapicommand" don't have access to this command.
            director = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=bareos_password,
                **self.director_extra_options
            )


class PythonBareosAclTest(PythonBareosJsonBase):
    def test_restore_with_client_acl(self):
        """
        Check if the restore command honors the client ACL.
        Login as console with access only to client = bareos-fd.
        Verify, that a restore can only be performed from this client.

        It checks the intercative restore command,
        therefore it can not use the Json console.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        console_bareos_fd_username = u"client-bareos-fd"
        console_bareos_fd_password = u"secret"

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        result = director_root.call("run job=backup-bareos-fd level=Full yes")
        logger.debug(str(result))

        jobIdBareosFdFull = result["run"]["jobid"]

        result = director_root.call("wait jobid={}".format(jobIdBareosFdFull))

        result = director_root.call("run job=backup-test2-fd level=Full yes")
        logger.debug(str(result))

        jobIdTestFdFull = result["run"]["jobid"]

        result = director_root.call("wait jobid={}".format(jobIdTestFdFull))

        #
        # login as console with ACLs
        #
        bareos_password = bareos.bsock.Password(console_bareos_fd_password)
        console_bareos_fd = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=console_bareos_fd_username,
            password=bareos_password,
            **self.director_extra_options
        )

        result = console_bareos_fd.call("restore")
        logger.debug(str(result))

        #
        # restore: 1: List last 20 Jobs run
        #
        # This requires access to the "sqlquery" command,
        # which this console does not have.
        #
        result = console_bareos_fd.call("1")
        logger.debug(str(result))
        self.assertEqual(b"SQL query not authorized.", result.strip())

        result = console_bareos_fd.call("restore")

        #
        # restore: 2: List Jobs where a given File is saved
        #
        # Only the bareos-fd client should be accessable
        # and is therefore autoselected.
        #
        result = console_bareos_fd.call("2")
        logger.debug(str(result))
        self.assertIn(b"Automatically selected Client: bareos-fd", result)
        result = console_bareos_fd.call("Makefile")
        logger.debug(str(result))
        self.assertIn(b"/Makefile ", result)
        # result = console_bareos_fd.call('.')

        #
        # restore: 3: Enter list of comma separated JobIds to select
        #
        # We select a job that did run on a client
        # we don't have access to.
        # Therefore we expect that we are not allowed to access this jobid.
        # However, we got access.
        # TODO: This has to be fixed in the Bareos Director.
        #
        result = console_bareos_fd.call("3")
        logger.debug(str(result))
        result = console_bareos_fd.call(jobIdTestFdFull)
        logger.debug(str(result))
        # TODO: This is a bug.
        #       ACL checking does not work here,
        #       because this jobid should be accessable in this console.
        # self.assertIn(b'No Job found for JobId', result)
        result = console_bareos_fd.call("find *")
        logger.debug(str(result))
        self.assertIn(b"/Makefile", result)
        result = console_bareos_fd.call("done")

        result = console_bareos_fd.call("restore")

        #
        # 4: Enter SQL list command
        #
        # This requires access to the "sqlquery" command,
        # which this console does not have.
        #
        result = console_bareos_fd.call("4")
        logger.debug(str(result))
        self.assertEqual(b"SQL query not authorized.", result.strip())

        result = console_bareos_fd.call("restore")

        #
        # 5: Select the most recent backup for a client
        #
        # Only the bareos-fd client should be accessable
        # and is therefore autoselected.
        #
        result = console_bareos_fd.call("5")
        logger.debug(str(result))
        self.assertIn(b"Automatically selected Client: bareos-fd", result)
        result = console_bareos_fd.call("done")
        logger.debug(str(result))
        # result = console_bareos_fd.call('.')

        # The remaining options are not tested,
        # as we assume that we have covered the relevant cases.


class PythonBareosJsonAclTest(PythonBareosJsonBase):
    def test_json_list_media_with_pool_acl(self):
        """
        This tests checks if the Pool ACL works with the "llist media all" command.

        login as admin
          run a Full job. It gets stored in pool Full
          modify the backup directory
          run a Incremental job. It gets stored in pool Incremental
          verifies that at least 1 volume exists in the Full pool.
          verifies that at least 1 volume exists in the Incremental pool.
        login as console 'poolfull'
          verifies that "llist media all" only shows volumes from the Full pool.
        login as console 'poolnotfull'
          verifies that "llist media all" only shows volumes not from the Full pool.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        console_password = u"secret"

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # result = director_root.call('run job=backup-bareos-fd level=Full yes')
        # logger.debug(str(result))

        # jobIdFull = result['run']['jobid']

        # wait for job to finish, otherwise next incremental is upgraded to Full.
        # result = director_root.call('wait jobid={}'.format(jobIdFull))

        jobIdFull = self.run_job(director_root, "backup-bareos-fd", "Full", wait=True)

        # make sure, timestamp differs
        sleep(2)

        self.append_to_file(u"{}/extrafile.txt".format(self.backup_directory), "Test\n")

        sleep(2)

        # result = director_root.call('run job=backup-bareos-fd level=Incremental yes')
        # logger.debug(str(result))

        # jobIdIncr = result['run']['jobid']

        # result = director_root.call('wait jobid={}'.format(jobIdIncr))

        jobIdIncr = self.run_job(
            director_root, "backup-bareos-fd", "Incremental", wait=True
        )

        result = director_root.call("list jobs")
        logger.debug(str(result))

        self._test_job_result(result["jobs"], jobIdFull)
        self._test_job_result(result["jobs"], jobIdIncr)

        result = director_root.call("list volume pool=Full count")
        self.assertTrue(
            int(result["volumes"][0]["count"]) >= 1, u"Full pool contains no volumes."
        )

        result = director_root.call("list volume pool=Incremental count")
        self.assertTrue(
            int(result["volumes"][0]["count"]) >= 1,
            u"Incremental pool contains no volumes.",
        )

        # without Pool ACL restrictions,
        # 'list media all' returns all volumes from all pools.
        result = director_root.call("list media all")
        logger.debug(str(result))
        self.assertGreaterEqual(len(result["volumes"]), 2)

        #
        # login as console 'poolfull'
        #
        bareos_password = bareos.bsock.Password(console_password)
        console_poolfull = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name="poolfull",
            password=bareos_password,
            **self.director_extra_options
        )

        # 'list media all' returns an error,
        # as the current user has limited Pool ACL permissions.
        # This behavior describes the current behavior.
        # Improvements on the server side welcome.
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = console_poolfull.call("list media all")

        result = console_poolfull.call("llist media all")
        logger.debug(str(result))

        self.assertGreaterEqual(len(result["volumes"]), 1)

        for volume in result["volumes"]:
            self.assertEqual(volume["pool"], "Full")

        #
        # login as console 'poolnotfull'
        #
        self._test_no_volume_in_pool("poolnotfull", console_password, "Full")

        #
        # use profile without Pool restrictions
        # and overwrite the Pool ACL in the console.
        #
        console_overwrite = "overwritepoolacl"
        self.configure_add(
            director_root,
            "consoles",
            console_overwrite,
            u"console={} password={} profile=operator poolacl=!Full tlsenable=no".format(
                console_overwrite, console_password
            ),
        )

        # TODO:
        # IMHO this is a bug.
        # This console should not see volumes in the Full pool.
        # It needs to be fixed in the server side code.
        with self.assertRaises(AssertionError):
            self._test_no_volume_in_pool(console_overwrite, console_password, "Full")

    def test_json_list_jobid_with_job_acl(self):
        """
        This tests checks if the Job ACL works with the "list jobs" and "list jobid=<>" commands.

        login as operator
          run a backup-bareos-fd job.
          create and run a backup-bareos-fd-test job.
          verifies that both jobs are visible by the list command.
        login as a console that can only see backup-bareos-fd jobs
          verifies that the backup-bareos-fd is visible.
          verifies that the backup-bareos-fd is not visible.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        console_username = u"job-backup-bareos-fd"
        console_password = u"secret"
        jobname1 = u"backup-bareos-fd"
        jobname2 = u"backup-bareos-fd-test"

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        jobid1 = self.run_job(
            director=director_root, jobname=jobname1, level=u"Full", wait=True
        )

        self.configure_add(
            director_root,
            "jobs",
            jobname2,
            "job name={} client=bareos-fd jobdefs=DefaultJob".format(jobname2),
        )

        jobid2 = self.run_job(
            director=director_root, jobname=jobname2, level=u"Full", wait=True
        )

        #
        # both jobid should be visible
        #
        self._test_list_with_valid_jobid(director_root, jobid1)
        self._test_list_with_valid_jobid(director_root, jobid2)

        #
        # login as console_username
        #
        bareos_password = bareos.bsock.Password(console_password)
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=console_username,
            password=bareos_password,
            **self.director_extra_options
        )

        #
        # only jobid1 should be visible
        #
        self._test_list_with_valid_jobid(director, jobid1)
        self._test_list_with_invalid_jobid(director, jobid2)


class PythonBareosJsonRunScriptTest(PythonBareosJsonBase):
    def test_backup_runscript_client_defaults(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-client-defaults"
        level = None
        expected_log = ": ClientBeforeJob: jobname={}".format(jobname)

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_log
        )

    def test_backup_runscript_client(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-client"
        level = None
        expected_log = ": ClientBeforeJob: jobname={}".format(jobname)

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: ClientBeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_log
        )

    def test_backup_runscript_server(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-runscript-server"
        level = None
        expected_logs = [
            ": BeforeJob: jobname={}".format(jobname),
            ": BeforeJob: daemon=bareos-dir",
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_admin_runscript_server(self):
        """
        Run a job which contains a runscript.
        Check the JobLog if the runscript worked as expected.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-server"
        level = None
        expected_logs = [
            ": BeforeJob: jobname={}".format(jobname),
            ": BeforeJob: daemon=bareos-dir",
            ": BeforeJob: jobtype=Admin",
        ]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        #    "time": "2019-12-02 00:07:34",
        #    "logtext": "bareos-dir JobId 76: BeforeJob: jobname=admin-runscript-server\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )

    def test_admin_runscript_client(self):
        """
        RunScripts configured with "RunsOnClient = yes" (default)
        are not executed in Admin Jobs.
        Instead, a warning is written to the joblog.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "admin-runscript-client"
        level = None
        expected_logs = [": Invalid runscript definition"]

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # Example log entry:
        # {
        # "time": "2019-12-12 13:23:16",
        # "logtext": "bareos-dir JobId 7: Warning: Invalid runscript definition (command=...). Admin Jobs only support local runscripts.\n"
        # },

        jobId = self.run_job_and_search_joblog(
            director_root, jobname, level, expected_logs
        )


class PythonBareosJsonConfigTest(PythonBareosJsonBase):
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
            u"Resource {} in {} already exists.".format(newclient, resourcesname),
        )

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            director.call("show client={}".format(newclient))

        self.configure_add(
            director,
            resourcesname,
            newclient,
            u"client={} password={} address=127.0.0.1".format(newclient, newpassword),
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
            u"Resource {} in {} already exists.".format(testname, resourcesname),
        )

        self.configure_add(
            director,
            resourcesname,
            testname,
            u'job jobdefs=DefaultJob name={} description="{}" runafterjob="{}" priority={}'.format(
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


class PythonBareosDeleteTest(PythonBareosJsonBase):
    def test_delete_jobid(self):
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

        jobname = u"backup-bareos-fd"

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        jobid = self.run_job(director, jobname, level="Full", wait=True)
        job = self.list_jobid(director, jobid)
        result = director.call("list volume jobid={}".format(jobid))
        volume = result["volumes"][0]["volumename"]
        result = director.call("delete volume={} yes".format(volume))
        logger.debug(str(result))
        self.assertIn(jobid, result["deleted"]["jobids"])
        self.assertIn(volume, result["deleted"]["volumes"])
        with self.assertRaises(ValueError):
            job = self.list_jobid(director, jobid)


class PythonBareosBvfsTest(PythonBareosJsonBase):
    def get_sql_table_count(self, director, sql_from, minimum=None, maximum=None):
        # expect:
        # .sql query="SELECT count(*) FROM Job WHERE HasCache!=0;"
        # .api 0
        # +----------+
        # | count    |
        # +----------+
        # | 12       |
        # +----------+
        # .api 2
        # "query": [
        #   {
        #     "count": "12"
        #   }
        # ]
        logger = logging.getLogger()
        cmd = '.sql query="SELECT count(*) FROM {};"'.format(sql_from)
        count = int(director.call(cmd)["query"][0]["count"])
        if minimum is not None:
            self.assertGreaterEqual(
                count,
                minimum,
                "{}: expecting at least {} entries, got {}".format(
                    sql_from, minimum, count
                ),
            )
        if maximum is not None:
            self.assertLessEqual(
                count,
                maximum,
                "{}: expecting at most {} entries, got {}".format(
                    sql_from, minimum, count
                ),
            )
        logger.debug(
            "{}: {} entries ({}, {})\n".format(sql_from, count, minimum, maximum)
        )
        return count

    def prepare_bvfs_jobs(self, director, jobname, format_params):
        self.append_to_file(format_params["BackupFileExtra"], "Test Content: initial\n")

        jobid1 = self.run_job(director, jobname, level="Full", wait=True)
        files1 = director.call("list files jobid={}".format(jobid1))["filenames"]

        # modify file and rerun backup
        self.append_to_file(
            format_params["BackupFileExtra"],
            "Additional content, after job {}\n".format(jobid1),
        )

        jobid2 = self.run_job(director, jobname, wait=True)

        bvfs_jobids = director.call(".bvfs_get_jobids jobid={}".format(jobid2))[
            "jobids"
        ]
        backup_job_ids = [jobid["id"] for jobid in bvfs_jobids]
        self.assertGreaterEqual(
            len(backup_job_ids),
            2,
            u"Expecting at least 2 jobs, got {}.".format(backup_job_ids),
        )
        format_params["BackupJobIds"] = ",".join(backup_job_ids)

        result = director.call(
            ".bvfs_update jobid={BackupJobIds}".format(**format_params)
        )

        self.get_sql_table_count(
            director, "Job WHERE HasCache!=0", minimum=len(backup_job_ids)
        )
        self.get_sql_table_count(director, "PathHierarchy", minimum=1)
        self.get_sql_table_count(director, "PathVisibility", minimum=1)

        return bvfs_jobids

    def verify_bvfs_dirs(self, bvfs_lsdirs_result, expected_dirs):
        # bvfs_lsdirs_result contains the result of ".bvfs_lsdir ...".
        # Example:
        # .api 0
        # 10    0   0   A A A A A A A A A A A A A A .
        # 9   0   0   A A A A A A A A A A A A A A /
        # 8   0   0   A A A A A A A A A A A A A A BPIPE_ROOT/
        # .api 2
        # "directories": [
        #   {
        #      "type": "D",
        #      "pathid": 10,
        #      "fileid": 0,
        #      "jobid": 0,
        #      "lstat": "A A A A A A A A A A A A A A",
        #      "name": ".",
        #      "fullpath": ".",
        #      "stat": {
        #          ...
        #      },
        #      "linkfileindex": 0
        #   },
        #   {
        #     "type": "D",
        #     "pathid": 15,
        #     "fileid": 0,
        #     "jobid": 0,
        #     "lstat": "A A A A A A A A A A A A A A",
        #     "name": "/",
        #     "fullpath": "/",
        #     "stat": {
        #        ...
        #     },
        #     "linkfileindex": 0
        #   },
        #   {
        #     "type": "D",
        #     "pathid": 14,
        #     "fileid": 0,
        #     "jobid": 0,
        #     "lstat": "A A A A A A A A A A A A A A",
        #     "name": "BPIPE-ROOT/",
        #     "fullpath": "BPIPE-ROOT/",
        #     "stat": {
        #       ...
        #     },
        #     "linkfileindex": 0
        #   }
        # ]

        directories = [item["name"] for item in bvfs_lsdirs_result]

        for directory in expected_dirs:
            self.assertIn(
                directory,
                directories,
                'Expecting "{}" in list of directories, instead received: {}'.format(
                    directory, bvfs_lsdirs_result
                ),
            )

    def test_bvfs(self):
        """
        Test BVFS functionality:
        Run 1 full and 1 incremental backup job.
        These jobs have the normal "/" directory and an additional BPIPE-ROOT/ directory.
        Also do a restore using bvfs.
        """

        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = u"backup-bareos-fd-bpipe"

        format_params = {
            "Client": self.client,
            "BvfsPathId": "b201",
            "BackupDirectory": self.backup_directory,
            "BackupFileNameExtra": "bvfstest.txt",
            "BackupFileExtra": self.backup_directory + "/bvfstest.txt",
        }

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        bvfs_jobids = self.prepare_bvfs_jobs(director, jobname, format_params)

        bvfs_get_root_path = director.call(
            ".bvfs_lsdir jobid={BackupJobIds} path=".format(**format_params)
        )["directories"]

        self.verify_bvfs_dirs(bvfs_get_root_path, ["/", "BPIPE-ROOT/"])

        # Test offset
        cmd = ".bvfs_lsdir jobid={BackupJobIds} path= offset=1000 limit=1000".format(
            **format_params
        )
        bvfs_get_root_path_offset = director.call(cmd)["directories"]
        self.assertEqual(
            len(bvfs_get_root_path_offset),
            0,
            'Expecting "{}" to return empty list, instead received: {}'.format(
                cmd, bvfs_get_root_path_offset
            ),
        )

        cmd = ".bvfs_lsdir jobid={BackupJobIds} path=/".format(**format_params)
        bvfs_lsdir_root = director.call(cmd)["directories"]
        # expect:
        # 9   0   0   0   A A A A A A A A A A A A A A .
        # 10  0   0   0   A A A A A A A A A A A A A A ..
        # 8   0   0   0   A A A A A A A A A A A A A A TOP_DIRECTORY
        self.assertGreaterEqual(
            len(bvfs_lsdir_root),
            3,
            'Expecting at least 3 directories from "{}", instead received: {}'.format(
                cmd, bvfs_lsdir_root
            ),
        )

        cmd = ".bvfs_lsdir jobid={BackupJobIds} path={BackupDirectory}/".format(
            **format_params
        )
        bvfs_lsdir_BackupDirectory = director.call(cmd)["directories"]
        # expect:
        # 1 0   19  1   x GoHK EHt C GHH GHH A BAA BAA I BWDNOj BZwlgI BZwlgI A A C .
        # 2   0   0   0   A A A A A A A A A A A A A A ..
        # need to get dirid of "."
        backup_directory_pathid = next(
            directory["pathid"]
            for directory in bvfs_lsdir_BackupDirectory
            if directory["name"] == "."
        )
        self.assertIsInstance(
            backup_directory_pathid,
            int,
            'Failed to determine "pathid" of {} from {}. Received: {}'.format(
                cmd, format_params["BackupDirectory"], bvfs_lsdir_BackupDirectory
            ),
        )
        format_params["BackupDirectoryPathId"] = backup_directory_pathid

        cmd = ".bvfs_lsfiles jobid={BackupJobIds} path={BackupDirectory}/".format(
            **format_params
        )
        bvfs_lsfiles_BackupDirectory = director.call(cmd)["files"]
        backup_extrafile_id = next(
            directory["fileid"]
            for directory in bvfs_lsfiles_BackupDirectory
            if directory["name"] == "{BackupFileNameExtra}".format(**format_params)
        )
        self.assertIsInstance(
            backup_extrafile_id,
            int,
            'Failed to "{}" from "{}". Received: {}'.format(
                format_params["BackupFileNameExtra"], cmd, bvfs_lsdir_BackupDirectory
            ),
        )

        # test limit.
        cmd = ".bvfs_lsfiles jobid={BackupJobIds} path={BackupDirectory}/ offset 0 limit 1".format(
            **format_params
        )
        bvfs_lsfiles_BackupDirectory_limit1 = director.call(cmd)["files"]
        # TODO: bug, this returns multiple entires
        # self.assertEqual(len(bvfs_lsfiles_BackupDirectory_limit1), 1, 'Expecting 1 file from "{}", instead received: {}'.format(cmd, bvfs_lsfiles_BackupDirectory_limit1))

        cmd = ".bvfs_versions jobid=0 client={Client} path={BackupDirectory}/ fname={BackupFileNameExtra}".format(
            **format_params
        )
        bvfs_versions_extrafile = director.call(cmd)["versions"]
        self.assertGreaterEqual(
            len(bvfs_versions_extrafile),
            2,
            'Expecting at least 2 versions of file "{}" from "{}". Received: {}'.format(
                format_params["BackupFileNameExtra"], cmd, bvfs_versions_extrafile
            ),
        )

        cmd = ".bvfs_lsfiles jobid={BackupJobIds} path=BPIPE-ROOT/".format(
            **format_params
        )
        bvfs_lsfiles_bpipe_directory = director.call(cmd)["files"]
        files = [item["name"] for item in bvfs_lsfiles_bpipe_directory]
        self.assertIn(
            "bpipe-dir-as-root-test.txt",
            files,
            'Failed to get "bpipe-dir-as-root-test.txt" from "{}". Received: {}'.format(
                cmd, bvfs_lsfiles_bpipe_directory
            ),
        )

        #
        # now do a restore
        #

        # syntax:
        #   .bvfs_restore path=b201 fileid=numlist dirid=numlist hardlink=numlist path=b201
        cmd = ".bvfs_restore path={BvfsPathId} jobid={BackupJobIds} dirid={BackupDirectoryPathId}".format(
            **format_params
        )
        bvfs_restore_prepare = director.call(cmd)
        self.get_sql_table_count(director, format_params["BvfsPathId"], minimum=1)

        cmd = "restore client={Client} file=?{BvfsPathId} yes".format(**format_params)
        bvfs_restore = director.call(cmd)
        jobId = bvfs_restore["run"]["jobid"]
        self.wait_job(director, jobId)

        bvfs_cleanup = director.call(
            ".bvfs_cleanup path={BvfsPathId}".format(**format_params)
        )

        # table does not exist any more, so query will fail.
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            self.get_sql_table_count(director, format_params["BvfsPathId"])

        bvfs_clear_cache = director.call(".bvfs_clear_cache yes")

        self.get_sql_table_count(director, "Job WHERE HasCache!=0", maximum=0)
        self.get_sql_table_count(director, "PathHierarchy", maximum=0)
        self.get_sql_table_count(director, "PathVisibility", maximum=0)

        ## check for differences between original files and restored files
        # check_restore_diff ${BackupDirectory}

    def test_bvfs_and_dbcheck(self):
        """
        Test BVFS together with dbcheck:
        previous versions (<= 17.2.4) had a problem with bvfs after running dbcheck.
        Therefore this test checks
          * check bvfs root directory
          * run all dbcheck fixes
          * check bvfs root directory again
        """

        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = u"backup-bareos-fd-bpipe"

        format_params = {
            "Client": self.client,
            "BvfsPathId": "b201",
            "BackupDirectory": self.backup_directory,
            "BackupFileNameExtra": "bvfstest.txt",
            "BackupFileExtra": self.backup_directory + "/bvfstest.txt",
        }

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        bvfs_jobids = self.prepare_bvfs_jobs(director, jobname, format_params)

        bvfs_get_root_path = director.call(
            ".bvfs_lsdir jobid={BackupJobIds} path=".format(**format_params)
        )["directories"]

        self.verify_bvfs_dirs(bvfs_get_root_path, ["/", "BPIPE-ROOT/"])

        self.run_dbcheck()

        bvfs_get_root_path = director.call(
            ".bvfs_lsdir jobid={BackupJobIds} path=".format(**format_params)
        )["directories"]

        self.verify_bvfs_dirs(bvfs_get_root_path, ["/", "BPIPE-ROOT/"])

        self.run_dbcheck()

        bvfs_clear_cache = director.call(".bvfs_clear_cache yes")
        result = director.call(
            ".bvfs_update jobid={BackupJobIds}".format(**format_params)
        )

        self.run_dbcheck()


class PythonBareosFiledaemonTest(PythonBareosBase):
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


class PythonBareosShowTest(PythonBareosJsonBase):
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


def get_env():
    """
    Get attributes as environment variables,
    if not available or set use defaults.
    """

    config_directory = os.environ.get("confdir")
    if config_directory:
        PythonBareosBase.config_directory = config_directory

    director_root_password = os.environ.get("dir_password")
    if director_root_password:
        PythonBareosBase.director_root_password = director_root_password

    director_port = os.environ.get("BAREOS_DIRECTOR_PORT")
    if director_port:
        PythonBareosBase.director_port = director_port

    filedaemon_director_password = os.environ.get("fd_password")
    if filedaemon_director_password:
        PythonBareosBase.filedaemon_director_password = filedaemon_director_password

    filedaemon_port = os.environ.get("BAREOS_FD_PORT")
    if filedaemon_port:
        PythonBareosBase.filedaemon_port = filedaemon_port

    tls_version_str = os.environ.get("PYTHON_BAREOS_TLS_VERSION")
    if tls_version_str is not None:
        tls_version_parser = bareos.bsock.TlsVersionParser()
        tls_version = tls_version_parser.get_protocol_version_from_string(
            tls_version_str
        )
        if tls_version is not None:
            PythonBareosBase.director_extra_options["tls_version"] = tls_version
        else:
            print(
                "Environment variable PYTHON_BAREOS_TLS_VERSION has invalid value ({}). Valid values: {}".format(
                    tls_version_str,
                    ", ".join(tls_version_parser.get_protocol_versions()),
                )
            )

    backup_directory = os.environ.get("BackupDirectory")
    if backup_directory:
        PythonBareosBase.backup_directory = backup_directory

    dbcheck_binary = os.environ.get("BAREOS_DBCHECK_BINARY")
    if dbcheck_binary:
        PythonBareosBase.dbcheck_binary = dbcheck_binary

    if os.environ.get("REGRESS_DEBUG"):
        PythonBareosBase.debug = True


if __name__ == "__main__":
    get_env()
    unittest.main()
