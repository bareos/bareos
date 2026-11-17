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
import socket
import struct
import subprocess
import tempfile
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
from bareos_unittest.tree_browser_console import TreeBrowserConsole


class RestoreTreeSignalTest(bareos_unittest.Json):
    def test_restore_latest_accepts_filesetclient(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        result = console.call("restore latest filesetclient=SelfTest@bareos-fd")
        self.assertIn(f"JobId={backup}".encode(), result)
        self.assertNotIn(b"Select a FileSet@Client combination", result)
        self.assertIn(b"You have selected the following JobId", result)
        console.call("quit")

    def test_restore_rejects_invalid_filesetclient(self):
        username = self.get_operator_username()
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        for value in ("invalid", "@bareos-fd", "SelfTest@", "SelfTest@bareos-fd@x"):
            with self.subTest(value=value):
                result = console.call(f"restore latest filesetclient={value}")
                self.assertIn(b"Expected fileset@client", result)

    def test_restore_rejects_conflicting_filesetclient(self):
        username = self.get_operator_username()
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        for argument in (
            "filesetclient=SelfTest@bareos-fd",
            "client=bareos-fd",
            "fileset=SelfTest",
            "jobid=1",
            "file=/nonexistent",
            "directory=/nonexistent",
            "fileregex=.*",
        ):
            with self.subTest(argument=argument):
                result = console.call(
                    f"restore latest filesetclient=SelfTest@bareos-fd {argument}"
                )
                self.assertIn(b"filesetclient cannot be combined", result)

    def test_restore_filesetclient_respects_acls(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        for acl in (
            "clientacl=!bareos-fd",
            "filesetacl=!SelfTest",
            "jobacl=!backup-bareos-fd",
        ):
            with self.subTest(acl=acl):
                name = "filesetclient-deny-" + acl.split("=")[0]
                self.configure_add(
                    admin,
                    "consoles",
                    name,
                    f"console={name} password=secret profile=operator "
                    f"{acl} tlsenable=no tlsrequire=no",
                )
                console = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name=name,
                    password=bareos.bsock.Password("secret"),
                    **self.director_extra_options,
                )
                self.addCleanup(console.close)
                result = console.call("restore latest filesetclient=SelfTest@bareos-fd")
                self.assertNotIn(b"You have selected the following JobId", result)
                self.assertIn(b"No accessible FileSet@Client", result)

    def test_equal_start_fulls_remain_distinct_restore_points(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        data = tempfile.NamedTemporaryFile(dir=os.environ["BackupDirectory"])
        self.addCleanup(data.close)
        data.write(b"full")
        data.flush()
        first = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={first} yes")
        data.write(b"incremental one")
        data.flush()
        os.utime(data.name, (2100000000, 2100000000))
        first_incremental = self.run_job(
            admin, "backup-bareos-fd", "Incremental", wait=True
        )
        self.addCleanup(admin.call, f"delete jobid={first_incremental} yes")
        data.write(b"differential")
        data.flush()
        os.utime(data.name, (2100000000, 2100000000))
        first_differential = self.run_job(
            admin, "backup-bareos-fd", "Differential", wait=True
        )
        self.addCleanup(admin.call, f"delete jobid={first_differential} yes")
        data.write(b"incremental after differential")
        data.flush()
        os.utime(data.name, (2100000000, 2100000000))
        after_differential = self.run_job(
            admin, "backup-bareos-fd", "Incremental", wait=True
        )
        self.addCleanup(admin.call, f"delete jobid={after_differential} yes")
        second = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        second_present = [True]
        self.addCleanup(
            lambda: (
                admin.call(f"delete jobid={second} yes") if second_present[0] else None
            )
        )
        data.write(b"incremental two")
        data.flush()
        os.utime(data.name, (2100000000, 2100000000))
        second_incremental = self.run_job(
            admin, "backup-bareos-fd", "Incremental", wait=True
        )
        self.addCleanup(admin.call, f"delete jobid={second_incremental} yes")
        admin.call(
            '.sql query="UPDATE Job SET '
            f"StartTime=(SELECT StartTime FROM Job WHERE JobId={first}),"
            f"JobTDate=(SELECT JobTDate FROM Job WHERE JobId={first}) "
            f"WHERE JobId IN ({first_incremental},{first_differential},"
            f'{after_differential},{second},{second_incremental})"'
        )

        cases = (
            ("restore", second, {second, second_incremental}),
            ("restore", first, {first, first_differential, after_differential}),
            ("restore restorepointmode=job", second, {second}),
            ("restore restorepointmode=job", first, {first}),
        )
        for command, selected_job, expected in cases:
            with self.subTest(command=command, selected_job=selected_job):
                console = bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    name=username,
                    password=self.get_operator_password(username),
                    **self.director_extra_options,
                )
                try:
                    console.call(command)
                    points = console.call("2")
                    if b"FileSet@Client combinations" in points:
                        tuple_match = re.search(
                            rb"(?m)^\s*(\d+): SelfTest@bareos-fd\s*$", points
                        )
                        self.assertIsNotNone(tuple_match, points)
                        points = console.call(tuple_match.group(1).decode())
                    self.assertIn(f"JobId={first}".encode(), points)
                    self.assertIn(f"JobId={second}".encode(), points)
                    choice = re.search(
                        rb"(?m)^\s*(\d+):[^\n]*\bJobId="
                        + str(selected_job).encode()
                        + rb"\b",
                        points,
                    )
                    self.assertIsNotNone(choice, points)
                    result = console.call(choice.group(1).decode())
                    match = re.search(
                        rb"You have selected the following JobIds?: ([0-9,]+)",
                        result,
                    )
                    self.assertIsNotNone(match, result)
                    self.assertEqual(
                        {int(jobid) for jobid in match.group(1).split(b",")},
                        {int(jobid) for jobid in expected},
                    )
                    console.call("quit")
                finally:
                    console.close()

        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        console.call("restore")
        points = console.call("2")
        if b"FileSet@Client combinations" in points:
            tuple_match = re.search(rb"(?m)^\s*(\d+): SelfTest@bareos-fd\s*$", points)
            self.assertIsNotNone(tuple_match, points)
            points = console.call(tuple_match.group(1).decode())
        choice = re.search(
            rb"(?m)^\s*(\d+):[^\n]*\bJobId=" + str(first).encode() + rb"\b",
            points,
        )
        self.assertIsNotNone(choice, points)
        admin.call(f"delete jobid={second} yes")
        second_present[0] = False
        result = console.call(choice.group(1).decode())
        match = re.search(
            rb"You have selected the following JobIds?: ([0-9,]+)", result
        )
        self.assertIsNotNone(match, result)
        self.assertEqual(
            {int(jobid) for jobid in match.group(1).split(b",")},
            {int(first), int(first_differential), int(after_differential)},
        )
        console.call("quit")

    def test_reopened_tree_restores_changed_selection(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console, columns=160)
        source_dir = os.path.join(
            os.environ["BackupDirectory"], "weird-files", "simple-dir-1"
        )
        selected = os.path.join(source_dir, "other-file1.txt")
        excluded = os.path.join(source_dir, "other-file2.txt")
        with tempfile.TemporaryDirectory(dir=os.environ["tmp"]) as destination:
            user_bootstrap = os.path.join(destination, "user-owned.bsr")
            with open(user_bootstrap, "w", encoding="utf-8") as bootstrap:
                bootstrap.write("user-owned bootstrap\n")
            self.assertIn(
                "Restore selection",
                browser.run(
                    f"restore jobid={backup} all client=bareos-fd "
                    f"restoreclient=bareos-fd where={destination}"
                ),
            )
            self.assertIn("Restore Options", browser.type_text("r"))
            browser.press("home")
            for _ in range(6):
                browser.press("down")
            browser.press("enter")
            for _ in range(3):
                browser.press("down")
            self.assertIn(
                "Please enter the Bootstrap file name:", browser.press("enter")
            )
            self.assertIn("Restore Options", browser.submit_line(user_bootstrap))
            browser.press("home")
            browser.press("down")
            browser.press("down")
            self.assertIn("Restore selection", browser.press("enter"))
            browser.type_text("c")
            browser.submit_line("unmark *")
            self.assertIn(
                "1 file newly marked", browser.submit_line(f"mark {selected}")
            )
            self.assertIn("Restore Options", browser.submit_line("done"))
            browser.press("end")
            response = browser.press("enter")
            match = re.search(r"Job queued\. JobId=(\d+)", response)
            self.assertIsNotNone(match, response)
            restore = int(match.group(1))
            self.addCleanup(admin.call, f"delete jobid={restore} yes")
            self.wait_job(admin, restore)
            restored = os.path.join(destination, selected.lstrip("/"))
            not_restored = os.path.join(destination, excluded.lstrip("/"))
            with open(selected, "rb") as source, open(restored, "rb") as result:
                self.assertEqual(source.read(), result.read())
            self.assertFalse(os.path.exists(not_restored))
            with open(user_bootstrap, encoding="utf-8") as bootstrap:
                self.assertEqual(bootstrap.read(), "user-owned bootstrap\n")

    def test_reopened_tree_cannot_restore_an_empty_selection(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console, columns=160)
        self.assertIn(
            "Restore selection",
            browser.run(
                f"restore jobid={backup} all client=bareos-fd restoreclient=bareos-fd"
            ),
        )
        self.assertIn("Restore Options", browser.type_text("r"))
        browser.press("home")
        browser.press("down")
        browser.press("down")
        self.assertIn("Restore selection", browser.press("enter"))
        browser.type_text("u")
        response = browser.type_text("r")
        self.assertIn("No files selected to be restored", response)
        self.assertNotIn("Restore Options", response)

    def test_restore_advanced_menu_exposes_jobid(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")

        visual = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(visual.close)
        browser = TreeBrowserConsole(visual, columns=160)
        self.assertIn(
            "Restore Options", browser.run(f"run job=RestoreFiles jobid={backup}")
        )
        browser.press("home")
        for _ in range(5):
            browser.press("down")
        screen = browser.press("enter")
        self.assertIn("JobId", screen)
        for _ in range(4):
            browser.press("down")
        browser.press("enter")
        browser.press("end")
        self.assertIn("Please enter a JobId for restore:", browser.press("enter"))
        browser.press("cancel")

        classic = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(classic.close)
        self.assertIn(
            b"OK to run?", classic.call(f"run job=RestoreFiles jobid={backup}")
        )
        self.assertIn(b"Advanced Options", classic.call("mod"))
        self.assertIn(b"JobId", classic.call("8"))
        self.assertIn(b"Please enter a JobId for restore:", classic.call("5"))

    def test_restore_empty_where_uses_original_location(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console, columns=160)
        self.assertIn(
            "Restore selection",
            browser.run(
                f"restore jobid={backup} all client=bareos-fd "
                'restoreclient=bareos-fd where=""'
            ),
        )
        screen = browser.type_text("r")
        self.assertIn("Restore Options", screen)
        self.assertIn("(original location)", screen)
        browser.press("cancel")
        self.assertIn(
            "Restore selection",
            browser.run(
                f"restore jobid={backup} all client=bareos-fd "
                "restoreclient=bareos-fd"
            ),
        )
        screen = browser.type_text("r")
        self.assertIn("Restore Options", screen)
        self.assertNotIn("(original location)", screen)
        browser.press("cancel")

    def test_restore_tree_stops_when_client_sends_signal(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console)
        screen = browser.run(
            f"restore jobid={backup} all client=bareos-fd restoreclient=bareos-fd"
        )
        self.assertIn("Restore selection", screen)
        console.socket.sendall(struct.pack("!i", Constants.BNET_EOD))
        response = console.recv_msg().decode("utf-8")
        self.assertIn("Restore not done.", response)
        self.assertNotIn("Bootstrap records written", response)
        self.assertNotIn("Restore Options", response)


class PythonBareosPlainTest(bareos_unittest.Base):
    def test_api_input_signals_precede_question(self):
        for signals in (
            (Constants.BNET_YESNO, Constants.BNET_TEXT_INPUT),
            (Constants.BNET_TEXT_INPUT,),
        ):
            with self.subTest(signals=signals):
                client, server = socket.socketpair()
                self.addCleanup(client.close)
                self.addCleanup(server.close)
                console = LowLevel()
                console.socket = client
                question = b"Enter a value: "
                response = b"Accepted\n"
                packets = b"".join(struct.pack("!i", signal) for signal in signals)
                packets += struct.pack("!i", len(question)) + question
                packets += struct.pack("!i", len(response)) + response
                packets += struct.pack("!i", Constants.BNET_EOD)
                server.sendall(packets)
                self.assertEqual(console.recv_msg(), question)
                self.assertEqual(console.recv_msg(), response)

    def test_selection_signal_follows_menu(self):
        client, server = socket.socketpair()
        self.addCleanup(client.close)
        self.addCleanup(server.close)
        console = LowLevel()
        console.socket = client
        menu = b"Select an option: "
        response = b"Selected\n"
        server.sendall(
            struct.pack("!i", len(menu))
            + menu
            + struct.pack("!i", Constants.BNET_SELECT_INPUT)
            + struct.pack("!i", len(response))
            + response
            + struct.pack("!i", Constants.BNET_EOD)
        )
        self.assertEqual(console.recv_msg(), menu)
        self.assertEqual(console.recv_msg(), response)

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
                **self.director_extra_options,
            )

    def test_login_as_root(self):
        logger = logging.getLogger()

        bareos_password = bareos.bsock.Password(self.director_root_password)
        if bareos.bsock.DirectorConsole.is_tls_psk_available():
            director = bareos.bsock.DirectorConsole(
                address=self.director_address,
                port=self.director_port,
                password=bareos_password,
                **self.director_extra_options,
            )
            whoami = director.call("whoami").decode("utf-8")
            self.assertEqual("root", whoami.rstrip())
        else:
            with self.assertRaises(bareos.exceptions.AuthenticationError):
                bareos.bsock.DirectorConsole(
                    address=self.director_address,
                    port=self.director_port,
                    password=bareos_password,
                    **self.director_extra_options,
                )

    def test_login_as_user(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
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
                    **self.director_extra_options,
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
                    **self.director_extra_options,
                )

    def test_no_autodisplay_command(self):
        """
        The console has no access to the "autodisplay" command.
        However, the initialization of DirectorConsole calls this command.
        However, the error should not be visible to the console.
        """
        logger = logging.getLogger()

        username = "noautodisplaycommand"
        password = "secret"

        bareos_password = bareos.bsock.Password(password)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=bareos_password,
            **self.director_extra_options,
        )

        # get the list of all command
        result = director.call(".help all")
        # logger.debug(str(result))

        # verify, the result contains command. We test for the list command.
        self.assertIn("list", str(result))
        # verify, the result does not contain the autodisplay command.
        self.assertNotIn("autodisplay", str(result))

        # check if the result of 'whoami' only contains the expected result.
        result = director.call("whoami").decode("utf-8")
        logger.debug(str(result))

        self.assertEqual(username, result.rstrip())

    def test_dot_help_accepts_full_as_flag_only(self):
        username = self.get_operator_username(tls=False)
        password = self.get_operator_password(username)
        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        result = director.call(".help full").decode("utf-8")

        self.assertIn(".help", result)

        error_result = director.call(".help full=yes").decode("utf-8")

        self.assertIn("The full option does not take a value.", error_result)

    def test_json_without_json_backend(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )
        result = director.call(".api json").decode("utf-8")
        result = director.call("whoami").decode("utf-8")
        logger.debug(str(result))
        content = json.loads(str(result))
        logger.debug(str(content))
        self.assertEqual(content["result"]["whoami"].rstrip(), username)
