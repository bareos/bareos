#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
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
from bareos_fd_consts import bJobMessageType, bFileType, bRCs
import os
import re
import BareosFdPluginBaseclass


class BareosFdPluginLocalFileset(
    BareosFdPluginBaseclass.BareosFdPluginBaseclass
):  # noqa
    """
    Simple Bareos-FD-Plugin-Class that parses a file and backups all files
    listed there Filename is taken from plugin argument 'filename'
    """

    def __init__(self, context, plugindef, mandatory_options=None):
        bareosfd.DebugMessage(
            context,
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        if mandatory_options is None:
            mandatory_options = ["filename"]
        # Last argument of super constructor is a list of mandatory arguments
        super(BareosFdPluginLocalFileset, self).__init__(
            context, plugindef, mandatory_options
        )
        self.files_to_backup = []
        self.allow = None
        self.deny = None

    def filename_is_allowed(self, context, filename, allowregex, denyregex):
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
            bareosfd.DebugMessage(
                context, 100, "File %s denied by configuration\n" % (filename)
            )
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "File %s denied by configuration\n" % (filename),
            )
            return False
        else:
            return True

    def start_backup_job(self, context):
        """
        At this point, plugin options were passed and checked already.
        We try to read from filename and setup the list of file to backup
        in self.files_to_backup
        """
        bareosfd.DebugMessage(
            context,
            100,
            "Using %s to search for local files\n" % self.options["filename"],
        )
        if os.path.exists(self.options["filename"]):
            try:
                config_file = open(self.options["filename"], "rb")
            except:
                bareosfd.DebugMessage(
                    context,
                    100,
                    "Could not open file %s\n" % (self.options["filename"]),
                )
                return bRCs["bRC_Error"]
        else:
            bareosfd.DebugMessage(
                context, 100, "File %s does not exist\n" % (self.options["filename"])
            )
            return bRCs["bRC_Error"]
        # Check, if we have allow or deny regular expressions defined
        if "allow" in self.options:
            self.allow = re.compile(self.options["allow"])
        if "deny" in self.options:
            self.deny = re.compile(self.options["deny"])

        for listItem in config_file.read().splitlines():
            if os.path.isfile(listItem) and self.filename_is_allowed(
                context, listItem, self.allow, self.deny
            ):
                self.files_to_backup.append(listItem)
            if os.path.isdir(listItem):
                fullDirName = listItem
                # FD requires / at the end of a directory name
                if not fullDirName.endswith("/"):
                    fullDirName += "/"
                self.files_to_backup.append(fullDirName)
                for topdir, dirNames, fileNames in os.walk(listItem):
                    for fileName in fileNames:
                        if self.filename_is_allowed(
                            context,
                            os.path.join(topdir, fileName),
                            self.allow,
                            self.deny,
                        ):
                            self.files_to_backup.append(os.path.join(topdir, fileName))
                    for dirName in dirNames:
                        fullDirName = os.path.join(topdir, dirName) + "/"
                        self.files_to_backup.append(fullDirName)
        bareosfd.DebugMessage(
            context, 150, "Filelist: %s\n" % (self.files_to_backup),
        )

        if not self.files_to_backup:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "No (allowed) files to backup found\n",
            )
            return bRCs["bRC_Error"]
        else:
            return bRCs["bRC_OK"]

    def start_backup_file(self, context, savepkt):
        """
        Defines the file to backup and creates the savepkt. In this example
        only files (no directories) are allowed
        """
        bareosfd.DebugMessage(context, 100, "start_backup_file() called\n")
        if not self.files_to_backup:
            bareosfd.DebugMessage(context, 100, "No files to backup\n")
            return bRCs["bRC_Skip"]

        file_to_backup = self.files_to_backup.pop()
        bareosfd.DebugMessage(context, 100, "file: " + file_to_backup + "\n")

        mystatp = bareosfd.StatPacket()
        try:
            statp = os.stat(file_to_backup)
        except Exception as e:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "Could net get stat-info for file %s: \"%s\"" % (file_to_backup, e.message),
            )
        # As of Bareos 19.2.7 attribute names in bareosfd.StatPacket differ from os.stat
        # In this case we have to translate names
        # For future releases consistent names are planned, allowing to assign the
        # complete stat object in one rush
        if hasattr(mystatp, "st_uid"):
            mystatp = statp
        else:
            mystatp.mode = statp.st_mode
            mystatp.ino = statp.st_ino
            mystatp.dev = statp.st_dev
            mystatp.nlink = statp.st_nlink
            mystatp.uid = statp.st_uid
            mystatp.gid = statp.st_gid
            mystatp.size = statp.st_size
            mystatp.atime = statp.st_atime
            mystatp.mtime = statp.st_mtime
            mystatp.ctime = statp.st_ctime
        savepkt.fname = file_to_backup
        # os.islink will detect links to directories only when
        # there is no trailing slash - we need to perform checks
        # on the stripped name but use it with trailing / for the backup itself
        if os.path.islink(file_to_backup.rstrip("/")):
            savepkt.type = bFileType["FT_LNK"]
            savepkt.link = os.readlink(file_to_backup.rstrip("/"))
            bareosfd.DebugMessage(context, 150, "file type is: FT_LNK\n")
        elif os.path.isfile(file_to_backup):
            savepkt.type = bFileType["FT_REG"]
            bareosfd.DebugMessage(context, 150, "file type is: FT_REG\n")
        elif os.path.isdir(file_to_backup):
            savepkt.type = bFileType["FT_DIREND"]
            savepkt.link = file_to_backup
            bareosfd.DebugMessage(
                context, 150, "file %s type is: FT_DIREND\n" % file_to_backup
            )
        else:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_WARNING"],
                "File %s of unknown type" % (file_to_backup),
            )
            return bRCs["bRC_Skip"]

        savepkt.statp = mystatp
        bareosfd.DebugMessage(context, 150, "file statpx " + str(savepkt.statp) + "\n")

        return bRCs["bRC_OK"]

    def set_file_attributes(self, context, restorepkt):
        # Python attribute setting does not work properly with links
        if restorepkt.type == bFileType["FT_LNK"]:
            return bRCs["bRC_OK"]
        file_name = restorepkt.ofname
        file_attr = restorepkt.statp
        bareosfd.DebugMessage(
            context,
            150,
            "Set file attributes " + file_name + " with stat " + str(file_attr) + "\n",
        )
        try:
            os.chown(file_name, file_attr.uid, file_attr.gid)
            os.chmod(file_name, file_attr.mode)
            os.utime(file_name, (file_attr.atime, file_attr.mtime))
        except Exception as e:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_WARNING"],
                "Could net set attributes for file %s: \"%s\"" % (file_to_backup, e.message),
            )

        return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        """
        Here we return 'bRC_More' as long as our list files_to_backup is not
        empty and bRC_OK when we are done
        """
        bareosfd.DebugMessage(
            context, 100, "end_backup_file() entry point in Python called\n"
        )
        if self.files_to_backup:
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
