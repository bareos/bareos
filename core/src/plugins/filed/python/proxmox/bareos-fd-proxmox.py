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
from BareosFdWrapper import *  # noqa
import bareosfd
import BareosFdPluginBaseclass
import subprocess
import shlex
import tempfile
import os
import time
from datetime import datetime

@BareosPlugin
class BareosFdProxmox(BareosFdPluginBaseclass.BareosFdPluginBaseclass):

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef)
        )
        self.mandatory_options_guestid = ["guestid"]
        
        super(BareosFdProxmox, self).__init__(plugindef)
        self.events = []
        self.events.append(bareosfd.bEventStartBackupJob)
        self.events.append(bareosfd.bEventStartRestoreJob)
        self.events.append(bareosfd.bEventEndRestoreJob)
        bareosfd.RegisterEvents(self.events)

    def parse_plugin_definition(self, plugindef):
        """
        Parses the plugin arguments
        """
        bareosfd.DebugMessage(
            100,
            "parse_plugin_definition() was called in module %s\n" % (__name__),
        )
        super(BareosFdProxmox, self).parse_plugin_definition(plugindef)
        bareosfd.DebugMessage(
            100,
            "self.options are: = %s\n" % (self.options)
        )
        # Full ("F") or Restore (" ")
        if chr(self.level) not in "F ":
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "Only Full Backups are currently supported\n" )
            return bareosfd.bRC_Error
        

        return bareosfd.bRC_OK

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(100, __name__ + ":start_backup_file() called\n")

        log_path="."
        self.stderr_log_file = tempfile.NamedTemporaryFile(dir=log_path, delete=False, mode='r+b')
        vzdump_params = shlex.split(f"vzdump {self.options['guestid']} --stdout")
        vzdump_process = subprocess.Popen(
                    vzdump_params,
                    bufsize=-1,
                    stdin=open("/dev/null"),
                    stdout=subprocess.PIPE,
                    stderr=self.stderr_log_file,
                    #stderr=subprocess.PIPE,
                    close_fds=True,
                )
        self.vzdump_process = vzdump_process
        os.rename(self.stderr_log_file.name, "vzdump.log")
       
        bareosfd.JobMessage( bareosfd.M_INFO, f"Executing {vzdump_params}\n")
        # wait for vzdump to start backup ('sending archive to stdout ....')
        started = False
        while not started:
            time.sleep (1)
            with open("vzdump.log") as logfile:
                for line in logfile.readlines():
                    if 'sending archive to stdout' in line:
                        bareosfd.JobMessage( bareosfd.M_INFO, line)
                        started = True
                    #INFO: VM Name: winrecover
                    #INFO: CT Name: container1
                    elif 'Name:' in line:
                        vmorct = line.split()[1]
                        self.vmname = " ".join(line.split()[3:])
        if vmorct == "VM":
            self.guesttype =  "qemu"
        else:
            self.guesttype =  "lxc"

        #                                    vzdump-qemu-112-2025_09_12-00_00_00.vma 
        now = datetime.now()
        datestring = now.strftime("%Y_%m_%d-%H_%M_%S")
        self.file_to_backup = f"/var/lib/vz/dump/vzdump-{self.guesttype}-{self.options['guestid']}-{datestring}.vma"

        bareosfd.JobMessage( bareosfd.M_INFO, f"Backing up {self.guesttype} guest \"{self.vmname}\" to virtual file {self.file_to_backup}\n" )

        # create a regular stat packet
        statp = bareosfd.StatPacket()
        savepkt.statp = statp
        savepkt.type = bareosfd.FT_REG
        savepkt.fname = self.file_to_backup

        self.current_logfile = open("vzdump.log")
        for line in self.current_logfile.readlines():
            bareosfd.JobMessage( bareosfd.M_INFO, line)

        return bareosfd.bRC_OK


    def create_file(self, restorepkt):

        """
        """
        bareosfd.DebugMessage(
            100,
            "create_file() called with %s\n" % (restorepkt),
        )

        if self.options.get("restoretodisk") == "yes":
            restorepkt.create_status = bareosfd.CF_CORE
            return bareosfd.bRC_OK



        if "vzdump-lxc" in restorepkt.ofname:
            self.recoverycommand = f"pct restore {self.options['guestid']} - --rootfs / --force yes"

        else:
            if "vzdump-qemu" in restorepkt.ofname:
                self.recoverycommand = f"qmrestore - {self.options['guestid']} --force yes"

        log_path="."
        self.stderr_log_file = tempfile.NamedTemporaryFile(dir=log_path, delete=False, mode='r+b')
        vzdump_params = shlex.split(self.recoverycommand)
        vzdump_process = subprocess.Popen(
                    vzdump_params,
                    bufsize=-1,
                    stdin=subprocess.PIPE,
                    stdout=self.stderr_log_file,
                    stderr=self.stderr_log_file,
                    close_fds=True,
                )
        self.vzdump_process = vzdump_process
        bareosfd.JobMessage( bareosfd.M_INFO, f"Executing {vzdump_params}\n")
        os.rename(self.stderr_log_file.name, "restore.log")

        self.current_logfile = open("restore.log")
        for line in self.current_logfile.readlines():
            bareosfd.JobMessage( bareosfd.M_INFO, line)

        restorepkt.create_status = bareosfd.CF_EXTRACT


        return bareosfd.bRC_OK

    def plugin_io(self, IOP):

        self.FNAME = IOP.fname
        bareosfd.DebugMessage(
            200,
            (
                "plugin_io() called with function %s"
                " IOP.count=%s, self.FNAME is set to %s\n"
            )
            % (IOP.func, IOP.count, self.FNAME),
        )

        if IOP.func == bareosfd.IO_OPEN:
            self.FNAME = IOP.fname
            bareosfd.DebugMessage(
                100, "IO_OPEN: %s\n" % (self.FNAME)
            )
            if self.options.get("restoretodisk") == "yes":
                IOP.status = bareosfd.iostat_do_in_core
                self.file = open(self.FNAME, "wb")
                IOP.filedes = self.file.fileno()
                bareosfd.JobMessage( bareosfd.M_INFO, f"restoring to file {self.FNAME}\n")
                return bareosfd.bRC_OK
            #return super(BareosFdProxmox, self).plugin_io(IOP)

            # TODO: Check if restore or backup to determine
            if self.vzdump_process.stdin:
                IOP.filedes = self.vzdump_process.stdin.fileno()
            else:
                IOP.filedes = self.vzdump_process.stdout.fileno()
            IOP.status = bareosfd.iostat_do_in_core

            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_CLOSE:
            self.FNAME = IOP.fname
            bareosfd.DebugMessage(
                100, "IO_CLOSE: %s\n" % (self.FNAME)
            )

            if self.options.get("restoretodisk") == "yes":
                self.file.close()
                return bareosfd.bRC_OK

            for line in self.current_logfile.readlines():
                bareosfd.JobMessage( bareosfd.M_INFO, line)

            # check for process return code
            if self.vzdump_process.returncode:
                bareosfd.DebugMessage(
                100,
                "plugin_io() bareos_vadp_dumper returncode: %s\n"
                % (bareos_vadp_dumper_returncode),
                )
                return bareosfd.bRC_ERR

            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_SEEK:
            bareosfd.DebugMessage(
                100, "IO_SEEK: %s\n" % (self.FNAME)
            )
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_READ:
            bareosfd.DebugMessage(
                100, "IO_READ: %s\n" % (self.FNAME)
            )
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_WRITE:
            bareosfd.DebugMessage(
                100, "IO_WRITE: %s\n" % (self.FNAME)
            )
            try:
                self.vzdump_process.stdin.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0

            except IOError as e:
                bareosfd.DebugMessage(100, "plugin_io[IO_WRITE]: IOError: %s\n" % (e))
                #self.vadp.end_dumper()
                IOP.status = 0
                IOP.io_errno = e.errno
                return bareosfd.bRC_Error

            return bareosfd.bRC_OK

    def end_backup_file(self):

        bareosfd.DebugMessage(100, "end_backup_file() called\n")

        # print rest of vzdump log 
        for line in self.current_logfile.readlines():
            bareosfd.JobMessage( bareosfd.M_INFO, line)
        
        # check for return code
        if self.vzdump_process.returncode:
            bareosfd.DebugMessage(
                100,
                "end_backup_file() bareos_vadp_dumper returncode: %s\n"
                % (bareos_vadp_dumper_returncode),
            )
            return bareosfd.bRC_ERR

        bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK
