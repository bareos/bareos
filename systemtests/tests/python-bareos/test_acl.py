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

import json
import logging
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from time import monotonic, sleep
import unittest
import uuid
import warnings

import bareos.bsock
from bareos.bsock.constants import Constants
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.bsock.lowlevel import LowLevel
import bareos.exceptions

import bareos_unittest
from bareos_unittest.tree_browser_console import TreeBrowserConsole


class PythonBareosAclTest(bareos_unittest.Json):
    def test_visual_restore_run_requires_a_source(self):
        username = self.get_operator_username()
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console)
        screen = browser.run("run job=RestoreFiles")
        self.assertIn("Restore Options", screen)
        screen = browser.press("enter")
        self.assertIn("Please enter a JobId for restore:", screen)
        self.assertNotIn("Job queued", screen)
        screen = browser.submit_line("2147483647")
        self.assertIn("not found in catalog", screen)
        self.assertNotIn("Job queued", screen)

        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        for supplied in (False, True):
            with self.subTest(supplied=supplied):
                command = "run job=RestoreFiles"
                if supplied:
                    command += f" jobid={backup}"
                screen = browser.run(command)
                self.assertIn("Restore Options", screen)
                screen = browser.press("enter")
                if not supplied:
                    self.assertIn("Please enter a JobId for restore:", screen)
                    screen = browser.submit_line(str(backup))
                self.assertNotIn("Please enter a JobId for restore:", screen)
                queued = re.search(r"Job queued\. JobId=(\d+)", screen)
                self.assertIsNotNone(queued, screen)
                jobid = queued.group(1)
                self.addCleanup(admin.call, f"delete jobid={jobid} yes")
                admin.call(f"wait jobid={jobid}")

    def test_visual_relocation_accepts_period_in_text(self):
        username = self.get_operator_username()
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console, columns=140)
        browser.run("run job=RestoreFiles")
        browser.press("home")
        browser.press("down")
        browser.press("down")
        browser.press("enter")
        browser.press("right")
        browser.press("right")
        browser.press("down")
        browser.press("enter")
        screen = browser.type_text(".dir")
        self.assertIn("Type text", screen)
        self.assertRegex(screen, r"Source prefix\s+/home\.dir")
        screen = browser.press("enter")
        self.assertRegex(screen, r"Source prefix\s+/home\.dir")
        browser.press("enter")
        browser.type_text(".discard")
        screen = browser.press("cancel")
        self.assertRegex(screen, r"Source prefix\s+/home\.dir(?:\s|│)")
        screen = browser.type_text(".")
        self.assertIn("Restore Options", screen)
        browser.press("cancel")

    def test_destination_browser_search_accepts_period(self):
        username = self.get_operator_username()
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console, columns=140)
        browser.run("run job=RestoreFiles")
        browser.press("home")
        browser.press("down")
        screen = browser.type_text("b")
        self.assertIn("Browse destination client", screen)
        screen = browser.type_text("/")
        self.assertIn("Type substring", screen)
        screen = browser.type_text("needle.dir")
        self.assertIn("Type substring", screen)
        self.assertIn("Search: needle.dir", screen)
        screen = browser.press("enter")
        self.assertIn('Search: "needle.dir"', screen)
        browser.press("cancel")
        browser.press("cancel")
        browser.press("cancel")

    def test_restore_copies_require_complete_authorized_chain(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        full = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={full} yes")
        sleep(2)
        path = os.path.join(self.backup_directory, f"copy-chain-{uuid.uuid4().hex}")
        with open(path, "w") as data:
            data.write("incremental data\n")
        self.addCleanup(os.remove, path)
        sleep(2)
        incremental = self.run_job(admin, "backup-bareos-fd", "Incremental", wait=True)
        self.addCleanup(admin.call, f"delete jobid={incremental} yes")
        prefix = f"copy-chain-{uuid.uuid4().hex}"
        self.addCleanup(
            admin.call, f".sql query=\"DELETE FROM Job WHERE Job LIKE '{prefix}%'\""
        )
        for suffix, source, name in (
            ("full1", full, "backup-bareos-fd"),
            ("full2", full, "backup-bareos-fd"),
            ("incr", incremental, "backup-test2-fd"),
        ):
            admin.call(
                '.sql query="INSERT INTO Job '
                "(Job,Name,Type,Level,ClientId,JobStatus,StartTime,EndTime,"
                "JobTDate,FileSetId,PriorJobId) "
                f"SELECT '{prefix}-{suffix}','{name}','C',Level,ClientId,"
                "JobStatus,StartTime,EndTime,JobTDate,FileSetId,JobId "
                f'FROM Job WHERE JobId={source}"'
            )
        copy_rows = admin.call(
            f".sql query=\"SELECT JobId FROM Job WHERE Job LIKE '{prefix}%'\""
        )["query"]
        copy_ids = {str(row["jobid"]) for row in copy_rows}
        self.assertEqual(len(copy_ids), 3)

        for scenario in ("denied", "missing", "complete"):
            if scenario == "missing":
                admin.call(
                    f'.sql query="UPDATE Job SET PriorJobId=0 '
                    f"WHERE Job='{prefix}-incr'\""
                )
            elif scenario == "complete":
                admin.call(
                    f'.sql query="UPDATE Job SET PriorJobId={incremental},'
                    f"Name='backup-bareos-fd' WHERE Job='{prefix}-incr'\""
                )
            with self.subTest(scenario=scenario):
                console = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name="job-backup-bareos-fd",
                    password=bareos.bsock.Password("secret"),
                    **self.director_extra_options,
                )
                try:
                    result = console.call(
                        "restore client=bareos-fd fileset=SelfTest select copies yes"
                    )
                finally:
                    console.close()
                selected = re.search(
                    rb"You have selected the following JobIds: ([0-9,]+)", result
                )
                self.assertIsNotNone(selected, result)
                selected_ids = set(selected.group(1).decode().split(","))
                if scenario == "complete":
                    self.assertEqual(selected_ids, copy_ids)
                    self.assertNotIn(b"No complete authorized copy chain", result)
                else:
                    self.assertEqual(selected_ids, {str(full), str(incremental)})
                    self.assertIn(b"No complete authorized copy chain", result)

    def test_restore_latest_rejects_before_cutoff(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        jobid = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={jobid} yes")
        for command in (
            'restore latest before="2000-01-01 00:00:00"',
            'restore before="2000-01-01 00:00:00" latest',
        ):
            with self.subTest(command=command):
                console = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name="client-bareos-fd",
                    password=bareos.bsock.Password("secret"),
                    **self.director_extra_options,
                )
                result = console.call(command)
                self.assertIn(b"latest cannot be combined with before", result)
                self.assertNotIn(b"Automatically selected restore point:", result)

        for command in ("restore latest", "restore latest current"):
            with self.subTest(command=command):
                console = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name="client-bareos-fd",
                    password=bareos.bsock.Password("secret"),
                    **self.director_extra_options,
                )
                result = console.call(command)
                self.assertIn(b"Automatically selected restore point:", result)
                console.call("done")

    def test_restore_discovers_historical_fileset_version(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        jobid = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={jobid} yes")
        self.addCleanup(
            admin.call, ".sql query=\"DELETE FROM FileSet WHERE MD5='history-test'\""
        )
        admin.call(
            '.sql query="INSERT INTO FileSet (FileSet,MD5,CreateTime,FileSetText) '
            "SELECT FileSet,'history-test','2099-01-01 00:00:00',FileSetText "
            f'FROM FileSet WHERE FileSetId=(SELECT FileSetId FROM Job WHERE JobId={jobid})"'
        )
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name="client-bareos-fd",
            password=bareos.bsock.Password("secret"),
            **self.director_extra_options,
        )
        console.call("restore")
        result = console.call("1")
        self.assertIn(b"Automatically selected restore point:", result)
        console.call("done")

    def test_pluginhints_jobid_respects_job_and_client_acls(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        allowed = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={allowed} yes")
        denied = self.run_job(admin, "backup-test2-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={denied} yes")
        for name in ("job-backup-bareos-fd", "client-bareos-fd"):
            with self.subTest(console=name):
                console = bareos.bsock.DirectorConsoleJson(
                    address=self.director_address,
                    port=self.director_port,
                    name=name,
                    password=bareos.bsock.Password("secret"),
                    **self.director_extra_options,
                )
                console.call(f".pluginhints jobid={allowed}")
                with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
                    console.call(f".pluginhints jobid={denied}")

    def test_job_pagination_reports_acl_filtered_rows(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        for job in ("backup-bareos-fd", "backup-test2-fd"):
            jobid = self.run_job(admin, job, "Full", wait=True)
            self.addCleanup(admin.call, f"delete jobid={jobid} yes")
        restricted = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name="job-backup-bareos-fd",
            password=bareos.bsock.Password("secret"),
            **self.director_extra_options,
        )
        expected = restricted.call("list jobs sortby=jobid")["jobs"]
        rows = []
        filtered = 0
        for offset in range(len(expected) + 1):
            result = restricted.call(f"list jobs sortby=jobid limit=1 offset={offset}")
            page = result.get("jobs", [])
            suppressed = result["meta"]["range"]["filtered"]
            self.assertEqual(page, expected[offset : offset + 1])
            rows.extend(page)
            filtered += suppressed
        self.assertGreater(filtered, 0)
        self.assertEqual(rows, expected)

    def test_catalog_status_without_authorized_catalog(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        name = "catalog-status-denied"
        self.configure_add(
            admin,
            "consoles",
            name,
            f"console={name} password=secret profile=operator "
            "catalogacl=!MyCatalog tlsenable=no tlsrequire=no",
        )
        for api in (False, True):
            with self.subTest(api=api):
                console = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name=name,
                    password=bareos.bsock.Password("secret"),
                    **self.director_extra_options,
                )
                console.call("gui on")
                if api:
                    console.call(".api 2")
                result = console.call("status catalog").decode("utf-8")
                if api:
                    self.assertRegex(result, r'"status"\s*:\s*"unavailable"')
                else:
                    self.assertIn("Status: unavailable", result)
                self.assertIn("Failed to open catalog database", result)
                self.assertIn("bareos", console.call("version").decode("utf-8").lower())

    def test_volume_comment_checks_pool_after_interactive_prompt(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        restricted = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name="poolfull",
            password=bareos.bsock.Password("secret"),
            **self.director_extra_options,
        )
        volume = f"CommentPromptRace-{uuid.uuid4().hex}"
        admin.call("update pool=Incremental")
        admin.call(f"label volume={volume} storage=File pool=Full")
        try:
            admin.call(f'update volume={volume} comment="original"')
            restricted.call(f"update volume={volume}")
            prompt = restricted.call("19")
            self.assertIn(b"Enter new Comment", prompt)
            admin.call(f"update volume={volume} pool=Incremental")
            result = restricted.call("changed")
            self.assertIn(b"Not authorized", result)
            record = admin.call(f"llist volume={volume}")["volume"]
            self.assertEqual(record["comment"], "original")

            admin.call(f"update volume={volume} pool=Full")
            restricted.call(f"update volume={volume}")
            self.assertIn(b"Enter new Comment", restricted.call("19"))
            observer = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=self.get_operator_password(username),
                **self.director_extra_options,
            )
            admin.call('.sql query="BEGIN"')
            transaction_open = True
            try:
                admin.call(
                    '.sql query="UPDATE Media SET PoolId='
                    "(SELECT PoolId FROM Pool WHERE Name='Incremental') "
                    f"WHERE VolumeName='{volume}'\""
                )
                with ThreadPoolExecutor(max_workers=1) as executor:
                    pending = executor.submit(restricted.call, "changed")
                    try:
                        deadline = monotonic() + 10
                        while monotonic() < deadline:
                            waiting = observer.call(
                                '.sql query="SELECT count(*) AS waiting FROM '
                                "pg_stat_activity WHERE wait_event_type='Lock' "
                                "AND query LIKE '%UPDATE Media SET Comment=%'\""
                            )["query"][0]["waiting"]
                            if int(waiting):
                                break
                            sleep(0.1)
                        else:
                            restricted.close()
                            self.fail("Comment update did not wait for the pool move")
                    finally:
                        admin.call('.sql query="COMMIT"')
                        transaction_open = False
                    self.assertIn(
                        b"Volume pool changed",
                        self.wait_for_console_call(pending, restricted),
                    )
            finally:
                if transaction_open:
                    admin.call('.sql query="ROLLBACK"')
                admin.call(
                    f'.sql query="UPDATE Media SET PoolId='
                    f"(SELECT PoolId FROM Pool WHERE Name='Full') "
                    f"WHERE VolumeName='{volume}'\""
                )
            record = admin.call(f"llist volume={volume}")["volume"]
            self.assertEqual(record["comment"], "original")
        finally:
            if restricted.socket is not None:
                restricted.call(".")
            admin.call(f"delete volume={volume} yes")
            os.remove(f"storage/{volume}")

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
            **self.director_extra_options,
        )

        jobIdBareosFdFull = self.get_backup_jobid(
            director_root, "backup-bareos-fd", level="Full"
        )
        jobIdTestFdFull = self.get_backup_jobid(
            director_root, "backup-test2-fd", level="Full"
        )

        #
        # login as console with ACLs
        #
        bareos_password = bareos.bsock.Password(console_bareos_fd_password)
        console_bareos_fd = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=console_bareos_fd_username,
            password=bareos_password,
            **self.director_extra_options,
        )
        console_bareos_fd_json = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=console_bareos_fd_username,
            password=bareos_password,
            **self.director_extra_options,
        )

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            console_bareos_fd_json.call(".clientbrowse client=test2-fd path=/")

        result = console_bareos_fd.call("restore")
        logger.debug(str(result))

        #
        # restore: 1: Select a FileSet@Client combination (latest backup)
        #
        # Only the bareos-fd client should be accessible
        # and is therefore autoselected.
        #
        result = console_bareos_fd.call("1")
        logger.debug(str(result))
        self.assertIn(
            b"Automatically selected FileSet@Client: SelfTest@bareos-fd", result
        )
        # The latest restore point is now selected automatically,
        # without any further interactive prompt.
        self.assertIn(b"Automatically selected restore point:", result)
        # Exit the file tree selection
        result = console_bareos_fd.call("done")
        logger.debug(str(result))

        result = console_bareos_fd.call("restore")

        #
        # restore: 2: Select a FileSet@Client combination (custom restore
        # point)
        #
        # Only the bareos-fd client should be accessible
        # and is therefore autoselected. Since more than one restore
        # chain exists, the interactive restore chain picker is shown
        # and we select the first (newest) one manually.
        #
        result = console_bareos_fd.call("2")
        logger.debug(str(result))
        self.assertIn(
            b"Automatically selected FileSet@Client: SelfTest@bareos-fd", result
        )
        self.assertIn(b"Select restore point", result)
        result = console_bareos_fd.call("1")
        logger.debug(str(result))
        # Exit the file tree selection
        result = console_bareos_fd.call("done")
        logger.debug(str(result))

        result = console_bareos_fd.call("restore")

        #
        # restore: 13: List last 20 Jobs run
        #
        # This requires access to the "sqlquery" command,
        # which this console does not have.
        #
        result = console_bareos_fd.call("13")
        logger.debug(str(result))
        self.assertEqual(b"SQL query not authorized.", result.strip())

        result = console_bareos_fd.call("restore")

        #
        # restore: 14: List Jobs where a given File is saved
        #
        # Only the bareos-fd client should be accessable
        # and is therefore autoselected.
        #
        result = console_bareos_fd.call("14")
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
            **self.director_extra_options,
        )

        jobIdFull = self.get_backup_jobid(director_root, "backup-bareos-fd", "Full")

        # make sure, timestamp differs
        sleep(2)

        self.append_to_file("{}/extrafile.txt".format(self.backup_directory), "Test\n")

        sleep(2)

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
            **self.director_extra_options,
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

        volumes = director_root.call("llist media all")["volumes"]
        allowed_volume = next(v["volumename"] for v in volumes if v["pool"] == "Full")
        denied_volume = next(
            v["volumename"] for v in volumes if v["pool"] == "Incremental"
        )
        director_root.call(f'update volume="{denied_volume}" comment="original"')
        console_poolfull.call(f'update volume="{allowed_volume}" comment="allowed"')
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            console_poolfull.call(f'update volume="{denied_volume}" comment="denied"')
        volumes = {
            v["volumename"]: v["comment"]
            for v in director_root.call("llist media all")["volumes"]
        }
        self.assertEqual(volumes[allowed_volume], "allowed")
        self.assertEqual(volumes[denied_volume], "original")

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

    def test_jobmedia_respects_job_client_and_current_pool_acls(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        full = str(self.run_job(admin, "backup-bareos-fd", "Full", wait=True))
        self.addCleanup(admin.call, f"delete jobid={full} yes")
        self.append_to_file(f"{self.backup_directory}/extrafile.txt", "JobMedia ACL\n")
        incremental = str(
            self.run_job(admin, "backup-bareos-fd", "Incremental", wait=True)
        )
        self.addCleanup(admin.call, f"delete jobid={incremental} yes")
        other = str(self.run_job(admin, "backup-test2-fd", "Full", wait=True))
        self.addCleanup(admin.call, f"delete jobid={other} yes")
        jobids = {full, incremental, other}
        consoles = {}
        cases = (
            ("job-backup-bareos-fd", {full, incremental}),
            ("client-bareos-fd", {full, incremental}),
            ("poolfull", {full, other}),
            ("poolnotfull", {incremental}),
        )
        for name, _allowed in cases:
            consoles[name] = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name=name,
                password=bareos.bsock.Password("secret"),
                **self.director_extra_options,
            )

        for command in ("list", "llist"):
            baseline = admin.call(f"{command} jobmedia")["jobmedia"]
            self.assertTrue(jobids <= {row["jobid"] for row in baseline})
            columns = {
                "jobmediaid",
                "jobid",
                "volumename",
                "firstindex",
                "lastindex",
                "startfile",
                "endfile",
                "startblock",
                "endblock",
                "jobbytes",
            }
            if command == "llist":
                columns.add("mediaid")
            self.assertEqual(set(baseline[0]), columns)
            volumes = {row["volumename"] for row in baseline if row["jobid"] in jobids}
            for name, allowed in cases:
                console = consoles[name]
                for selector in ("", *(f' volume="{volume}"' for volume in volumes)):
                    with self.subTest(command=command, console=name, selector=selector):
                        selected = admin.call(f"{command} jobmedia{selector}")[
                            "jobmedia"
                        ]
                        expected = {
                            row["jobmediaid"]
                            for row in selected
                            if row["jobid"] in allowed
                        }
                        rows = console.call(f"{command} jobmedia{selector}")["jobmedia"]
                        actual = {
                            row["jobmediaid"] for row in rows if row["jobid"] in jobids
                        }
                        self.assertEqual(actual, expected)
                        for row in rows:
                            self.assertEqual(set(row), columns)
                for jobid in allowed:
                    rows = console.call(f"{command} jobmedia jobid={jobid}")["jobmedia"]
                    self.assertTrue(rows)
                    self.assertEqual({row["jobid"] for row in rows}, {jobid})
                if name.startswith("pool"):
                    for jobid in jobids - allowed:
                        self.assertEqual(
                            console.call(f"{command} jobmedia jobid={jobid}")[
                                "jobmedia"
                            ],
                            [],
                        )

        full_rows = admin.call(f"llist jobmedia jobid={full}")["jobmedia"]
        volume = full_rows[0]["volumename"]
        usage_command = f'list volumeusage volume="{volume}"'
        for name, visible in (("poolfull", True), ("poolnotfull", False)):
            segments = consoles[name].call(usage_command)["volumeusage"]["segments"]
            self.assertEqual(full in {str(row["jobid"]) for row in segments}, visible)
        moved_ids = {
            row["jobmediaid"] for row in full_rows if row["volumename"] == volume
        }
        try:
            admin.call(f'update volume="{volume}" pool=Incremental')
            for command in ("list", "llist"):
                denied = consoles["poolfull"].call(f"{command} jobmedia jobid={full}")[
                    "jobmedia"
                ]
                allowed = consoles["poolnotfull"].call(
                    f"{command} jobmedia jobid={full}"
                )["jobmedia"]
                self.assertTrue(
                    moved_ids.isdisjoint(row["jobmediaid"] for row in denied)
                )
                self.assertTrue(moved_ids <= {row["jobmediaid"] for row in allowed})
            for name, visible in (("poolfull", False), ("poolnotfull", True)):
                segments = consoles[name].call(usage_command)["volumeusage"]["segments"]
                self.assertEqual(
                    full in {str(row["jobid"]) for row in segments}, visible
                )
        finally:
            admin.call(f'update volume="{volume}" pool=Full')

        original_client = admin.call(
            f'.sql query="SELECT ClientId FROM Job WHERE JobId={full}"'
        )["query"][0]["clientid"]
        try:
            admin.call(f'.sql query="UPDATE Job SET ClientId=0 WHERE JobId={full}"')
            for command in ("list", "llist"):
                rows = consoles["client-bareos-fd"].call(f"{command} jobmedia")[
                    "jobmedia"
                ]
                self.assertIn(full, {row["jobid"] for row in rows})
            segments = consoles["client-bareos-fd"].call(usage_command)["volumeusage"][
                "segments"
            ]
            self.assertIn(full, {str(row["jobid"]) for row in segments})
            admin.call(
                f'.sql query="UPDATE Job SET ClientId=2147483647 WHERE JobId={full}"'
            )
            for command in ("list", "llist"):
                rows = consoles["job-backup-bareos-fd"].call(f"{command} jobmedia")[
                    "jobmedia"
                ]
                self.assertNotIn(full, {row["jobid"] for row in rows})
            segments = consoles["job-backup-bareos-fd"].call(usage_command)[
                "volumeusage"
            ]["segments"]
            self.assertNotIn(full, {str(row["jobid"]) for row in segments})
        finally:
            admin.call(
                f'.sql query="UPDATE Job SET ClientId={int(original_client)} WHERE JobId={full}"'
            )

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
            **self.director_extra_options,
        )

        jobid1 = self.get_backup_jobid(
            director=director_root, jobname=jobname1, level="Full"
        )

        self.configure_add(
            director_root,
            "jobs",
            jobname2,
            "job name={} client=bareos-fd jobdefs=DefaultJob".format(jobname2),
        )

        jobid2 = self.get_backup_jobid(
            director=director_root, jobname=jobname2, level="Full"
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
            **self.director_extra_options,
        )

        #
        # only jobid1 should be visible
        #
        self._test_list_with_valid_jobid(director, jobid1)
        self._test_list_with_invalid_jobid(director, jobid2)

        for list_command in ("list", "llist"):
            for jobid, visible in ((jobid1, True), (jobid2, False)):
                root_rows = director_root.call(
                    f"{list_command} jobmedia jobid={jobid}"
                )["jobmedia"]
                self.assertTrue(root_rows)
                for volume in {row["volumename"] for row in root_rows}:
                    rows = director.call(f'{list_command} jobmedia volume="{volume}"')[
                        "jobmedia"
                    ]
                    ids = {int(row["jobid"]) for row in rows}
                    self.assertEqual(int(jobid) in ids, visible)
                    for row in rows:
                        self.assertNotIn("jobname", row)
                        self.assertNotIn("clientname", row)

        director_root.call(f'update jobid={jobid2} comment="original"')
        director.call(f'update jobid={jobid1} comment="allowed"')
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            director.call(f'update jobid={jobid2} comment="denied"')
        self.assertEqual(
            director_root.call(f"llist jobid={jobid1}")["jobs"][0]["comment"],
            "allowed",
        )
        self.assertEqual(
            director_root.call(f"llist jobid={jobid2}")["jobs"][0]["comment"],
            "original",
        )

    def _test_status_subscription(self, username, password):
        logger = logging.getLogger()

        configured_subscriptions = "10"

        director_root = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        result = director_root.call("status subscription all")
        self.assertEqual(configured_subscriptions, result["unit-summary"]["configured"])

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
            **self.director_extra_options,
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

    def test_limiting_where_acl(self):
        """
        Try different where options on restore.
        """
        logger = logging.getLogger()

        username = "limited-operator"
        password = "secret"

        jobname = "backup-bareos-fd"
        client = "bareos-fd"
        # console WhereACL is expected to be:
        # WhereAcl = <allowed_restore_path>, "!*all*"
        allowed_restore_path = "{}/tmp/bareos-restores-{}".format(os.getcwd(), username)

        console = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        # retrieve or create a jobid of a valid backup job
        backup_jobid = self.get_backup_jobid(console, jobname, level="Full")

        # restore with default where path
        restore_jobid = self.run_restore(console, client=client, jobid=backup_jobid)

        # restore with allowed where path
        restore_jobid = self.run_restore(
            console,
            client=client,
            jobid=backup_jobid,
            extra="where={}".format(allowed_restore_path),
        )

        # try to restore with non-allowed where path
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            restore_jobid = self.run_restore(
                console, client=client, jobid=backup_jobid, extra="where=/tmp/INVALID"
            )

        # try to restore with non-allowed empty where path
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            restore_jobid = self.run_restore(
                console, client=client, jobid=backup_jobid, extra="where="
            )
