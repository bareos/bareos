#!/usr/bin/env python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
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

"""Real Director regression tests; no browser, Vue bundle or Python packages."""

import base64
import contextlib
import hashlib
import http.client
import ipaddress
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import unittest
from unittest import mock


ROOT = Path.cwd()
WORK = ROOT / "working"
DEFAULT_CONFIG = ROOT / "config/default.ini"
DIRECTOR_PORT = int(os.environ["BAREOS_DIRECTOR_PORT"])
PROXY_PORT = int(os.environ["BAREOS_WEBUI_PROXY_PORT"])
PROXY = os.environ["BAREOS_WEBUI_PROXY_BINARY"]
STARTUP_PROXY = (ROOT / "startup-binary").read_text().strip()
TIMEOUT = 20


def require_free_port(host, port):
    family = socket.AF_INET6 if ":" in host else socket.AF_INET
    with socket.socket(family) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.bind((host, port))
        except OSError as error:
            raise RuntimeError(
                f"Port conflict at {host}:{port}; refusing to stop its owner: {error}"
            ) from error


def non_loopback_address():
    ip = shutil.which("ip")
    if ip:
        result = subprocess.run(
            [ip, "-j", "-4", "address", "show", "scope", "global"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=TIMEOUT,
        )
        candidates = [
            address["local"]
            for interface in json.loads(result.stdout)
            for address in interface.get("addr_info", [])
            if address["family"] == "inet"
        ]
    elif shutil.which("ifconfig"):
        result = subprocess.run(
            [shutil.which("ifconfig"), "-a"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=TIMEOUT,
        )
        candidates = re.findall(r"^\s+inet\s+(?:addr:)?(\S+)", result.stdout, re.M)
    else:
        candidates = [
            entry[4][0]
            for entry in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)
        ]
    for candidate in candidates:
        address = ipaddress.ip_address(candidate)
        if not address.is_loopback and not address.is_unspecified:
            try:
                require_free_port(candidate, DIRECTOR_PORT)
                return candidate
            except RuntimeError:
                continue
    raise RuntimeError("Prerequisite unavailable: no bindable non-loopback local IPv4")


@contextlib.contextmanager
def running_process(arguments, name, host, port, env=None):
    log_path = ROOT / "log" / f"{name}.out"
    with log_path.open("wb") as log:
        process = subprocess.Popen(arguments, stdout=log, stderr=log, env=env)
        try:
            deadline = time.monotonic() + TIMEOUT
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f"{name} exited with {process.returncode}")
                try:
                    with socket.create_connection((host, port), timeout=0.2):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                raise RuntimeError(f"{name} did not become ready at {host}:{port}")
            yield process
            if process.poll() is not None:
                raise RuntimeError(f"{name} exited unexpectedly: {process.returncode}")
        except Exception:
            log.flush()
            print(log_path.read_text(errors="replace"), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


def director_config(name, address, port, pam=False):
    destination = WORK / name
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(ROOT / "etc/bareos", destination)
    resources = destination / "bareos-dir.d"
    config = resources / "director/bareos-dir.conf"
    contents = re.sub(r"Port = \d+", f"Port = {port}", config.read_text())
    contents = contents.rstrip()
    if contents.endswith("}"):
        contents = contents[:-1]
    contents += f"\nAddress = {address}\n}}\n"
    config.write_text(contents)
    # Prove resource ACL filtering, not merely the Console's broad Profile.
    (resources / "job/ProxyForbiddenJob.conf").write_text(
        "Job {\nName = ProxyForbiddenJob\nJobDefs = DefaultJob\n}\n"
    )
    if pam:
        pam_console = resources / "console/PamConsole.conf"
        pam_console.write_text(
            pam_console.read_text().replace(
                'Password = "secret"', 'Password = "proxy-console#;secret"'
            )
        )
        (resources / "console/PamConsoleOther.conf").write_text(
            pam_console.read_text()
            .replace("PamConsole", "PamConsoleOther")
            .replace("proxy-console#;secret", "other-console-secret")
        )
        (resources / "profile/ProxyRestricted.conf").write_text(
            "Profile {\nName = ProxyRestricted\n"
            "Command ACL = whoami, .api, .jobs\n"
            "Job ACL = backup-bareos-fd\n}\n"
        )
        (resources / "user/user1.conf").write_text(
            "User {\nName = user1\nProfile = ProxyRestricted\n}\n"
        )
    return destination


def director_environment():
    env = os.environ.copy()
    env["PAM_WRAPPER"] = "1"
    env["PAM_WRAPPER_SERVICE_DIR"] = str(ROOT / "etc/pam.d")
    libraries = os.environ["PAM_WRAPPER_LIBRARIES"]
    # ASan, when linked, must precede pam_wrapper in LD_PRELOAD.
    linked = subprocess.run(
        ["ldd", os.environ["BAREOS_DIRECTOR_BINARY"]],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        timeout=TIMEOUT,
    )
    asan = re.search(r"libasan\.so\S* => (\S+)", linked.stdout)
    if asan:
        libraries = asan[1] + ":" + libraries
    env["LD_PRELOAD"] = libraries
    return env


@contextlib.contextmanager
def director(name, address, port, pam=False):
    require_free_port(address, port)
    config = director_config(name, address, port, pam)
    with running_process(
        [os.environ["BAREOS_DIRECTOR_BINARY"], "-f", "-c", str(config)],
        name,
        address,
        port,
        director_environment() if pam else None,
    ):
        yield


def proxy_config(path, address, director_port, proxy_port, selector, cleartext=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    os.chown(str(path.parent), -1, os.getegid())
    path.parent.chmod(0o750)
    path.write_text(
        f"[listen]\naddress = 127.0.0.1\nport = {proxy_port}\n\n"
        f"[{selector}]\ndirector_name = bareos-dir\n"
        f"address = {address}\nport = {director_port}\n"
        f"tls_psk_disable = {'yes' if cleartext else 'no'}\n"
    )
    os.chown(str(path), -1, os.getegid())
    path.chmod(0o640)


def http_request(port, method, path, body=None, cookie=None):
    connection = http.client.HTTPConnection("localhost", port, timeout=TIMEOUT)
    headers = {"Content-Type": "application/json"}
    if cookie:
        headers["Cookie"] = cookie
    try:
        connection.request(method, path, json.dumps(body) if body else None, headers)
        response = connection.getresponse()
        return response.status, dict(response.getheaders()), response.read().decode()
    finally:
        connection.close()


class WebSocket:
    """Small bounded RFC 6455 client using only the Python standard library."""

    def __init__(self, port, cookie):
        self.socket = socket.create_connection(("localhost", port), timeout=TIMEOUT)
        self.reader = self.socket.makefile("rb")
        key = base64.b64encode(os.urandom(16)).decode()
        self.socket.sendall(
            (
                "GET /ws HTTP/1.1\r\n"
                f"Host: localhost:{port}\r\n"
                "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
                f"Cookie: {cookie}\r\n\r\n"
            ).encode()
        )
        status = self.reader.readline()
        headers = {}
        while True:
            line = self.reader.readline()
            if not line or line == b"\r\n":
                break
            field, value = line.decode().split(":", 1)
            headers[field.lower()] = value.strip()
        accept = base64.b64encode(
            hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
            ).digest()
        ).decode()
        if b" 101 " not in status or headers.get("sec-websocket-accept") != accept:
            self.close()
            raise AssertionError(f"WebSocket upgrade failed: {status!r}")

    def close(self):
        self.reader.close()
        self.socket.close()

    def send(self, message):
        payload = json.dumps(message).encode()
        mask = os.urandom(4)
        size = len(payload)
        header = (
            bytes([0x81, 0x80 | size])
            if size < 126
            else bytes([0x81, 0x80 | 126]) + struct.pack("!H", size)
        )
        self.socket.sendall(
            header
            + mask
            + bytes(value ^ mask[index % 4] for index, value in enumerate(payload))
        )

    def read_exactly(self, count):
        result = self.reader.read(count)
        if len(result) != count:
            raise AssertionError("Premature WebSocket close")
        return result

    def receive(self):
        first, second = self.read_exactly(2)
        if first != 0x81 or second & 0x80:
            raise AssertionError(f"Unexpected WebSocket frame: {first}, {second}")
        size = second & 0x7F
        if size == 126:
            size = struct.unpack("!H", self.read_exactly(2))[0]
        elif size == 127:
            size = struct.unpack("!Q", self.read_exactly(8))[0]
        if size > 1024 * 1024:
            raise AssertionError("Oversized WebSocket response")
        return json.loads(self.read_exactly(size))

    def command(self, command):
        self.send({"type": "command", "id": command, "command": command})
        for _ in range(20):
            message = self.receive()
            if message["type"] in ("raw_response", "response", "error"):
                return message
        raise AssertionError("No console command response")


class ProxyTest(unittest.TestCase):
    def login(self, port, selector, credentials=None):
        credentials = credentials or {"username": "admin", "password": "secret"}
        status, headers, body = http_request(
            port, "POST", f"/api/session/directors/{selector}/login", credentials
        )
        self.assertEqual(status, 200, body)
        cookie_header = headers["Set-Cookie"]
        self.assertIn("HttpOnly", cookie_header)
        cookie = cookie_header.split(";", 1)[0]
        self.assertNotIn(credentials["password"], cookie)
        for secret_field in ("password", "pam_username", "pam_password"):
            self.assertNotIn(f'"{secret_field}"', body)
        self.assertNotIn(credentials["password"], body)
        status, _, info = http_request(port, "GET", "/api/session", cookie=cookie)
        self.assertEqual(status, 200, info)
        self.assertIn(
            {"director": selector, "username": credentials["username"]},
            json.loads(info)["authenticatedDirectors"],
        )
        return cookie

    @contextlib.contextmanager
    def session(self, port, selector, cookie, mode="raw"):
        with contextlib.closing(WebSocket(port, cookie)) as websocket:
            websocket.send({"type": "session", "director": selector, "mode": mode})
            authenticated = websocket.receive()
            self.assertEqual(authenticated["type"], "auth_ok", authenticated)
            self.assertEqual(authenticated["director"], "bareos-dir")
            self.assertEqual(authenticated["mode"], mode)
            self.assertEqual(authenticated["transport"], "TLS-PSK")
            yield websocket

    def round_trip(self, port, selector):
        status, _, body = http_request(port, "GET", "/api/directors")
        self.assertEqual(status, 200, body)
        self.assertEqual(json.loads(body)["directors"], [selector])
        cookie = self.login(port, selector)
        with self.session(port, selector, cookie) as websocket:
            response = websocket.command("whoami")
            self.assertEqual(response["type"], "raw_response", response)
            self.assertEqual(response["text"].strip().splitlines()[0], "admin")
        return cookie

    def test_configured(self):
        address = non_loopback_address()
        require_free_port("127.0.0.1", PROXY_PORT)
        config = WORK / "configured.ini"
        selector = "non-loopback-fixture"
        proxy_config(config, address, DIRECTOR_PORT, PROXY_PORT, selector)
        config.chmod(0o644)
        try:
            result = subprocess.run(
                [PROXY, "--config", str(config)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=TIMEOUT,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unsafe configuration file", result.stderr)
            self.assertIn(str(config), result.stderr)
            self.assertIn("mode 0644", result.stderr)
            self.assertIn("chmod 0640", result.stderr)
        finally:
            config.chmod(0o640)
        with director("configured-director", address, DIRECTOR_PORT):
            # The Director is bound specifically to the interface address:
            # an accidental localhost fallback must not be able to succeed.
            with socket.socket() as sock:
                self.assertNotEqual(sock.connect_ex(("127.0.0.1", DIRECTOR_PORT)), 0)
            with running_process(
                [PROXY, "--config", str(config)],
                "configured-proxy",
                "127.0.0.1",
                PROXY_PORT,
            ):
                self.round_trip(PROXY_PORT, selector)

    def test_defaults(self):
        DEFAULT_CONFIG.parent.mkdir(parents=True, exist_ok=True)
        os.chown(str(DEFAULT_CONFIG.parent), -1, os.getegid())
        DEFAULT_CONFIG.parent.chmod(0o750)
        if DEFAULT_CONFIG.exists():
            self.fail(f"Unexpected stale fixture configuration: {DEFAULT_CONFIG}")
        for port in (9101, 9104):
            for address in {
                entry[4][0] for entry in socket.getaddrinfo("localhost", port)
            }:
                require_free_port(address, port)
        require_free_port("127.0.0.1", PROXY_PORT)
        config = WORK / "explicit.ini"
        proxy_config(config, "127.0.0.1", DIRECTOR_PORT, PROXY_PORT, "implicit-fixture")
        try:
            DEFAULT_CONFIG.write_text(config.read_text())
            os.chown(str(DEFAULT_CONFIG), -1, os.getegid())
            DEFAULT_CONFIG.chmod(0o640)
            DEFAULT_CONFIG.parent.chmod(0o755)
            try:
                result = subprocess.run(
                    [STARTUP_PROXY],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    timeout=TIMEOUT,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("unsafe configuration directory", result.stderr)
                self.assertIn("chmod 0750", result.stderr)
            finally:
                DEFAULT_CONFIG.parent.chmod(0o750)
            with director("implicit-director", "127.0.0.1", DIRECTOR_PORT):
                with running_process(
                    [STARTUP_PROXY], "implicit-proxy", "127.0.0.1", PROXY_PORT
                ):
                    self.round_trip(PROXY_PORT, "implicit-fixture")
            DEFAULT_CONFIG.unlink()
            with director("default-director", "127.0.0.1", 9101):
                with running_process(
                    [STARTUP_PROXY], "default-proxy", "localhost", 9104
                ):
                    self.round_trip(9104, "bareos-dir")

            # A usable implicit config makes accidental explicit-file fallback
            # especially visible: these invocations must exit, not start it.
            DEFAULT_CONFIG.write_text(config.read_text())
            os.chown(str(DEFAULT_CONFIG), -1, os.getegid())
            DEFAULT_CONFIG.chmod(0o640)
            invalid = WORK / "invalid.ini"
            invalid.write_text("[listen]\nport = not-a-port\n")
            os.chown(str(invalid), -1, os.getegid())
            invalid.chmod(0o640)
            missing = WORK / "does-not-exist.ini"
            self.assertFalse(missing.exists())
            for path, diagnostic in ((missing, str(missing)), (invalid, "integer")):
                with self.subTest(path=path):
                    result = subprocess.run(
                        [STARTUP_PROXY, "--config", str(path)],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        universal_newlines=True,
                        timeout=5,
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stdout + result.stderr)
        finally:
            try:
                DEFAULT_CONFIG.unlink()
            except FileNotFoundError:
                pass

    def test_pam(self):
        require_free_port("127.0.0.1", PROXY_PORT)
        config = WORK / "pam.ini"
        proxy_config(config, "127.0.0.1", DIRECTOR_PORT, PROXY_PORT, "pam-fixture")
        with config.open("a") as output:
            output.write(
                "pam_console_name = PamConsole\n"
                'pam_console_password = "proxy-console#;secret"\n'
                "\n[pam-reuse]\ndirector_name = bareos-dir\n"
                f"address = 127.0.0.1\nport = {DIRECTOR_PORT}\n"
                "tls_psk_disable = no\npam_console_name = PamConsoleOther\n"
                "pam_console_password = other-console-secret\n"
                "\n[ordinary-fixture]\ndirector_name = bareos-dir\n"
                f"address = 127.0.0.1\nport = {DIRECTOR_PORT}\n"
                "\n[pam-not-required]\ndirector_name = bareos-dir\n"
                f"address = 127.0.0.1\nport = {DIRECTOR_PORT}\n"
                "pam_console_name = admin\npam_console_password = secret\n"
            )
        config.chmod(0o600)
        WORK.chmod(0o750)
        for unsafe_path, unsafe_mode, safe_mode in (
            (config, 0o644, 0o600),
            (WORK, 0o755, 0o750),
        ):
            unsafe_path.chmod(unsafe_mode)
            try:
                result = subprocess.run(
                    [PROXY, "--config", str(config)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    timeout=TIMEOUT,
                )
                self.assertNotEqual(result.returncode, 0)
                diagnostic = result.stdout + result.stderr
                self.assertIn("unsafe configuration", diagnostic)
                self.assertIn(str(unsafe_path), diagnostic)
                self.assertIn(f"mode {unsafe_mode:04o}", diagnostic)
                recommended_mode = 0o640 if unsafe_path == config else 0o750
                self.assertIn(f"chmod {recommended_mode:04o}", diagnostic)
                self.assertIn("uid=", diagnostic)
                self.assertIn("gid=", diagnostic)
                self.assertNotIn("proxy-console#;secret", diagnostic)
            finally:
                unsafe_path.chmod(safe_mode)
        with director("pam-director", "127.0.0.1", DIRECTOR_PORT, pam=True):
            with running_process(
                [PROXY, "--config", str(config)], "pam-proxy", "127.0.0.1", PROXY_PORT
            ):
                # The unrestricted Console sees the resource hidden from User.
                status, _, targets = http_request(PROXY_PORT, "GET", "/api/directors")
                self.assertEqual(status, 200, targets)
                self.assertNotIn("PamConsole", targets)
                self.assertNotIn("proxy-console#;secret", targets)
                admin_cookie = self.login(PROXY_PORT, "ordinary-fixture")
                with self.session(
                    PROXY_PORT, "ordinary-fixture", admin_cookie
                ) as websocket:
                    self.assertIn(
                        "ProxyForbiddenJob", websocket.command(".jobs")["text"]
                    )
                credentials = {
                    "username": "user1",
                    "password": "user1",
                }
                # This is exactly the two-field payload sent by the Vue login.
                status, headers, body = http_request(
                    PROXY_PORT,
                    "POST",
                    "/api/session/directors/pam-fixture/login",
                    credentials,
                )
                self.assertEqual(status, 200, body)
                self.assertIn(
                    {"director": "pam-fixture", "username": "user1"},
                    json.loads(body)["authenticatedDirectors"],
                )
                self.assertNotIn("PamConsole", body)
                self.assertNotIn("proxy-console#;secret", body)
                self.assertNotIn('"password"', body)
                cookie = headers["Set-Cookie"].split(";", 1)[0]
                # Both modes authenticate afresh with retained PAM data.
                for mode in ("raw", "json"):
                    with self.session(
                        PROXY_PORT, "pam-fixture", cookie, mode
                    ) as websocket:
                        identity = websocket.command("whoami")
                        jobs_response = websocket.command(".jobs")
                        if mode == "json":
                            self.assertEqual(identity["data"]["whoami"], "user1")
                            jobs = [
                                job["name"] for job in jobs_response["data"]["jobs"]
                            ]
                        else:
                            self.assertEqual(
                                identity["text"].strip().splitlines()[0], "user1"
                            )
                            jobs = jobs_response["text"].splitlines()
                        self.assertIn("backup-bareos-fd", jobs)
                        self.assertNotIn("ProxyForbiddenJob", jobs)
                status, _, reused = http_request(
                    PROXY_PORT,
                    "POST",
                    "/api/session/reuse",
                    {"sourceDirector": "pam-fixture", "directors": ["pam-reuse"]},
                    cookie,
                )
                self.assertEqual(status, 200, reused)
                self.assertEqual(
                    json.loads(reused)["results"],
                    [{"director": "pam-reuse", "status": "authenticated"}],
                )
                self.assertNotIn("PamConsole", reused)
                self.assertNotIn("other-console-secret", reused)
                self.assertNotIn('"password"', reused)
                with self.session(PROXY_PORT, "pam-reuse", cookie, "json") as websocket:
                    self.assertEqual(
                        websocket.command("whoami")["data"]["whoami"], "user1"
                    )
                    self.assertEqual(
                        [
                            job["name"]
                            for job in websocket.command(".jobs")["data"]["jobs"]
                        ],
                        ["backup-bareos-fd"],
                    )
                for invalid in (
                    {**credentials, "password": "wrong-pam-password"},
                    {"username": "nonexistent-user", "password": "user1"},
                ):
                    with self.subTest(credentials=sorted(invalid)):
                        self.rejected_login(PROXY_PORT, "pam-fixture", invalid)
                # A misconfigured non-PAM Console must not bypass user authentication.
                self.rejected_login(PROXY_PORT, "pam-not-required", credentials)
            proxy_config(
                config, "127.0.0.1", DIRECTOR_PORT, PROXY_PORT, "cleartext", True
            )
            with config.open("a") as output:
                output.write(
                    "pam_console_name = PamConsole-notls\n"
                    "pam_console_password = secret\n"
                )
            result = subprocess.run(
                [PROXY, "--config", str(config)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                timeout=TIMEOUT,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("PAM requires TLS-PSK", result.stdout + result.stderr)

    def rejected_login(self, port, selector, credentials):
        status, headers, body = http_request(
            port, "POST", f"/api/session/directors/{selector}/login", credentials
        )
        self.assertEqual(status, 401, body)
        self.assertNotIn("Set-Cookie", headers)
        self.assertNotIn("wrong-pam-password", body)
        status, _, _ = http_request(port, "GET", "/api/session")
        self.assertEqual(status, 401)

    def test_interface_discovery(self):
        output = (
            "lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST>\n"
            "\tinet 127.0.0.1 netmask 0xff000000\n"
            "em0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST>\n"
            "\tinet 192.0.2.10 netmask 0xffffff00 broadcast 192.0.2.255\n"
            "\tinet 192.0.2.11 netmask 0xffffff00 broadcast 192.0.2.255\n"
        )
        with mock.patch.object(
            shutil,
            "which",
            side_effect=lambda name: "/sbin/ifconfig" if name == "ifconfig" else None,
        ), mock.patch.object(
            subprocess, "run", return_value=subprocess.CompletedProcess([], 0, output)
        ) as run, mock.patch(
            __name__ + ".require_free_port",
            side_effect=[RuntimeError("Port occupied"), None],
        ) as bind:
            self.assertEqual(non_loopback_address(), "192.0.2.11")
            self.assertEqual(
                bind.call_args_list,
                [
                    mock.call("192.0.2.10", DIRECTOR_PORT),
                    mock.call("192.0.2.11", DIRECTOR_PORT),
                ],
            )
            self.assertEqual(run.call_args[0][0], ["/sbin/ifconfig", "-a"])
            run.return_value.stdout = "\tinet 127.0.0.1 netmask 0xff000000\n"
            with self.assertRaisesRegex(RuntimeError, "no bindable non-loopback"):
                non_loopback_address()


if __name__ == "__main__":
    mode = sys.argv[1]
    suite = unittest.TestSuite([ProxyTest(f"test_{mode}")])
    if mode == "configured":
        suite.addTest(ProxyTest("test_interface_discovery"))
    sys.exit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
