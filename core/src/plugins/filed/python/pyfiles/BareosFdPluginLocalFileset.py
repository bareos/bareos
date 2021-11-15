#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2020 Bareos GmbH & Co. KG
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
