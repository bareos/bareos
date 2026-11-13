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
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
COMMAND = """#!/bin/sh
name=$(basename "$0")
printf '%s %s\\n' "$name" "$*" >> "$HOOK_LOG"
case "$name $*" in
  "apachectl configtest"|"apache2ctl configtest") exit "$CONFIG_STATUS" ;;
  "systemctl is-active --quiet "*) exit "$SERVICE_STATUS" ;;
esac
"""


class PackageHooksTest(unittest.TestCase):
    def run_hook(self, package, platform, active=True, valid=True):
        if platform == "debian":
            script = (ROOT / "debian" / f"{package}.postinst").read_text()
        else:
            spec = (ROOT / "core/platforms/packaging/bareos.spec").read_text()
            script = spec.split(f"%post -n {package}\n", 1)[1]
            script = script.split("%if 0%{?suse_version}\n", 1)[1]
            suse, other = script.split("%else\n", 1)
            script = suse if platform == "suse" else other.split("%endif", 1)[0]
        with tempfile.TemporaryDirectory(prefix="webui-hooks-") as directory:
            root = Path(directory)
            apache = root / "apache"
            for name in ("ssl.crt", "ssl.key", "vhosts.d"):
                (apache / name).mkdir(parents=True)
            (apache / "ssl.crt/server.crt").write_text("administrator certificate")
            (apache / "ssl.key/server.key").write_text("administrator key")
            sysconfig = root / "sysconfig"
            sysconfig.write_text('APACHE_SERVER_FLAGS=""\n')
            script = script.replace("/etc/sysconfig/apache2", str(sysconfig))
            script = script.replace("/etc/apache2", str(apache))
            before = {str(p): p.read_bytes() for p in apache.rglob("*") if p.is_file()}
            binary = root / "bin"
            binary.mkdir()
            log = root / "calls"
            log.touch()
            for name in (
                "a2enmod",
                "a2enconf",
                "a2ensite",
                "a2enflag",
                "apachectl",
                "apache2ctl",
                "systemctl",
                "openssl",
            ):
                command = binary / name
                command.write_text(COMMAND)
                command.chmod(0o755)
            result = subprocess.run(
                [
                    "sh" if platform == "debian" else "bash",
                    "-c",
                    script,
                    "postinst",
                    "configure",
                ],
                env={
                    **os.environ,
                    "PATH": f"{binary}:{os.environ['PATH']}",
                    "HOOK_LOG": str(log),
                    "CONFIG_STATUS": "0" if valid else "1",
                    "SERVICE_STATUS": "0" if active else "3",
                },
                capture_output=True,
                text=True,
            )
            after = {str(p): p.read_bytes() for p in apache.rglob("*") if p.is_file()}
            return result, log.read_text().splitlines(), before, after

    def test_installation_preserves_tls_and_service_state(self):
        for package in ("bareos-webui-php", "bareos-webui-vue"):
            for platform in ("debian", "suse", "el"):
                for active in (False, True):
                    with self.subTest(
                        package=package, platform=platform, active=active
                    ):
                        result, calls, before, after = self.run_hook(
                            package, platform, active
                        )
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(before, after)
                        self.assertFalse(
                            any(
                                call.startswith(("openssl ", "a2enflag ", "a2ensite "))
                                for call in calls
                            )
                        )
                        self.assertFalse(
                            any(
                                call.startswith("a2enmod ")
                                and "ssl" in call.split()[1:]
                                for call in calls
                            )
                        )
                        self.assertFalse(
                            any(
                                call.startswith(
                                    ("systemctl start ", "systemctl restart ")
                                )
                                for call in calls
                            )
                        )
                        service = "httpd" if platform == "el" else "apache2"
                        self.assertEqual(f"systemctl reload {service}" in calls, active)

    def test_invalid_configuration_prevents_reload(self):
        for package in ("bareos-webui-php", "bareos-webui-vue"):
            for platform in ("debian", "suse", "el"):
                with self.subTest(package=package, platform=platform):
                    result, calls, before, after = self.run_hook(
                        package, platform, valid=False
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(before, after)
                    self.assertFalse(
                        any(call.startswith("systemctl reload ") for call in calls)
                    )


if __name__ == "__main__":
    unittest.main()
