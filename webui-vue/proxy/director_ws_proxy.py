#!/usr/bin/env python3
"""
Bareos Director WebSocket Proxy
================================
Bridges the Vue WebUI (browser) to the Bareos Director daemon (port 9101).

Each browser WebSocket connection authenticates to the director as a named
console user.  Director commands are executed in a thread pool because the
python-bareos library is synchronous.

Protocol (JSON over WebSocket)
------------------------------
1. Client → proxy  auth request:
     {"type": "auth", "username": "admin", "password": "secret",
      "director": "bareos-dir", "host": "localhost", "port": 9101}

2. Proxy → client  on success:
     {"type": "auth_ok", "director": "bareos-dir"}

   Proxy → client  on failure:
     {"type": "auth_error", "message": "..."}

3. Client → proxy  command:
     {"type": "command", "id": "1", "command": "list jobs"}

4. Proxy → client  response:
     {"type": "response", "id": "1", "command": "list jobs", "data": {...}}

   Proxy → client  command error:
     {"type": "error", "id": "1", "command": "list jobs", "message": "..."}

Environment variables
---------------------
  BAREOS_DIRECTOR_HOST  default director hostname (default: localhost)
  BAREOS_DIRECTOR_PORT  default director port     (default: 9101)
  BAREOS_DIRECTOR_NAME  default director name     (default: bareos-dir)
  WS_HOST               bind address              (default: localhost)
  WS_PORT               bind port                 (default: 8765)

Usage
-----
  pip install -r requirements.txt
  python director_ws_proxy.py
"""

import asyncio
import json
import logging
import os
import sys

import websockets
import websockets.exceptions

# --- resolve python-bareos from the repository --------------------------------
_here = os.path.dirname(os.path.abspath(__file__))
_bareos_lib = os.path.join(_here, "..", "..", "python-bareos")
if os.path.isdir(_bareos_lib):
    sys.path.insert(0, os.path.abspath(_bareos_lib))

import bareos.bsock
import bareos.exceptions

# ------------------------------------------------------------------------------

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)-8s %(name)s: %(message)s",
)
log = logging.getLogger("bareos-ws-proxy")

DEFAULT_DIRECTOR_HOST = os.environ.get("BAREOS_DIRECTOR_HOST", "localhost")
DEFAULT_DIRECTOR_PORT = int(os.environ.get("BAREOS_DIRECTOR_PORT", 9101))
DEFAULT_DIRECTOR_NAME = os.environ.get("BAREOS_DIRECTOR_NAME", "bareos-dir")
WS_HOST = os.environ.get("WS_HOST", "localhost")
WS_PORT = int(os.environ.get("WS_PORT", 8765))


def _director_connect(host, port, dirname, username, password):
    """Blocking: create and authenticate a DirectorConsoleJson connection."""
    return bareos.bsock.DirectorConsoleJson(
        address=host,
        port=port,
        dirname=dirname,
        name=username,
        password=bareos.bsock.Password(password),
        # Allow fallback to unencrypted protocol for dev environments
        # that don't have TLS-PSK configured.
        protocolversion=None,
    )


def _director_connect_raw(host, port, dirname, username, password):
    """Blocking: create and authenticate a raw-text DirectorConsole connection."""
    return bareos.bsock.DirectorConsole(
        address=host,
        port=port,
        dirname=dirname,
        name=username,
        password=bareos.bsock.Password(password),
        protocolversion=None,
    )


def _director_call(director, command):
    """Blocking: execute a command and return the result dict."""
    return director.call(command)


def _director_call_raw(director, command):
    """Blocking: execute a command in raw text mode, return a string."""
    result = director.call(command)
    if isinstance(result, (bytes, bytearray)):
        return result.decode("utf-8", errors="replace")
    return str(result) if result is not None else ""


async def handle_client(websocket):
    """Handle one browser WebSocket session."""
    peer = websocket.remote_address
    log.info("Client connected: %s", peer)
    director = None

    try:
        # ── Step 1: authentication ──────────────────────────────────────────
        try:
            raw = await asyncio.wait_for(websocket.recv(), timeout=15)
        except asyncio.TimeoutError:
            log.warning("Auth timeout from %s", peer)
            return

        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            await websocket.send(json.dumps({"type": "error", "message": "Expected JSON auth message"}))
            return

        if msg.get("type") != "auth":
            await websocket.send(json.dumps({"type": "error", "message": "First message must be type=auth"}))
            return

        username = msg.get("username", "admin")
        password = msg.get("password", "")
        director_name = msg.get("director", DEFAULT_DIRECTOR_NAME)
        host = msg.get("host", DEFAULT_DIRECTOR_HOST)
        port = int(msg.get("port", DEFAULT_DIRECTOR_PORT))
        mode = msg.get("mode", "json")   # "json" (default) or "raw"

        log.info("Auth attempt: user=%s director=%s host=%s:%d mode=%s", username, director_name, host, port, mode)

        try:
            if mode == "raw":
                director = await asyncio.to_thread(
                    _director_connect_raw, host, port, director_name, username, password
                )
            else:
                director = await asyncio.to_thread(
                    _director_connect, host, port, director_name, username, password
                )
        except bareos.exceptions.AuthenticationError as exc:
            await websocket.send(json.dumps({"type": "auth_error", "message": f"Authentication failed: {exc}"}))
            log.warning("Auth failed for %s: %s", peer, exc)
            return
        except Exception as exc:
            await websocket.send(json.dumps({"type": "auth_error", "message": f"Connection error: {exc}"}))
            log.warning("Director connection error for %s: %s", peer, exc)
            return

        await websocket.send(json.dumps({"type": "auth_ok", "director": director_name, "mode": mode}))
        log.info("Client %s authenticated as %s on %s (mode=%s)", peer, username, director_name, mode)

        # ── Step 2: command loop ────────────────────────────────────────────
        async for raw_msg in websocket:
            try:
                req = json.loads(raw_msg)
            except json.JSONDecodeError:
                await websocket.send(json.dumps({"type": "error", "message": "Invalid JSON"}))
                continue

            if req.get("type") != "command":
                await websocket.send(json.dumps({"type": "error", "message": "Expected type=command"}))
                continue

            command = str(req.get("command", "")).strip()
            req_id = req.get("id")

            if not command:
                continue

            log.debug("Command from %s [id=%s]: %r", peer, req_id, command)

            try:
                if mode == "raw":
                    text = await asyncio.to_thread(_director_call_raw, director, command)
                    await websocket.send(json.dumps({
                        "type": "raw_response",
                        "id": req_id,
                        "command": command,
                        "text": text,
                    }))
                else:
                    result = await asyncio.to_thread(_director_call, director, command)
                    await websocket.send(json.dumps({
                        "type": "response",
                        "id": req_id,
                        "command": command,
                        "data": result,
                    }))
            except bareos.exceptions.Error as exc:
                await websocket.send(json.dumps({
                    "type": "error",
                    "id": req_id,
                    "command": command,
                    "message": str(exc),
                }))
                log.debug("Director error for %s: %s", peer, exc)
            except Exception as exc:
                await websocket.send(json.dumps({
                    "type": "error",
                    "id": req_id,
                    "command": command,
                    "message": f"Proxy error: {exc}",
                }))
                log.warning("Unexpected error for %s: %s", peer, exc)

    except websockets.exceptions.ConnectionClosed:
        log.info("Client %s disconnected", peer)
    except Exception as exc:
        log.error("Unhandled error for %s: %s", peer, exc, exc_info=True)
    finally:
        if director is not None:
            try:
                await asyncio.to_thread(director.disconnect)
            except Exception:
                pass
        log.info("Session ended: %s", peer)


async def main():
    log.info("Bareos Director WebSocket Proxy starting")
    log.info("  Listening on ws://%s:%d", WS_HOST, WS_PORT)
    log.info("  Default director: %s @ %s:%d", DEFAULT_DIRECTOR_NAME, DEFAULT_DIRECTOR_HOST, DEFAULT_DIRECTOR_PORT)

    async with websockets.serve(handle_client, WS_HOST, WS_PORT):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    asyncio.run(main())
