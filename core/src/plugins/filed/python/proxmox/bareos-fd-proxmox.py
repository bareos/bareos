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

# pylint: disable=invalid-name
# pylint: enable=invalid-name

"""A Bareos plugin for Proxmox VE"""

import subprocess
from os import read, close, pipe, O_NONBLOCK
from fcntl import fcntl, F_SETPIPE_SZ, F_GETFL, F_SETFL
from select import poll, POLLIN
from datetime import datetime

from BareosFdWrapper import *  # noqa # pylint: disable=import-error,wildcard-import
import bareosfd  # pylint: disable=import-error
import BareosFdPluginBaseclass  # pylint: disable=import-error


class LogPipe:
    """special Pipe object to capture log output from subprocess.Popen()"""

    def __init__(self):
        self._read_fd, self._write_fd = pipe()
        # make the reading end, but not the writing end non-blocking and create
        # a poll-object for it
        fcntl(self._read_fd, F_SETFL, fcntl(self._read_fd, F_GETFL) | O_NONBLOCK)
        self._read_poller = poll()
        self._read_poller.register(self._read_fd)

        # Increase pipe buffer size to maximum value.
        # While the core does I/O we won't be called. Therefore we want a large
        # buffer so io_process will not block on a full pipe while logging.
        # This will, of course, affect both ends of the pipe.
        # TODO make this as large as possible
        fcntl(self._write_fd, F_SETPIPE_SZ, 1024)

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


@BareosPlugin  # pylint: disable=undefined-variable
class BareosFdProxmox(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """Plugin main class"""

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100, f"Constructor called in module {__name__} with plugindef={plugindef}\n"
        )
        self.mandatory_options_guestid = ["guestid"]

        super().__init__(plugindef)

        self.file = None
        self.io_process = None
        self.log_pipe = LogPipe()

    def parse_plugin_definition(self, plugindef):
        """
        Parses the plugin arguments
        """
        bareosfd.DebugMessage(
            100, f"parse_plugin_definition() was called in module {__name__}\n"
        )
        super().parse_plugin_definition(plugindef)
        bareosfd.DebugMessage(100, f"self.options are: = {self.options}\n")
        # Full ("F") or Restore (" ")
        if chr(self.level) not in "F ":
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

        vzdump_cmd = ("vzdump", self.options["guestid"], "--stdout")
        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(vzdump_cmd)}\n")
        self.io_process = subprocess.Popen(  # pylint: disable=consider-using-with
            vzdump_cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=self.log_pipe,
        )

        guesttype = None
        vmname = None
        try:
            for line in self.log_pipe.readlines(init_timeout=10000, read_timeout=500):
                bareosfd.JobMessage(bareosfd.M_INFO, line)
                # INFO: VM Name: winrecover
                # INFO: CT Name: container1
                if "Name:" in line:
                    if line.split()[1] == "VM":
                        guesttype = "qemu"
                    else:
                        guesttype = "lxc"
                    vmname = " ".join(line.split()[3:])
                elif "sending archive to stdout" in line:
                    write_started = True
        except TimeoutError:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, "Malformed log data received from vzdump.\n"
            )
            return bareosfd.bRC_Error

        ts = datetime.now().strftime("%Y_%m_%d-%H_%M_%S")
        filename = "-".join(
            (
                "/var/lib/vz/dump/vzdump",
                guesttype,
                self.options["guestid"],
                f"{ts}.vma",
            )
        )

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            f'Backing up {guesttype} guest "{vmname}" to virtual file {filename}\n',
        )

        # create a regular stat packet
        statp = bareosfd.StatPacket()
        savepkt.statp = statp
        savepkt.type = bareosfd.FT_REG
        savepkt.fname = filename

        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """Create file during restore"""
        bareosfd.DebugMessage(100, f"create_file() called with {restorepkt}\n")

        if self.options.get("restoretodisk") == "yes":
            restorepkt.create_status = bareosfd.CF_CORE
            return bareosfd.bRC_OK

        if "vzdump-lxc" in restorepkt.ofname:
            recovery_cmd = (
                "pct",
                "restore",
                self.options["guestid"],
                "-",
                "--rootfs",
                "/",
                "--force",
                "yes",
            )

        elif "vzdump-qemu" in restorepkt.ofname:
            recovery_cmd = ("qmrestore", "-", self.options["guestid"], "--force", "yes")
        else:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "no 'vzdump-lxc' or 'vzdump-qemu' in filename, only 'restoretodisk=yes' possible",
            )
            return bareosfd.bRC_Error

        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(recovery_cmd)}\n")
        self.io_process = subprocess.Popen(  # pylint: disable=consider-using-with
            recovery_cmd,
            stdin=subprocess.PIPE,
            stdout=self.log_pipe,
            stderr=self.log_pipe,
        )

        self._despool_log(init_timeout=10000)

        restorepkt.create_status = bareosfd.CF_EXTRACT

        return bareosfd.bRC_OK

    def plugin_io_open(self, iop):
        """Open file for backup or restore"""
        if self.options.get("restoretodisk") == "yes":
            iop.status = bareosfd.iostat_do_in_core
            self.file = open(iop.fname, "wb")  # pylint: disable=consider-using-with
            iop.filedes = self.file.fileno()
            bareosfd.JobMessage(bareosfd.M_INFO, f"restoring to file {iop.fname}\n")
            return bareosfd.bRC_OK

        # TODO: Check if restore or backup to determine
        if self.io_process.stdin:
            iop.filedes = self.io_process.stdin.fileno()
        else:
            iop.filedes = self.io_process.stdout.fileno()
        iop.status = bareosfd.iostat_do_in_core

        return bareosfd.bRC_OK

    def plugin_io_close(self, iop):
        """Close file for backup or restore"""
        del iop

        if self.options.get("restoretodisk") == "yes":
            self.file.close()
            return bareosfd.bRC_OK

        self._despool_log(init_timeout=2000)

        if self.io_process.returncode:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"program returned {self.io_process.returncode}"
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def plugin_io_seek(self, iop):
        """Seek in file"""
        del iop
        # TODO: this should probably fail!
        return bareosfd.bRC_OK

    def plugin_io_read(self, iop):
        """Read a block of data for backup"""
        del iop
        # TODO: this should probably fail!
        return bareosfd.bRC_OK

    def plugin_io_write(self, iop):
        """Write a block of data to restore"""
        try:
            self.io_process.stdin.write(iop.buf)
            iop.status = iop.count
            iop.io_errno = 0

        except IOError as e:
            bareosfd.DebugMessage(100, f"plugin_io[IO_WRITE]: IOError: {e}\n")
            iop.status = 0
            iop.io_errno = e.errno
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def end_backup_file(self):
        """Called after all data was read"""

        self._despool_log(init_timeout=0)

        if self.io_process.returncode:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"program returned {self.io_process.returncode}"
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK

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
