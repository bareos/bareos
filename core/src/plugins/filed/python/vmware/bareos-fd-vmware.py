#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2024 Bareos GmbH & Co. KG
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
#
# Author: Stephan Duehr
# Renout Gerrits, Oct 2017, added thumbprint
#
# Bareos-fd-vmware is a python Bareos FD Plugin intended to backup and
# restore VMware images and configuration

import json
import os
import io
import time
import tempfile
import subprocess
import shlex
import signal
import ssl
import socket
import hashlib
import re
import traceback
from sys import version_info
import urllib3.util
import requests

try:
    import configparser
except ImportError:
    from six.moves import configparser

from pyVim.connect import SmartConnect, Disconnect
from pyVmomi import vim
from pyVmomi import vmodl
import pyVim.task
from pyVmomi.VmomiSupport import VmomiJSONEncoder

# if OrderedDict is not available from collection (eg. SLES11),
# the additional package python-ordereddict must be used
try:
    from collections import OrderedDict
except ImportError:
    from ordereddict import OrderedDict

from BareosFdWrapper import *  # noqa
import bareosfd
import BareosFdPluginBaseclass

# Job replace code as defined in src/include/baconfig.h like this:
# #define REPLACE_ALWAYS   'a'
# #define REPLACE_IFNEWER  'w'
# #define REPLACE_NEVER    'n'
# #define REPLACE_IFOLDER  'o'
# In python, we get this in restorepkt.replace as integer.
# This may be added to bareos_fd_consts in the future:
bReplace = dict(ALWAYS=ord("a"), IFNEWER=ord("w"), NEVER=ord("n"), IFOLDER=ord("o"))


@BareosPlugin
class BareosFdPluginVMware(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """
    VMware related backup and restore BAREOS fd-plugin methods
    """

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        bareosfd.DebugMessage(
            100,
            "Python Version: %s.%s.%s\n"
            % (version_info.major, version_info.minor, version_info.micro),
        )
        super(BareosFdPluginVMware, self).__init__(plugindef)
        self.events = []
        self.events.append(bareosfd.bEventStartBackupJob)
        self.events.append(bareosfd.bEventStartRestoreJob)
        self.events.append(bareosfd.bEventEndRestoreJob)
        bareosfd.RegisterEvents(self.events)
        self.mandatory_options_default = ["vcserver", "vcuser", "vcpass"]
        self.mandatory_options_vmname = ["dc", "folder", "vmname"]
        self.optional_options = [
            "uuid",
            "vcthumbprint",
            "transport",
            "log_path",
            "localvmdk",
            "vadp_dumper_verbose",
            "verifyssl",
            "quiesce",
            "cleanup_tmpfiles",
            "restore_esxhost",
            "restore_cluster",
            "restore_resourcepool",
            "restore_datastore",
            "restore_powerstate",
            "poweron_timeout",
            "config_file",
            "snapshot_retries",
            "snapshot_retry_wait",
            "enable_cbt",
            "do_io_in_core",
            "vadp_dumper_multithreading",
            "vadp_dumper_sectors_per_call",
            "vadp_dumper_query_allocated_blocks_chunk_size",
        ]
        self.allowed_options = (
            self.mandatory_options_default
            + self.mandatory_options_vmname
            + self.optional_options
        )
        self.utf8_options = ["vmname", "folder"]
        self.config = None
        self.config_parsed = False

        self.vadp = BareosVADPWrapper()
        self.vadp.plugin = self
        self.vm_config_info_saved = False
        self.current_object_index = int(time.time())
        self.do_io_in_core = True
        self.data_stream = None

    def parse_plugin_definition(self, plugindef):
        """
        Parses the plugin arguments
        """
        bareosfd.DebugMessage(
            100,
            "parse_plugin_definition() was called in module %s\n" % (__name__),
        )
        super(BareosFdPluginVMware, self).parse_plugin_definition(plugindef)

        # if the option config_file is present, parse the given file
        config_file = self.options.get("config_file")
        if config_file:
            if not self.parse_config_file():
                return bareosfd.bRC_Error

        self.vadp.options = self.options

        return bareosfd.bRC_OK

    def parse_config_file(self):
        """
        Parse the config file given in the config_file plugin option
        """
        if self.config_parsed:
            return True

        bareosfd.DebugMessage(
            100,
            "BareosFdPluginVMware: parse_config_file(): parse %s\n"
            % (self.options["config_file"]),
        )

        self.config = configparser.ConfigParser()

        try:
            self.config.readfp(open(self.options["config_file"]))
        except IOError as err:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "BareosFdPluginVMware: Error reading config file %s: %s\n"
                % (self.options["config_file"], err.strerror),
            )
            return False

        self.config_parsed = True

        return self.check_config()

    def check_config(self):
        """
        Check the configuration and set or override options if necessary
        """
        bareosfd.DebugMessage(100, "BareosFdPluginVMware: check_config()\n")
        mandatory_sections = ["vmware_plugin_options"]

        for section in mandatory_sections:
            if not self.config.has_section(section):
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "BareosFdPluginVMware: Section [%s] missing in config file %s\n"
                    % (section, self.options["config_file"]),
                )
                return False

            for option in self.config.options(section):
                if option not in self.allowed_options:
                    bareosfd.JobMessage(
                        bareosfd.M_FATAL,
                        "BareosFdPluginVMware: Invalid option %s in Section [%s] in config file %s\n"
                        % (option, section, self.options["config_file"]),
                    )
                    return False

                plugin_option = self.options.get(option)
                if plugin_option:
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        "BareosFdPluginVMware: Overriding plugin option %s:%s from config file %s\n"
                        % (option, repr(plugin_option), self.options["config_file"]),
                    )

                self.options[option] = self.config.get(section, option)

        return True

    def check_options(self, mandatory_options=None):
        """
        Check Plugin options
        Note: this is called by parent class parse_plugin_definition(),
        to handle plugin options entered at restore, the real check
        here is done by check_plugin_options() which is called from
        start_backup_job() and start_restore_job()
        """
        return bareosfd.bRC_OK

    def check_plugin_options(self, mandatory_options=None):
        """
        Check Plugin options
        """
        bareosfd.DebugMessage(
            100, "BareosFdPluginVMware:check_plugin_options() called\n"
        )

        if mandatory_options is None:
            # not passed, use default
            mandatory_options = self.mandatory_options_default

        if "uuid" in self.options:
            disallowed_options = list(
                set(self.options).intersection(set(self.mandatory_options_vmname))
            )
            if disallowed_options:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Option(s) '%s' not allow with uuid.\n"
                    % ",".join(disallowed_options),
                )
                return bareosfd.bRC_Error

        else:
            mandatory_options = list(
                set(mandatory_options).union(set(self.mandatory_options_vmname))
            )

        if not self._check_option_boolean("localvmdk"):
            return bareosfd.bRC_Error

        if self.options.get("localvmdk") == "yes":
            mandatory_options = list(
                set(mandatory_options)
                - set(["vcserver", "vcuser", "vcpass", "folder", "dc", "vmname"])
            )

        for option in mandatory_options:
            if option not in self.options:
                bareosfd.DebugMessage(100, "Option '%s' not defined.\n" % (option))
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Option '%s' not defined.\n" % option,
                )

                return bareosfd.bRC_Error

        invalid_options = ",".join(list(set(self.options) - set(self.allowed_options)))
        if invalid_options:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "BareosFdPluginVMware: Invalid plugin options: %s\n"
                % (invalid_options),
            )
            return bareosfd.bRC_Error

        for option in self.utf8_options:
            if self.options.get(option):
                # make sure to convert to utf8
                bareosfd.DebugMessage(
                    100,
                    "Type of option %s is %s\n" % (option, type(self.options[option])),
                )
                bareosfd.DebugMessage(
                    100,
                    "Converting Option %s=%s to utf8\n"
                    % (option, self.options[option]),
                )
                self.options[option] = str(self.options[option])
                bareosfd.DebugMessage(
                    100,
                    "Type of option %s is %s\n" % (option, type(self.options[option])),
                )

        # strip trailing "/" from folder option value value
        if "folder" in self.options:
            self.options["folder"] = self.vadp.proper_path(self.options["folder"])

        if self.options.get("restore_powerstate"):
            if self.options["restore_powerstate"] not in ["on", "off", "previous"]:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Invalid value '%s' for restore_powerstate, valid: on, off or previous.\n"
                    % self.options["restore_powerstate"],
                )
                return bareosfd.bRC_Error
        else:
            # restore previous power state by default
            self.options["restore_powerstate"] = "previous"

        if not self._check_option_integer_positive("poweron_timeout"):
            return bareosfd.bRC_Error

        if self.options.get("poweron_timeout"):
            self.vadp.poweron_timeout = int(self.options["poweron_timeout"])

        if not self._check_option_integer_positive("snapshot_retries"):
            return bareosfd.bRC_Error

        if self.options.get("snapshot_retries"):
            self.vadp.snapshot_retries = int(self.options["snapshot_retries"])

        if not self._check_option_integer_positive("snapshot_retry_wait"):
            return bareosfd.bRC_Error

        if self.options.get("snapshot_retry_wait"):
            self.vadp.snapshot_retry_wait = int(self.options["snapshot_retry_wait"])

        if not self._check_option_boolean("enable_cbt"):
            return bareosfd.bRC_Error

        if self.options.get("enable_cbt") == "no":
            self.vadp.enable_cbt = False

        if not self._check_option_boolean("do_io_in_core"):
            return bareosfd.bRC_Error

        if self.options.get("do_io_in_core") == "no":
            self.do_io_in_core = False

        if not self._check_option_boolean("vadp_dumper_verbose"):
            return bareosfd.bRC_Error

        if self.options.get("vadp_dumper_verbose") == "yes":
            self.vadp.dumper_verbose = True

        if not self._check_option_boolean("vadp_dumper_multithreading"):
            return bareosfd.bRC_Error

        if self.options.get("vadp_dumper_multithreading") == "no":
            self.vadp.dumper_multithreading = False

        if not self._check_option_integer_positive("vadp_dumper_sectors_per_call"):
            return bareosfd.bRC_Error

        if self.options.get("vadp_dumper_sectors_per_call"):
            self.vadp.dumper_sectors_per_call = int(
                self.options["vadp_dumper_sectors_per_call"]
            )

        if not self._check_option_integer_positive(
            "vadp_dumper_query_allocated_blocks_chunk_size"
        ):
            return bareosfd.bRC_Error

        if self.options.get("vadp_dumper_query_allocated_blocks_chunk_size"):
            self.vadp.dumper_query_allocated_blocks_chunk_size = int(
                self.options["vadp_dumper_query_allocated_blocks_chunk_size"]
            )

        for option in self.options:
            bareosfd.DebugMessage(
                100,
                "Using Option %s=%s\n"
                % (option, StringCodec.encode(self.options[option])),
            )

        if not self.options.get("localvmdk") == "yes":
            if "vcthumbprint" in self.options:
                # fetch the thumbprint and compare it with the passed thumbprint,
                # warn if they don't match as VixDiskLib will return "unknown error"
                # when running bareos-vadp-dumper
                if not self.vadp.fetch_vcthumbprint():
                    return bareosfd.bRC_Error
                if self.options["vcthumbprint"] != self.vadp.fetched_vcthumbprint:
                    bareosfd.JobMessage(
                        bareosfd.M_ERROR,
                        "The configured vcthumbprint %s does not match the server thumbprint %s, "
                        "check and update your config! Otherwise bareos_vadp_dumper will get the "
                        "error message 'Unknown error' from the API\n"
                        % (
                            self.options["vcthumbprint"],
                            self.vadp.fetched_vcthumbprint,
                        ),
                    )

            else:
                # if vcthumbprint is not given in options, retrieve it
                if not self.vadp.retrieve_vcthumbprint():
                    return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def _check_option_boolean(self, option_name):
        """
        Check a boolean option for valid values.
        If a value is given, it must be "yes" or "no",
        otherwise the calling code must decide what's the default.
        """
        if self.options.get(option_name) is None:
            return True
        if self.options[option_name] not in ("yes", "no"):
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Invalid value '%s' for option %s, valid: yes or no.\n"
                % (self.options[option_name], option_name),
            )
            return False
        return True

    def _check_option_integer_positive(self, option_name):
        """
        Check an integer option for valid and positive values.
        If a value is given, it must be a valid integer and greater than 0,
        otherwise the calling code must decide what's the default.
        """
        if self.options.get(option_name) is None:
            return True

        integer_value = None

        try:
            integer_value = int(self.options[option_name])
        except ValueError:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Invalid value '%s' for %s, only digits allowed\n"
                % (self.options[option_name], option_name),
            )
            return False

        if integer_value <= 0:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Invalid value '%s' for %s, only positive values allowed\n"
                % (self.options[option_name], option_name),
            )
            return False

        return True

    def start_backup_job(self):
        """
        Start of Backup Job. Called just before backup job really start.
        Overload this to arrange whatever you have to do at this time.
        """
        bareosfd.DebugMessage(100, "BareosFdPluginVMware:start_backup_job() called\n")

        check_option_bRC = self.check_plugin_options()
        if check_option_bRC != bareosfd.bRC_OK:
            return check_option_bRC

        if not self.vadp.connect_vmware():
            return bareosfd.bRC_Error

        if "uuid" in self.options:
            self.vadp.backup_path = "/VMS/%s" % (self.options["uuid"])
        else:
            self.vadp.backup_path = self.vadp.proper_path(
                "/VMS/%s/%s/%s"
                % (
                    self.options["dc"],
                    self.options["folder"],
                    self.options["vmname"],
                )
            )

        return self.vadp.prepare_vm_backup()

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(100, "BareosFdPluginVMware:start_backup_file() called\n")

        if not self.vadp.files_to_backup:
            if not self.vadp.disk_devices:
                bareosfd.JobMessage(bareosfd.M_FATAL, "No disk devices found\n")
                return bareosfd.bRC_Error

            self.vadp.disk_device_to_backup = self.vadp.disk_devices.pop(0)
            self.vadp.files_to_backup = []

            self.vadp.files_to_backup.append(
                "%s/%s"
                % (
                    self.vadp.backup_path,
                    self.vadp.disk_device_to_backup["fileNameRoot"],
                )
            )

            self.vadp.files_to_backup.insert(
                0, self.vadp.files_to_backup[0] + "_cbt.json"
            )

        self.vadp.file_to_backup = self.vadp.files_to_backup.pop(0)

        bareosfd.DebugMessage(
            100,
            "file: %s\n" % (StringCodec.encode(self.vadp.file_to_backup)),
        )

        if self.vadp.file_to_backup.endswith("vm_config.json"):
            self.create_restoreobject_savepacket(
                savepkt,
                StringCodec.encode(self.vadp.file_to_backup),
                bytearray(self.vadp.vm_config_info_json, "utf-8"),
            )

        elif self.vadp.file_to_backup.endswith("vm_info.json"):
            self.create_restoreobject_savepacket(
                savepkt,
                StringCodec.encode(self.vadp.file_to_backup),
                bytearray(self.vadp.vm_info_json, "utf-8"),
            )
        elif self.vadp.file_to_backup.endswith("vm_metadata.json"):
            self.create_restoreobject_savepacket(
                savepkt,
                StringCodec.encode(self.vadp.file_to_backup),
                bytearray(self.vadp.vm_metadata_json, "utf-8"),
            )

        elif self.vadp.file_to_backup.endswith("_cbt.json"):
            if not self.vadp.get_vm_disk_cbt():
                return bareosfd.bRC_Error

            self.create_restoreobject_savepacket(
                savepkt,
                StringCodec.encode(self.vadp.file_to_backup[: -len("_cbt.json")]),
                bytearray(self.vadp.changed_disk_areas_json, "utf-8"),
            )

        elif self.vadp.file_to_backup.endswith(".nvram"):
            savepkt.statp = bareosfd.StatPacket()
            savepkt.fname = StringCodec.encode(self.vadp.file_to_backup)
            savepkt.type = bareosfd.FT_REG

        else:
            # start bareos_vadp_dumper
            self.vadp.start_dumper("dump")

            # create a regular stat packet
            statp = bareosfd.StatPacket()
            savepkt.statp = statp
            savepkt.fname = StringCodec.encode(self.vadp.file_to_backup)
            if chr(self.level) != "F":
                # add level and timestamp to fname in savepkt
                savepkt.fname = "%s+%s+%s" % (
                    StringCodec.encode(self.vadp.file_to_backup),
                    chr(self.level),
                    repr(self.vadp.create_snap_tstamp),
                )
            savepkt.type = bareosfd.FT_REG

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            "Starting backup of %s\n" % StringCodec.encode(self.vadp.file_to_backup),
        )
        return bareosfd.bRC_OK

    def start_restore_job(self):
        """
        Start of Restore Job. Called , if you have Restore objects.
        Overload this to handle restore objects, if applicable
        """
        bareosfd.DebugMessage(100, "BareosFdPluginVMware:start_restore_job() called\n")

        check_option_bRC = self.check_plugin_options()
        if check_option_bRC != bareosfd.bRC_OK:
            return check_option_bRC

        if (
            not self.options.get("localvmdk") == "yes"
            and not self.vadp.connect_vmware()
        ):
            return bareosfd.bRC_Error

        return self.vadp.prepare_vm_restore()

    def start_restore_file(self, cmd):
        bareosfd.DebugMessage(
            100,
            "BareosFdPluginVMware:start_restore_file() called with %s\n" % (cmd),
        )
        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """
        Assumes that the virtual disk already
        exists with the same name that has been backed up.
        This should work for restoring the same VM.
        For restoring to a new different VM, additional steps
        must be taken, because the disk path will be different.
        """
        bareosfd.DebugMessage(
            100,
            "BareosFdPluginVMware:create_file() called with %s\n" % (restorepkt),
        )

        if restorepkt.ofname.endswith(".nvram"):
            bareosfd.DebugMessage(
                100,
                "BareosFdPluginVMware:create_file() restoring NVRAM file %s\n"
                % (os.path.basename(restorepkt.ofname)),
            )
            restorepkt.create_status = bareosfd.CF_EXTRACT
            return bareosfd.bRC_OK

        tmp_path = "/var/tmp/bareos-vmware-plugin"
        if restorepkt.where != "":
            objectname = "/" + os.path.relpath(restorepkt.ofname, restorepkt.where)
        else:
            objectname = restorepkt.ofname

        json_filename = tmp_path + objectname + "_cbt.json"
        # for now, restore requires no snapshot to exist so disk to
        # be written must be the the root-disk, even if a manual snapshot
        # existed when the backup was run. So the diskPath in JSON will
        # be set to diskPathRoot
        cbt_data = self.vadp.restore_objects_by_objectname[objectname]["data"]

        if self.vadp.restore_disk_paths_map:
            cbt_data["DiskParams"]["diskPathRoot"] = self.vadp.restore_disk_paths_map[
                cbt_data["DiskParams"]["diskPathRoot"]
            ]

        cbt_data["DiskParams"]["diskPath"] = cbt_data["DiskParams"]["diskPathRoot"]

        # If a new VM was created, its VM moref must be written to the JSON file for vadp dumper
        # instead of the VM moref that was stored at backup time
        if self.vadp.vm:
            cbt_data["ConnParams"]["VmMoRef"] = "moref=" + self.vadp.vm._GetMoId()

        self.vadp.writeStringToFile(json_filename, json.dumps(cbt_data))
        self.cbt_json_local_file_path = json_filename

        if self.options.get("localvmdk") == "yes":
            self.vadp.restore_vmdk_file = (
                restorepkt.where + "/" + cbt_data["DiskParams"]["diskPathRoot"]
            )
            # check if this is the "Full" part of restore, for inc/diff the
            # the restorepkt.ofname has trailing "+I+..." or "+D+..."
            if os.path.basename(self.vadp.restore_vmdk_file) == os.path.basename(
                restorepkt.ofname
            ):
                if os.path.exists(self.vadp.restore_vmdk_file):
                    if restorepkt.replace in (bReplace["IFNEWER"], bReplace["IFOLDER"]):
                        bareosfd.JobMessage(
                            bareosfd.M_FATAL,
                            "This Plugin only implements Replace Mode 'Always' or 'Never'\n",
                        )
                        self.vadp.cleanup_tmp_files()
                        return bareosfd.bRC_Error

                    if restorepkt.replace == bReplace["NEVER"]:
                        bareosfd.JobMessage(
                            bareosfd.M_FATAL,
                            "File %s exist, but Replace Mode is 'Never'\n"
                            % (StringCodec.encode(self.vadp.restore_vmdk_file)),
                        )
                        self.vadp.cleanup_tmp_files()
                        return bareosfd.bRC_Error

                    # Replace Mode is ALWAYS if we get here
                    try:
                        os.unlink(self.vadp.restore_vmdk_file)
                    except OSError as e:
                        bareosfd.JobMessage(
                            bareosfd.M_FATAL,
                            "Error deleting File %s cause: %s\n"
                            % (
                                StringCodec.encode(self.vadp.restore_vmdk_file),
                                e.strerror,
                            ),
                        )
                        self.vadp.cleanup_tmp_files()
                        return bareosfd.bRC_Error

        if not self.vadp.start_dumper("restore"):
            return bareosfd.bRC_ERROR

        if restorepkt.type == bareosfd.FT_REG:
            restorepkt.create_status = bareosfd.CF_EXTRACT
        return bareosfd.bRC_OK

    def check_file(self, fname):
        bareosfd.DebugMessage(
            100,
            "BareosFdPluginVMware:check_file() called with fname %s\n"
            % (StringCodec.encode(fname)),
        )
        return bareosfd.bRC_Seen

    def plugin_io_open_nvram(self, IOP):
        """
        Plugin IO funtion to open NVRAM file
        """
        # There is no real file to be opened here. For the backup case, the
        # NVRAM file content has already been downloaded from the datastore
        # at this stage and its content is stored in the variable
        # self.vadp.vm_nvram_content.
        # For restore, the data will first be stored in the same variable,
        # then it must be uploaded to the datastore.
        bareosfd.DebugMessage(
            100,
            (
                "BareosFdPluginVMware:plugin_io_open_nvram() called with function %s"
                " IOP.count=%s, self.FNAME is set to %s\n"
            )
            % (IOP.func, IOP.count, self.FNAME),
        )

        if IOP.flags & (os.O_CREAT | os.O_WRONLY):
            bareosfd.DebugMessage(
                100,
                "BareosFdPluginVMware:plugin_io_open_nvram(): Restoring %s\n"
                % (self.FNAME),
            )
            self.data_stream = io.BytesIO()
        else:
            bareosfd.DebugMessage(
                100,
                "BareosFdPluginVMware:plugin_io_open_nvram(): Backing up %s\n"
                % (self.FNAME),
            )
            self.data_stream = io.BytesIO(self.vadp.vm_nvram_content)

        IOP.status = bareosfd.iostat_do_in_plugin
        return bareosfd.bRC_OK

    def plugin_io_close_nvram(self, IOP):
        """
        Plugin IO funtion to close NVRAM file
        """
        bareosfd.DebugMessage(
            100,
            ("plugin_io[IO_CLOSE]: was called for %s, jobtype %s\n")
            % (self.FNAME, self.jobType),
        )

        if self.jobType == "R":
            self.vadp.vm_nvram_content = self.data_stream.getvalue()
            if self.vadp.restore_vm_created:
                self.vadp.put_vm_nvram()

        bareosfd.DebugMessage(
            100,
            ("plugin_io[IO_CLOSE]: size of %s is %s, sha1sum: %s\n")
            % (
                os.path.basename(self.FNAME),
                len(self.vadp.vm_nvram_content),
                hashlib.sha1(self.vadp.vm_nvram_content).hexdigest(),
            ),
        )
        self.data_stream = None

        return bareosfd.bRC_OK

    def plugin_io(self, IOP):
        bareosfd.DebugMessage(
            200,
            (
                "BareosFdPluginVMware:plugin_io() called with function %s"
                " IOP.count=%s, self.FNAME is set to %s\n"
            )
            % (IOP.func, IOP.count, self.FNAME),
        )

        self.vadp.keepalive()

        if IOP.func == bareosfd.IO_OPEN:
            self.FNAME = IOP.fname
            bareosfd.DebugMessage(
                100, "self.FNAME was set to %s from IOP.fname\n" % (self.FNAME)
            )

            if self.FNAME.endswith(".nvram"):
                return self.plugin_io_open_nvram(IOP)

            try:
                if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                    # this is a restore

                    bareosfd.DebugMessage(
                        100,
                        "Open file %s for writing with %s\n" % (self.FNAME, IOP),
                    )

                    # create_file() should have started
                    # bareos_vadp_dumper, check:
                    # if self.vadp.dumper_process:
                    if self.vadp.check_dumper():
                        bareosfd.DebugMessage(
                            100,
                            ("plugin_io: bareos_vadp_dumper running with PID %s\n")
                            % (self.vadp.dumper_process.pid),
                        )
                        if self.do_io_in_core:
                            bareosfd.DebugMessage(
                                100,
                                (
                                    "plugin_io: doing IO in core for restore, filedescriptor: %s\n"
                                )
                                % (self.vadp.dumper_process.stdin.fileno()),
                            )
                            IOP.filedes = self.vadp.dumper_process.stdin.fileno()
                            IOP.status = bareosfd.iostat_do_in_core
                    else:
                        bareosfd.JobMessage(
                            bareosfd.M_FATAL,
                            "plugin_io: bareos_vadp_dumper not running\n",
                        )
                        return bareosfd.bRC_Error

                else:
                    bareosfd.DebugMessage(
                        100,
                        "plugin_io: trying to open %s for reading\n" % (self.FNAME),
                    )
                    # this is a backup
                    # start_backup_file() should have started
                    # bareos_vadp_dumper, check:
                    if self.vadp.dumper_process:
                        bareosfd.DebugMessage(
                            100,
                            ("plugin_io: bareos_vadp_dumper running with" " PID %s\n")
                            % (self.vadp.dumper_process.pid),
                        )
                        if self.do_io_in_core:
                            bareosfd.DebugMessage(
                                100,
                                (
                                    "plugin_io: doing IO in core for backup, filedescriptor: %s\n"
                                )
                                % (self.vadp.dumper_process.stdout.fileno()),
                            )
                            IOP.filedes = self.vadp.dumper_process.stdout.fileno()
                            IOP.status = bareosfd.iostat_do_in_core
                    else:
                        bareosfd.JobMessage(
                            bareosfd.M_FATAL,
                            "plugin_io: bareos_vadp_dumper not running\n",
                        )
                        return bareosfd.bRC_Error

            except OSError as os_error:
                IOP.status = -1
                bareosfd.DebugMessage(
                    100,
                    "plugin_io: failed to open %s: %s\n"
                    % (self.FNAME, os_error.strerror),
                )
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "plugin_io: failed to open %s: %s\n"
                    % (self.FNAME, os_error.strerror),
                )
                return bareosfd.bRC_Error

            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_CLOSE:
            if self.FNAME.endswith(".nvram"):
                return self.plugin_io_close_nvram(IOP)

            if self.jobType == "B":
                # Backup Job
                bareosfd.DebugMessage(
                    100,
                    (
                        "plugin_io: calling end_dumper() to wait for"
                        " PID %s to terminate\n"
                    )
                    % (self.vadp.dumper_process.pid),
                )
                bareos_vadp_dumper_returncode = self.vadp.end_dumper()
                if bareos_vadp_dumper_returncode != 0:
                    bareosfd.JobMessage(
                        bareosfd.M_FATAL,
                        (
                            "plugin_io[IO_CLOSE]: bareos_vadp_dumper returncode:"
                            " %s error output:\n%s\n"
                        )
                        % (bareos_vadp_dumper_returncode, self.vadp.get_dumper_err()),
                    )
                    return bareosfd.bRC_Error

            elif self.jobType == "R":
                # Restore Job
                bareosfd.DebugMessage(
                    100,
                    "Closing Pipe to bareos_vadp_dumper for %s\n" % (self.FNAME),
                )
                if self.vadp.dumper_process:
                    self.vadp.dumper_process.stdin.close()
                bareosfd.DebugMessage(
                    100,
                    (
                        "plugin_io: calling end_dumper() to wait for"
                        " PID %s to terminate\n"
                    )
                    % (self.vadp.dumper_process.pid),
                )
                bareos_vadp_dumper_returncode = self.vadp.end_dumper()
                if bareos_vadp_dumper_returncode != 0:
                    bareosfd.JobMessage(
                        bareosfd.M_FATAL,
                        ("plugin_io[IO_CLOSE]: bareos_vadp_dumper returncode:" " %s\n")
                        % (bareos_vadp_dumper_returncode),
                    )
                    return bareosfd.bRC_Error

            else:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "plugin_io[IO_CLOSE]: unknown Job Type %s\n" % (self.jobType),
                )
                return bareosfd.bRC_Error

            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_SEEK:
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_READ:
            IOP.buf = bytearray(IOP.count)
            if self.data_stream:
                # backup nvram file
                bareosfd.DebugMessage(
                    100,
                    "plugin_io[IO_READ]: backup nvram: %s IOP.count=%s\n"
                    % (os.path.basename(self.FNAME), IOP.count),
                )
                IOP.status = self.data_stream.readinto(IOP.buf)
                bareosfd.DebugMessage(
                    100,
                    "plugin_io[IO_READ]: backup nvram: IOP.status=%s IOP.io_errno=%s\n"
                    % (IOP.status, IOP.io_errno),
                )
            else:
                IOP.status = self.vadp.dumper_process.stdout.readinto(IOP.buf)
            IOP.io_errno = 0

            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_WRITE:
            if self.data_stream:
                # restore nvram file
                bareosfd.DebugMessage(
                    100,
                    "plugin_io[IO_WRITE]: restore nvram: %s\n"
                    % (os.path.basename(self.FNAME)),
                )
                IOP.status = self.data_stream.write(IOP.buf)
                bareosfd.DebugMessage(
                    100,
                    "plugin_io[IO_WRITE]: restore nvram: IOP.status=%s IOP.io_errno=%s\n"
                    % (IOP.status, IOP.io_errno),
                )
                IOP.io_errno = 0
                return bareosfd.bRC_OK

            try:
                self.vadp.dumper_process.stdin.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as e:
                bareosfd.DebugMessage(100, "plugin_io[IO_WRITE]: IOError: %s\n" % (e))
                self.vadp.end_dumper()
                IOP.status = 0
                IOP.io_errno = e.errno
                return bareosfd.bRC_Error
            return bareosfd.bRC_OK

    def handle_plugin_event(self, event):
        if event in self.events:
            self.jobType = chr(bareosfd.GetValue(bareosfd.bVarType))
            bareosfd.DebugMessage(100, "jobType: %s\n" % (self.jobType))

        if event == bareosfd.bEventJobEnd:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with bEventJobEnd\n"
            )
            bareosfd.DebugMessage(
                100,
                "Disconnecting from VSphere API on host %s with user %s\n"
                % (self.options["vcserver"], self.options["vcuser"]),
            )

            self.vadp.cleanup()

        elif event == bareosfd.bEventEndBackupJob:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with bEventEndBackupJob\n"
            )

            # This is called even if the VM wasn't found and a fatal job
            # message was emitted, so if no snapshot was taken, there's
            # nothing to do here.
            if not self.vadp.create_snap_tstamp:
                return bareosfd.bRC_OK

            bareosfd.DebugMessage(100, "removing Snapshot\n")
            # Normal snapshot removal usually failed when there were long
            # delays during backup, so that keepalive had to reconnect
            # to vCenter. Retrieving the VM details again and cleaning
            # up any snapshots taken by this plugin is more reliable.
            self.vadp.get_vm_details()

            if self.options.get("cleanup_tmpfiles") == "no":
                bareosfd.DebugMessage(
                    100,
                    "not deleting snapshot, cleanup_tmpfiles is set to no\n",
                )
            else:
                self.vadp.cleanup_vm_snapshots()

        elif event == bareosfd.bEventEndFileSet:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with bEventEndFileSet\n"
            )

        elif event == bareosfd.bEventStartBackupJob:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with bEventStartBackupJob\n"
            )

            return self.start_backup_job()

        elif event == bareosfd.bEventStartRestoreJob:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event() called with bEventStartRestoreJob\n",
            )

            return self.start_restore_job()

        elif event == bareosfd.bEventEndRestoreJob:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event() called with bEventEndRestoreJob\n",
            )

            if self.vadp.vm:
                return self.vadp.restore_power_state()

            return bareosfd.bRC_OK

        else:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with unexpected event %s\n" % (event)
            )

        return bareosfd.bRC_OK

    def end_backup_file(self):
        bareosfd.DebugMessage(100, "BareosFdPluginVMware:end_backup_file() called\n")
        if self.vadp.disk_devices or self.vadp.files_to_backup:
            bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_More\n")
            return bareosfd.bRC_More

        bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK

    def restore_object_data(self, ROP):
        """
        Note:
        This is called in two cases:
        - on diff/inc backup (should be called only once)
        - on restore (for every job id being restored)
        But at the point in time called, it is not possible
        to distinguish which of them it is, because job type
        is "I" until the bEventStartBackupJob event
        """
        bareosfd.DebugMessage(
            100,
            "BareosFdPluginVMware:restore_object_data() called with ROP:%s\n" % (ROP),
        )
        bareosfd.DebugMessage(
            100,
            "ROP.object_name(%s): %s\n" % (type(ROP.object_name), ROP.object_name),
        )
        bareosfd.DebugMessage(
            100,
            "ROP.plugin_name(%s): %s\n" % (type(ROP.plugin_name), ROP.plugin_name),
        )
        bareosfd.DebugMessage(
            100,
            "ROP.object_len(%s): %s\n" % (type(ROP.object_len), ROP.object_len),
        )
        bareosfd.DebugMessage(
            100,
            "ROP.object_full_len(%s): %s\n"
            % (type(ROP.object_full_len), ROP.object_full_len),
        )
        bareosfd.DebugMessage(
            100, "ROP.object(%s): %s\n" % (type(ROP.object), ROP.object)
        )

        if "vm_config.json" in ROP.object_name:
            self.vadp.restore_vm_config_json = ROP.object
            return bareosfd.bRC_OK

        if "vm_info.json" in ROP.object_name:
            self.vadp.restore_vm_info_json = ROP.object
            return bareosfd.bRC_OK

        if "vm_metadata.json" in ROP.object_name:
            self.vadp.restore_vm_metadata_json = ROP.object
            return bareosfd.bRC_OK

        ro_data = self.vadp.json2cbt(ROP.object)
        ro_filename = ro_data["DiskParams"]["diskPathRoot"]
        # self.vadp.restore_objects_by_diskpath is used on backup
        # in get_vm_disk_cbt()
        # self.vadp.restore_objects_by_objectname is used on restore
        # by create_file()
        self.vadp.restore_objects_by_diskpath[ro_filename] = []
        self.vadp.restore_objects_by_diskpath[ro_filename].append(
            {"json": ROP.object, "data": ro_data}
        )
        self.vadp.restore_objects_by_objectname[ROP.object_name] = (
            self.vadp.restore_objects_by_diskpath[ro_filename][-1]
        )
        return bareosfd.bRC_OK

    def create_restoreobject_savepacket(self, savepkt, virtual_filename, object_data):
        """
        Create a savepacket for a restore object
        """
        # create a stat packet for a restore object
        statp = bareosfd.StatPacket()
        savepkt.statp = statp
        savepkt.type = bareosfd.FT_RESTORE_FIRST
        # set the fname of the restore object to the vmdk name
        # by stripping of the _cbt.json suffix
        if chr(self.level) != "F":
            # add level and timestamp to fname in savepkt
            savepkt.fname = "%s+%s+%s" % (
                virtual_filename,
                chr(self.level),
                repr(self.vadp.create_snap_tstamp),
            )
        else:
            savepkt.fname = virtual_filename

        savepkt.object_name = savepkt.fname
        savepkt.object = object_data
        savepkt.object_len = len(savepkt.object)
        savepkt.object_index = self.get_next_object_index()

    def get_next_object_index(self):
        """
        Get the current object index and increase it
        """
        self.current_object_index += 1
        return self.current_object_index


class BareosVADPWrapper(object):
    """
    VADP specific class.
    """

    def __init__(self):
        self.si = None
        self.si_last_keepalive = None
        self.vm = None
        self.create_snap_task = None
        self.create_snap_result = None
        self.create_snap_tstamp = None
        self.file_to_backup = None
        self.files_to_backup = []
        self.disk_devices = None
        self.disk_device_to_backup = None
        self.cbt_json_local_file_path = None
        self.dumper_process = None
        self.dumper_stderr_log = None
        self.changed_disk_areas_json = None
        self.restore_objects_by_diskpath = {}
        self.restore_objects_by_objectname = {}
        self.options = None
        self.skip_disk_modes = ["independent_nonpersistent", "independent_persistent"]
        self.restore_vmdk_file = None
        self.plugin = None
        self.vm_config_info_json = None
        self.backup_path = None
        self.vm_config_info_json = None
        self.vm_info_json = None
        self.vm_metadata_json = None
        self.restore_vm_config_json = None
        self.restore_vm_info_json = None
        self.restore_vm_info = None
        self.restore_vm_metadata_json = None
        self.restore_vm_metadata = None
        self.dc = None
        self.vmfs_vm_path_changed = False
        self.vmfs_vm_path = None
        self.datastore_vm_file_rex = re.compile(r"\[(.+?)\] (.+)")
        self.slashes_rex = re.compile(r"\/+")
        self.poweron_timeout = 15
        self.restore_disk_paths_map = OrderedDict()
        self.fetched_vcthumbprint = None
        self.snapshot_retries = 3
        self.snapshot_retry_wait = 5
        self.snapshot_prefix = "BareosTmpSnap_jobId"
        self.enable_cbt = True
        self.dumper_verbose = False
        self.dumper_multithreading = True
        self.dumper_sectors_per_call = 16384
        self.dumper_query_allocated_blocks_chunk_size = 1024
        self.vm_nvram_path = None
        self.vm_nvram_content = None
        self.restore_vm_created = False

    def connect_vmware(self):
        # this prevents from repeating on second call
        if self.si:
            bareosfd.DebugMessage(
                100,
                "connect_vmware(): connection to server %s already exists\n"
                % (self.options["vcserver"]),
            )
            return True

        bareosfd.DebugMessage(
            100,
            "connect_vmware(): connecting server %s\n" % (self.options["vcserver"]),
        )

        retry_no_ssl = False

        try:
            self.si = SmartConnect(
                host=self.options["vcserver"],
                user=self.options["vcuser"],
                pwd=self.options["vcpass"],
                port=443,
            )

        except IOError as ioerror:
            if (
                "CERTIFICATE_VERIFY_FAILED" in ioerror.strerror
                and self.options.get("verifyssl") != "yes"
            ):
                retry_no_ssl = True
            else:
                self._handle_connect_vmware_excpetion(ioerror)
        except Exception as e:
            self._handle_connect_vmware_exception(e)

        if retry_no_ssl:
            try:
                self.si = SmartConnect(
                    host=self.options["vcserver"],
                    user=self.options["vcuser"],
                    pwd=self.options["vcpass"],
                    port=443,
                    disableSslCertValidation=True,
                )

            except Exception as e:
                self._handle_connect_vmware_exception(e)

        if not self.si:
            return False

        self.si_last_keepalive = int(time.time())

        bareosfd.DebugMessage(
            100,
            ("Successfully connected to VSphere API on host %s with user %s\n")
            % (self.options["vcserver"], self.options["vcuser"]),
        )

        return True

    def _handle_connect_vmware_exception(self, caught_exception):
        """
        Handle exception from connect_vmware
        """
        bareosfd.DebugMessage(
            100,
            "Exception %s caught in connect_vmware() for host %s with user %s: %s\n"
            % (
                type(caught_exception),
                self.options["vcserver"],
                self.options["vcuser"],
                str(caught_exception),
            ),
        )

        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            "Cannot connect to host %s with user %s and password, reason: %s\n"
            % (
                self.options["vcserver"],
                self.options["vcuser"],
                str(caught_exception),
            ),
        )

    def cleanup(self):
        Disconnect(self.si)

    def keepalive(self):
        # Prevent from vSphere API timeout by calling CurrentTime() every
        # 10min (600s) to keep alive the connection and session

        # ignore keepalive if there is no connection to the API
        if self.si is None:
            return

        if int(time.time()) - self.si_last_keepalive > 600:
            try:
                self.si.CurrentTime()
            except vim.fault.NotAuthenticated as keepalive_error:
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    "keepalive failed with %s, last keepalive was %s s ago, trying to reconnect.\n"
                    % (keepalive_error.msg, time.time() - self.si_last_keepalive),
                )
                self.si = None
                self.connect_vmware()
                self.get_vm_details()

            self.si_last_keepalive = int(time.time())

    def get_vm_details(self):
        """
        Get VM details
        """
        if "uuid" in self.options:
            if not self.get_vm_details_by_uuid():
                bareosfd.DebugMessage(
                    100,
                    "Error getting details for VM with UUID %s\n"
                    % (self.options["uuid"]),
                )
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Error getting details for VM with UUID %s\n"
                    % (self.options["uuid"]),
                )
                return False
        else:
            if not self.get_vm_details_dc_folder_vmname():
                debug_message = "No VM with Folder/Name %s/%s found in DC %s\n" % (
                    self.options["folder"],
                    self.options["vmname"],
                    self.options["dc"],
                )
                bareosfd.DebugMessage(100, StringCodec.encode(debug_message))
                return False

        bareosfd.DebugMessage(
            100,
            "Successfully got details for VM %s\n" % (StringCodec.encode(self.vm.name)),
        )
        return True

    def prepare_vm_backup(self):
        """
        prepare VM backup:
        - take snapshot
        - get disk devices
        """
        if not self.get_vm_details():
            return bareosfd.bRC_Error

        # check if the VM supports CBT and that CBT is enabled
        if not self.vm.capability.changeTrackingSupported:
            bareosfd.DebugMessage(
                100,
                "Error VM %s does not support CBT\n"
                % (StringCodec.encode(self.vm.name)),
            )
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error VM %s does not support CBT\n"
                % (StringCodec.encode(self.vm.name)),
            )
            return bareosfd.bRC_Error

        if not self.vm.config.changeTrackingEnabled:
            if self.enable_cbt:
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    "Error vm %s is not CBT enabled, enabling it now.\n"
                    % (StringCodec.encode(self.vm.name)),
                )
                if self.vm.snapshot is not None:
                    bareosfd.JobMessage(
                        bareosfd.M_FATAL,
                        "Error VM %s must not have any snapshots before enabling CBT\n"
                        % (StringCodec.encode(self.vm.name)),
                    )
                    return bareosfd.bRC_Error

                cspec = vim.vm.ConfigSpec()
                cspec.changeTrackingEnabled = True
                task = self.vm.ReconfigVM_Task(cspec)
                pyVim.task.WaitForTask(task)

            else:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Error vm %s is not CBT enabled\n"
                    % (StringCodec.encode(self.vm.name)),
                )
                return bareosfd.bRC_Error

        bareosfd.DebugMessage(
            100,
            "Creating Snapshot on VM %s\n" % (StringCodec.encode(self.vm.name)),
        )

        # cleanup leftover snapshots from previous backups (if any exist)
        # before creating a new snapshot
        self.cleanup_vm_snapshots()

        if not self.create_vm_snapshot():
            bareosfd.DebugMessage(
                100,
                "Error creating snapshot on VM %s\n"
                % (StringCodec.encode(self.vm.name)),
            )
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error creating snapshot on VM %s\n"
                % (StringCodec.encode(self.vm.name)),
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(
            100,
            "Successfully created snapshot on VM %s\n"
            % (StringCodec.encode(self.vm.name)),
        )

        self.vmconfig2json()
        self.check_vmconfig_backup()

        bareosfd.DebugMessage(
            100,
            "Getting Disk Devices on VM %s from snapshot\n"
            % (StringCodec.encode(self.vm.name)),
        )
        self.get_vm_snap_disk_devices()
        if not self.disk_devices:
            bareosfd.DebugMessage(
                100,
                "Error getting Disk Devices on VM %s from snapshot\n"
                % (StringCodec.encode(self.vm.name)),
            )
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error getting Disk Devices on VM %s from snapshot\n"
                % (StringCodec.encode(self.vm.name)),
            )
            return bareosfd.bRC_Error

        if self.get_vm_nvram():
            self.files_to_backup.append(
                "%s/%s" % (self.backup_path, os.path.basename(self.vm_nvram_path))
            )

        return bareosfd.bRC_OK

    def prepare_vm_restore(self):
        """
        prepare VM restore:
        - get vm details
        - ensure vm is powered off
        - get disk devices
        """

        if self.options.get("localvmdk") == "yes":
            bareosfd.DebugMessage(
                100,
                "prepare_vm_restore(): restore to local vmdk, skipping checks\n",
            )
            return bareosfd.bRC_OK

        if "uuid" in self.options:
            if not self.get_vm_details_by_uuid():
                error_message = (
                    "No VM with UUID %s found, create VM with UUID not implemented\n"
                    % (self.options["uuid"])
                )
                bareosfd.DebugMessage(100, StringCodec.encode(error_message))
                bareosfd.JobMessage(bareosfd.M_FATAL, StringCodec.encode(error_message))
                return bareosfd.bRC_Error
        else:
            if not self.get_vm_details_dc_folder_vmname(
                vm_not_found_error_level=bareosfd.M_INFO
            ):
                debug_message = (
                    "No VM with Folder/Name %s/%s found in DC %s, recreating it now\n"
                    % (
                        self.options["folder"],
                        self.options["vmname"],
                        self.options["dc"],
                    )
                )
                bareosfd.DebugMessage(100, StringCodec.encode(debug_message))
                if not self.create_vm():
                    # job message is already done within create_vm
                    return bareosfd.bRC_Error

                self.restore_custom_fields()

        bareosfd.DebugMessage(
            100,
            "Successfully got details for VM %s\n" % (StringCodec.encode(self.vm.name)),
        )

        vm_power_state = self.vm.summary.runtime.powerState
        if vm_power_state != "poweredOff":
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error VM %s must be poweredOff for restore, but is %s\n"
                % (self.vm.name, vm_power_state),
            )
            return bareosfd.bRC_Error

        if self.vm.snapshot is not None:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error VM %s must not have any snapshots before restore\n"
                % (StringCodec.encode(self.vm.name)),
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(
            100,
            "Getting Disk Devices on VM %s\n" % (StringCodec.encode(self.vm.name)),
        )
        self.get_vm_disk_devices()
        if not self.disk_devices:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error getting Disk Devices on VM %s\n"
                % (StringCodec.encode(self.vm.name)),
            )
            return bareosfd.bRC_Error

        # make sure backed up disks match VM disks
        if not self.check_vm_disks_match():
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def get_vm_details_dc_folder_vmname(
        self, vm_not_found_error_level=bareosfd.M_FATAL
    ):
        """
        Get details of VM given by plugin options dc, folder, vmname
        and save result in self.vm
        Returns True on success, False otherwise
        """
        vm_path = self.proper_path(
            "/%s/%s" % (self.options["folder"], self.options["vmname"])
        )
        dc_vm_path = self.proper_path(
            "/%s/vm/%s/%s"
            % (self.options["dc"], self.options["folder"], self.options["vmname"])
        )
        search_index = self.si.content.searchIndex

        found_dc = search_index.FindByInventoryPath("/" + self.options["dc"])
        if not found_dc:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Could not find DC %s \n" % (StringCodec.encode(self.options["dc"])),
            )
            return False
        self.dc = found_dc

        found_vm = search_index.FindByInventoryPath(dc_vm_path)
        if not found_vm:
            bareosfd.JobMessage(
                vm_not_found_error_level,
                "Could not find VM %s in DC %s\n"
                % (StringCodec.encode(vm_path), StringCodec.encode(self.dc.name)),
            )
            return False

        self.vm = found_vm
        return True

    def get_vm_details_by_uuid(self):
        """
        Get details of VM given by plugin options uuid
        and save result in self.vm
        Returns True on success, False otherwise
        """
        search_index = self.si.content.searchIndex
        self.vm = search_index.FindByUuid(None, self.options["uuid"], True, True)
        if self.vm is None:
            return False
        else:
            return True

    def _get_dcftree(self, dcf, folder, vm_folder):
        """
        Recursive function to get VMs with folder names
        """
        for vm_or_folder in vm_folder.childEntity:
            if isinstance(vm_or_folder, vim.VirtualMachine):
                dcf[folder + "/" + vm_or_folder.name] = vm_or_folder
            elif isinstance(vm_or_folder, vim.Folder):
                self._get_dcftree(dcf, folder + "/" + vm_or_folder.name, vm_or_folder)
            elif isinstance(vm_or_folder, vim.VirtualApp):
                # vm_or_folder is a vApp in this case, contains a list a VMs
                for vapp_vm in vm_or_folder.vm:
                    dcf[folder + "/" + vm_or_folder.name + "/" + vapp_vm.name] = vapp_vm

    def _get_vm_folders(self, vm_folders, current_folder_name, current_folder):
        """
        Recursive function to get all VM folders
        """
        vm_folders[current_folder_name] = current_folder
        vm_folder_view = self.si.content.viewManager.CreateContainerView(
            current_folder, [vim.Folder], False
        )
        for vm_folder in vm_folder_view.view:
            # avoid "//" when current_folder_name is "/"
            subfolder_name = self.proper_path(
                current_folder_name + "/" + vm_folder.name
            )
            self._get_vm_folders(vm_folders, subfolder_name, vm_folder)
        vm_folder_view.Destroy()

    def find_or_create_vm_folder(self, folder_name):
        all_vm_folders = {}
        self._get_vm_folders(all_vm_folders, "/", self.dc.vmFolder)
        if folder_name not in all_vm_folders:
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "VM folder %s not found, creating it\n"
                % (StringCodec.encode(folder_name)),
            )
            # the prefixed "/" will produce an empty string element in split result,
            # so we must skip element 0
            current_folder_path = "/"
            current_folder = self.dc.vmFolder
            for folder_part in folder_name.split("/")[1:]:
                current_folder_path += "/" + folder_part
                # avoid leading "//"
                current_folder_path = self.proper_path(current_folder_path)
                if current_folder_path in all_vm_folders:
                    current_folder = all_vm_folders[current_folder_path]
                    continue
                current_folder = current_folder.CreateFolder(folder_part)
                all_vm_folders[current_folder_path] = current_folder

        return all_vm_folders[folder_name]

    def get_objects_by_folder(self, start_folder, folder_name, objects_by_folder):
        """
        Generic Recursive function to get objects by folder.
        Here, folder means a folder path, using "/" as a folder separator.
        An empty dict must be passed initially, it will be filled with
        folder path as key and managed object as value.
        """
        for current_object in start_folder.childEntity:
            if isinstance(current_object, vim.Folder):
                self.get_objects_by_folder(
                    current_object,
                    folder_name + "/" + current_object.name,
                    objects_by_folder,
                )
            else:
                objects_by_folder[
                    self.proper_path(folder_name + "/" + current_object.name)
                ] = current_object

        return

    def _get_resource_pools(self, cluster):
        """
        Get resource pools by folder like structure

        Resource pools can be nested. Each cluster has a default resource pool
        which is its resourcePool property and it's a single item.
        But the resourcePool property of the default resource pool is a list
        of resource pools, where each of this can have a list of resource pools.
        Like this, resource pools can be arbitrarily nested.
        """
        resource_pools = {}
        resource_pools["/"] = cluster.resourcePool
        stack = []
        for resource_pool in cluster.resourcePool.resourcePool:
            stack.append(("/", resource_pool))
        while stack:
            folder, resource_pool = stack.pop()
            path = self.proper_path(folder + "/" + resource_pool.name)
            resource_pools[path] = resource_pool
            for sub_resource_pool in resource_pool.resourcePool:
                stack.append((path, sub_resource_pool))

        return resource_pools

    def _get_vm_folder_path(self, vm):
        """
        Get the folder path of a given vm
        """
        current_obj = vm
        vm_folder_path = ""
        while current_obj.parent:
            current_obj = current_obj.parent
            if isinstance(current_obj.parent, vim.Datacenter):
                break
            if (
                hasattr(current_obj, "childType")
                and "VirtualMachine" in current_obj.childType
            ):
                vm_folder_path = "/" + current_obj.name + vm_folder_path

        vm_folder_path += "/" + vm.name
        return vm_folder_path

    def get_resource_pool_by_path(self, resource_pool_path, cluster):
        """
        Get resource pool by path

        As resource pools can be nested, unique specfication requires a path-like
        parameter.
        """
        resource_pools = self._get_resource_pools(cluster)
        if resource_pool_path not in resource_pools:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Resource pool %s not found in cluster %s!\n"
                % (
                    StringCodec.encode(resource_pool_path),
                    StringCodec.encode(cluster.name),
                ),
            )
            return False

        return resource_pools[resource_pool_path]

    def get_vm_snapshots_by_name(
        self, snapshots=None, snapshot_name=None, startswith_match=True
    ):
        """
        Recursive function to get snapshot of the VM
        """
        found_snapshots = []

        if snapshots is None:
            if not self.vm.snapshot:
                # no snapshots exist
                return None

            snapshots = self.vm.snapshot.rootSnapshotList

        if snapshot_name is None:
            snapshot_name = self.snapshot_prefix

        for snapshot in snapshots:
            if startswith_match:
                if snapshot.name.startswith(snapshot_name):
                    found_snapshots.append(snapshot)
            else:
                if snapshot.name == snapshot_name:
                    found_snapshots.append(snapshot)

            found_snapshots.extend(
                self.get_vm_snapshots_by_name(
                    snapshots=snapshot.childSnapshotList,
                    snapshot_name=snapshot_name,
                    startswith_match=startswith_match,
                )
            )

        return found_snapshots

    def cleanup_vm_snapshots(self):
        """
        Cleanup all existing temporary snapshots

        Remove all snapshots which have been created by this plugin.
        """
        snapshots = self.get_vm_snapshots_by_name()
        if not snapshots:
            bareosfd.DebugMessage(
                100,
                "cleanup_vm_snapshots(): No snapshots found for VM %s\n"
                % (StringCodec.encode(self.vm.name)),
            )
            return

        remove_snap_tasks = []
        for snapshot_info in snapshots:
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "Cleaning up snapshot %s from VM %s\n"
                % (
                    StringCodec.encode(snapshot_info.name),
                    StringCodec.encode(self.vm.name),
                ),
            )
            try:
                remove_snap_tasks.append(
                    snapshot_info.snapshot.RemoveSnapshot_Task(removeChildren=False)
                )
            except vmodl.MethodFault as err:
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Failed to remove snapshot %s from VM %s: %s\n"
                    % (
                        StringCodec.encode(snapshot_info.name),
                        StringCodec.encode(self.vm.name),
                        err.msg,
                    ),
                )

        pyVim.task.WaitForTasks(remove_snap_tasks)

    def create_vm(self):
        """
        Create a new VM using JSON configuration data
        """

        destination_host = None
        resource_pool = None
        datacenter = None
        create_vm_task = None
        cluster_path = None
        datastore_name = None

        config_info = json.loads(self.restore_vm_config_json)

        if self.options.get("restore_datastore"):
            datastore_name = self.options["restore_datastore"]

        # prevent from MAC address conflicts
        self.check_mac_address(config_info)

        # prevent from duplicate UUID
        self.check_uuid(config_info)

        transformer = BareosVmConfigInfoToSpec(config_info, vadp=self)
        used_datastore_names = transformer.get_datastore_names()
        if len(used_datastore_names) > 1:
            bareosfd.DebugMessage(
                100,
                "Restoring VM %s using multiple datastores: %s\n"
                % (
                    StringCodec.encode(self.options["vmname"]),
                    StringCodec.encode(used_datastore_names),
                ),
            )
            bareosfd.DebugMessage(
                100,
                "Datastores in DC %s: %s\n"
                % (
                    StringCodec.encode(self.dc.name),
                    StringCodec.encode([ds.name for ds in self.dc.datastore]),
                ),
            )

        config = transformer.transform(
            target_datastore_name=datastore_name, target_vm_name=self.options["vmname"]
        )

        if datastore_name and datastore_name != transformer.orig_vm_datastore_name:
            if not self.find_managed_object_by_name([vim.Datastore], datastore_name):
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Datastore %s not found!\n" % (StringCodec.encode(datastore_name)),
                )
                return False
            self.vmfs_vm_path_changed = True

        for child in self.si.content.rootFolder.childEntity:
            if child.name == self.options["dc"]:
                datacenter = child
                break
        else:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Datacenter %s not found!\n" % (StringCodec.encode(self.options["dc"])),
            )
            return False

        if self.options.get("restore_resourcepool") and not self.options.get(
            "restore_cluster"
        ):
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Option restore_resourcepool requires option restore_cluster!\n",
            )
            return False

        if self.options.get("restore_cluster"):
            clusters_by_folder = {}
            self.get_objects_by_folder(datacenter.hostFolder, "/", clusters_by_folder)
            cluster_path = self.proper_path(self.options["restore_cluster"])
            if cluster_path in clusters_by_folder:
                if self.options.get("restore_resourcepool"):
                    resource_pool = self.get_resource_pool_by_path(
                        self.options["restore_resourcepool"],
                        clusters_by_folder[cluster_path],
                    )
                    if not resource_pool:
                        return False

                if resource_pool is None:
                    resource_pool = clusters_by_folder[cluster_path].resourcePool

            else:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Could not find cluster %s in DC %s!\n"
                    % (cluster_path, self.options["dc"]),
                )
                return False

        if self.options.get("restore_esxhost"):
            destination_host = self.find_managed_object_by_name(
                [vim.HostSystem], self.options["restore_esxhost"]
            )

            if not destination_host:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Could not find ESX host %s!\n" % (self.options["restore_esxhost"]),
                )
                return False

        elif not self.options.get("restore_cluster"):
            # neither restore_esxhost nor restore_cluster were passed, so we must
            # use saved esx host name from restore object
            if not self.restore_vm_metadata:
                self.restore_vm_metadata = json.loads(self.restore_vm_metadata_json)

            destination_host = self.find_managed_object_by_name(
                [vim.HostSystem], self.restore_vm_metadata["esx_host_name"]
            )

            if not destination_host:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Could not find ESX host %s!\n" % (self.options["restore_esxhost"]),
                )
                return False

        if resource_pool is None:
            resource_pool = destination_host.parent.resourcePool

        target_folder = self.find_or_create_vm_folder(self.options["folder"])

        # check if target host has target datastore
        if destination_host:
            target_datastore_name = datastore_name
            if target_datastore_name is None:
                # if no restore_datastore was passed, use datastore from backup
                target_datastore_name = transformer.orig_vm_datastore_name
            if target_datastore_name not in [
                ds.name for ds in destination_host.datastore
            ]:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Datastore %s is not available on ESX host %s!\n"
                    % (target_datastore_name, destination_host.name),
                )
                return False

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            "Creating VM %s in folder %s (%s)\n"
            % (
                StringCodec.encode(self.options["vmname"]),
                StringCodec.encode(self.options["folder"]),
                StringCodec.encode(target_folder.name),
            ),
        )

        vm_create_try_count = 0
        while not self.restore_vm_created:
            vm_create_try_count += 1
            if vm_create_try_count > 5:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Giving up creating VM name %s after %s unsuccessful tries\n"
                    % (
                        StringCodec.encode(self.options["vmname"]),
                        vm_create_try_count - 1,
                    ),
                )
                return False

            try:
                bareosfd.DebugMessage(
                    100,
                    "Creating VM: %s in VMFS path %s, resource_pool: %s, destination_host: %s\n"
                    % (
                        StringCodec.encode(self.options["vmname"]),
                        config.files.vmPathName,
                        resource_pool,
                        destination_host,
                    ),
                )
                create_vm_task = target_folder.CreateVm(
                    config, pool=resource_pool, host=destination_host
                )
                pyVim.task.WaitForTask(create_vm_task)
                bareosfd.DebugMessage(
                    100,
                    "VM created: %s\n" % (StringCodec.encode(self.options["vmname"])),
                )
                self.restore_vm_created = True

            except vim.fault.DuplicateName:
                # VM with same name exists in target folder
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Error creating VM, duplicate name: %s\n"
                    % (StringCodec.encode(self.options["vmname"])),
                )
                return False

            except vim.fault.AlreadyExists:
                # VM directory already exists in VMFS
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Error creating VM, name %s already exists in VMFS, retrying so VM path in VMFS will have _n suffix\n"
                    % (StringCodec.encode(self.options["vmname"])),
                )
                if datastore_name is None:
                    datastore_name = transformer.new_vm_datastore_name

                # Setting files.vmPathName to datastore name in square brackets only,
                # this implicitly creates the dir on VMFS, including _n suffix if needed.
                config.files = vim.vm.FileInfo()
                config.files.vmPathName = "[%s]" % (datastore_name)
                self.vmfs_vm_path_changed = True

            except vim.fault.InvalidDeviceSpec as exc_invalid_dev_spec:
                # Can happen for virtualCdrom when connected to ISO on NFS datastore which
                # is not available at the time when the VM is created.
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Error creating VM %s: %s\n"
                    % (
                        StringCodec.encode(self.options["vmname"]),
                        exc_invalid_dev_spec.faultMessage[0].message,
                    ),
                )

                if (
                    len(exc_invalid_dev_spec.faultMessage) != 2
                    or exc_invalid_dev_spec.faultMessage[1].arg[0].value
                    != "VirtualCdrom"
                ):
                    bareosfd.JobMessage(
                        bareosfd.M_FATAL,
                        "Unexpected InvalidDeviceSpec exception %s\n"
                        % (StringCodec.encode(str(exc_invalid_dev_spec)),),
                    )
                    return False

                dev_spec = config.deviceChange[exc_invalid_dev_spec.deviceIndex].device
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Invalid VirtualCdrom backing file: %s, creating VM with disconnected VirtualCdrom\n"
                    % (dev_spec.backing.fileName),
                )
                dev_spec.connectable.connected = False
                dev_spec.connectable.startConnected = False
                dev_spec.backing = vim.vm.device.VirtualCdrom.RemoteAtapiBackingInfo()
                dev_spec.backing.useAutoDetect = False

            except vim.fault.CannotCreateFile as exc_cannot_create_file:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Error creating VM, name %s, vim.fault.CannotCreateFile exception, "
                    "please check if datastore is available on target. "
                    "API returned this error message: %s\n"
                    % (
                        StringCodec.encode(self.options["vmname"]),
                        StringCodec.encode(str(exc_cannot_create_file)),
                    ),
                )
                return False

            except Exception as ex:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Error creating VM, name %s, exception %s\n"
                    % (
                        StringCodec.encode(self.options["vmname"]),
                        StringCodec.encode(str(ex)),
                    ),
                )
                return False

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            "Successfully created VM %s\n"
            % (StringCodec.encode(self.options["vmname"])),
        )
        self.vm = create_vm_task.info.result

        # If transformer.disk_device_change_delayed is not empty, there are disks in other
        # datastores which must be added one-by-one here after VM was created.
        if transformer.disk_device_change_delayed:
            self.add_disk_devices_to_vm(transformer.disk_device_change_delayed)

        if transformer.config_spec_delayed:
            self.adapt_vm_config(transformer.config_spec_delayed)

        return True

    def create_vm_snapshot(self):
        """
        Creates a snapshot
        """
        snapshot_try_count = 0
        snapshot_name = "%s_%s" % (self.snapshot_prefix, self.plugin.jobId)

        enable_quiescing = True
        if self.options.get("quiesce") == "no":
            enable_quiescing = False
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "Guest quescing on snapshot was disabled by configuration, backup may be inconsistent\n",
            )

        while snapshot_try_count <= self.snapshot_retries:
            snapshot_try_count += 1
            try:
                self.create_snap_task = self.vm.CreateSnapshot_Task(
                    name=snapshot_name,
                    description="Bareos Tmp Snap jobId %s jobName %s"
                    % (self.plugin.jobId, self.plugin.jobName),
                    memory=False,
                    quiesce=enable_quiescing,
                )
            except vmodl.MethodFault as e:
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    "Failed to create snapshot: %s Trying again in %s s\n"
                    % (e.msgi, self.snapshot_retry_wait),
                )
                time.sleep(self.snapshot_retry_wait)
                continue

            try:
                pyVim.task.WaitForTask(self.create_snap_task)
            except vim.fault.ApplicationQuiesceFault as quiescing_error:
                bareosfd.JobMessage(
                    bareosfd.M_INFO,
                    "Snapshot error: %s Trying again in %s s\n"
                    % (quiescing_error.msg, self.snapshot_retry_wait),
                )
                time.sleep(self.snapshot_retry_wait)
                continue

            break

        else:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Snapshot failed after %s tries, giving up\n" % (snapshot_try_count),
            )
            return False

        if snapshot_try_count > 1:
            bareosfd.JobMessage(
                bareosfd.M_INFO,
                "Snapshot succeeded after %s tries\n" % (snapshot_try_count),
            )

        self.create_snap_result = self.create_snap_task.info.result
        self.create_snap_tstamp = time.time()
        return True

    def get_vm_snap_disk_devices(self):
        """
        Get the disk devices from the created snapshot
        Assumption: Snapshot successfully created
        """
        self.get_disk_devices(self.create_snap_result.config.hardware.device)

    def get_vm_disk_devices(self):
        """
        Get the disk devices from vm
        """
        self.get_disk_devices(self.vm.config.hardware.device)

    def get_disk_devices(self, devicespec):
        """
        Get disk devices from a devicespec
        """
        self.disk_devices = []
        for hw_device in devicespec:
            if type(hw_device) == vim.vm.device.VirtualDisk:
                if hw_device.backing.diskMode in self.skip_disk_modes:
                    bareosfd.JobMessage(
                        bareosfd.M_INFO,
                        "Skipping Disk %s because mode is %s\n"
                        % (
                            StringCodec.encode(
                                self.get_vm_disk_root_filename(hw_device.backing)
                            ),
                            hw_device.backing.diskMode,
                        ),
                    )
                    continue

                self.disk_devices.append(
                    {
                        "deviceKey": hw_device.key,
                        "fileName": hw_device.backing.fileName,
                        "fileNameRoot": self.get_vm_disk_root_filename(
                            hw_device.backing
                        ),
                        "changeId": hw_device.backing.changeId,
                    }
                )

    def get_vm_disk_root_filename(self, disk_device_backing):
        """
        Get the disk name from the ende of the parents chain
        When snapshots exist, the original disk filename is
        needed. If no snapshots exist, the disk has no parent
        and the filename is the same.
        """
        actual_backing = disk_device_backing
        while actual_backing.parent:
            actual_backing = actual_backing.parent
        return actual_backing.fileName

    def get_vm_disk_cbt(self):
        """
        Get CBT Information
        """
        cbt_changeId = "*"
        if chr(self.plugin.level) in ["I", "D"]:
            if (
                self.disk_device_to_backup["fileNameRoot"]
                not in self.restore_objects_by_diskpath
            ):
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Disk %s not found in previous backup, migrating disks is not yet supported for "
                    "incremental or differential backups. A new full level backup of this job is required.\n"
                    % (self.disk_device_to_backup["fileNameRoot"]),
                )
                return False

            if (
                len(
                    self.restore_objects_by_diskpath[
                        self.disk_device_to_backup["fileNameRoot"]
                    ]
                )
                > 1
            ):
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "More than one CBT info for Diff/Inc exists\n",
                )
                return False

            cbt_changeId = self.restore_objects_by_diskpath[
                self.disk_device_to_backup["fileNameRoot"]
            ][0]["data"]["DiskParams"]["changeId"]
            bareosfd.DebugMessage(
                100,
                "get_vm_disk_cbt(): using changeId %s from restore object\n"
                % (cbt_changeId),
            )

        try:
            self.changed_disk_areas = self.vm.QueryChangedDiskAreas(
                snapshot=self.create_snap_result,
                deviceKey=self.disk_device_to_backup["deviceKey"],
                startOffset=0,
                changeId=cbt_changeId,
            )
        except vim.fault.FileFault as error:
            if cbt_changeId == "*":
                bareosfd.JobMessage(bareosfd.M_FATAL, "Get CBT failed:\n")
                for cbt_error_message in [
                    fault.message for fault in error.faultMessage
                ]:
                    bareosfd.JobMessage(bareosfd.M_FATAL, cbt_error_message + "\n")
                return False
            else:
                # Note: A job message with M_ERROR will result in termination
                #       Backup OK -- with warnings
                bareosfd.JobMessage(bareosfd.M_ERROR, "Get CBT failed:\n")
                for cbt_error_message in [
                    fault.message for fault in error.faultMessage
                ]:
                    bareosfd.JobMessage(bareosfd.M_ERROR, cbt_error_message + "\n")
                bareosfd.JobMessage(bareosfd.M_WARNING, "Falling back to full CBT\n")
                self.changed_disk_areas = self.vm.QueryChangedDiskAreas(
                    snapshot=self.create_snap_result,
                    deviceKey=self.disk_device_to_backup["deviceKey"],
                    startOffset=0,
                    changeId="*",
                )
                bareosfd.JobMessage(bareosfd.M_INFO, "Successfully got full CBT\n")
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "To minimize restore time, it is recommended to run this job at full level next time.\n",
                )

        self.cbt2json()
        return True

    def check_vm_disks_match(self):
        """
        Check if the backed up disks match selected VM disks
        """
        backed_up_disks = set()

        for disk_path in self.restore_objects_by_diskpath.keys():
            bareosfd.DebugMessage(
                100,
                "check_vm_disks_match(): checking disk path %s\n" % (disk_path),
            )
            if self.restore_disk_paths_map:
                # adapt for restore to different datastore or different VMFS path
                bareosfd.DebugMessage(
                    100,
                    "check_vm_disks_match(): checking disk path %s with map: %s\n"
                    % (disk_path, self.restore_disk_paths_map),
                )
                bareosfd.DebugMessage(
                    100,
                    "check_vm_disks_match(): adapting disk path %s to recreated disk path %s\n"
                    % (disk_path, self.restore_disk_paths_map[disk_path]),
                )
                backed_up_disks.add(self.restore_disk_paths_map[disk_path])
            else:
                backed_up_disks.add(disk_path)

        vm_disks = set([disk_dev["fileNameRoot"] for disk_dev in self.disk_devices])

        if backed_up_disks == vm_disks:
            bareosfd.DebugMessage(
                100,
                "check_vm_disks_match(): OK, VM disks match backed up disks\n",
            )
            return True

        bareosfd.JobMessage(
            bareosfd.M_WARNING,
            "VM Disks: %s\n" % (StringCodec.encode(", ".join(vm_disks))),
        )
        bareosfd.JobMessage(
            bareosfd.M_WARNING,
            "Backed up Disks: %s\n" % (StringCodec.encode(", ".join(backed_up_disks))),
        )
        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            "ERROR: VM disks not matching backed up disks\n",
        )
        return False

    def vmconfig2json(self):
        """
        Convert the VM config info from the snapshot to JSON
        """
        # This will be saved in a restore object
        self.vm_config_info_json = json.dumps(
            self.create_snap_result.config, cls=VmomiJSONEncoder
        )
        self.files_to_backup.append("%s/%s" % (self.backup_path, "vm_config.json"))

        # Note: the vm property of the snapshot result also contains a config property,
        # but it can't be used to create a VM because it reflects the state *after* the
        # snapshot, while the config property represents the state *of the snapshot*,
        # so that it contains the correct disks.
        # However, we also need the VM info to be able to restore custom attributes etc.
        self.vm_info_json = json.dumps(self.create_snap_result.vm, cls=VmomiJSONEncoder)
        self.files_to_backup.append("%s/%s" % (self.backup_path, "vm_info.json"))

        # To be able to restore to the same host and same cluster:
        # The runtime property (VirtualMachineRuntimeInfo) has the host property, but
        # it's only the reference. Here the name is needed.
        vm_metadata = {}
        vm_metadata["esx_host_name"] = self.create_snap_result.vm.runtime.host.name
        self.vm_metadata_json = json.dumps(vm_metadata)
        self.files_to_backup.append("%s/%s" % (self.backup_path, "vm_metadata.json"))

    def check_vmconfig_backup(self):
        """
        Check the vmconfig at backup time, warn about possible restore problems
        """
        config_info = json.loads(self.vm_config_info_json)
        transformer = BareosVmConfigInfoToSpec(config_info, vadp=self)

        # A job message was emitted here when multiple data stores were used,
        # which is fixed now. So now try to transform the VM metadata as it
        # would be done when recreating the VM for restore. Note that a job
        # message with level M_ERROR will cause the backup job to terminate
        # as "Backup OK -- with warnings"
        try:
            transformer.transform()
        except Exception as transform_exception:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "Failed to transform VM %s metadata: %s, recreating this VM will not be possible!\n"
                % (
                    StringCodec.encode(self.options["vmname"]),
                    str(transform_exception),
                ),
            )
            bareosfd.DebugMessage(
                100,
                "check_vmconfig_backup(): Unexpected VM Metadata transform exception %s(%s)\n"
                % (str(transform_exception), type(transform_exception)),
            )
            for traceback_line in traceback.format_exception(transform_exception):
                bareosfd.DebugMessage(100, traceback_line)
            return False

        return True

    def restore_custom_fields(self):
        """
        Restore Custom Fields
        """

        if not self.restore_vm_info:
            self.restore_vm_info = json.loads(self.restore_vm_info_json)

        available_fields = dict(
            [(f["key"], f["name"]) for f in self.restore_vm_info["availableField"]]
        )
        for custom_field in self.restore_vm_info["customValue"]:
            self.vm.setCustomValue(
                key=available_fields[custom_field["key"]], value=custom_field["value"]
            )

    def cbt2json(self):
        """
        Convert CBT data into json serializable structure and
        return it as json string
        """

        # the order of keys in JSON data must be preserved for
        # bareos_vadp_dumper to work properly, this is done
        # by using the OrderedDict type
        cbt_data = OrderedDict()
        cbt_data["ConnParams"] = {}
        cbt_data["ConnParams"]["VmMoRef"] = "moref=" + self.vm._moId
        cbt_data["ConnParams"]["VsphereHostName"] = self.options["vcserver"]
        cbt_data["ConnParams"]["VsphereUsername"] = self.options["vcuser"]
        cbt_data["ConnParams"]["VspherePassword"] = self.options["vcpass"]
        cbt_data["ConnParams"]["VsphereThumbPrint"] = ":".join(
            [
                self.options["vcthumbprint"][i : i + 2]
                for i in range(0, len(self.options["vcthumbprint"]), 2)
            ]
        )
        cbt_data["ConnParams"]["VsphereSnapshotMoRef"] = self.create_snap_result._moId

        # disk params for bareos_vadp_dumper
        cbt_data["DiskParams"] = {}
        cbt_data["DiskParams"]["diskPath"] = self.disk_device_to_backup["fileName"]
        cbt_data["DiskParams"]["diskPathRoot"] = self.disk_device_to_backup[
            "fileNameRoot"
        ]
        cbt_data["DiskParams"]["changeId"] = self.disk_device_to_backup["changeId"]

        # cbt data for bareos_vadp_dumper
        cbt_data["DiskChangeInfo"] = {}
        cbt_data["DiskChangeInfo"]["startOffset"] = self.changed_disk_areas.startOffset
        cbt_data["DiskChangeInfo"]["length"] = self.changed_disk_areas.length
        cbt_data["DiskChangeInfo"]["changedArea"] = []
        for extent in self.changed_disk_areas.changedArea:
            cbt_data["DiskChangeInfo"]["changedArea"].append(
                {"start": extent.start, "length": extent.length}
            )

        self.changed_disk_areas_json = json.dumps(cbt_data)
        self.writeStringToFile(
            "/var/tmp" + StringCodec.encode(self.file_to_backup),
            self.changed_disk_areas_json,
        )

    def json2cbt(self, cbt_json_string):
        """
        Convert JSON string from restore object to ordered dict
        to preserve the key order required for bareos_vadp_dumper
        to work properly
        return OrderedDict
        """

        # the order of keys in JSON data must be preserved for
        # bareos_vadp_dumper to work properly
        cbt_data = OrderedDict()
        cbt_keys_ordered = ["ConnParams", "DiskParams", "DiskChangeInfo"]

        if version_info.major < 3:
            cbt_json_string = str(cbt_json_string)

        cbt_data_tmp = json.loads(cbt_json_string)
        for cbt_key in cbt_keys_ordered:
            cbt_data[cbt_key] = cbt_data_tmp[cbt_key]

        return cbt_data

    def dumpJSONfile(self, filename, data):
        """
        Write a Python data structure in JSON format to the given file.
        Note: obsolete, no longer used because order of keys in JSON
              string must be preserved
        """
        bareosfd.DebugMessage(
            100, "dumpJSONfile(): writing JSON data to file %s\n" % (filename)
        )
        try:
            out = open(filename, "w")
            json.dump(data, out)
            out.close()
            bareosfd.DebugMessage(
                100,
                "dumpJSONfile(): successfully wrote JSON data to file %s\n"
                % (filename),
            )

        except IOError as io_error:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    "dumpJSONFile(): failed to write JSON data to file %s,"
                    " reason: %s\n"
                )
                % (StringCodec.encode(filename), io_error.strerror),
            )

    def writeStringToFile(self, filename, data_string):
        """
        Write a String to the given file.
        """
        bareosfd.DebugMessage(
            100,
            "writeStringToFile(): writing String to file %s\n" % (filename),
        )
        # ensure the directory for writing the file exists
        self.mkdir(os.path.dirname(filename))
        try:
            out = open(filename, "w")
            out.write(data_string)
            out.close()
            bareosfd.DebugMessage(
                100,
                "writeStringTofile(): successfully wrote String to file %s\n"
                % (filename),
            )

        except IOError as io_error:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    "writeStingToFile(): failed to write String to file %s,"
                    " reason: %s\n"
                )
                % (filename, io_error.strerror),
            )

        # the following path must be passed to bareos_vadp_dumper as parameter
        self.cbt_json_local_file_path = filename

    def start_dumper(self, cmd):
        """
        Start bareos_vadp_dumper
        Parameters
        - cmd: must be "dump" or "restore"
        """
        bareos_vadp_dumper_bin = "bareos_vadp_dumper_wrapper.sh"

        # options for bareos_vadp_dumper:
        # -S: Cleanup on Start
        # -D: Cleanup on Disconnect
        # -M: Save metadata of VMDK on dump action
        # -R: Restore metadata of VMDK on restore action
        # -l: Write to a local VMDK
        # -C: Create local VMDK
        # -d: Specify local VMDK name
        # -f: Specify forced transport method
        # -v - Verbose output
        bareos_vadp_dumper_opts = {}
        bareos_vadp_dumper_opts["dump"] = "-S -D -M"
        if "transport" in self.options:
            bareos_vadp_dumper_opts["dump"] += " -f %s" % self.options["transport"]
        if self.restore_vmdk_file:
            if os.path.exists(self.restore_vmdk_file):
                # restore of diff/inc, local vmdk exists already,
                # handling of replace options is done in create_file()
                bareos_vadp_dumper_opts["restore"] = "-l -R -d "
            else:
                # restore of full, must pass -C to create local vmdk
                # and make sure the target directory exists
                self.mkdir(os.path.dirname(self.restore_vmdk_file))
                bareos_vadp_dumper_opts["restore"] = "-l -C -R -d "
            bareos_vadp_dumper_opts["restore"] += (
                '"' + StringCodec.encode(self.restore_vmdk_file) + '"'
            )
        else:
            # Note: -c omits disk geometry checks, this is necessary to allow
            # recreating and restoring VMs which have been deployed from OVA
            # as the disk may have different geometry initially after creation.
            # -S  Cleanup on start
            # -D  Cleanup on disconnect
            # -R  Restore metadata of VMDK on restore action
            bareos_vadp_dumper_opts["restore"] = "-S -D -R -c"
            if "transport" in self.options:
                bareos_vadp_dumper_opts["restore"] += (
                    " -f %s" % self.options["transport"]
                )

        if self.dumper_verbose:
            bareos_vadp_dumper_opts[cmd] = "-v " + bareos_vadp_dumper_opts[cmd]

        if self.dumper_multithreading:
            bareos_vadp_dumper_opts[cmd] += " -m"

        bareos_vadp_dumper_opts[cmd] += " -s %s" % self.dumper_sectors_per_call
        bareos_vadp_dumper_opts[cmd] += (
            " -k %s" % self.dumper_query_allocated_blocks_chunk_size
        )

        bareosfd.DebugMessage(
            100,
            "start_dumper(): dumper options: %s\n"
            % (repr(bareos_vadp_dumper_opts[cmd])),
        )

        bareosfd.DebugMessage(
            100,
            "Type of self.cbt_json_local_file_path is %s\n"
            % (type(self.cbt_json_local_file_path)),
        )

        bareosfd.DebugMessage(
            100,
            "self.cbt_json_local_file_path: %s\n" % (self.cbt_json_local_file_path),
        )

        bareos_vadp_dumper_command = '%s %s %s "%s"' % (
            bareos_vadp_dumper_bin,
            bareos_vadp_dumper_opts[cmd],
            cmd,
            self.cbt_json_local_file_path,
        )

        bareosfd.DebugMessage(
            100,
            "start_dumper(): dumper command(repr): %s\n"
            % (repr(bareos_vadp_dumper_command)),
        )

        bareos_vadp_dumper_command_args = shlex.split(bareos_vadp_dumper_command)
        bareosfd.DebugMessage(
            100,
            "start_dumper(): bareos_vadp_dumper_command_args: %s\n"
            % (repr(bareos_vadp_dumper_command_args)),
        )

        log_path = "/var/log/bareos"
        if self.options.get("log_path"):
            log_path = self.options["log_path"]

        stderr_log_fd = tempfile.NamedTemporaryFile(dir=log_path, delete=False)

        bareos_vadp_dumper_process = None
        bareos_vadp_dumper_logfile = None
        try:
            if cmd == "dump":
                # backup
                bareos_vadp_dumper_process = subprocess.Popen(
                    bareos_vadp_dumper_command_args,
                    bufsize=-1,
                    stdin=open("/dev/null"),
                    stdout=subprocess.PIPE,
                    stderr=stderr_log_fd,
                    close_fds=True,
                )
            else:
                # restore
                bareos_vadp_dumper_process = subprocess.Popen(
                    bareos_vadp_dumper_command_args,
                    bufsize=-1,
                    stdin=subprocess.PIPE,
                    stdout=open("/dev/null"),
                    stderr=stderr_log_fd,
                    close_fds=True,
                )

            # rename the stderr log file to one containing the JobId and PID
            bareos_vadp_dumper_logfile = "%s/bareos_vadp_dumper.%s-%s.log" % (
                log_path,
                bareosfd.GetValue(bareosfd.bVarJobId),
                bareos_vadp_dumper_process.pid,
            )
            os.rename(stderr_log_fd.name, bareos_vadp_dumper_logfile)
            bareosfd.DebugMessage(
                100,
                "start_dumper(): started %s, log stderr to %s\n"
                % (repr(bareos_vadp_dumper_command), bareos_vadp_dumper_logfile),
            )

        except Exception as e:
            # kill children if they arent done
            if bareos_vadp_dumper_process:
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Failed to start %s: %s\n"
                    % (repr(bareos_vadp_dumper_command), str(e)),
                )
                if (
                    bareos_vadp_dumper_process is not None
                    and bareos_vadp_dumper_process.returncode is None
                ):
                    bareosfd.JobMessage(
                        bareosfd.M_WARNING,
                        "Killing probably stuck %s PID %s with signal 9\n"
                        % (
                            repr(bareos_vadp_dumper_command),
                            bareos_vadp_dumper_process.pid,
                        ),
                    )
                    os.kill(bareos_vadp_dumper_process.pid, 9)
                try:
                    if bareos_vadp_dumper_process is not None:
                        bareosfd.DebugMessage(
                            100,
                            "Waiting for command %s PID %s to terminate\n"
                            % (
                                repr(bareos_vadp_dumper_command),
                                bareos_vadp_dumper_process.pid,
                            ),
                        )
                        os.waitpid(bareos_vadp_dumper_process.pid, 0)
                        bareosfd.DebugMessage(
                            100,
                            "Command %s PID %s terminated\n"
                            % (
                                repr(bareos_vadp_dumper_command),
                                bareos_vadp_dumper_process.pid,
                            ),
                        )

                except Exception as e:
                    bareosfd.DebugMessage(
                        100,
                        "Failed to wait for PID %s to terminate:%s\n"
                        % (
                            bareos_vadp_dumper_process.pid,
                            str(e),
                        ),
                    )

                raise
            else:
                raise

        # bareos_vadp_dumper should be running now, set the process object
        # for further processing
        self.dumper_process = bareos_vadp_dumper_process
        self.dumper_stderr_log = bareos_vadp_dumper_logfile

        # check if dumper is running to catch any error that occured
        # immediately after starting it
        if not self.check_dumper():
            return False

        return True

    def end_dumper(self):
        """
        Wait for bareos_vadp_dumper to terminate
        """
        bareos_vadp_dumper_returncode = None
        # Wait up to 120s for bareos_vadp_dumper to terminate,
        # if still running, send SIGTERM and wait up to 60s.
        # This handles cancelled jobs properly and prevents
        # from infinite loop if something unexpected goes wrong.
        timeout = 120
        start_time = int(time.time())
        sent_sigterm = False
        while self.dumper_process.poll() is None:
            if int(time.time()) - start_time > timeout:
                bareosfd.DebugMessage(
                    100,
                    "Timeout wait for bareos_vadp_dumper PID %s to terminate\n"
                    % (self.dumper_process.pid),
                )
                if not sent_sigterm:
                    bareosfd.DebugMessage(
                        100,
                        "sending SIGTERM to bareos_vadp_dumper PID %s\n"
                        % (self.dumper_process.pid),
                    )
                    os.kill(self.dumper_process.pid, signal.SIGTERM)
                    sent_sigterm = True
                    timeout = 60
                    start_time = int(time.time())
                    continue
                else:
                    bareosfd.DebugMessage(
                        100,
                        "Giving up to wait for bareos_vadp_dumper PID %s to terminate\n"
                        % (self.dumper_process.pid),
                    )
                    break

            bareosfd.DebugMessage(
                100,
                "Waiting for bareos_vadp_dumper PID %s to terminate\n"
                % (self.dumper_process.pid),
            )
            time.sleep(1)

        bareos_vadp_dumper_returncode = self.dumper_process.returncode
        bareosfd.DebugMessage(
            100,
            "end_dumper() bareos_vadp_dumper returncode: %s\n"
            % (bareos_vadp_dumper_returncode),
        )
        if bareos_vadp_dumper_returncode != 0:
            self.check_dumper()
        else:
            self.dumper_process = None

        self.cleanup_tmp_files()

        return bareos_vadp_dumper_returncode

    def get_dumper_err(self):
        """
        Read vadp_dumper stderr output file and return its content
        """
        dumper_log_file = open(self.dumper_stderr_log, "r")
        err_msg = dumper_log_file.read()
        dumper_log_file.close()
        return err_msg

    def check_dumper(self):
        """
        Check if vadp_dumper has unexpectedly terminated, if so
        generate fatal job message
        """
        bareosfd.DebugMessage(100, "BareosFdPluginVMware:check_dumper() called\n")

        if self.dumper_process.poll() is not None:
            bareos_vadp_dumper_returncode = self.dumper_process.returncode
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                (
                    "check_dumper(): bareos_vadp_dumper returncode:"
                    " %s error output:\n%s\n"
                )
                % (bareos_vadp_dumper_returncode, self.get_dumper_err()),
            )
            return False

        bareosfd.DebugMessage(
            100,
            "BareosFdPluginVMware:check_dumper() dumper seems to be running\n",
        )

        return True

    def cleanup_tmp_files(self):
        """
        Cleanup temporary files
        """

        if self.options.get("cleanup_tmpfiles") == "no":
            bareosfd.DebugMessage(
                100,
                "end_dumper() not deleting temporary file, cleanup_tmpfiles is set to no\n",
            )
            return True

        # delete temporary json file
        if not self.cbt_json_local_file_path:
            # not set, nothing to do
            return True

        bareosfd.DebugMessage(
            100,
            "end_dumper() deleting temporary file %s\n"
            % (self.cbt_json_local_file_path),
        )
        try:
            os.unlink(self.cbt_json_local_file_path)
        except OSError as e:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                "Could not delete %s: %s\n"
                % (
                    StringCodec.encode(self.cbt_json_local_file_path),
                    e.strerror,
                ),
            )

        self.cbt_json_local_file_path = None

        return True

    def fetch_vcthumbprint(self):
        """
        Retrieve the SSL Cert thumbprint from VC Server
        """
        if self.fetched_vcthumbprint:
            return True

        success = True
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        wrappedSocket = ssl.wrap_socket(sock)
        bareosfd.DebugMessage(
            100,
            "retrieve_vcthumbprint() Retrieving SSL ThumbPrint from %s\n"
            % (self.options["vcserver"]),
        )
        try:
            wrappedSocket.connect((self.options["vcserver"], 443))
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Could not retrieve SSL Cert from %s: %s\n"
                % (self.options["vcserver"], str(e)),
            )
            success = False
        else:
            der_cert_bin = wrappedSocket.getpeercert(True)
            thumb_sha1 = hashlib.sha1(der_cert_bin).hexdigest()
            self.fetched_vcthumbprint = thumb_sha1.upper()

        wrappedSocket.close()
        return success

    def retrieve_vcthumbprint(self):
        """
        Retrieve the SSL Cert thumbprint from VC Server
        and store it in options
        """
        if not self.fetch_vcthumbprint():
            return False

        self.options["vcthumbprint"] = self.fetched_vcthumbprint
        return True

    def find_managed_object_by_name(
        self, object_types, object_name, start_folder=None, recursive=True
    ):
        """
        Find a managed object by name
        """
        found_managed_object = None
        if start_folder is None:
            start_folder = self.si.content.rootFolder

        view = self.si.content.viewManager.CreateContainerView(
            start_folder, object_types, recursive
        )

        for managed_object in view.view:
            if managed_object.name == object_name:
                found_managed_object = managed_object
                break

        view.Destroy()
        return found_managed_object

    def get_vmfs_vm_path(self):
        """
        Get the VMs path in VMFS
        """
        if not self.vmfs_vm_path:
            self.vmfs_vm_path = os.path.dirname(self.vm.config.files.vmPathName) + "/"
        return self.vmfs_vm_path

    def restore_power_state(self):
        """
        Restore power state according to restore_powerstate option
        """

        if not self.restore_vm_info and self.restore_vm_info_json:
            self.restore_vm_info = json.loads(self.restore_vm_info_json)

        if self.options["restore_powerstate"] == "off":
            return bareosfd.bRC_OK

        if (
            self.options["restore_powerstate"] == "previous"
            and self.restore_vm_info is None
        ):
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                "Previous power state of VM %s unknown, skipping restore powerstate\n"
                % (self.vm.name),
            )
            return bareosfd.bRC_Error

        if self.options["restore_powerstate"] == "on" or (
            self.options["restore_powerstate"] == "previous"
            and self.restore_vm_info["runtime"]["powerState"] == "poweredOn"
        ):
            bareosfd.DebugMessage(
                100,
                "restore_power_state(): Powering on VM %s\n" % (self.vm.name),
            )
            poweron_task = self.dc.PowerOnMultiVM_Task(vm=[self.vm])
            pyVim.task.WaitForTask(poweron_task)
            bareosfd.DebugMessage(
                100,
                "restore_power_state(): Power on Task status for VM %s: %s\n"
                % (self.vm.name, poweron_task.info.state),
            )

            # Check if it was really powered on, as when DRS Automation is set
            # to manual, the task will terminate with success, but VM is not powered on.
            # Also when DRS Automation is set to fully automated, the task terminates
            # with success state before the VM is powered on. So there seems to be
            # no reliable solution for this problem. Workaround: poll power state
            # with timeout. It's configurable, plugin option poweron_timeout

            poweron_start_time = time.time()
            # This could probably be implemented better by using WaitForUpdates()
            while self.vm.runtime.powerState != "poweredOn":
                time.sleep(1)
                if time.time() - poweron_start_time >= self.poweron_timeout:
                    break

            if self.vm.runtime.powerState != "poweredOn":
                # This warning will cause the job termination to be
                # Restore OK -- with warnings
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Could not power on VM %s: power state is %s\n"
                    % (self.vm.name, self.vm.runtime.powerState),
                )
                return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def check_mac_address(self, config_info):
        """
        Check if a MAC address in this config info exists and remove it from
        this config info if applicable to avoid conflicts
        """
        all_mac_addr = {}
        vm_view = self.si.content.viewManager.CreateContainerView(
            self.si.content.rootFolder, [vim.VirtualMachine], True
        )
        all_vms = [vm for vm in vm_view.view]
        vm_view.Destroy()

        for vm in all_vms:
            for dev in vm.config.hardware.device:
                if isinstance(dev, vim.vm.device.VirtualEthernetCard):
                    if dev.macAddress not in all_mac_addr:
                        all_mac_addr[dev.macAddress] = []
                    all_mac_addr[dev.macAddress].append(vm)

        for device in [
            dev for dev in config_info["hardware"]["device"] if "macAddress" in dev
        ]:
            mac_address = device["macAddress"]
            if mac_address in all_mac_addr:
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "MAC address %s already exists in VM(s): %s, restored VM will get generated MAC address\n"
                    % (
                        mac_address,
                        [
                            self._get_vm_folder_path(vm)
                            for vm in all_mac_addr[mac_address]
                        ],
                    ),
                )
                device["addressType"] = "generated"

    def check_uuid(self, config_info):
        """
        Check for duplicate VM uuid
        """
        vms = self.si.content.searchIndex.FindAllByUuid(
            uuid=config_info["uuid"], vmSearch=True
        )
        if not vms:
            return

        for vm in vms:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                "UUID %s already exists in VM: %s, restored VM will get new generated UUID\n"
                % (config_info["uuid"], self._get_vm_folder_path(vm)),
            )

        del config_info["uuid"]

    def add_disk_devices_to_vm(self, device_changes):
        """
        Add devices to VM
        """
        disk_index = 0
        for device_spec in device_changes:
            config_spec = vim.vm.ConfigSpec()
            device_spec.device.controllerKey = abs(device_spec.device.controllerKey)
            config_spec.deviceChange = [device_spec]
            device_created = False
            backing_path_changed = False
            while not device_created:
                reconfig_task = self.vm.ReconfigVM_Task(spec=config_spec)
                try:
                    pyVim.task.WaitForTask(reconfig_task)
                    device_created = True
                except vim.fault.FileAlreadyExists:
                    device_spec.device.backing.fileName = (
                        "[%s] " % device_spec.device.backing.datastore.name
                    )
                    backing_path_changed = True

            # When handling the FileAlreadyExists exception, we only pass the datastore name
            # and the backing path to the disk will be created by the API, then mapping in self.restore_disk_paths_map
            # must be corrected. This is only possible by walking the VMs devices and detect the added disks path,
            # because the task result does not contain it.
            if backing_path_changed:
                self.restore_disk_paths_map[
                    list(self.restore_disk_paths_map)[disk_index]
                ] = [
                    device.backing.fileName
                    for device in self.vm.config.hardware.device
                    if type(device) == vim.vm.device.VirtualDisk
                ][
                    disk_index
                ]
            disk_index += 1

    def adapt_vm_config(self, config_spec):
        """
        Adapt VM config
        """
        adapt_config_task = self.vm.ReconfigVM_Task(config_spec)
        pyVim.task.WaitForTask(adapt_config_task)

    def get_vm_nvram(self):
        """
        Get VM NVRAM

        Retrieve the VMs NVRAM file.
        The content of the name property normally is
        "[<DatastoreName>] <VmName>/<VmName>.nvram"
        for example
        "[esxi2-ds1] deb12test2/deb12test2.nvram"

        Note: A VM that has never been switched on has no nvram file
        """
        found_nvram_file_names = [
            layout_file.name
            for layout_file in self.vm.layoutEx.file
            if layout_file.type == "nvram"
        ]
        if found_nvram_file_names:
            self.vm_nvram_path = found_nvram_file_names[0]
        if not self.vm_nvram_path:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                "Error getting NVRAM path for VM: %s\n" % self.vm.name,
            )
            return False

        # needed to retrieve NVRAM file content:
        # - datastore name
        # - datacenter name -> self.dc.name
        # - nvram file path
        # - service instance object -> self.si

        ds_vm_file_match = self.datastore_vm_file_rex.match(self.vm_nvram_path)
        if not ds_vm_file_match:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error getting datastore name from nvram file path: %s\n"
                % self.vm_nvram_path,
            )
            return False

        vm_nvram_datastore_name = ds_vm_file_match.group(1)
        vm_nvram_file_path = ds_vm_file_match.group(2)
        self.vm_nvram_content = self.retrieve_file_content_from_datastore(
            vm_nvram_datastore_name, vm_nvram_file_path
        )
        if not self.vm_nvram_content:
            bareosfd.DebugMessage(
                100,
                "get_vm_nvram(): Error getting nvrame file %s\n" % (self.vm_nvram_path),
            )
            return False

        bareosfd.DebugMessage(
            100,
            "get_vm_nvram(): Successfully got %s, size: %s, type: %s\n"
            % (
                self.vm_nvram_path,
                len(self.vm_nvram_content),
                type(self.vm_nvram_content),
            ),
        )
        return True

    def put_vm_nvram(self):
        """
        Put VM NVRAM

        Upload the VMs NVRAM file.
        The content of the name property normally is
        "[<DatastoreName>] <VmName>/<VmName>.nvram"
        for example
        "[esxi2-ds1] deb12test2/deb12test2.nvram"
        As this will probably be different on restore in most cases, the most
        simple option is to use vm.config.files.vmPathName and replace .vmx
        by .nvram
        """
        restore_vm_nvram_path = (
            os.path.splitext(self.vm.config.files.vmPathName)[0] + ".nvram"
        )
        bareosfd.DebugMessage(
            100,
            "put_vm_nvram():  %s\n" % (restore_vm_nvram_path),
        )
        ds_vm_file_match = self.datastore_vm_file_rex.match(restore_vm_nvram_path)
        if not ds_vm_file_match:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error getting datastore name from nvram file path: %s\n"
                % restore_vm_nvram_path,
            )
            return False

        vm_nvram_datastore_name = ds_vm_file_match.group(1)
        vm_nvram_file_path = ds_vm_file_match.group(2)
        if self.upload_file_content_to_datastore(
            vm_nvram_datastore_name, vm_nvram_file_path, self.vm_nvram_content
        ):
            bareosfd.DebugMessage(
                100,
                "put_vm_nvram(): successfully restored NVRAM to %s, size: %s\n"
                % (restore_vm_nvram_path, len(self.vm_nvram_content)),
            )
            return True
        return False

    def retrieve_file_content_from_datastore(self, datastore_name, file_path):
        """
        Retrieve file content from datastore

        Note: This will only work for small files, main purpose here is the NVRAM file.
        """
        verify_cert = True
        if self.options.get("verifyssl") != "yes":
            verify_cert = False
        cookie = self._get_cookie_from_si()
        request_params = {}
        request_params["dsName"] = datastore_name
        request_params["dcPath"] = self.dc.name
        file_url = "https://%s:443/folder/%s" % (self.options["vcserver"], file_path)
        request_headers = {"Content-Type": "application/octet-stream"}

        file_content = None
        with requests.Session() as s:
            # retries will be done if either server is unreachable,
            # or if the response succeeded but status is in status_forcelist,
            # see https://httpstat.us/ for list of status codes
            retries = urllib3.util.Retry(
                total=3,
                backoff_factor=0.1,
                status_forcelist=[429, 500, 502, 503, 504],
                allowed_methods={"GET"},
            )
            s.mount("https://", requests.adapters.HTTPAdapter(max_retries=retries))
            try:
                response = s.get(
                    file_url,
                    params=request_params,
                    headers=request_headers,
                    cookies=cookie,
                    verify=verify_cert,
                    timeout=5,
                )
                response.raise_for_status()
            except Exception as exc:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Unexpected Error retrieving NVRAM file: %s\n" % str(exc),
                )
                return False

            file_content = response.content

        return file_content

    def upload_file_content_to_datastore(self, datastore_name, file_path, file_content):
        """
        Upload file content to datastore

        Note: This will only work for small files, main purpose here is the NVRAM file.
        """
        verify_cert = True
        if self.options.get("verifyssl") != "yes":
            verify_cert = False
        cookie = self._get_cookie_from_si()
        request_params = {}
        request_params["dsName"] = datastore_name
        request_params["dcPath"] = self.dc.name
        file_url = "https://%s:443/folder/%s" % (self.options["vcserver"], file_path)
        request_headers = {"Content-Type": "application/octet-stream"}

        # Check if file_content is type bytes or bytearray and length > 0,
        # the data parameter to put would even accept None which would create an
        # empty file remote.
        if not (isinstance(file_content, bytes) or isinstance(file_content, bytearray)):
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error saving NVRAM, type of file_content must be bytes or bytearray but is %s\n"
                % type(file_content),
            )
            return False

        if len(file_content) == 0:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Error saving NVRAM, size of file_content must be > 0\n",
            )
            return False

        with requests.Session() as s:
            # retries will be done if either server is unreachable,
            # or if the response succeeded but status is in status_forcelist,
            # see https://httpstat.us/ for list of status codes
            retries = urllib3.util.Retry(
                total=3,
                backoff_factor=0.1,
                status_forcelist=[429, 500, 502, 503, 504],
                allowed_methods={"PUT"},
            )
            s.mount("https://", requests.adapters.HTTPAdapter(max_retries=retries))
            try:
                response = s.put(
                    file_url,
                    params=request_params,
                    data=file_content,
                    headers=request_headers,
                    cookies=cookie,
                    verify=verify_cert,
                    timeout=5,
                )
                response.raise_for_status()
            except Exception as exc:
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    "Unexpected Error uploading NVRAM file: %s\n" % str(exc),
                )
                return False

            bareosfd.DebugMessage(
                100,
                "upload_file_content_to_datastore(): uploaded NVRAM to %s, size: %s, status: %s\n"
                % (
                    file_url,
                    len(file_content),
                    response.status_code,
                ),
            )

        return True

    def _get_cookie_from_si(self):
        """
        Get the client cookie from service instance
        """
        client_cookie = self.si._stub.cookie
        cookie_name = client_cookie.split("=", 1)[0]
        cookie_value = client_cookie.split("=", 1)[1].split(";", 1)[0]
        cookie_path = (
            client_cookie.split("=", 1)[1].split(";", 1)[1].split(";", 1)[0].lstrip()
        )
        cookie_text = " " + cookie_value + "; $" + cookie_path
        cookie = dict()
        cookie[cookie_name] = cookie_text

        return cookie

    # helper functions ############

    def mkdir(self, directory_name):
        """
        Utility Function for creating directories,
        works like mkdir -p
        """
        try:
            os.stat(directory_name)
        except OSError:
            os.makedirs(directory_name)

    def proper_path(self, path):
        """
        Ensure proper path:
        - removes multiplied slashes
        - makes sure it starts with slash
        - makes sure it does not end with slash
        - if slashes only or the empty string were passed,
          returns a single slash
        """
        proper_path = self.slashes_rex.sub("/", "/" + path)
        proper_path = proper_path.rstrip("/")
        if proper_path == "":
            proper_path = "/"
        return proper_path


class StringCodec:
    @staticmethod
    def encode(var):
        if version_info.major < 3:
            return var.encode("utf-8")
        else:
            return var


class BareosVmConfigInfoToSpec(object):
    """
    A class to transform serialized VirtualMachineConfigInfo(vim.vm.ConfigInfo)
    into VirtualMachineConfigSpec(vim.vm.ConfigSpec)
    """

    def __init__(self, config_info, vadp=None):
        self.config_info = config_info
        self.datastore_rex = re.compile(r"\[(.+?)\]")
        self.backing_filename_snapshot_rex = re.compile(r"(-\d{6})\.vmdk$")
        self.target_datastore_name = None
        self.orig_vm_datastore_name = None
        self.new_vm_datastore_name = None
        self.new_target_vm_name = None
        self.disk_device_change_delayed = []
        self.config_spec_delayed = None
        self.vadp = vadp

    def transform(self, target_datastore_name=None, target_vm_name=None):
        config_spec = vim.vm.ConfigSpec()
        if target_vm_name:
            self.new_target_vm_name = target_vm_name
        self.target_datastore_name = target_datastore_name
        self.orig_vm_datastore_name = self._get_vm_datastore_name()
        self.new_vm_datastore_name = self.orig_vm_datastore_name
        if target_datastore_name:
            self.new_vm_datastore_name = target_datastore_name
        config_spec.alternateGuestName = self.config_info["alternateGuestName"]
        config_spec.annotation = self.config_info["annotation"]
        config_spec.bootOptions = self._transform_bootOptions()
        config_spec.changeTrackingEnabled = self.config_info["changeTrackingEnabled"]
        config_spec.cpuAffinity = self._transform_cpuAffinity()
        config_spec.cpuAllocation = self._transform_ResourceAllocationInfo(
            "cpuAllocation"
        )
        config_spec.cpuFeatureMask = self._transform_cpuFeatureMask()
        config_spec.cpuHotAddEnabled = self.config_info["cpuHotAddEnabled"]
        config_spec.cpuHotRemoveEnabled = self.config_info["cpuHotRemoveEnabled"]
        config_spec.deviceChange = self._transform_devices()
        config_spec.extraConfig = self._transform_extraConfig()
        config_spec.files = self._transform_files()
        config_spec.firmware = self.config_info["firmware"]
        config_spec.flags = self._transform_flags()
        config_spec.ftEncryptionMode = self.config_info["ftEncryptionMode"]
        config_spec.ftInfo = self._transform_ftInfo()
        config_spec.guestAutoLockEnabled = self.config_info["guestAutoLockEnabled"]
        config_spec.guestId = self.config_info["guestId"]
        config_spec.guestMonitoringModeInfo = self._transform_guestMonitoringModeInfo()
        config_spec.latencySensitivity = self._transform_latencySensitivity(
            self.config_info["latencySensitivity"]
        )
        config_spec.managedBy = self._transform_managedBy()
        config_spec.maxMksConnections = self.config_info["maxMksConnections"]
        config_spec.memoryAllocation = self._transform_ResourceAllocationInfo(
            "memoryAllocation"
        )
        config_spec.memoryHotAddEnabled = self.config_info["memoryHotAddEnabled"]
        config_spec.memoryMB = self.config_info["hardware"]["memoryMB"]
        config_spec.memoryReservationLockedToMax = self.config_info[
            "memoryReservationLockedToMax"
        ]
        config_spec.messageBusTunnelEnabled = self.config_info[
            "messageBusTunnelEnabled"
        ]
        config_spec.migrateEncryption = self.config_info["migrateEncryption"]
        config_spec.name = self.config_info["name"]
        if target_vm_name:
            config_spec.name = target_vm_name
        config_spec.nestedHVEnabled = self.config_info["nestedHVEnabled"]
        config_spec.numCoresPerSocket = self.config_info["hardware"][
            "numCoresPerSocket"
        ]
        config_spec.numCPUs = self.config_info["hardware"]["numCPU"]
        # Since vSphere API 7.0.3.0:
        if hasattr(config_spec, "pmem"):
            config_spec.pmem = self._transform_pmem()
        config_spec.pmemFailoverEnabled = self.config_info["pmemFailoverEnabled"]
        config_spec.powerOpInfo = self._transform_defaultPowerOps()
        # Since vSphere API 7.0.1.0:
        if "sevEnabled" in self.config_info:
            config_spec.sevEnabled = self.config_info["sevEnabled"]
        # Since vSphere API 7.0:
        if self.config_info.get("sgxInfo"):
            config_spec.sgxInfo = self._transform_sgxInfo()
        config_spec.swapPlacement = self.config_info["swapPlacement"]
        config_spec.tools = self._transform_tools()
        config_spec.uuid = self.config_info.get("uuid")
        config_spec.vAppConfig = self._transform_vAppConfig()
        config_spec.vAssertsEnabled = self.config_info["vAssertsEnabled"]
        # Since vSphere API 7.0:
        if "vcpuConfig" in self.config_info:
            config_spec.vcpuConfig = self._transform_VirtualMachineVcpuConfig()
        config_spec.version = self.config_info["version"]
        config_spec.virtualICH7MPresent = self.config_info["hardware"][
            "virtualICH7MPresent"
        ]
        config_spec.virtualSMCPresent = self.config_info["hardware"][
            "virtualSMCPresent"
        ]
        # Since vSphere API 7.0.3.0:
        if hasattr(config_spec, "vmOpNotificationToAppEnabled"):
            if "vmOpNotificationToAppEnabled" in self.config_info:
                config_spec.vmOpNotificationToAppEnabled = self.config_info[
                    "vmOpNotificationToAppEnabled"
                ]

        return config_spec

    def get_datastore_names(self):
        datastore_names = set()
        for file_property in [
            "vmPathName",
            "snapshotDirectory",
            "suspendDirectory",
            "logDirectory",
        ]:
            ds_match = self.datastore_rex.match(
                self.config_info["files"][file_property]
            )
            if ds_match:
                datastore_names.add(ds_match.group(1))
        for device in self.config_info["hardware"]["device"]:
            if device["_vimtype"] == "vim.vm.device.VirtualDisk":
                ds_match = self.datastore_rex.match(device["backing"]["fileName"])
                if ds_match:
                    datastore_names.add(ds_match.group(1))

        return list(datastore_names)

    def _extract_datastore_name(self, backing_path):
        backing_ds_match = self.datastore_rex.match(backing_path)
        if backing_ds_match:
            return backing_ds_match.group(1)

        raise RuntimeError(
            "Error getting datastore name from backing path %s" % (backing_path)
        )

    def _get_vm_datastore_name(self):
        return self._extract_datastore_name(self.config_info["files"]["vmPathName"])

    def _transform_bootOptions(self):
        boot_order_has_disk = False
        config_info_boot_options = self.config_info["bootOptions"]
        boot_options = vim.vm.BootOptions()
        boot_options.bootRetryDelay = config_info_boot_options["bootRetryDelay"]
        boot_options.bootRetryEnabled = config_info_boot_options["bootRetryEnabled"]
        boot_options.efiSecureBootEnabled = config_info_boot_options[
            "efiSecureBootEnabled"
        ]
        boot_options.enterBIOSSetup = config_info_boot_options["enterBIOSSetup"]
        boot_options.networkBootProtocol = config_info_boot_options[
            "networkBootProtocol"
        ]
        for boot_order in config_info_boot_options["bootOrder"]:
            boot_device = None
            if boot_order["_vimtype"] == "vim.vm.BootOptions.BootableCdromDevice":
                boot_device = vim.vm.BootOptions.BootableCdromDevice()
            elif boot_order["_vimtype"] == "vim.vm.BootOptions.BootableDiskDevice":
                boot_device = vim.vm.BootOptions.BootableDiskDevice()
                boot_device.deviceKey = boot_order["deviceKey"]
                boot_order_has_disk = True
            elif boot_order["_vimtype"] == "vim.vm.BootOptions.BootableEthernetDevice":
                boot_device = vim.vm.BootOptions.BootableEthernetDevice()
                boot_device.deviceKey = boot_order["deviceKey"]
            elif boot_order["_vimtype"] == "vim.vm.BootOptions.BootableFloppyDevice":
                boot_device = vim.vm.BootOptions.BootableFloppyDevice()

            boot_options.bootOrder.append(boot_device)

        # When bootOrder contains a disk, it must be set delayed to after disks were
        # added, otherwise it would fail.
        if boot_order_has_disk:
            if self.config_spec_delayed is None:
                self.config_spec_delayed = vim.vm.ConfigSpec()
                self.config_spec_delayed.bootOptions = boot_options
                return None

        return boot_options

    def _transform_cpuAffinity(self):
        if not self.config_info["cpuAffinity"]:
            return None
        cpu_affinity = vim.vm.AffinityInfo()
        for node_nr in self.config_info["cpuAffinity"]["affinitySet"]:
            cpu_affinity.affinitySet.append(node_nr)

        return cpu_affinity

    def _transform_cpuAllocation(self):
        if not self.config_info["cpuAllocation"]:
            return None

        return self._transform_ResourceAllocationInfo(self.config_info["cpuAllocation"])

    def _transform_memoryAllocation(self):
        if not self.config_info["memoryAllocation"]:
            return None

        return self._transform_ResourceAllocationInfo(
            self.config_info["memoryAllocation"]
        )

    def _transform_ResourceAllocationInfo(self, property_name):
        if not self.config_info[property_name]:
            return None

        config_info_allocation = self.config_info[property_name]
        resource_allocation_info = vim.ResourceAllocationInfo()
        resource_allocation_info.expandableReservation = config_info_allocation[
            "expandableReservation"
        ]
        resource_allocation_info.limit = config_info_allocation["limit"]
        resource_allocation_info.reservation = config_info_allocation["reservation"]
        resource_allocation_info.shares = vim.SharesInfo()
        resource_allocation_info.shares.level = config_info_allocation["shares"][
            "level"
        ]
        resource_allocation_info.shares.shares = config_info_allocation["shares"][
            "shares"
        ]

        return resource_allocation_info

    def _transform_cpuFeatureMask(self):
        if not self.config_info["cpuFeatureMask"]:
            return []
        cpu_feature_mask = []
        for cpu_id_info in self.config_info["cpuFeatureMask"]:
            cpu_id_info_spec = vim.vm.ConfigSpec.CpuIdInfoSpec()
            cpu_id_info_spec.info = vim.host.CpuIdInfo()
            cpu_id_info_spec.info.eax = cpu_id_info["eax"]
            cpu_id_info_spec.info.ebx = cpu_id_info["ebx"]
            cpu_id_info_spec.info.ecx = cpu_id_info["ecx"]
            cpu_id_info_spec.info.edx = cpu_id_info["edx"]
            cpu_id_info_spec.operation = vim.option.ArrayUpdateSpec.Operation().add
            cpu_feature_mask.append(cpu_id_info_spec)

        return cpu_feature_mask

    def _transform_devices(self):
        device_change = []
        default_devices = [
            "vim.vm.device.VirtualIDEController",
            "vim.vm.device.VirtualPS2Controller",
            "vim.vm.device.VirtualPCIController",
            "vim.vm.device.VirtualSIOController",
            "vim.vm.device.VirtualKeyboard",
            "vim.vm.device.VirtualVMCIDevice",
            "vim.vm.device.VirtualPointingDevice",
        ]

        omitted_devices = [
            "vim.vm.device.VirtualVideoCard",
        ]

        virtual_scsi_controllers = [
            "vim.vm.device.ParaVirtualSCSIController",
            "vim.vm.device.VirtualBusLogicController",
            "vim.vm.device.VirtualLsiLogicController",
            "vim.vm.device.VirtualLsiLogicSASController",
        ]

        virtual_misc_controllers = [
            "vim.vm.device.VirtualNVDIMMController",
            "vim.vm.device.VirtualNVMEController",
            "vim.vm.device.VirtualAHCIController",
        ]

        virtual_usb_controllers = [
            "vim.vm.device.VirtualUSBController",
            "vim.vm.device.VirtualUSBXHCIController",
        ]

        virtual_ethernet_cards = [
            "vim.vm.device.VirtualE1000",
            "vim.vm.device.VirtualE1000e",
            "vim.vm.device.VirtualPCNet32",
            "vim.vm.device.VirtualSriovEthernetCard",
            "vim.vm.device.VirtualVmxnet2",
            "vim.vm.device.VirtualVmxnet3",
        ]

        for device in self.config_info["hardware"]["device"]:
            if device["_vimtype"] in default_devices + omitted_devices:
                continue
            device_spec = vim.vm.device.VirtualDeviceSpec()
            device_spec.operation = vim.vm.device.VirtualDeviceSpec.Operation().add
            add_device = None
            add_disk_device_delayed = None

            if device["_vimtype"] in virtual_scsi_controllers:
                add_device = self._transform_virtual_scsi_controller(device)
            elif device["_vimtype"] in virtual_misc_controllers:
                add_device = self._transform_virtual_misc_controller(device)
            elif device["_vimtype"] in virtual_usb_controllers:
                add_device = self._transform_virtual_usb_controller(device)
            elif device["_vimtype"] == "vim.vm.device.VirtualCdrom":
                add_device = self._transform_virtual_cdrom(device)
            elif device["_vimtype"] == "vim.vm.device.VirtualDisk":
                device_spec.fileOperation = (
                    vim.vm.device.VirtualDeviceSpec.FileOperation().create
                )
                # As _transform_virtual_disk() will only change the backing datastore
                # for disks which were in same datastore as the VM, The backing datastore
                # will remain unchanged for disks in other datastores. It does not work
                # to create these together with the VM, they must be added delayed one-by-one
                # after the VM was created.
                # The best solution seems to be adding all disks delayed, to be able to create
                # a proper map of backed up to created disks, no matter in which datastore they
                # are.
                add_device = None
                add_disk_device_delayed = self._transform_virtual_disk(device)
            elif device["_vimtype"] in virtual_ethernet_cards:
                add_device = self._transform_virtual_ethernet_card(device)
            elif device["_vimtype"] == "vim.vm.device.VirtualFloppy":
                add_device = self._transform_virtual_floppy(device)
            elif device["_vimtype"] == "vim.vm.device.VirtualPCIPassthrough":
                add_device = self._transform_virtual_pci_passthrough(device)
            elif device["_vimtype"] == "vim.vm.device.VirtualEnsoniq1371":
                add_device = self._transform_virtual_ensoniq1371(device)
            else:
                raise RuntimeError(
                    "Error: Unknown Device Type %s" % (device["_vimtype"])
                )

            if add_device:
                device_spec.device = add_device
                device_change.append(device_spec)

            if add_disk_device_delayed:
                device_spec.device = add_disk_device_delayed
                self.disk_device_change_delayed.append(device_spec)

        return device_change

    def _transform_virtual_scsi_controller(self, device):
        add_device = None
        if device["_vimtype"] == "vim.vm.device.ParaVirtualSCSIController":
            add_device = vim.vm.device.ParaVirtualSCSIController()
        elif device["_vimtype"] == "vim.vm.device.VirtualBusLogicController":
            add_device = vim.vm.device.VirtualBusLogicController()
        elif device["_vimtype"] == "vim.vm.device.VirtualLsiLogicController":
            add_device = vim.vm.device.VirtualLsiLogicController()
        elif device["_vimtype"] == "vim.vm.device.VirtualLsiLogicSASController":
            add_device = vim.vm.device.VirtualLsiLogicSASController()
        else:
            raise RuntimeError(
                "Error: Unknown SCSI controller type %s" % (device["_vimtype"])
            )

        add_device.key = device["key"] * -1
        add_device.busNumber = device["busNumber"]
        add_device.sharedBus = device["sharedBus"]

        return add_device

    def _transform_virtual_misc_controller(self, device):
        add_device = None
        if device["_vimtype"] == "vim.vm.device.VirtualNVDIMMController":
            add_device = vim.vm.device.VirtualNVDIMMController()
        elif device["_vimtype"] == "vim.vm.device.VirtualNVMEController":
            add_device = vim.vm.device.VirtualNVMEController()
        elif device["_vimtype"] == "vim.vm.device.VirtualAHCIController":
            add_device = vim.vm.device.VirtualAHCIController()
        else:
            raise RuntimeError(
                "Error: Unknown controller type %s" % (device["_vimtype"])
            )

        add_device.key = device["key"] * -1
        add_device.busNumber = device["busNumber"]

        return add_device

    def _transform_connectable(self, device):
        connectable = vim.vm.device.VirtualDevice.ConnectInfo()
        connectable.allowGuestControl = device["connectable"]["allowGuestControl"]
        connectable.startConnected = device["connectable"]["startConnected"]
        # connected = True would only make sense for running VM
        connectable.connected = False
        return connectable

    def _transform_virtual_usb_controller(self, device):
        add_device = None
        if device["_vimtype"] == "vim.vm.device.VirtualUSBController":
            add_device = vim.vm.device.VirtualUSBController()
            add_device.ehciEnabled = device["ehciEnabled"]
        elif device["_vimtype"] == "vim.vm.device.VirtualUSBXHCIController":
            add_device = vim.vm.device.VirtualUSBXHCIController()
        else:
            raise RuntimeError(
                "Error: Unknown USB controller type %s" % (device["_vimtype"])
            )

        add_device.key = device["key"] * -1
        add_device.autoConnectDevices = device["autoConnectDevices"]

        return add_device

    def _transform_virtual_cdrom(self, device):
        add_device = vim.vm.device.VirtualCdrom()
        add_device.key = device["key"] * -1
        if (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualCdrom.AtapiBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualCdrom.AtapiBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        elif (
            device["backing"]["_vimtype"] == "vim.vm.device.VirtualCdrom.IsoBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualCdrom.IsoBackingInfo()
            add_device.backing.backingObjectId = device["backing"]["backingObjectId"]
            add_device.backing.datastore = vim.Datastore(device["backing"]["datastore"])
            add_device.backing.fileName = device["backing"]["fileName"]
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualCdrom.PassthroughBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualCdrom.PassthroughBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualCdrom.RemoteAtapiBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualCdrom.RemoteAtapiBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualCdrom.RemotePassthroughBackingInfo"
        ):
            add_device.backing = (
                vim.vm.device.VirtualCdrom.RemotePassthroughBackingInfo()
            )
            add_device.backing.exclusive = device["backing"]["exclusive"]
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        else:
            raise RuntimeError(
                "Error: Unknown CDROM backing type %s" % (device["backing"]["_vimtype"])
            )

        add_device.connectable = self._transform_connectable(device)
        self._transform_controllerkey_and_unitnumber(add_device, device)

        return add_device

    def _transform_virtual_floppy(self, device):
        add_device = vim.vm.device.VirtualFloppy()
        add_device.key = device["key"] * -1
        if (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualFloppy.DeviceBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualFloppy.DeviceBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualFloppy.RemoteDeviceBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualFloppy.RemoteDeviceBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        else:
            raise RuntimeError(
                "Unknown Backing for Floppy: %s" % (device["backing"]["_vimtype"])
            )

        add_device.connectable = self._transform_connectable(device)
        self._transform_controllerkey_and_unitnumber(add_device, device)

        return add_device

    def _transform_virtual_pci_passthrough(self, device):
        add_device = vim.vm.device.VirtualPCIPassthrough()
        add_device.key = device["key"] * -1
        if (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualPCIPassthrough.DeviceBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualPCIPassthrough.DeviceBackingInfo()
            add_device.backing.deviceId = device["backing"]["deviceId"]
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.id = device["backing"]["id"]
            add_device.backing.systemId = device["backing"]["systemId"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
            add_device.backing.vendorId = device["backing"]["vendorId"]

        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualPCIPassthrough.VmiopBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualPCIPassthrough.VmiopBackingInfo()
            add_device.backing.vgpu = device["backing"]["vgpu"]

            # Since vSphere API 7.0.2.0
            self._transform_property(
                property_name="migrateSupported",
                source_data=device["backing"],
                target_object=add_device.backing,
                minimum_pyvmomi_version="7.0.2.0",
            )

            # Since 8.0.0.1
            for property_name in ("enhancedMigrateCapability", "vgpuMigrateDataSizeMB"):
                self._transform_property(
                    property_name=property_name,
                    source_data=device["backing"],
                    target_object=add_device.backing,
                    minimum_pyvmomi_version="8.0.0.1",
                )

        else:
            raise RuntimeError(
                "Unknown Backing for VirtualPCIPassthrough: %s"
                % (device["backing"]["_vimtype"])
            )

        if device["connectable"]:
            add_device.connectable = self._transform_connectable(device)

        self._transform_controllerkey_and_unitnumber(add_device, device)

        return add_device

    def _transform_virtual_ensoniq1371(self, device):
        add_device = vim.vm.device.VirtualEnsoniq1371()
        add_device.key = device["key"] * -1
        if (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualSoundCard.DeviceBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualSoundCard.DeviceBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        else:
            raise RuntimeError(
                "Unknown Backing for VirtualEnsoniq1371: %s"
                % (device["backing"]["_vimtype"])
            )

        if device["connectable"]:
            add_device.connectable = self._transform_connectable(device)

        self._transform_controllerkey_and_unitnumber(add_device, device)

        return add_device

    def _transform_virtual_disk(self, device):
        target_datastore_name = None
        add_device = vim.vm.device.VirtualDisk()
        add_device.key = device["key"] * -1
        if (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualDisk.FlatVer2BackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualDisk.FlatVer2BackingInfo()
            # Limitation: If a target datastore was given, in the first implementation to support
            # VMs using disks in multiple datastores, only disks that have been stored
            # together with the VM will be transformed to the new datastore.
            # To transform multiple datastores, it would be required to provide a mapping
            # from old to new datastores, which would be complicated to pass with plugin options.

            # if a snapshot existed at backup time, the disk backing name will be like
            # [datastore1] tcl131-test1_1/tcl131-test1-000001.vmdk
            # the part "-000001" must be removed when recreating a VM:
            orig_disk_backing_path = self.backing_filename_snapshot_rex.sub(
                ".vmdk", device["backing"]["fileName"]
            )
            orig_disk_backing_datastore_name = self._extract_datastore_name(
                orig_disk_backing_path
            )

            # When datastore is not changed, restore disk path will be the same as backed up disk
            self.vadp.restore_disk_paths_map[orig_disk_backing_path] = (
                orig_disk_backing_path
            )

            # Note: self.target_datastore_name will only be present if explicitly specified on
            # restore using plugin option restore_datastore

            # if a new VM name was passed, the VMFS path must be adapted to avoid an unnecessarily
            # first step failing task trying to create an existing disk. However, this is still
            # not easily avoidable when restoring to different folder but same VM name.
            if orig_disk_backing_datastore_name == self.orig_vm_datastore_name:
                if self.target_datastore_name:
                    target_datastore_name = self.target_datastore_name
                elif (
                    self.new_target_vm_name
                    and self.config_info["name"] != self.new_target_vm_name
                ):
                    target_datastore_name = orig_disk_backing_datastore_name

            if target_datastore_name:
                bareosfd.DebugMessage(
                    100,
                    "_transform_virtual_disk(): new_vm_name: %s, config_info vm name: %s\n"
                    % (self.new_target_vm_name, self.config_info["name"]),
                )

                if (
                    self.new_target_vm_name
                    and self.config_info["name"] != self.new_target_vm_name
                ):
                    # replace both datastore name and VMFS directory in backing fileName,
                    # for example this will transform
                    #  [esxi2-ds1] deb12test2/deb12test2.vmdk
                    # into
                    #  [esxi2-ds1] deb12test2-rest/deb12test2-rest.vmdk
                    # or
                    #  [esxi2-ds1] deb12test2/deb12test2_1.vmdk
                    # into
                    #  [esxi2-ds1] deb12test2-rest/deb12test2-rest_1.vmdk
                    # and also adapt the datastore name if it changed.
                    new_disk_filename = os.path.basename(
                        orig_disk_backing_path
                    ).replace(self.config_info["name"], self.new_target_vm_name)
                    device["backing"]["fileName"] = "[%s] %s/%s" % (
                        target_datastore_name,
                        self.new_target_vm_name,
                        new_disk_filename,
                    )
                else:
                    # replace datastore name in backing fileName
                    device["backing"]["fileName"] = self.datastore_rex.sub(
                        "[" + target_datastore_name + "]",
                        orig_disk_backing_path,
                        count=1,
                    )
                self.vadp.restore_disk_paths_map[orig_disk_backing_path] = device[
                    "backing"
                ]["fileName"]

            backing_ds_name = self._extract_datastore_name(
                device["backing"]["fileName"]
            )

            ds_mo = [ds for ds in self.vadp.dc.datastore if ds.name == backing_ds_name][
                0
            ]
            add_device.backing.datastore = ds_mo

            add_device.backing.fileName = device["backing"]["fileName"]
            add_device.backing.digestEnabled = device["backing"]["digestEnabled"]
            add_device.backing.diskMode = device["backing"]["diskMode"]
            add_device.backing.eagerlyScrub = device["backing"]["eagerlyScrub"]
            # add_device.backing.keyId = device["backing"]["keyId"]
            add_device.backing.sharing = device["backing"]["sharing"]
            add_device.backing.thinProvisioned = device["backing"]["thinProvisioned"]
            # add_device.backing.uuid = device["backing"]["uuid"]
            add_device.backing.writeThrough = device["backing"]["writeThrough"]
            bareosfd.DebugMessage(
                100,
                "_transform_virtual_disk(): backing.fileName: %s\n"
                % (add_device.backing.fileName),
            )
        else:
            raise RuntimeError(
                "Unknown Backing for disk: %s" % (device["backing"]["_vimtype"])
            )

        add_device.storageIOAllocation = vim.StorageResourceManager.IOAllocationInfo()
        add_device.storageIOAllocation.shares = vim.SharesInfo()
        add_device.storageIOAllocation.shares.level = device["storageIOAllocation"][
            "shares"
        ]["level"]
        add_device.storageIOAllocation.shares.shares = device["storageIOAllocation"][
            "shares"
        ]["shares"]
        add_device.storageIOAllocation.limit = device["storageIOAllocation"]["limit"]
        add_device.storageIOAllocation.reservation = device["storageIOAllocation"][
            "reservation"
        ]

        self._transform_controllerkey_and_unitnumber(add_device, device)
        add_device.capacityInBytes = device["capacityInBytes"]

        return add_device

    def _transform_virtual_ethernet_card(self, device):
        add_device = None
        if device["_vimtype"] == "vim.vm.device.VirtualE1000":
            add_device = vim.vm.device.VirtualE1000()
        elif device["_vimtype"] == "vim.vm.device.VirtualE1000e":
            add_device = vim.vm.device.VirtualE1000e()
        elif device["_vimtype"] == "vim.vm.device.VirtualPCNet32":
            add_device = vim.vm.device.VirtualPCNet32()
        elif device["_vimtype"] == "vim.vm.device.VirtualSriovEthernetCard":
            add_device = vim.vm.device.VirtualSriovEthernetCard()
        elif device["_vimtype"] == "vim.vm.device.VirtualVmxnet":
            add_device = vim.vm.device.VirtualVmxnet()
        elif device["_vimtype"] == "vim.vm.device.VirtualVmxnet2":
            add_device = vim.vm.device.VirtualVmxnet2()
        elif device["_vimtype"] == "vim.vm.device.VirtualVmxnet3":
            add_device = vim.vm.device.VirtualVmxnet3()
        elif device["_vimtype"] == "vim.vm.device.VirtualVmxnet3Vrdma":
            add_device = vim.vm.device.VirtualVmxnet3Vrdma()
        else:
            raise RuntimeError("Unknown ethernet card type: %s" % (device["_vimtype"]))

        if (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualEthernetCard.NetworkBackingInfo"
        ):
            add_device.backing = vim.vm.device.VirtualEthernetCard.NetworkBackingInfo()
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
            add_device.backing.network = vim.Network(device["backing"]["network"])
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualEthernetCard.DistributedVirtualPortBackingInfo"
        ):
            add_device.backing = (
                vim.vm.device.VirtualEthernetCard.DistributedVirtualPortBackingInfo()
            )
            add_device.backing.port = vim.dvs.PortConnection()
            add_device.backing.port.portgroupKey = device["backing"]["port"][
                "portgroupKey"
            ]
            add_device.backing.port.switchUuid = device["backing"]["port"]["switchUuid"]
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualEthernetCard.OpaqueNetworkBackingInfo"
        ):
            add_device.backing = (
                vim.vm.device.VirtualEthernetCard.OpaqueNetworkBackingInfo()
            )
            add_device.backing.opaqueNetworkId = device["backing"]["opaqueNetworkId"]
            add_device.backing.opaqueNetworkType = device["backing"][
                "opaqueNetworkType"
            ]
        elif (
            device["backing"]["_vimtype"]
            == "vim.vm.device.VirtualEthernetCard.LegacyNetworkBackingInfo"
        ):
            add_device.backing = (
                vim.vm.device.VirtualEthernetCard.LegacyNetworkBackingInfo()
            )
            add_device.backing.deviceName = device["backing"]["deviceName"]
            add_device.backing.useAutoDetect = device["backing"]["useAutoDetect"]
        else:
            raise RuntimeError(
                "Unknown ethernet backing type: %s" % (device["backing"]["_vimtype"])
            )

        add_device.key = device["key"] * -1
        add_device.connectable = self._transform_connectable(device)
        self._transform_controllerkey_and_unitnumber(add_device, device)
        # Note: MAC address preservation is not safe with addressType "manual", the
        # server does not check for conflicts. The calling code should check for
        # MAC address conflicts before and set addressType to "generated" or "assigned"
        # then the server will detect conflicts and generate a new MAC if needed.
        if "macAddress" in device:
            add_device.macAddress = device["macAddress"]
            add_device.addressType = device["addressType"]
        add_device.externalId = device["externalId"]
        add_device.resourceAllocation = (
            vim.vm.device.VirtualEthernetCard.ResourceAllocation()
        )
        add_device.resourceAllocation.limit = device["resourceAllocation"]["limit"]
        add_device.resourceAllocation.reservation = device["resourceAllocation"][
            "reservation"
        ]
        add_device.resourceAllocation.share = vim.SharesInfo()
        add_device.resourceAllocation.share.shares = device["resourceAllocation"][
            "share"
        ]["shares"]
        add_device.resourceAllocation.share.level = device["resourceAllocation"][
            "share"
        ]["level"]
        add_device.uptCompatibilityEnabled = device["uptCompatibilityEnabled"]
        add_device.wakeOnLanEnabled = device["wakeOnLanEnabled"]

        return add_device

    def _transform_extraConfig(self):
        extra_config = []
        for option_value in self.config_info["extraConfig"]:
            extra_config.append(vim.option.OptionValue())
            extra_config[-1].key = option_value["key"]
            extra_config[-1].value = option_value["value"]

        return extra_config

    def _transform_files(self):
        files = vim.vm.FileInfo()

        if self.target_datastore_name:
            # replace datastore name in all properties
            for property_name in [
                "ftMetadataDirectory",
                "logDirectory",
                "snapshotDirectory",
                "suspendDirectory",
                "vmPathName",
            ]:
                if self.config_info["files"].get(property_name):
                    self.config_info["files"][property_name] = self.datastore_rex.sub(
                        "[" + self.target_datastore_name + "]",
                        self.config_info["files"][property_name],
                        count=1,
                    )

        files.ftMetadataDirectory = self.config_info["files"]["ftMetadataDirectory"]
        files.logDirectory = self.config_info["files"]["logDirectory"]
        files.snapshotDirectory = self.config_info["files"]["snapshotDirectory"]
        files.suspendDirectory = self.config_info["files"]["suspendDirectory"]
        files.vmPathName = self.config_info["files"]["vmPathName"]

        if self.new_target_vm_name:
            files.vmPathName = "[%s] %s/%s.vmx" % (
                self.new_vm_datastore_name,
                self.new_target_vm_name,
                self.new_target_vm_name,
            )
            # The values of the following properties could have pointed to different
            # datastores and/or directories, but that's very uncommon. It's
            # definitely better to set the directory names to the new VM name,
            # so that all files related to the VM will be in the same directory.
            for property_name in [
                "ftMetadataDirectory",
                "logDirectory",
                "snapshotDirectory",
                "suspendDirectory",
            ]:
                if self.config_info["files"][property_name] is not None:
                    setattr(
                        files,
                        property_name,
                        "[%s] %s/"
                        % (self.new_vm_datastore_name, self.new_target_vm_name),
                    )

        return files

    def _transform_flags(self):
        flags = vim.vm.FlagInfo()
        flags.cbrcCacheEnabled = self.config_info["flags"]["cbrcCacheEnabled"]
        flags.disableAcceleration = self.config_info["flags"]["disableAcceleration"]
        flags.diskUuidEnabled = self.config_info["flags"]["diskUuidEnabled"]
        flags.enableLogging = self.config_info["flags"]["enableLogging"]
        flags.faultToleranceType = self.config_info["flags"]["faultToleranceType"]
        flags.monitorType = self.config_info["flags"]["monitorType"]
        flags.snapshotLocked = self.config_info["flags"]["snapshotLocked"]
        flags.snapshotPowerOffBehavior = self.config_info["flags"][
            "snapshotPowerOffBehavior"
        ]
        flags.useToe = self.config_info["flags"]["useToe"]
        flags.vbsEnabled = self.config_info["flags"]["vbsEnabled"]
        flags.virtualExecUsage = self.config_info["flags"]["virtualExecUsage"]
        flags.virtualMmuUsage = self.config_info["flags"]["virtualMmuUsage"]
        flags.vvtdEnabled = self.config_info["flags"]["vvtdEnabled"]

        return flags

    def _transform_ftInfo(self):
        if self.config_info["ftInfo"] is None:
            return None

        ft_info = vim.vm.FaultToleranceConfigInfo()
        ft_info.configPaths = []
        for config_path in self.config_info["ftInfo"]["configPaths"]:
            ft_info.configPaths.append(config_path)
        ft_info.instanceUuids = []
        for instance_uuid in self.config_info["ftInfo"]["instanceUuids"]:
            ft_info.instanceUuids.append(instance_uuid)
        ft_info.role = self.config_info["ftInfo"]["role"]

        return ft_info

    def _transform_guestMonitoringModeInfo(self):
        guest_monitoring_mode_info = vim.vm.GuestMonitoringModeInfo()
        if self.config_info.get("guestMonitoringModeInfo"):
            guest_monitoring_mode_info.gmmAppliance = self.config_info[
                "guestMonitoringModeInfo"
            ].get("gmmAppliance")
            guest_monitoring_mode_info.gmmFile = self.config_info[
                "guestMonitoringModeInfo"
            ].get("gmmFile")

        return guest_monitoring_mode_info

    def _transform_latencySensitivity(self, latency_sensitivity_config):
        latency_sensitivity = vim.LatencySensitivity()
        if latency_sensitivity_config["level"] == "custom":
            # Note: custom level is deprecrated since 5.5
            raise RuntimeError(
                "Invalid latency sensitivity: custom (deprecated since 5.5)"
            )

        latency_sensitivity.level = latency_sensitivity_config["level"]

        return latency_sensitivity

    def _transform_managedBy(self):
        if self.config_info["managedBy"] is None:
            return None

        managed_by = vim.ext.ManagedByInfo()
        managed_by.extensionKey = self.config_info["managedBy"]["extensionKey"]
        managed_by.type = self.config_info["managedBy"]["type"]

        return managed_by

    def _transform_pmem(self):
        if (
            self.config_info.get("pmem") is None
            or self.config_info["pmem"].get("snapshotMode") is None
        ):
            return None

        pmem = vim.vm.VirtualPMem()
        pmem.snapshotMode = self.config_info["pmem"]["snapshotMode"]

        return pmem

    def _transform_defaultPowerOps(self):
        power_ops = vim.vm.DefaultPowerOpInfo()
        power_ops.defaultPowerOffType = self.config_info["defaultPowerOps"][
            "defaultPowerOffType"
        ]
        power_ops.defaultResetType = self.config_info["defaultPowerOps"][
            "defaultResetType"
        ]
        power_ops.defaultSuspendType = self.config_info["defaultPowerOps"][
            "defaultSuspendType"
        ]
        power_ops.powerOffType = self.config_info["defaultPowerOps"]["powerOffType"]
        power_ops.resetType = self.config_info["defaultPowerOps"]["resetType"]
        power_ops.standbyAction = self.config_info["defaultPowerOps"]["standbyAction"]
        power_ops.suspendType = self.config_info["defaultPowerOps"]["suspendType"]

        return power_ops

    def _transform_sgxInfo(self):
        sgx_info = vim.vm.SgxInfo()
        sgx_info.epcSize = self.config_info["sgxInfo"]["epcSize"]
        sgx_info.flcMode = self.config_info["sgxInfo"].get("flcMode")
        sgx_info.lePubKeyHash = self.config_info["sgxInfo"].get("lePubKeyHash")

        return sgx_info

    def _transform_tools(self):
        tools_config_info = vim.vm.ToolsConfigInfo()
        tools_config_info.afterPowerOn = self.config_info["tools"]["afterPowerOn"]
        tools_config_info.afterResume = self.config_info["tools"]["afterResume"]
        tools_config_info.beforeGuestReboot = self.config_info["tools"][
            "beforeGuestReboot"
        ]
        tools_config_info.beforeGuestShutdown = self.config_info["tools"][
            "beforeGuestShutdown"
        ]
        tools_config_info.beforeGuestStandby = self.config_info["tools"][
            "beforeGuestStandby"
        ]
        tools_config_info.syncTimeWithHost = self.config_info["tools"][
            "syncTimeWithHost"
        ]
        # Since vSphere API 7.0.1.0
        if "syncTimeWithHostAllowed" in self.config_info["tools"]:
            tools_config_info.syncTimeWithHostAllowed = self.config_info["tools"][
                "syncTimeWithHostAllowed"
            ]
        tools_config_info.toolsUpgradePolicy = self.config_info["tools"][
            "toolsUpgradePolicy"
        ]

        return tools_config_info

    def _transform_vAppConfig(self):
        if self.config_info["vAppConfig"] is None:
            return None

        vapp_config_spec = vim.vApp.VmConfigSpec()
        vapp_config_spec.eula = self.config_info["vAppConfig"]["eula"]
        vapp_config_spec.installBootRequired = self.config_info["vAppConfig"][
            "installBootRequired"
        ]
        vapp_config_spec.installBootStopDelay = self.config_info["vAppConfig"][
            "installBootStopDelay"
        ]
        vapp_config_spec.ipAssignment = self._transform_VAppIpAssignmentInfo(
            self.config_info["vAppConfig"]["ipAssignment"]
        )
        vapp_config_spec.ovfEnvironmentTransport = []
        for transport in self.config_info["vAppConfig"]["ovfEnvironmentTransport"]:
            vapp_config_spec.ovfEnvironmentTransport.append(transport)

        vapp_config_spec.ovfSection = self._transform_VAppOvfSectionInfo()
        vapp_config_spec.product = self._transform_VAppProductInfo()
        vapp_config_spec.property = self._transform_VAppPropertyInfo()

        return vapp_config_spec

    def _transform_VAppIpAssignmentInfo(self, ip_assignment_info):
        ip_assignment = vim.vApp.IPAssignmentInfo()
        ip_assignment.ipAllocationPolicy = ip_assignment_info["ipAllocationPolicy"]
        ip_assignment.ipProtocol = ip_assignment_info["ipProtocol"]
        ip_assignment.supportedAllocationScheme = []
        for scheme in ip_assignment_info["supportedAllocationScheme"]:
            ip_assignment.supportedAllocationScheme.append(scheme)
        ip_assignment.supportedIpProtocol = []
        for protocol in ip_assignment_info["supportedIpProtocol"]:
            ip_assignment.supportedIpProtocol.append(protocol)

        return ip_assignment

    def _transform_VAppOvfSectionInfo(self):
        ovf_section_specs = []
        for ovf_section_info in self.config_info["vAppConfig"]["ovfSection"]:
            ovf_section_spec = vim.vApp.OvfSectionSpec()
            ovf_section_spec.operation = "add"
            ovf_section_spec.info = vim.vApp.OvfSectionInfo()
            ovf_section_spec.info.atEnvelopeLevel = ovf_section_info["atEnvelopeLevel"]
            ovf_section_spec.info.contents = ovf_section_info["contents"]
            ovf_section_spec.info.key = ovf_section_info["key"]
            ovf_section_spec.info.namespace = ovf_section_info["namespace"]
            ovf_section_spec.info.type = ovf_section_info["type"]
            ovf_section_specs.append(ovf_section_spec)

        return ovf_section_specs

    def _transform_VAppProductInfo(self):
        product_specs = []
        for product_info in self.config_info["vAppConfig"]["product"]:
            product_spec = vim.vApp.ProductSpec()
            product_spec.operation = "add"
            product_spec.info = vim.vApp.ProductInfo()
            product_spec.info.appUrl = product_info["appUrl"]
            product_spec.info.classId = product_info["classId"]
            product_spec.info.fullVersion = product_info["fullVersion"]
            product_spec.info.instanceId = product_info["instanceId"]
            product_spec.info.key = product_info["key"]
            product_spec.info.name = product_info["name"]
            product_spec.info.productUrl = product_info["productUrl"]
            product_spec.info.vendor = product_info["vendor"]
            product_spec.info.vendorUrl = product_info["vendorUrl"]
            product_spec.info.version = product_info["version"]
            product_specs.append(product_spec)

        return product_specs

    def _transform_VAppPropertyInfo(self):
        property_specs = []
        for property_info in self.config_info["vAppConfig"]["property"]:
            property_spec = vim.vApp.PropertySpec()
            property_spec.operation = "add"
            property_spec.info = vim.vApp.PropertyInfo()
            property_spec.info.category = property_info["category"]
            property_spec.info.classId = property_info["classId"]
            property_spec.info.defaultValue = property_info["defaultValue"]
            property_spec.info.description = property_info["description"]
            property_spec.info.id = property_info["id"]
            property_spec.info.instanceId = property_info["instanceId"]
            property_spec.info.key = property_info["key"]
            property_spec.info.label = property_info["label"]
            property_spec.info.type = property_info["type"]
            property_spec.info.typeReference = property_info["typeReference"]
            property_spec.info.userConfigurable = property_info["userConfigurable"]
            property_spec.info.value = property_info["value"]
            property_specs.append(property_spec)

        return property_specs

    def _transform_VirtualMachineVcpuConfig(self):
        spec_vcpu_configs = []
        for vcpu_config in self.config_info["vcpuConfig"]:
            spec_vcpu_config = vim.vm.VcpuConfig()
            spec_vcpu_config.latencySensitivity = self._transform_latencySensitivity(
                vcpu_config["latencySensitivity"]
            )
            spec_vcpu_configs.append(spec_vcpu_config)

        return spec_vcpu_configs

    def _transform_controllerkey_and_unitnumber(self, add_device, device):
        # Looks like negative values must not be used for default devices,
        # the second VirtualIDEController seems to have key 201, that always seems to be the case.
        # Same for VirtualCdrom, getting error "The device '1' is referring to a nonexisting controller '-200'."
        # Looks like negative values must not be used for default devices,
        # the VirtualSIOController seems to have key 400, that seems to be always the case.
        # Looks like negative values must not be used for default devices,
        # the VirtualPCIController always seems to have key 100.
        default_devices_controller_keys = [100, 200, 201, 400]
        add_device.controllerKey = device["controllerKey"]
        if device["controllerKey"] not in default_devices_controller_keys:
            add_device.controllerKey = device["controllerKey"] * -1
        add_device.unitNumber = device["unitNumber"]

    def _transform_property(
        self,
        property_name=None,
        source_data=None,
        target_object=None,
        minimum_pyvmomi_version=None,
    ):
        if source_data.get(property_name) is not None:
            if not hasattr(target_object, property_name):
                raise RuntimeError(
                    "Missing property %s in %s, pyVmomi %s or greater required"
                    % (property_name, source_data["_vimtype"], minimum_pyvmomi_version)
                )

            setattr(target_object, property_name, source_data[property_name])


# vim: tabstop=4 shiftwidth=4 softtabstop=4 expandtab
