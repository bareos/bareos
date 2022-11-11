#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2022 Bareos GmbH & Co. KG
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
# Author: Maik Aussendorf
#
# Bareos python plugins class that adds files from a local list to
# the backup fileset

import bareosfd
import os
import re
from BareosFdPluginLocalFilesBaseclass import BareosFdPluginLocalFilesBaseclass
import stat


class BareosFdPluginLocalFileset(BareosFdPluginLocalFilesBaseclass):  # noqa
    """
    Simple Bareos-FD-Plugin-Class that parses a file and backups all files
    listed there Filename is taken from plugin argument 'filename'
    """

    def __init__(self, plugindef, mandatory_options=None):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        super(BareosFdPluginLocalFileset, self).__init__(plugindef, mandatory_options)

    def filename_is_allowed(self, filename, allowregex, denyregex):
        """
        Check, if filename is allowed.
        True, if matches allowreg and not denyregex.
        If allowreg is None, filename always matches
        If denyreg is None, it never matches
        """
        if allowregex is None or allowregex.search(filename):
            allowed = True
        else:
            allowed = False
        if denyregex is None or not denyregex.search(filename):
            denied = False
        else:
            denied = True
        if not allowed or denied:
            bareosfd.DebugMessage(100, "File %s denied by configuration\n" % (filename))
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "File %s denied by configuration\n" % (filename),
            )
            return False
        else:
            return True

    def start_backup_job(self):
        """
        At this point, plugin options were passed and checked already.
        We try to read from filename and setup the list of file to backup
        in self.files_to_backup
        """
        bareosfd.DebugMessage(
            100,
            "Using %s to search for local files\n" % self.options["filename"],
        )
        if os.path.exists(self.options["filename"]):
            try:
                config_file = open(self.options["filename"], "r")
            except:
                bareosfd.DebugMessage(
                    100,
                    "Could not open file %s\n" % (self.options["filename"]),
                )
                return bareosfd.bRC_Error
        else:
            bareosfd.DebugMessage(
                100, "File %s does not exist\n" % (self.options["filename"])
            )
            return bareosfd.bRC_Error
        # Check, if we have allow or deny regular expressions defined
        if "allow" in self.options:
            self.allow = re.compile(self.options["allow"])
        if "deny" in self.options:
            self.deny = re.compile(self.options["deny"])

        for listItem in config_file.read().splitlines():
            if os.path.isfile(listItem) and self.filename_is_allowed(
                listItem, self.allow, self.deny
            ):
                self.append_file_to_backup(listItem)
            if os.path.isdir(listItem):
                fullDirName = listItem
                # FD requires / at the end of a directory name
                if not fullDirName.endswith(tuple("/")):
                    fullDirName += "/"
                self.append_file_to_backup(fullDirName)
                for topdir, dirNames, fileNames in os.walk(listItem):
                    for fileName in fileNames:
                        if self.filename_is_allowed(
                            os.path.join(topdir, fileName),
                            self.allow,
                            self.deny,
                        ):
                            self.append_file_to_backup(os.path.join(topdir, fileName))
                    for dirName in dirNames:
                        fullDirName = os.path.join(topdir, dirName) + "/"
                        self.append_file_to_backup(fullDirName)
        bareosfd.DebugMessage(150, "Filelist: %s\n" % (self.files_to_backup))

        if not self.files_to_backup:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "No (allowed) files to backup found\n",
            )
            return bareosfd.bRC_Error
        else:
            return bareosfd.bRC_OK

    def plugin_io_open(self, IOP):
        self.FNAME = IOP.fname
        bareosfd.DebugMessage(250, "io_open: self.FNAME is set to %s\n" % (self.FNAME))
        if os.path.isdir(self.FNAME):
            bareosfd.DebugMessage(100, "%s is a directory\n" % (self.FNAME))
            self.fileType = "FT_DIR"
            bareosfd.DebugMessage(
                100,
                "Did not open file %s of type %s\n" % (self.FNAME, self.fileType),
            )
            return bareosfd.bRC_OK
        elif os.path.islink(self.FNAME):
            self.fileType = "FT_LNK"
            bareosfd.DebugMessage(
                100,
                "Did not open file %s of type %s\n" % (self.FNAME, self.fileType),
            )
            return bareosfd.bRC_OK
        elif os.path.exists(self.FNAME) and stat.S_ISFIFO(os.stat(self.FNAME).st_mode):
            self.fileType = "FT_FIFO"
            bareosfd.DebugMessage(
                100,
                "Did not open file %s of type %s\n" % (self.FNAME, self.fileType),
            )
            return bareosfd.bRC_OK
        else:
            self.fileType = "FT_REG"
            bareosfd.DebugMessage(
                150,
                "file %s has type %s - trying to open it\n"
                % (self.FNAME, self.fileType),
            )
        try:
            if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                bareosfd.DebugMessage(
                    100,
                    "Open file %s for writing with %s\n" % (self.FNAME, IOP),
                )
                dirname = os.path.dirname(self.FNAME)
                if not os.path.exists(dirname):
                    bareosfd.DebugMessage(
                        100,
                        "Directory %s does not exist, creating it now\n" % (dirname),
                    )
                    os.makedirs(dirname)
                self.file = open(self.FNAME, "wb")
            else:
                bareosfd.DebugMessage(
                    100,
                    "Open file %s for reading with %s\n" % (self.FNAME, IOP),
                )
                self.file = open(self.FNAME, "rb")

            # do io in core
            # IOP.filedes = self.file.fileno()
            # IOP.status =  bareosfd.io_status_core
            ##IOP.status = 1

            #  do io in plugin
            IOP.status = bareosfd.io_status_plugin
            ##IOP.status = 0

        except:
            IOP.status = -1
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK
