#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

# silence module-name warning
# pylint: disable=invalid-name
# pylint: enable=invalid-name

"""A Bareos plugin for Proxmox VE"""

import subprocess
from os import read, close, pipe, O_NONBLOCK
from os.path import basename
from fcntl import fcntl, F_SETPIPE_SZ, F_GETFL, F_SETFL
from select import poll, POLLIN

from BareosFdWrapper import *  # noqa # pylint: disable=import-error,wildcard-import
import bareosfd  # pylint: disable=import-error
import BareosFdPluginBaseclass  # pylint: disable=import-error


class LogPipe:
    """special Pipe object to capture log output from subprocess.Popen()"""

    def __init__(self):
        self._read_fd, self._write_fd = pipe()

        # make reading end non-blocking so we can poll() with timeout
        fcntl(self._read_fd, F_SETFL, fcntl(self._read_fd, F_GETFL) | O_NONBLOCK)
        self._read_poller = poll()
        self._read_poller.register(self._read_fd)

        # Increase pipe buffer size to maximum value.
        # While the core does I/O we won't be called. Therefore we want a large
        # buffer so io_process will not block on a full pipe while logging.
        # This will, of course, affect both ends of the pipe.
        with open("/proc/sys/fs/pipe-max-size", "r", encoding="ascii") as file:
            max_size = int(file.read().rstrip())
            fcntl(self._write_fd, F_SETPIPE_SZ, max_size)

    def __del__(self):
        close(self._write_fd)
        close(self._read_fd)

    def fileno(self):
        """Popen() will call this to retrieve a file descriptor."""
        return self._write_fd

    def readlines(
        self, *, init_timeout=60000, read_timeout=100, block_size=1024, soft_fail=False
    ):
        """Generator emulating normal readlines() behaviour."""
        try:
            data = self.read(block_size, timeout=init_timeout)
        except TimeoutError as e:
            if soft_fail:
                return
            raise TimeoutError("timeout waiting for start of data") from e
        while True:
            newline_pos = data.find(b"\n")
            if newline_pos == -1:
                # data does not contain a newline, so we need more input
                try:
                    # fetch more data
                    data += self.read(block_size, timeout=read_timeout)
                except TimeoutError as e:
                    if len(data) == 0:
                        # timeout on end of line: no more data
                        return
                    raise TimeoutError("timeout waiting for end of line") from e
            else:
                # pick first line from data, leave the rest
                yield data[: newline_pos + 1].decode("utf-8", errors="replace")
                data = data[newline_pos + 1 :]

    def read(self, length, *, timeout=100):
        """read() like call with timeout in milliseconds."""
        res = self._read_poller.poll(timeout)
        if len(res) == 0:
            raise TimeoutError

        # there must be exactly one result and that must have our read_fd as fd
        assert len(res) == 1
        fd, event = res[0]
        assert fd == self._read_fd

        if event & POLLIN:
            return read(self._read_fd, length)
        # some other event happened, probably an error
        raise IOError(f"Unexpected bitmask on poll(): {event}")


class UnknownOptionException(Exception):
    """Raised when using an unknown option."""


class BareosFdProxmoxOptions:
    """Holds plugin options."""

    def __init__(self):
        self._options = {
            "force": self.make_opt(bool, False),
            "guestid": self.make_opt(int),
            "restorepath": self.make_opt(str, "/var/lib/vz/dump"),
            "restoretodisk": self.make_opt(bool, False),
        }

    @staticmethod
    def make_opt(type_, default=None):
        """Create a new option"""
        return {"type": type_, "value": default, "value_set": False}

    def check(self):
        """Check if options are complete."""
        success = True
        for name, option in self._options.items():
            bareosfd.DebugMessage(250, f"Option '{name}': {option}'\n")
            if not option["value_set"] and option["value"] is None:
                success = False
                bareosfd.JobMessage(
                    bareosfd.M_FATAL, f"Mandatory option {name} not defined.\n"
                )
        return success

    def __contains__(self, key):
        try:
            return self._options[key]["value_set"]
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __setitem__(self, key, value):
        try:
            opt = self._options[key]
            if opt["type"] == str:
                opt["value"] = value
            elif opt["type"] == int:
                try:
                    opt["value"] = int(value, base=10)
                except ValueError as e:
                    raise ValueError(
                        f"invalid value '{value}' for integer option '{key}'"
                    ) from e
            elif opt["type"] == bool:
                tmp = value.strip().lower()
                if tmp in ("yes", "on", "true", "1"):
                    opt["value"] = True
                elif tmp in ("no", "off", "false", "0"):
                    opt["value"] = False
                else:
                    raise ValueError(
                        f"invalid value '{value}' for boolean option '{key}'"
                    )
            opt["value_set"] = True
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __getitem__(self, key):
        try:
            return self._options[key]["value"]
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __getattr__(self, key):
        try:
            return self._options[key]["value"]
        except KeyError as e:
            raise UnknownOptionException(key) from e


@BareosPlugin  # pylint: disable=undefined-variable
class BareosFdProxmox(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """Plugin main class"""

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100, f"Constructor called in module {__name__} with plugindef={plugindef}\n"
        )

        self.options = BareosFdProxmoxOptions()
        super().__init__(plugindef)

        self.file = None
        self.io_process = None
        self.log_pipe = LogPipe()

    def check_options(self, mandatory_options=None):
        """Check plugin options for completeness."""
        del mandatory_options
        return self.options.check()

    def parse_plugin_definition(self, plugindef):
        """
        Parses the plugin arguments
        """
        bareosfd.DebugMessage(
            100, f"parse_plugin_definition() was called in module {__name__}\n"
        )
        try:
            if not super().parse_plugin_definition(plugindef):
                return bareosfd.bRC_Error
        except UnknownOptionException as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"Unknown option '{e}' passed to plugin\n"
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, f"self.options are: = {self.options}\n")
        # Full ("F") or Restore (" ")
        if chr(self.level) not in "F ":
            bareosfd.DebugMessage(10, f"unsupported level: {self.level}\n")
            bareosfd.JobMessage(
                bareosfd.M_FATAL, "Only Full Backups are currently supported\n"
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(100, f"{__name__}:start_backup_file() called\n")

        guest_id = self.options.guestid

        vzdump_cmd = ("vzdump", f"{guest_id}", "--stdout")
        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(vzdump_cmd)}\n")
        self.io_process = subprocess.Popen(  # pylint: disable=consider-using-with
            vzdump_cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=self.log_pipe,
        )

        guest_type = None
        guest_name = None
        backup_ts = None
        write_started = False

        try:
            for line in self.log_pipe.readlines(init_timeout=10000, read_timeout=500):
                bareosfd.JobMessage(bareosfd.M_INFO, line)
                if line.startswith("INFO: Starting Backup of VM"):
                    # """INFO: Starting Backup of VM 999010 (qemu)"""
                    if line.endswith("(lxc)\n"):
                        guest_type = "lxc"
                        guest_name = f"CT {guest_id}"
                    elif line.endswith("(qemu)\n"):
                        guest_type = "qemu"
                        guest_name = f"VM {guest_id}"
                    else:
                        bareosfd.JobMessage(bareosfd.M_FATAL, "Unsupported guest type.")
                        return bareosfd.bRC_Error
                    bareosfd.DebugMessage(100, f"guest type: '{guest_type}'\n")
                elif line.startswith("INFO: Backup started at "):
                    # """INFO: Backup started at 2025-11-04 16:38:07"""
                    backup_ts = line[24:-1].translate(str.maketrans("- :", "_-_"))
                    bareosfd.DebugMessage(100, f"backup ts: '{backup_ts}'\n")
                elif (guest_type == "lxc" and line.startswith("INFO: CT Name:")) or (
                    guest_type == "qemu" and line.startswith("INFO: VM Name:")
                ):
                    # """INFO: VM Name: plugtestvm"""
                    guest_name = line[15:-1]
                    bareosfd.DebugMessage(100, f"guest name: '{guest_name}'\n")
                elif "sending archive to stdout" in line:
                    # """INFO: sending archive to stdout"
                    write_started = True
                    bareosfd.DebugMessage(100, "start marker found\n")
        except TimeoutError as e:
            bareosfd.JobMessage(bareosfd.M_FATAL, f"vzdump log output stalled: {e}\n")
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(
            100,
            f"guest_type={guest_type}, write_started={write_started}, backup_ts={backup_ts}\n",
        )

        if not self._check_io_process():
            return bareosfd.bRC_Error

        if not guest_type or not write_started or not backup_ts:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Not all required information could be retrieved from log.\n",
            )
            return bareosfd.bRC_Error

        filename = (
            f"@PROXMOX/{guest_name}/vzdump-{guest_type}-{guest_id}-{backup_ts}.vma"
        )

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            f'Backing up {guest_type} guest "{guest_name}" to virtual file {filename}\n',
        )

        # create a regular stat packet
        savepkt.statp = bareosfd.StatPacket()
        savepkt.type = bareosfd.FT_REG
        savepkt.fname = filename

        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """Create file during restore"""
        bareosfd.DebugMessage(100, f"create_file() called with {restorepkt}\n")

        restorepkt.create_status = bareosfd.CF_EXTRACT

        if self.options.restoretodisk:
            return bareosfd.bRC_OK

        if "vzdump-lxc" in restorepkt.ofname:
            recovery_cmd = [
                "pct",
                "restore",
                f"{self.options.guestid}",
                "-",
                "--rootfs",
                "/",
            ]
        elif "vzdump-qemu" in restorepkt.ofname:
            recovery_cmd = ["qmrestore", "-", f"{self.options.guestid}"]
        else:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "no 'vzdump-lxc' or 'vzdump-qemu' in filename, only 'restoretodisk=yes' possible",
            )
            return bareosfd.bRC_Error

        if self.options.force:
            recovery_cmd.extend(["--force", "yes"])

        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(recovery_cmd)}\n")
        self.io_process = subprocess.Popen(  # pylint: disable=consider-using-with
            recovery_cmd,
            stdin=subprocess.PIPE,
            stdout=self.log_pipe,
            stderr=self.log_pipe,
        )

        self._despool_log(init_timeout=10000)

        return bareosfd.bRC_OK

    def plugin_io_open(self, iop):
        """Open file for backup or restore"""
        iop.status = bareosfd.iostat_do_in_core

        if chr(self.level) == "F":
            # backup from io_process' stdout
            iop.filedes = self.io_process.stdout.fileno()
        elif chr(self.level) == " " and self.options.restoretodisk:
            # restore to a file
            filename = f"{self.options.restorepath}/{basename(iop.fname)}"

            self.file = open(filename, "wb")  # pylint: disable=consider-using-with
            iop.filedes = self.file.fileno()

            bareosfd.JobMessage(bareosfd.M_INFO, f"restoring to file {filename}\n")
        elif chr(self.level) == " ":
            # restore to io_process' stdin
            iop.filedes = self.io_process.stdin.fileno()
        else:
            # other levels already catched at job start
            assert False

        return bareosfd.bRC_OK

    def plugin_io_close(self, iop):
        """Close file for backup or restore"""
        del iop

        if self.options.restoretodisk:
            self.file.close()
        return bareosfd.bRC_OK

    def plugin_io_seek(self, iop):
        """Seek in file, handled by core"""
        raise NotImplementedError

    def plugin_io_read(self, iop):
        """Read a block of data for backup, handled by core"""
        raise NotImplementedError

    def plugin_io_write(self, iop):
        """Write a block of data to restore, handled by core"""
        raise NotImplementedError

    def end_backup_file(self):
        """Called after all data was read"""

        self._despool_log(init_timeout=0)

        if not self._wait_for_io_process():
            return bareosfd.bRC_Error
        bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK

    def end_restore_file(self):
        """Called after all data was written"""
        if self.options.restoretodisk:
            return bareosfd.bRC_OK

        # tell the process we're done writing
        self.io_process.stdin.close()

        self._despool_log(init_timeout=0)

        if not self._wait_for_io_process():
            return bareosfd.bRC_Error
        bareosfd.DebugMessage(100, "end_restore_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK

    def get_acl(self, acl):
        """Returns no ACL."""
        del acl
        return bareosfd.bRC_OK

    def set_acl(self, acl):
        """Set ACL provided by core. Not supported in this plugin."""
        raise NotImplementedError

    def get_xattr(self, xattr):
        """Returns no extended attributes."""
        del xattr
        return bareosfd.bRC_OK

    def set_xattr(self, xattr):
        """Set extended attributes provided by core. Not supported in this plugin."""
        raise NotImplementedError

    def _despool_log(self, *, init_timeout=0, read_timeout=100):
        """fetches all log lines from the LogPipe (if any) and emits JobMessages."""
        for line in self.log_pipe.readlines(
            init_timeout=init_timeout, read_timeout=read_timeout, soft_fail=True
        ):
            if line.startswith("ERROR:"):
                bareosfd.JobMessage(bareosfd.M_ERROR, line)
            elif line.startswith("WARNING:"):
                bareosfd.JobMessage(bareosfd.M_WARNING, line)
            else:
                bareosfd.JobMessage(bareosfd.M_INFO, line)

    def _check_io_process(self):
        ret = self.io_process.poll()
        if ret is None or ret == 0:
            return True  # still running

        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            f"'{' '.join(self.io_process.args)}' exited with code {ret}\n",
        )
        return False

    def _wait_for_io_process(self, timeout=10):
        bareosfd.JobMessage(bareosfd.M_INFO, "waiting for command to finish\n")
        while self.io_process.returncode is None:
            self._despool_log()
            try:
                self.io_process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                pass
        return self._check_io_process()
