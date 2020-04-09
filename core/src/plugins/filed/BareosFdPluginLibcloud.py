#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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
# Original Author: Alexandre Bruyelles <git@jack.fr.eu.org>
#

import BareosFdPluginBaseclass
import bareosfd
import bareos_fd_consts
import ConfigParser as configparser
import datetime
import dateutil.parser
from bareos_libcloud_api.bucket_explorer import JOB_TYPE
from bareos_libcloud_api.debug import debugmessage
from bareos_libcloud_api.debug import jobmessage
from bareos_libcloud_api.debug import set_plugin_context
import io
import itertools
import libcloud
import multiprocessing
import os
from time import sleep

from BareosLibcloudApi import SUCCESS
from BareosLibcloudApi import ERROR
from BareosLibcloudApi import BareosLibcloudApi
from bareos_fd_consts import bRCs, bCFs, bIOPS, bJobMessageType, bFileType
from libcloud.storage.types import Provider
from libcloud.storage.types import ObjectDoesNotExistError
from sys import version_info


class FilenameConverter:
    __pathprefix = "PYLIBCLOUD:/"

    @staticmethod
    def BucketToBackup(filename):
        return FilenameConverter.__pathprefix + filename

    @staticmethod
    def BackupToBucket(filename):
        return filename.replace(FilenameConverter.__pathprefix, "")


def str2bool(data):
    if data == "false" or data == "False":
        return False
    if data == "true" or data == "True":
        return True
    raise Exception("%s: not a boolean" % (data,))


class IterStringIO(io.BufferedIOBase):
    def __init__(self, iterable):
        self.iter = itertools.chain.from_iterable(iterable)

    def read(self, n=None):
        return bytearray(itertools.islice(self.iter, None, n))


class BareosFdPluginLibcloud(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    def __init__(self, context, plugindef):
        set_plugin_context(context)
        debugmessage(
            100, "BareosFdPluginLibcloud called with plugindef: %s" % (plugindef,)
        )

        super(BareosFdPluginLibcloud, self).__init__(context, plugindef)
        super(BareosFdPluginLibcloud, self).parse_plugin_definition(context, plugindef)
        self.__parse_options(context)

        self.last_run = datetime.datetime.fromtimestamp(self.since)
        self.last_run = self.last_run.replace(tzinfo=None)

        # The backup job in process
        # Set to None when the whole backup is completed
        # Restore's path will not touch this
        self.current_backup_job = {}
        self.number_of_objects_to_backup = 0
        self.FILE = None
        self.active = True

    def __parse_options(self, context):
        accurate = bareos_fd_consts.bVariable["bVarAccurate"]
        accurate = bareosfd.GetValue(context, accurate)
        if accurate is None or accurate == 0:
            self.options["accurate"] = False
        else:
            self.options["accurate"] = True

    def parse_plugin_definition(self, context, plugindef):
        debugmessage(100, "parse_plugin_definition()")
        config_filename = self.options.get("config_file")
        if config_filename:
            if self.__parse_config_file(context, config_filename):
                return bRCs["bRC_OK"]
        debugmessage(100, "Could not load configfile %s" % (config_filename))
        jobmessage("M_FATAL", "Could not load configfile %s" % (config_filename))
        return bRCs["bRC_Error"]

    def __parse_config_file(self, context, config_filename):
        """
        Parse the config file given in the config_file plugin option
        """
        debugmessage(100, "parse_config_file(): parse %s\n" % (config_filename))

        self.config = configparser.ConfigParser()

        try:
            if version_info[:3] < (3, 2):
                self.config.readfp(open(config_filename))
            else:
                self.config.read_file(open(config_filename))
        except (IOError, OSError) as err:
            debugmessage(
                100,
                "Error reading config file %s: %s\n"
                % (self.options["config_file"], err.strerror),
            )
            return False

        return self.__check_config(context, config_filename)

    def __check_config(self, context, config_filename):
        """
        Check the configuration and set or override options if necessary,
        considering mandatory: username and password in the [credentials] section
        """
        mandatory_sections = ["credentials", "host", "misc"]
        mandatory_options = {}
        mandatory_options["credentials"] = ["username", "password"]
        mandatory_options["host"] = ["hostname", "port", "provider", "tls"]
        mandatory_options["misc"] = ["nb_worker", "queue_size", "prefetch_size", "temporary_download_directory"]

        # this maps config file options to libcloud options
        option_map = {
            "hostname": "host",
            "port": "port",
            "provider": "provider",
            "username": "key",
            "password": "secret",
        }

        for section in mandatory_sections:
            if not self.config.has_section(section):
                debugmessage(
                    100,
                    "BareosFdPluginLibcloud: Section [%s] missing in config file %s\n"
                    % (section, config_filename),
                )
                return False

            for option in mandatory_options[section]:
                if not self.config.has_option(section, option):
                    debugmessage(
                        100,
                        "BareosFdPluginLibcloud: Option [%s] missing in Section %s\n"
                        % (option, section),
                    )
                    return False

                value = self.config.get(section, option)

                try:
                    if option == "tls":
                        self.options["secure"] = str2bool(value)
                    elif option == "nb_worker":
                        self.options["nb_worker"] = int(value)
                    elif option == "queue_size":
                        self.options["queue_size"] = int(value)
                    elif option == "prefetch_size":
                        self.options["prefetch_size"] = eval(value)
                    elif option == "temporary_download_directory":
                        self.options["temporary_download_directory"] = value
                    else:
                        self.options[option_map[option]] = value
                except:
                    debugmessage(
                        100,
                        "Could not evaluate: %s in config file %s"
                        % (value, config_filename),
                    )
                    return False

        return True

    def start_backup_job(self, context):
        jobmessage(
            "M_INFO",
            "Start backup, try to connect to %s:%s"
            % (self.options["host"], self.options["port"]),
        )

        if BareosLibcloudApi.probe_driver(self.options) == "failed":
            jobmessage(
                "M_FATAL",
                "Could not connect to libcloud driver: %s:%s"
                % (self.options["host"], self.options["port"]),
            )
            return bRCs["bRC_Error"]

        jobmessage(
            "M_INFO",
            "Connected, last backup: %s (ts: %s)" % (self.last_run, self.since),
        )

        self.api = None
        try:
            self.api = BareosLibcloudApi(
                self.options, self.last_run, self.options["temporary_download_directory"]
            )
            debugmessage(100, "BareosLibcloudApi started")
        except Exception as e:
            debugmessage(100, "Error: %s" % e)
            jobmessage("M_FATAL", "Starting BareosLibcloudApi failed: %s" % e)
            return bRCs["bRC_Cancel"]

        return bRCs["bRC_OK"]

    def end_backup_job(self, context):
        if self.active:
            self.__shutdown()
        return bRCs["bRC_OK"]

    def check_file(self, context, fname):
        # All existing files/objects are passed to bareos
        # If bareos has not seen one, it does not exists anymore
        return bRCs["bRC_Error"]

    def __shutdown(self):
        self.active = False
        jobmessage(
            "M_INFO",
            "BareosFdPluginLibcloud finished with %d files"
            % (self.number_of_objects_to_backup)
        )

        debugmessage(100, "Shut down BareosLibcloudApi")
        self.api.shutdown()
        debugmessage(100, "BareosLibcloudApi is shut down")

    def start_backup_file(self, context, savepkt):
        error = False
        while self.active:
            if self.api.check_worker_messages() != SUCCESS:
                self.active = False
                error = True
            else:
                self.current_backup_job = self.api.get_next_job()
                if self.current_backup_job != None:
                    break
                elif self.api.worker_ready():
                    self.active = False
                else:
                    sleep(0.01)

        if not self.active:
            self.__shutdown()
            savepkt.fname = "empty"  # dummy value
            if error:
                jobmessage("M_FATAL", "Shutdown after worker error")
                return bRCs["bRC_Cancel"]
            else:
                return bRCs["bRC_Skip"]

        filename = FilenameConverter.BucketToBackup(
            "%s/%s"
            % (self.current_backup_job["bucket"], self.current_backup_job["name"],)
        )
        jobmessage("M_INFO", "Backup file: %s" % (filename,))

        statp = bareosfd.StatPacket()
        statp.size = self.current_backup_job["size"]
        statp.mtime = self.current_backup_job["mtime"]
        statp.atime = 0
        statp.ctime = 0

        savepkt.statp = statp
        savepkt.fname = filename
        savepkt.type = bareos_fd_consts.bFileType["FT_REG"]

        if self.current_backup_job["type"] == JOB_TYPE.DOWNLOADED:
            self.FILE = self.current_backup_job["data"]
        elif self.current_backup_job["type"] == JOB_TYPE.TEMP_FILE:
            try:
                self.FILE = io.open(self.current_backup_job["tmpfile"], "rb")
            except Exception as e:
                jobmessage("M_FATAL", "Could not open temporary file for reading." % e)
                self.__shutdown()
                return bRCs["bRC_Error"]
        elif self.current_backup_job["type"] == JOB_TYPE.STREAM:
            try:
                self.FILE = IterStringIO(self.current_backup_job["data"].as_stream())
            except ObjectDoesNotExistError:
                jobmessage(
                    "M_WARNING",
                    "Skipped file %s because it does not exist anymore"
                    % (self.current_backup_job["name"]),
                )
                return bRCs["bRC_Skip"]
        else:
            raise Exception(value='Wrong argument for current_backup_job["type"]')

        return bRCs["bRC_OK"]

    def create_file(self, context, restorepkt):
        debugmessage(
            100, "create_file() entry point in Python called with %s\n" % (restorepkt)
        )
        FNAME = FilenameConverter.BackupToBucket(restorepkt.ofname)
        dirname = os.path.dirname(FNAME)
        if not os.path.exists(dirname):
            jobmessage(
                "M_INFO", "Directory %s does not exist, creating it\n" % dirname
            )
            os.makedirs(dirname)
        if restorepkt.type == bFileType["FT_REG"]:
            restorepkt.create_status = bCFs["CF_EXTRACT"]
        return bRCs["bRC_OK"]

    def plugin_io(self, context, IOP):
        if self.current_backup_job is None:
            return bRCs["bRC_Error"]
        if IOP.func == bIOPS["IO_OPEN"]:
            # Only used by the 'restore' path
            if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                self.FILE = open(FilenameConverter.BackupToBucket(IOP.fname), "wb")
            return bRCs["bRC_OK"]

        elif IOP.func == bIOPS["IO_READ"]:
            IOP.buf = bytearray(IOP.count)
            IOP.io_errno = 0
            if self.FILE is None:
                return bRCs["bRC_Error"]
            try:
                buf = self.FILE.read(IOP.count)
                IOP.buf[:] = buf
                IOP.status = len(buf)
                return bRCs["bRC_OK"]
            except IOError as e:
                jobmessage(
                    "M_ERROR",
                    "Cannot read from %s/%s: %s"
                    % (self.current_backup_job["bucket"], self.current_backup_job["name"], e),
                )
                IOP.status = 0
                IOP.io_errno = e.errno
                return bRCs["bRC_Error"]

        elif IOP.func == bIOPS["IO_WRITE"]:
            try:
                self.FILE.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as msg:
                IOP.io_errno = -1
                jobmessage("M_ERROR", "Failed to write data: %s" % (msg,))
            return bRCs["bRC_OK"]
        elif IOP.func == bIOPS["IO_CLOSE"]:
            if self.FILE:
                self.FILE.close()
            return bRCs["bRC_OK"]

        return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        if self.current_backup_job is not None:
            self.number_of_objects_to_backup += 1
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]
