#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2026 Bareos GmbH & Co. KG
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
import unittest

import bareos.bsock
from bareos.bsock.constants import Constants
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.bsock.lowlevel import LowLevel
import bareos.exceptions

import bareos_unittest
from bareos_unittest.tree_browser_console import TreeBrowserConsole


class BareosFdPythonPluginOptions(bareos_unittest.Json):
    def test_visual_plugin_options_saved_when_finishing_selection(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(
            admin, "backup-bareos-fd-pluginoptions", "Full", wait=True
        )
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
        screen = browser.run(
            f"restore jobid={backup} all client=bareos-fd restoreclient=bareos-fd"
        )
        self.assertIn("Focus: Plugin Options", screen)
        for _ in range(10):
            browser.press("down")
        screen = browser.press("enter")
        self.assertIn("Choose option", screen)
        browser.type_text("extraconfig")
        browser.press("enter")
        browser.type_text("visual-value")
        screen = browser.press("enter")
        self.assertIn("extraconfig=visual-value", screen)
        screen = browser.type_text("r")
        self.assertIn("Restore Options", screen)
        self.assertIn("extraconfig=visual-value", screen)
        browser.press("cancel")

    def test_visual_plugin_options_respect_console_acl(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(admin.close)
        backup = self.run_job(
            admin, "backup-bareos-fd-pluginoptions", "Full", wait=True
        )
        self.addCleanup(admin.call, f"delete jobid={backup} yes")
        console = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name="plugin-options-denied",
            password=bareos.bsock.Password("secret"),
            **self.director_extra_options,
        )
        self.addCleanup(console.close)
        browser = TreeBrowserConsole(console, columns=160)
        screen = browser.run(
            f"restore jobid={backup} all client=bareos-fd restoreclient=bareos-fd"
        )
        self.assertIn("Focus: Plugin Options", screen)
        for _ in range(10):
            browser.press("down")
        browser.press("enter")
        browser.type_text("extraconfig")
        browser.press("enter")
        browser.type_text("denied-value")
        browser.press("enter")
        screen = browser.press("tab")
        self.assertIn('No authorization for "PluginOptions"', screen)
        self.assertIn("─ > Plugin Options", screen)
        self.assertIn("extraconfig=denied-value", screen)
        screen = browser.type_text("r")
        self.assertIn('No authorization for "PluginOptions"', screen)
        self.assertIn("─ > Plugin Options", screen)
        self.assertIn("extraconfig=denied-value", screen)
        browser.press("cancel")
        browser.type_text("q")

    def test_without_extra_pluginoptions(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-pluginoptions"

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.get_backup_jobid(director, jobname, level="Full")
        jobid = self.run_restore(
            director,
            client=self.client,
            jobname="RestoreFiles",
            jobid=jobid,
            fileset="PluginOptionsTest",
            wait=True,
        )

    def test_additional_pluginoptions(self):
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-pluginoptions"

        backup_dumpfile = "tmp/backup-pluginoptions.json"
        restore_dumpfile = "tmp/restore-pluginoptions.json"
        try:
            os.remove(backup_dumpfile)
            os.remove(restore_dumpfile)
        except FileNotFoundError:
            pass

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.run_job(
            director,
            jobname,
            level="Full",
            extra="pluginoptions=python:extraconfig=backup:dumpoptions={}".format(
                backup_dumpfile
            ),
            wait=True,
        )
        with open(backup_dumpfile, "r") as f:
            data = json.load(f)
        self.assertEqual(data["filesetoption"], "fileset")
        self.assertEqual(data["extraconfig"], "backup")

        # The plugin option dumpoptions triggers the plugin
        # to store the pluginoptions into a json file.
        jobid = self.run_restore(
            director,
            client=self.client,
            jobname="RestoreFiles",
            jobid=jobid,
            fileset="PluginOptionsTest",
            extra="pluginoptions=python:extraconfig=restore:dumpoptions={}".format(
                restore_dumpfile
            ),
            wait=True,
        )

        with open(restore_dumpfile, "r") as f:
            data = json.load(f)
        self.assertEqual(data["filesetoption"], "fileset")
        self.assertEqual(data["extraconfig"], "restore")

    @unittest.expectedFailure
    def test_overwrite_pluginoptions(self):
        """Overwrites a plugin option
        already defined in the Bareos director fileset configuration.
        However, currently the plugin only sees the original value.
        This can be considered as a bug.
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-pluginoptions"

        backup_dumpfile = "tmp/backup-overwrite-pluginoptions.json"
        restore_dumpfile = "tmp/restore-overwrite-pluginoptions.json"
        try:
            os.remove(backup_dumpfile)
            os.remove(restore_dumpfile)
        except FileNotFoundError:
            pass

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.run_job(
            director,
            jobname,
            level="Full",
            extra="pluginoptions=python:filesetoption=console:dumpoptions={}".format(
                backup_dumpfile
            ),
            wait=True,
        )
        with open(backup_dumpfile, "r") as f:
            data = json.load(f)
        self.assertEqual(data["filesetoption"], "console")

        # The plugin option dumpoptions triggers the plugin
        # to store the pluginoptions into a json file.
        jobid = self.run_restore(
            director,
            client=self.client,
            jobname="RestoreFiles",
            jobid=jobid,
            extra="pluginoptions=python:filesetoption=console:dumpoptions={}".format(
                restore_dumpfile
            ),
            wait=True,
        )

        with open(restore_dumpfile, "r") as f:
            data = json.load(f)
        self.assertEqual(data["filesetoption"], "console")
