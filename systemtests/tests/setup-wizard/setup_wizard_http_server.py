#!/usr/bin/env python3

#
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
#

import http.client
import http.server
import json
import os
import pathlib
import select
import shlex
import socket
import socketserver
import subprocess
import urllib.parse


DIST_DIR = pathlib.Path(os.environ["BAREOS_SETUP_DIST_DIR"])
BCONFIG_HOST = os.environ.get("BAREOS_BCONFIG_HOST", "127.0.0.1")
BCONFIG_PORT = int(os.environ["BAREOS_BCONFIG_PORT"])
WS_PROXY_HOST = os.environ.get("BAREOS_SETUP_WS_PROXY_HOST", "127.0.0.1")
WS_PROXY_PORT = int(os.environ["BAREOS_SETUP_WS_PROXY_PORT"])
LISTEN_HOST = os.environ.get("BAREOS_SETUP_LISTEN_HOST", "127.0.0.1")
LISTEN_PORT = int(os.environ["BAREOS_SETUP_LISTEN_PORT"])
STACK_LOG_DIR = pathlib.Path(
    os.environ.get("BAREOS_SETUP_LOG_DIR", "/var/log/setup-wizard")
)
STORAGE_BOOTSTRAP_LOG = STACK_LOG_DIR / "storage-bootstrap.log"
SETUP_DEFAULTS = {
    "repositoryPath": os.environ.get("BAREOS_SETUP_DEFAULT_REPOSITORY_PATH", ""),
    "runtimeRoot": os.environ.get("BAREOS_SETUP_DEFAULT_RUNTIME_ROOT", ""),
    "daemonAddress": os.environ.get("BAREOS_SETUP_DEFAULT_DAEMON_ADDRESS", ""),
    "directorPort": int(os.environ["BAREOS_SETUP_DEFAULT_DIRECTOR_PORT"]),
    "clientPort": int(os.environ["BAREOS_SETUP_DEFAULT_CLIENT_PORT"]),
    "storagePort": int(os.environ["BAREOS_SETUP_DEFAULT_STORAGE_PORT"]),
}
SETUP_DEFAULTS_SCRIPT = (
    "<script>"
    f"window.__BAREOS_SETUP_DEFAULTS__ = {json.dumps(SETUP_DEFAULTS)};"
    "</script>"
)


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DIST_DIR), **kwargs)

    def do_GET(self):
        if (
            self.path == "/ws"
            and self.headers.get("Upgrade", "").lower() == "websocket"
        ):
            self._proxy_websocket()
            return

        if self.path.startswith("/api/bconfig/"):
            self._proxy_http_request(BCONFIG_HOST, BCONFIG_PORT)
            return

        if self.path == "/" or self.path == "/index.html":
            self._serve_index()
            return

        if self.path.startswith("/assets/"):
            super().do_GET()
            return

        self.path = "/index.html"
        self._serve_index()

    def do_POST(self):
        if self.path == "/api/test/storage-bootstrap/run":
            self._start_storage_bootstrap()
            return
        self._proxy_http_request(BCONFIG_HOST, BCONFIG_PORT)

    def do_PUT(self):
        self._proxy_http_request(BCONFIG_HOST, BCONFIG_PORT)

    def do_DELETE(self):
        self._proxy_http_request(BCONFIG_HOST, BCONFIG_PORT)

    def _proxy_http_request(self, upstream_host, upstream_port):
        target = urllib.parse.urlsplit(self.path)
        body = None
        content_length = self.headers.get("Content-Length")
        if content_length is not None:
            body = self.rfile.read(int(content_length))

        connection = http.client.HTTPConnection(
            upstream_host, upstream_port, timeout=30
        )
        upstream_headers = {
            key: value
            for key, value in self.headers.items()
            if key.lower()
            not in {
                "host",
                "connection",
                "keep-alive",
                "proxy-authenticate",
                "proxy-authorization",
                "te",
                "trailers",
                "transfer-encoding",
                "upgrade",
            }
        }
        upstream_headers["Host"] = f"{upstream_host}:{upstream_port}"

        path = target.path
        if target.query:
            path = f"{path}?{target.query}"

        connection.request(self.command, path, body=body, headers=upstream_headers)
        response = connection.getresponse()
        payload = response.read()

        self.send_response(response.status, response.reason)
        for key, value in response.getheaders():
            if key.lower() in {"connection", "transfer-encoding"}:
                continue
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(payload)

    def _proxy_websocket(self):
        upstream = socket.create_connection((WS_PROXY_HOST, WS_PROXY_PORT), timeout=30)
        try:
            request_line = f"{self.command} {self.path} {self.request_version}\r\n"
            upstream.sendall(request_line.encode("ascii"))
            for key, value in self.headers.items():
                upstream.sendall(f"{key}: {value}\r\n".encode("latin-1"))
            upstream.sendall(b"\r\n")

            self.connection.setblocking(False)
            upstream.setblocking(False)

            while True:
                readable, _, exceptional = select.select(
                    [self.connection, upstream], [], [self.connection, upstream]
                )
                if exceptional:
                    break

                for current, peer in (
                    (self.connection, upstream),
                    (upstream, self.connection),
                ):
                    if current not in readable:
                        continue
                    try:
                        chunk = current.recv(65536)
                    except BlockingIOError:
                        continue
                    if not chunk:
                        return
                    peer.sendall(chunk)
        finally:
            upstream.close()

    def _serve_index(self):
        index_path = DIST_DIR / "index.html"
        payload = index_path.read_text(encoding="utf-8").replace(
            "</head>", f"{SETUP_DEFAULTS_SCRIPT}</head>", 1
        )
        encoded = payload.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def _start_storage_bootstrap(self):
        content_length = self.headers.get("Content-Length")
        if content_length is None:
            self.send_error(411, "Content-Length required")
            return

        try:
            payload = json.loads(self.rfile.read(int(content_length)))
        except json.JSONDecodeError:
            self.send_error(400, "Invalid JSON body")
            return

        command = payload.get("command")
        if not isinstance(command, str) or not command.strip():
            self.send_error(400, "command must be a non-empty string")
            return

        argv = shlex.split(command)
        if argv and argv[0] == "sudo":
            argv = argv[1:]
        if not argv or pathlib.Path(argv[0]).name != "bareos-sd":
            self.send_error(400, "command must invoke bareos-sd")
            return
        if "--discovery" not in argv:
            self.send_error(400, "command must enable discovery mode")
            return

        try:
            if payload.get("stop_service"):
                subprocess.run(
                    ["systemctl", "stop", "bareos-sd.service"],
                    check=True,
                    capture_output=True,
                    text=True,
                )

            STACK_LOG_DIR.mkdir(parents=True, exist_ok=True)
            with STORAGE_BOOTSTRAP_LOG.open("ab") as log_file:
                process = subprocess.Popen(
                    argv,
                    stdout=log_file,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
        except subprocess.CalledProcessError as error:
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(
                json.dumps(
                    {
                        "error": "failed to prepare storage bootstrap",
                        "details": error.stderr or error.stdout or str(error),
                    }
                ).encode("utf-8")
            )
            return

        self.send_response(202)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps({"pid": process.pid}).encode("utf-8"))


class ThreadingHttpServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


def main():
    if not DIST_DIR.exists():
        raise SystemExit(f"dist directory not found: {DIST_DIR}")

    server = ThreadingHttpServer((LISTEN_HOST, LISTEN_PORT), Handler)
    print(
        f"setup-wizard test server listening on http://{LISTEN_HOST}:{LISTEN_PORT}",
        flush=True,
    )
    server.serve_forever()


if __name__ == "__main__":
    main()
