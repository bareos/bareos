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
import tempfile
import os
import time
from datetime import datetime

from BareosFdWrapper import *  # noqa # pylint: disable=import-error,wildcard-import
import bareosfd  # pylint: disable=import-error
import BareosFdPluginBaseclass  # pylint: disable=import-error


@BareosPlugin  # pylint: disable=undefined-variable
class BareosFdProxmox(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """Plugin main class"""

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100, f"Constructor called in module {__name__} with plugindef={plugindef}\n"
        )
        self.mandatory_options_guestid = ["guestid"]

        super().__init__(plugindef)

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
                bareosfd.M_ERROR, "Only Full Backups are currently supported\n"
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(100, __name__ + ":start_backup_file() called\n")

        log_path = "."
        self.stderr_log_file = tempfile.NamedTemporaryFile(
            dir=log_path, delete=False, mode="r+b"
        )
        vzdump_cmd = ("vzdump", self.options["guestid"], "--stdout")
        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(vzdump_cmd)}\n")
        self.io_process = subprocess.Popen(
            vzdump_cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=self.stderr_log_file,
            # stderr=subprocess.PIPE,
        )
        os.rename(self.stderr_log_file.name, "vzdump.log")

        # wait for vzdump to start backup ('sending archive to stdout ....')
        started = False
        while not started:
            time.sleep(1)
            with open("vzdump.log", encoding="utf-8") as logfile:
                for line in logfile.readlines():
                    if "sending archive to stdout" in line:
                        bareosfd.JobMessage(bareosfd.M_INFO, line)
                        started = True
                    # INFO: VM Name: winrecover
                    # INFO: CT Name: container1
                    elif "Name:" in line:
                        if line.split()[1] == "VM":
                            self.guesttype = "qemu"
                        else:
                            self.guesttype = "lxc"
                        self.vmname = " ".join(line.split()[3:])

        ts = datetime.now().strftime("%Y_%m_%d-%H_%M_%S")
        filename = "-".join(
            (
                "/var/lib/vz/dump/vzdump",
                self.guesttype,
                self.options["guestid"],
                f"{ts}.vma",
            )
        )

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            f'Backing up {self.guesttype} guest "{self.vmname}" to virtual file {filename}\n',
        )

        # create a regular stat packet
        statp = bareosfd.StatPacket()
        savepkt.statp = statp
        savepkt.type = bareosfd.FT_REG
        savepkt.fname = filename

        self.current_logfile = open("vzdump.log", encoding="utf-8")
        for line in self.current_logfile.readlines():
            bareosfd.JobMessage(bareosfd.M_INFO, line)

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
            return bareosfd.bRC_ERR

        log_path = "."
        self.stderr_log_file = tempfile.NamedTemporaryFile(
            dir=log_path, delete=False, mode="r+b"
        )
        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(recovery_cmd)}\n")
        self.io_process = subprocess.Popen(
            recovery_cmd,
            stdin=subprocess.PIPE,
            stdout=self.stderr_log_file,
            stderr=self.stderr_log_file,
        )
        os.rename(self.stderr_log_file.name, "restore.log")

        self.current_logfile = open("restore.log", encoding="utf-8")
        for line in self.current_logfile.readlines():
            bareosfd.JobMessage(bareosfd.M_INFO, line)

        restorepkt.create_status = bareosfd.CF_EXTRACT

        return bareosfd.bRC_OK

    def plugin_io_open(self, iop):
        """Open file for backup or restore"""
        bareosfd.DebugMessage(100, f"IO_OPEN: {iop.fname}\n")
        if self.options.get("restoretodisk") == "yes":
            iop.status = bareosfd.iostat_do_in_core
            self.file = open(iop.fname, "wb")
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
        bareosfd.DebugMessage(100, f"IO_CLOSE: {iop.fname}\n")

        if self.options.get("restoretodisk") == "yes":
            self.file.close()
            return bareosfd.bRC_OK

        for line in self.current_logfile.readlines():
            bareosfd.JobMessage(bareosfd.M_INFO, line)

        if self.io_process.returncode:
            bareosfd.DebugMessage(
                100,
                f"plugin_io() vzdump returncode: {self.io_process.returncode}\n",
            )
            return bareosfd.bRC_ERR

        return bareosfd.bRC_OK

    def plugin_io_seek(self, iop):
        """Seek in file"""
        # TODO: this should probably fail!
        bareosfd.DebugMessage(100, f"IO_SEEK: {iop.fname}\n")
        return bareosfd.bRC_OK

    def plugin_io_read(self, iop):
        """Read a block of data for backup"""
        # TODO: this should probably fail!
        bareosfd.DebugMessage(100, f"IO_READ: {iop.fname}\n")
        return bareosfd.bRC_OK

    def plugin_io_write(self, iop):
        """Write a block of data to restore"""
        bareosfd.DebugMessage(100, f"IO_WRITE: {iop.fname}\n")
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
        bareosfd.DebugMessage(100, "end_backup_file() called\n")

        # print rest of vzdump log
        for line in self.current_logfile.readlines():
            bareosfd.JobMessage(bareosfd.M_INFO, line)

        if self.io_process.returncode:
            bareosfd.DebugMessage(
                100,
                f"plugin_io() vzdump returncode: {self.io_process.returncode}\n",
            )
            return bareosfd.bRC_ERR

        bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK
