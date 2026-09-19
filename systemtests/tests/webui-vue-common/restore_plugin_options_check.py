#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

"""
Drives the interactive, split-screen restore tree browser and Plugin
Options editor (core/src/dird/ua_tree_browser.cc) against the synthetic
per-plugin fixture jobs created by testrunner-create-backup, using
bareos_unittest.tree_browser_console.TreeBrowserConsole.

This only exercises restore *selection*: it stops before the restore job
is actually started, matching the fixture's documented purpose (the
fixture jobs do not carry real plugin data, only a rewritten
FileSetText so hint resolution behaves like the real plugin would).
"""

import os
import sys

import bareos.bsock

from bareos_unittest.tree_browser_console import TreeBrowserConsole

# One representative case per plugin-hint resolution path (module_name=
# alias match, raw-loader-name fallback match, and the generic "python"
# hint), to keep this smoke test fast while still covering the different
# ways ResolvePluginRestoreHint() can succeed. Expected text is the
# hint's human-readable display_name, shown in the "p Hints" panel (see
# core/src/dird/restore_plugin_hints.cc).
CASES = [
    ("PluginOptionsTest-qumulo", "Qumulo by Yuzuy"),
    ("PluginOptionsTest-incus", "Incus"),
    ("PluginOptionsTest-tasks-oracle", "Tasks Oracle"),
    ("PluginOptionsTest-python", "Python plugin wrapper"),
]


def fail(message):
    print("FAILED: {}".format(message))
    sys.exit(1)


def connect():
    username = os.environ["BAREOS_WEBUI_USERNAME"]
    password = bareos.bsock.Password(os.environ["BAREOS_WEBUI_PASSWORD"])
    port = int(os.environ["BAREOS_DIRECTOR_PORT"])
    return bareos.bsock.DirectorConsole(
        address="127.0.0.1", port=port, name=username, password=password
    )


def connect_unrestricted():
    return bareos.bsock.DirectorConsole(
        address="127.0.0.1",
        port=int(os.environ["BAREOS_DIRECTOR_PORT"]),
        password=bareos.bsock.Password(os.environ["dir_password"]),
    )


def check_restore_menu_output_pause(director):
    tb = TreeBrowserConsole(director, lines=45, columns=140)
    screen = tb.run("restore")
    if "Select item" not in screen:
        fail("restore did not enter the selection menu: {}".format(screen))

    screen = tb.press("select:13")
    if "Press Enter to return to the restore menu:" not in screen:
        fail("last-jobs output was redrawn before it could be read: {}".format(screen))
    if "| jobid | client" not in screen:
        fail("last-jobs table was not displayed: {}".format(screen))

    screen = tb.submit_line()
    if "Select item" not in screen:
        fail("restore menu did not return after acknowledgement: {}".format(screen))


def check_one_fileset(director, fileset, expected_hint_text):
    tb = TreeBrowserConsole(director, lines=45, columns=140)

    screen = tb.run("restore client=bareos-fd fileset={}".format(fileset))
    if "Select item" not in screen:
        fail(
            "restore did not enter the interactive selection menu for "
            "{}: {}".format(fileset, screen)
        )

    screen = tb.press("select:1")  # "Select a FileSet@Client combination ..."
    if "FileSet@Client" not in screen:
        fail("did not get the FileSet@Client picker for {}: {}".format(fileset, screen))

    # The picker supports incremental text filtering; narrow down to our
    # FileSet by typing its name, then confirm the (only) remaining match.
    screen = tb.type_text(fileset)
    screen = tb.press("enter")

    if "Restore selection" not in screen:
        fail(
            "did not enter the split-screen restore selection for "
            "{}: {}".format(fileset, screen)
        )
    if "Plugin Options" not in screen:
        fail(
            "split screen is missing the Plugin Options pane for "
            "{}: {}".format(fileset, screen)
        )
    if "Resulting Plugin Options:" not in screen:
        fail(
            "plugin options string preview is missing for "
            "{}: {}".format(fileset, screen)
        )

    # A detected plugin backup already starts with focus on the Plugin
    # Options pane (see ua_tree_browser.cc), so we can edit it right away:
    # add one option, confirm the row, then leave without starting the
    # restore.
    if "Focus: Plugin Options" not in screen:
        fail(
            "expected the Plugin Options pane to have initial focus for "
            "{}: {}".format(fileset, screen)
        )

    # Confirm the resolved hint via the "p Hints" panel (only reachable
    # while the Files pane has focus), then return to editing options.
    screen = tb.press("tab")  # focus Files
    screen = tb.type_text("p")  # open the plugin hints panel
    if expected_hint_text not in screen:
        fail(
            "Hints panel did not advertise the expected hint '{}' for "
            "{}: {}".format(expected_hint_text, fileset, screen)
        )
    screen = tb.type_text("o")  # close hints, refocus Plugin Options pane
    if "Focus: Plugin Options" not in screen:
        fail(
            "closing the hints panel did not refocus the Plugin Options "
            "pane for {}: {}".format(fileset, screen)
        )

    # Move to the final "+ add option" row. Extra Down presses clamp at
    # the bottom, regardless of whether module_name was auto-prefilled.
    for _ in range(10):
        screen = tb.press("down")
    screen = tb.press("enter")
    if "Choose option" not in screen:
        fail("known-option chooser did not open for {}: {}".format(fileset, screen))

    # Typing while the chooser is open retains free-form custom options.
    screen = tb.type_text("module_path")
    screen = tb.press("enter")
    screen = tb.type_text("/opt/{}-demo".format(fileset))
    screen = tb.press("enter")
    if "module_path = /opt/{}-demo".format(fileset) not in screen:
        fail(
            "added plugin option is missing from the pane for "
            "{}: {}".format(fileset, screen)
        )
    if "module_path=/opt/{}-demo".format(fileset) not in screen:
        fail(
            "added plugin option is missing from the string preview for "
            "{}: {}".format(fileset, screen)
        )

    # Tab switches focus over to the Files pane and back, without losing
    # the option we just added.
    screen = tb.press("tab")
    if "Focus: Files" not in screen:
        fail(
            "Tab did not move focus to the Files pane for {}: {}".format(
                fileset, screen
            )
        )
    screen = tb.press("tab")
    if "Focus: Plugin Options" not in screen:
        fail(
            "Tab did not move focus back to the Plugin Options pane for "
            "{}: {}".format(fileset, screen)
        )
    if "module_path = /opt/{}-demo".format(fileset) not in screen:
        fail(
            "plugin option was lost after tabbing away and back for "
            "{}: {}".format(fileset, screen)
        )

    # Select the first documented option from the chooser and give it a
    # value, proving that known options no longer have to be typed manually.
    for _ in range(10):
        screen = tb.press("down")
    screen = tb.press("enter")
    if "Choose option" not in screen or (
        "[Required]" not in screen
        and "[Optional]" not in screen
        and "[Already in FileSet]" not in screen
    ):
        fail("grouped known options are missing for {}: {}".format(fileset, screen))
    screen = tb.press("enter")
    screen = tb.type_text("selected-value")
    screen = tb.press("enter")
    if "selected-value" not in screen:
        fail("selected known option was not added for {}: {}".format(fileset, screen))

    # Leave the browser without marking any files or confirming a
    # restore: this must not queue a restore job.
    screen = tb.press("cancel")
    screen = tb.type_text("q")
    if "Job queued" in screen:
        fail(
            "quitting the tree browser unexpectedly started a restore "
            "job for {}: {}".format(fileset, screen)
        )

    print("OK: {} -> {}".format(fileset, expected_hint_text))


def main():
    director = connect()
    check_restore_menu_output_pause(connect_unrestricted())
    for fileset, expected_hint_text in CASES:
        check_one_fileset(director, fileset, expected_hint_text)
    print("All restore-selection Plugin Options editor checks passed.")


if __name__ == "__main__":
    main()
