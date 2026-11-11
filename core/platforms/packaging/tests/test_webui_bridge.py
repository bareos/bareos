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
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]


@unittest.skipUnless(shutil.which("rpmspec"), "requires RPM spec expansion")
class WebuiBridgeTest(unittest.TestCase):
    def test_renamed_php_preserves_pre25_profiles_on_first_install(self):
        expanded = subprocess.check_output(
            [
                "rpmspec",
                "--parse",
                "--undefine",
                "fedora",
                "--define",
                "suse_version 1560",
                str(ROOT / "core/platforms/packaging/bareos.spec"),
            ],
            text=True,
            stderr=subprocess.PIPE,
        )
        scripts = [
            re.search(
                rf"(?ms)^%{section} webui(?:-php)?\n(.*?)(?=^%\w|\Z)",
                expanded,
            ).group(1)
            for section in ("pre", "posttrans")
        ]
        for legacy_version in ("24.0.0", "25.1.2", ""):
            for argument in ("1", "2"):
                with self.subTest(version=legacy_version, argument=argument):
                    with tempfile.TemporaryDirectory(
                        prefix="bareos-bridge-profiles-"
                    ) as directory:
                        temp = Path(directory)
                        binary = temp / "bin"
                        binary.mkdir()
                        rpm = binary / "rpm"
                        rpm.write_text(
                            '#!/bin/sh\n[ -n "$LEGACY_VERSION" ] || exit 1\n'
                            'if [ "$#" -gt 2 ]; then printf "%s" "$LEGACY_VERSION"; fi\n'
                        )
                        rpm.chmod(0o755)
                        profile_dir = temp / "etc/bareos-dir.d/profile"
                        profile_dir.mkdir(parents=True)
                        profiles = [
                            profile_dir / f"webui-{name}.conf"
                            for name in ("admin", "readonly")
                        ]
                        for profile in profiles:
                            profile.write_text(f"customized {profile.name}")
                        environment = {
                            **os.environ,
                            "PATH": f"{binary}:{os.environ['PATH']}",
                            "LEGACY_VERSION": legacy_version,
                        }
                        for index in (0, 1, 1):
                            script = (
                                scripts[index]
                                .replace("/etc/bareos", str(temp / "etc"))
                                .replace("/var/log/bareos", str(temp / "logs"))
                            )
                            result = subprocess.run(
                                ["bash", "-c", script, "scriptlet", argument],
                                env=environment,
                                capture_output=True,
                                text=True,
                            )
                            self.assertEqual(result.returncode, 0, result.stderr)
                            if index == 0:
                                for profile in profiles:
                                    backups = list(
                                        profile_dir.glob(
                                            f"{profile.name}.rpmupdate.*.keep"
                                        )
                                    )
                                    if legacy_version == "24.0.0":
                                        self.assertTrue(backups)
                                        # Emulate RPM moving the old configuration.
                                        profile.rename(profile.with_suffix(".rpmsave"))
                                    else:
                                        self.assertEqual(backups, [])
                        for profile in profiles:
                            self.assertEqual(
                                profile.read_text(), f"customized {profile.name}"
                            )
                        self.assertEqual(list(profile_dir.glob("*.keep")), [])


if __name__ == "__main__":
    unittest.main()
