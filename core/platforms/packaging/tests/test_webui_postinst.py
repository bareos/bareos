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
PACKAGES = ("bareos-webui-php", "bareos-webui-vue")
MOCK = """#!/bin/sh
name=$(basename "$0")
printf '%s %s\\n' "$name" "$*" >> "$TEST_LOG"
[ "$name $*" != "$FAIL_COMMAND" ] && [ "$name" != "$FAIL_COMMAND" ] || exit 1
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
    echo generated > "$TEST_APACHE/ssl.crt/existing-host-server.crt"
    echo generated > "$TEST_APACHE/ssl.key/existing-host-server.key"
    echo overwritten > "$TEST_APACHE/ssl.key/existing-host-ca.key"
    ;;
  "apachectl configtest")
    if [ -f "$TEST_APACHE/vhosts.d/_bareos-webui-default-ssl.conf" ]; then
      awk '/SSLCertificate(File|KeyFile)/ { print $2 }' \
        "$TEST_APACHE/vhosts.d/_bareos-webui-default-ssl.conf" |
        while read -r file; do test -s "$file" || exit 1; done || exit 1
    fi
    ;;
esac
if [ "$name" = openssl ]; then exec "$REAL_OPENSSL" "$@"; fi
"""


class WebuiPostinstTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("cmake"), "requires CMake")
    def test_debian_apache_path_does_not_depend_on_installed_directories(self):
        source = (ROOT / "webui-vue/CMakeLists.txt").read_text()
        selection = source.split("# Detect Apache configuration directory\n", 1)[1]
        selection = selection.split("\nconfigure_file(", 1)[0]
        with tempfile.TemporaryDirectory(prefix="bareos-apache-path-") as directory:
            temp = Path(directory)
            script = temp / "test.cmake"
            script.write_text(
                f'set(sysconfdir "{temp}/etc")\n'
                'set(CMAKE_SYSTEM_NAME "Linux")\n'
                "foreach(existing IN ITEMS no yes)\n"
                '  if(existing STREQUAL "yes")\n'
                '    file(MAKE_DIRECTORY "${sysconfdir}/httpd/conf.d")\n'
                "  endif()\n"
                "  foreach(BAREOS_PLATFORM IN ITEMS debian ubuntu univention)\n"
                f"{selection}\n"
                '    if(NOT HTTPD_CONF STREQUAL "${sysconfdir}/apache2/conf-available")\n'
                '      message(FATAL_ERROR "Wrong Apache directory: ${HTTPD_CONF}")\n'
                "    endif()\n"
                "  endforeach()\n"
                "endforeach()\n"
            )
            result = subprocess.run(
                ["cmake", "-P", str(script)], capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 0, result.stderr)

    @unittest.skipUnless(shutil.which("rpmspec"), "requires RPM spec expansion")
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
                rf"(?ms)^%{section} -n bareos-webui-php\n(.*?)(?=^%\w|\Z)",
                expanded,
            ).group(1)
            for section in ("pre", "posttrans")
        ]
        for legacy_version in ("24.0.0", "25.1.3", ""):
            for argument in ("1", "2"):
                with self.subTest(version=legacy_version, argument=argument):
                    with tempfile.TemporaryDirectory(
                        prefix="bareos-profiles-"
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
                            if index == 0 and legacy_version == "24.0.0":
                                for profile in profiles:
                                    self.assertTrue(
                                        list(
                                            profile_dir.glob(
                                                f"{profile.name}.rpmupdate.*.keep"
                                            )
                                        )
                                    )
                                    profile.rename(profile.with_suffix(".rpmsave"))
                        for profile in profiles:
                            self.assertEqual(
                                profile.read_text(), f"customized {profile.name}"
                            )
                        self.assertEqual(list(profile_dir.glob("*.keep")), [])

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
        fallback_certificate=False,
        fallback_key=False,
        hostname_files=False,
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
            for enabled, filename, content in (
                (
                    fallback_certificate,
                    "ssl.crt/bareos-webui.crt",
                    "existing fallback certificate",
                ),
                (fallback_key, "ssl.key/bareos-webui.key", "existing fallback key"),
                (
                    hostname_files,
                    "ssl.key/existing-host-ca.key",
                    "administrator CA key",
                ),
                (
                    hostname_files,
                    "ssl.crt/existing-host-server.crt",
                    "administrator certificate",
                ),
                (
                    hostname_files,
                    "ssl.key/existing-host-server.key",
                    "administrator key",
                ),
            ):
                if enabled:
                    (apache / filename).write_text(content)
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
                "openssl",
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
                "REAL_OPENSSL": shutil.which("openssl"),
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
                        fail="openssl" if failed_generation else "",
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
            self.assertIn("openssl req", calls)
            self.assertNotIn("gensslcert", calls)
            self.assertIn("vhosts.d/_bareos-webui-default-ssl.conf", files)
            self.assertIn("BEGIN CERTIFICATE", files["ssl.crt/bareos-webui.crt"])
            self.assertIn("PRIVATE KEY", files["ssl.key/bareos-webui.key"])

    def test_suse_preserves_hostname_material_when_generating_fallback(self):
        for package in PACKAGES:
            result, calls, files = self.run_hook(
                package, certificate=False, key=False, hostname_files=True
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn("gensslcert", calls)
            self.assertEqual(
                files["ssl.key/existing-host-ca.key"], "administrator CA key"
            )
            self.assertEqual(
                files["ssl.crt/existing-host-server.crt"], "administrator certificate"
            )
            self.assertEqual(
                files["ssl.key/existing-host-server.key"], "administrator key"
            )
            self.assertFalse(
                any(".bareos-webui-tls." in filename for filename in files)
            )

    def test_suse_reuses_complete_fallback_and_preserves_partial_pair(self):
        for package in PACKAGES:
            for complete in (False, True):
                with self.subTest(package=package, complete=complete):
                    result, calls, files = self.run_hook(
                        package,
                        certificate=False,
                        key=False,
                        fallback_certificate=True,
                        fallback_key=complete,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertNotIn("openssl req", calls)
                    self.assertNotIn("gensslcert", calls)
                    self.assertEqual(
                        files["ssl.crt/bareos-webui.crt"],
                        "existing fallback certificate",
                    )
                    self.assertEqual(
                        "vhosts.d/_bareos-webui-default-ssl.conf" in files, complete
                    )
                    if not complete:
                        self.assertIn("Incomplete", result.stderr)

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
