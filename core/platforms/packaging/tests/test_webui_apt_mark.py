#!/usr/bin/env python3
#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]


class WebuiAptMarkTest(unittest.TestCase):
    def test_only_bridge_migrations_mark_php_manual(self):
        # On upgrade from a 25.1.3 bridge to Bareos 26, bareos-webui switches
        # from PHP to Vue. Keep PHP installed by marking it manual so apt
        # autoremove cannot remove it; only bridge versions have this package.
        cases = (
            (("configure",), False),
            (("configure", ""), False),
            (("configure", "25.1.3~pre24.0e7742c39-128"), False),
            (("configure", "25.1.3~pre25"), True),
            (("configure", "25.1.3~pre29.48e5e02df-4"), True),
            (("configure", "25.1.3-1"), True),
            (("configure", "26.0.0~pre1613.051640b19-14"), False),
            (("abort-upgrade", "25.1.3-1"), False),
        )
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            log = temp / "apt-mark.log"
            apt_mark = temp / "apt-mark"
            apt_mark.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$TEST_LOG"\n')
            apt_mark.chmod(0o755)
            environment = {
                **os.environ,
                "PATH": f"{temp}:{os.environ['PATH']}",
                "TEST_LOG": str(log),
            }
            for arguments, expected in cases:
                with self.subTest(arguments=arguments):
                    log.unlink(missing_ok=True)
                    result = subprocess.run(
                        ["sh", str(ROOT / "debian/bareos-webui.postinst"), *arguments],
                        env=environment,
                        capture_output=True,
                        text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(log.exists(), expected)
                    if expected:
                        self.assertEqual(log.read_text(), "manual bareos-webui-php\n")

    def test_marking_failure_fails_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            apt_mark = temp / "apt-mark"
            apt_mark.write_text("#!/bin/sh\nexit 42\n")
            apt_mark.chmod(0o755)
            result = subprocess.run(
                [
                    "sh",
                    str(ROOT / "debian/bareos-webui.postinst"),
                    "configure",
                    "25.1.3-1",
                ],
                env={**os.environ, "PATH": f"{temp}:{os.environ['PATH']}"},
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 42, result.stderr)


if __name__ == "__main__":
    unittest.main()
