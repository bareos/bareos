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
Drives the Director's interactive, raw-mode restore tree browser (see
core/src/dird/ua_tree_browser.cc) over the plain Bareos console wire
protocol, without needing a real pseudo-terminal.

Background: a real interactive bconsole session negotiates raw-mode
selection screens purely with plain console commands:
  - console.cc's SendTerminalSize() just sends a normal console command,
    ".terminalsize <lines> <columns> [color]" (see DotTerminalsizeCmd in
    ua_cmds.cc). Sending this once is exactly what flips
    ua->terminal_height > 0 and satisfies TreeBrowserSupported()'s gate --
    no real tty/ioctl is required on the Director side at all.
  - Every keypress bconsole's ReadSelectionInput() reads locally is sent
    across the wire as a small plain-text command, e.g. "key:tab",
    "key:up"/"key:down"/"key:left"/"key:right", "key:enter",
    "key:backspace", "key:space", "key:text:<char>", "key:cancel",
    "key:select:<value>", or "resize:<rows>:<cols>" -- handled as
    ordinary input by ua_select.cc's SelectionInput handler, exactly like
    any other console command.

So this class only needs a normal bareos.bsock.DirectorConsole connection
(the same one python-bareos systemtests already use) to fully drive the
interactive tree browser and Plugin Options editor: no PTY/forkpty/tty
emulation is required.
"""


class TreeBrowserConsole:
    """Thin wrapper around a bareos.bsock.DirectorConsole connection that
    drives the interactive split-screen restore tree browser.
    """

    def __init__(self, director_console, lines=40, columns=120, color=False):
        """
        Args:
            director_console: an already-connected
                bareos.bsock.DirectorConsole (or compatible) instance.
            lines, columns: virtual terminal size to advertise, sized to
                comfortably fit the split-screen tree browser + Plugin
                Options pane without scrolling.
            color: whether to request ANSI color output (kept off by
                default so screen text stays simple to assert on).
        """
        self._console = director_console
        self.last_screen = ""
        self.send_terminal_size(lines, columns, color)

    def _call(self, command):
        result = self._console.call(command)
        self.last_screen = result.decode("utf-8", errors="replace")
        return self.last_screen

    def send_terminal_size(self, lines, columns, color=False):
        """Send the same ".terminalsize" handshake a real interactive
        bconsole sends once per connection (and again on resize), which is
        what allows the Director to enable the raw-mode selection UI.
        """
        cmd = ".terminalsize {} {}".format(lines, columns)
        if color:
            cmd += " color"
        return self._call(cmd)

    def run(self, command):
        """Send a regular console command (e.g. "restore"), returning the
        resulting screen text.
        """
        return self._call(command)

    def press(self, key_event):
        """Send one raw-mode key event, exactly as a real interactive
        bconsole would after a single keypress.

        Args:
            key_event (str): one of "up", "down", "left", "right", "tab",
                "enter", "backspace", "space", "cancel", or a value
                prefixed with "text:" (each character sent individually,
                as the client does) or "select:<value>".
        """
        if key_event.startswith("text:"):
            screen = self.last_screen
            for character in key_event[len("text:") :]:
                screen = self._call("key:text:" + character)
            return screen
        return self._call("key:" + key_event)

    def submit_line(self, text=""):
        """Submit one ordinary line-input response outside raw selection."""
        return self._call(text)

    def type_text(self, text):
        """Convenience helper: send one "key:text:<char>" command per
        character, matching how a real interactive client sends typed
        text one keystroke at a time.
        """
        return self.press("text:" + text)

    def wait_for_text(self, substring, attempts=1):
        """Return True once `substring` appears in the most recently
        received screen text. `attempts` > 1 allows a caller to poll
        across a few additional no-op resize refreshes if needed, though
        in practice each press()/run() call already blocks until the
        Director's response is fully received.
        """
        for _ in range(max(1, attempts)):
            if substring in self.last_screen:
                return True
        return substring in self.last_screen
