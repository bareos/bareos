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


class PythonBareosAclTest(bareos_unittest.Json):
    def test_restore_with_client_acl(self):
        """
        Check if the restore command honors the client ACL.
        Login as console with access only to client = bareos-fd.
        Verify, that a restore can only be performed from this client.

        It checks the interactive restore command,
        therefore it can not use the Json console.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        console_bareos_fd_username = "client-bareos-fd"
        console_bareos_fd_password = "secret"

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
        #       because this jobid should be accessible in this console.
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
        # Only the bareos-fd client should be accessible
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

        console_password = "secret"

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

        self.append_to_file("{}/extrafile.txt".format(self.backup_directory), "Test\n")

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
            int(result["volumes"][0]["count"]) >= 1, "Full pool contains no volumes."
        )

        result = director_root.call("list volume pool=Incremental count")
        self.assertTrue(
            int(result["volumes"][0]["count"]) >= 1,
            "Incremental pool contains no volumes.",
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
            "console={} password={} profile=operator poolacl=!Full tlsenable=no tlsrequire=no".format(
                console_overwrite, console_password
            ),
        )

        # This console should not see volumes in the Full pool.
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

        console_username = "job-backup-bareos-fd"
        console_password = "secret"
        jobname1 = "backup-bareos-fd"
        jobname2 = "backup-bareos-fd-test"

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        jobid1 = self.run_job(
            director=director_root, jobname=jobname1, level="Full", wait=True
        )

        self.configure_add(
            director_root,
            "jobs",
            jobname2,
            "job name={} client=bareos-fd jobdefs=DefaultJob".format(jobname2),
        )

        jobid2 = self.run_job(
            director=director_root, jobname=jobname2, level="Full", wait=True
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

    def _test_status_subscription(self, username, password):
        logger = logging.getLogger()

        configured_subscriptions = "10"

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        result = director_root.call("status subscription all")
        self.assertEqual(
            configured_subscriptions, result["total-units-required"]["configured"]
        )

    def test_status_subscription_admin(self):
        username = self.get_operator_username()
        password = self.get_operator_password(username)
        self._test_status_subscription(username, password)

    def test_status_subscription_user_fails(self):
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            self._test_status_subscription("client-bareos-fd", "secret")

    def test_limited_command_acl(self):
        """
        The console "limited-operator" uses the profile "operator",
        but disallows the command ".consoles".
        """
        logger = logging.getLogger()

        username = "limited-operator"
        password = "secret"

        console = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        # verify that the command ".consoles" is not allowed.
        # We expect that the Bareos Director will return something like:
        # {
        #   "jsonrpc": "2.0",
        #   "id": null,
        #   "error": {
        #     "code": 1,
        #     "message": "failed",
        #     "data": {
        #       "result": {},
        #       "messages": {
        #         "error": [
        #           ".consoles: is an invalid command.\n"
        #         ]
        #       }
        #     }
        #   }
        # }
        # DirectorConsoleJson will raise an exception, if the result contains the "error" key.
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = console.call(".consoles")

        # verify that substings of the ".consoles" command are not allowed.
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = console.call(".consol")

        # verify that other commands are allowed.
        result = console.call(".jobs")
        self.assertGreaterEqual(len(result["jobs"]), 1)
