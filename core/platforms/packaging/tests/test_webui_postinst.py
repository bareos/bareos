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

ROOT = Path(__file__).resolve().parents[4]
PACKAGES = ("bareos-webui-php", "bareos-webui-vue")
MOCK = """#!/bin/sh
name=$(basename "$0")
printf '%s %s\\n' "$name" "$*" >> "$TEST_LOG"
[ "$name $*" != "$FAIL_COMMAND" ] || exit 1
case "$name $*" in
  "apachectl -S"|"apache2ctl -S")
    [ "$EXISTING_VHOST" != 1 ] || echo '*:443 existing.conf'
    ;;
  "systemctl is-active --quiet "*)
    [ "$ACTIVE_APACHE" = 1 ] || exit 3
    ;;
  "a2enflag SSL")
    echo 'APACHE_SERVER_FLAGS="SSL"' > "$TEST_SYSCONFIG"
    ;;
  "gensslcert ")
    echo generated > "$TEST_APACHE/ssl.crt/server.crt"
    echo generated > "$TEST_APACHE/ssl.key/server.key"
    ;;
esac
"""


class WebuiPostinstTest(unittest.TestCase):
    def run_hook(
        self,
        package,
        *,
        debian=False,
        action="configure",
        ssl_flag=True,
        certificate=True,
        key=True,
        fail="",
        existing_vhost=False,
        active=True,
    ):
        with tempfile.TemporaryDirectory(prefix="bareos-postinst-") as directory:
            temp = Path(directory)
            apache = temp / "apache"
            for name in ("ssl.crt", "ssl.key", "vhosts.d"):
                (apache / name).mkdir(parents=True)
            cert = apache / "ssl.crt/server.crt"
            keyfile = apache / "ssl.key/server.key"
            if certificate:
                cert.write_text("original certificate")
            if key:
                keyfile.write_text("original key")
            sysconfig = temp / "sysconfig"
            sysconfig.write_text(f'APACHE_SERVER_FLAGS="{"SSL" if ssl_flag else ""}"')
            log = temp / "calls"
            log.touch()
            binary = temp / "bin"
            binary.mkdir()
            for name in (
                "a2enmod",
                "a2enflag",
                "a2enconf",
                "a2ensite",
                "apachectl",
                "apache2ctl",
                "systemctl",
                "gensslcert",
            ):
                command = binary / name
                command.write_text(MOCK)
                command.chmod(0o755)
            if debian:
                script = (ROOT / "debian" / f"{package}.postinst").read_text()
            else:
                spec = (ROOT / "core/platforms/packaging/bareos.spec").read_text()
                script = spec.split(f"%post -n {package}\n", 1)[1]
                script = script.split("%if 0%{?suse_version}\n", 1)[1]
                script = script.split("%else\n", 1)[0]
                script = script.replace("/etc/sysconfig/apache2", str(sysconfig))
                script = script.replace("/etc/apache2", str(apache))
                script = script.replace(
                    "/usr/bin/gensslcert", str(binary / "gensslcert")
                )
            environment = {
                **os.environ,
                "PATH": f"{binary}:{os.environ['PATH']}",
                "TEST_LOG": str(log),
                "TEST_APACHE": str(apache),
                "TEST_SYSCONFIG": str(sysconfig),
                "FAIL_COMMAND": fail,
                "EXISTING_VHOST": str(int(existing_vhost)),
                "ACTIVE_APACHE": str(int(active)),
            }
            result = subprocess.run(
                ["sh" if debian else "bash", "-c", script, "postinst", action],
                env=environment,
                capture_output=True,
                text=True,
            )
            files = {
                str(p.relative_to(apache)): p.read_text()
                for p in apache.rglob("*")
                if p.is_file()
            }
            return result, log.read_text(), files

    def test_debian_only_configures_on_configure(self):
        for package in PACKAGES:
            for action in ("abort-upgrade", "abort-remove", "abort-deconfigure"):
                with self.subTest(package=package, action=action):
                    result, calls, _ = self.run_hook(
                        package, debian=True, action=action
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(calls, "")
            result, calls, _ = self.run_hook(package, debian=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("a2ensite default-ssl", calls)
            self.assertIn("systemctl reload apache2", calls)

    def test_suse_restarts_only_when_enabling_startup_flag(self):
        for package in PACKAGES:
            for enabled in (False, True):
                with self.subTest(package=package, enabled=enabled):
                    result, calls, _ = self.run_hook(package, ssl_flag=enabled)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    action = "reload" if enabled else "restart"
                    self.assertIn(f"systemctl {action} apache2", calls)

    def test_suse_reports_enablement_failures(self):
        for package in PACKAGES:
            for command in ("a2enmod ssl", "a2enflag SSL"):
                with self.subTest(package=package, command=command):
                    result, calls, files = self.run_hook(package, fail=command)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("HTTPS setup skipped", result.stderr)
                    self.assertNotIn("vhosts.d/_bareos-webui-default-ssl.conf", files)
                    self.assertIn("apachectl configtest", calls)
                    self.assertIn("systemctl reload apache2", calls)

    def test_suse_skips_bad_certificate_but_applies_valid_config(self):
        for package in PACKAGES:
            for failed_generation in (False, True):
                with self.subTest(package=package, failed_generation=failed_generation):
                    result, calls, files = self.run_hook(
                        package,
                        certificate=not failed_generation,
                        key=False,
                        fail="gensslcert " if failed_generation else "",
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertTrue(result.stderr)
                    self.assertNotIn("vhosts.d/_bareos-webui-default-ssl.conf", files)
                    self.assertIn("systemctl reload apache2", calls)
                    if not failed_generation:
                        self.assertEqual(
                            files["ssl.crt/server.crt"], "original certificate"
                        )
                        self.assertNotIn("gensslcert", calls)

    def test_suse_generates_only_when_both_files_absent(self):
        for package in PACKAGES:
            result, calls, files = self.run_hook(package, certificate=False, key=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("gensslcert", calls)
            self.assertIn("vhosts.d/_bareos-webui-default-ssl.conf", files)

    def test_existing_tls_is_preserved(self):
        for package in PACKAGES:
            for debian in (False, True):
                with self.subTest(package=package, debian=debian):
                    result, calls, files = self.run_hook(
                        package,
                        debian=debian,
                        existing_vhost=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertNotIn("gensslcert", calls)
                    self.assertNotIn("a2ensite", calls)
                    self.assertEqual(
                        files["ssl.crt/server.crt"], "original certificate"
                    )
                    self.assertNotIn("vhosts.d/_bareos-webui-default-ssl.conf", files)

    def test_suse_does_not_reload_invalid_config_or_start_stopped_apache(self):
        for package in PACKAGES:
            result, calls, _ = self.run_hook(package, fail="apachectl configtest")
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn("systemctl reload", calls)
            self.assertNotIn("systemctl restart", calls)
            result, calls, _ = self.run_hook(package, active=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn("systemctl reload", calls)
            self.assertNotIn("systemctl restart", calls)


if __name__ == "__main__":
    unittest.main()
