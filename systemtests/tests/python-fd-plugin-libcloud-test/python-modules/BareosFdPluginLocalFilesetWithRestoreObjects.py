#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2019 Bareos GmbH & Co. KG
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
import hashlib
import time
import BareosFdPluginBaseclass


class BareosFdPluginLocalFilesetWithRestoreObjects(
    BareosFdPluginBaseclass.BareosFdPluginBaseclass
):
    """
    This Bareos-FD-Plugin-Class was created for automated test purposes only.
    It is based on the BareosFdPluginLocalFileset class that parses a file
    and backups all files listed there.
    Filename is taken from plugin argument 'filename'.
    In addition to the file backup and restore, this plugin also tests
    restore objects of different sizes. As restore objects are compressed
    automatically, when they are greater then 1000 bytes, both uncompressed
    and compressed restore objects are tested.
    """

    def __init__(self, context, plugindef):
        bareosfd.DebugMessage(
            context,
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        # Last argument of super constructor is a list of mandatory arguments
        super(BareosFdPluginLocalFilesetWithRestoreObjects, self).__init__(
            context, plugindef, ["filename"]
        )
        self.files_to_backup = []
        self.allow = None
        self.deny = None
        self.object_index_seq = int((time.time() - 1546297200) * 10)
        self.sha256sums_by_filename = {}

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
            "Using %s to search for local files\n" % (self.options["filename"]),
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
                for topdir, dirNames, fileNames in os.walk(listItem):
                    for fileName in fileNames:
                        if self.filename_is_allowed(
                            context,
                            os.path.join(topdir, fileName),
                            self.allow,
                            self.deny,
                        ):
                            self.files_to_backup.append(os.path.join(topdir, fileName))
                            if os.path.isfile(os.path.join(topdir, fileName)):
                                self.files_to_backup.append(
                                    os.path.join(topdir, fileName) + ".sha256sum"
                                )
                                self.files_to_backup.append(
                                    os.path.join(topdir, fileName) + ".abspath"
                                )
            else:
                if os.path.isfile(listItem):
                    self.files_to_backup.append(listItem + ".sha256sum")
                    self.files_to_backup.append(listItem + ".abspath")

        for longrestoreobject_length in range(998, 1004):
            self.files_to_backup.append(
                "%s.longrestoreobject" % longrestoreobject_length
            )

        if not self.files_to_backup:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "No (allowed) files to backup found\n",
            )
            return bRCs["bRC_Error"]
        else:
            return bRCs["bRC_Cancel"]

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

        statp = bareosfd.StatPacket()
        savepkt.statp = statp

        if file_to_backup.endswith(".sha256sum"):
            checksum = self.get_sha256sum(context, os.path.splitext(file_to_backup)[0])
            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.fname = file_to_backup
            savepkt.object_name = file_to_backup
            savepkt.object = bytearray(checksum)
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = self.object_index_seq
            self.object_index_seq += 1

        elif file_to_backup.endswith(".abspath"):
            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.fname = file_to_backup
            savepkt.object_name = file_to_backup
            savepkt.object = bytearray(os.path.splitext(file_to_backup)[0])
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = self.object_index_seq
            self.object_index_seq += 1

        elif file_to_backup.endswith(".longrestoreobject"):
            teststring_length = int(os.path.splitext(file_to_backup)[0])
            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.fname = file_to_backup
            savepkt.object_name = file_to_backup
            savepkt.object = bytearray("a" * teststring_length)
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = self.object_index_seq
            self.object_index_seq += 1

        else:
            savepkt.fname = file_to_backup
            savepkt.type = bFileType["FT_REG"]

        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Starting backup of %s\n" % (file_to_backup),
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

    def set_file_attributes(self, context, restorepkt):
        bareosfd.DebugMessage(
            context,
            100,
            "set_file_attributes() entry point in Python called with %s\n"
            % (str(restorepkt)),
        )

        orig_fname = "/" + os.path.relpath(restorepkt.ofname, restorepkt.where)
        restoreobject_sha256sum = self.sha256sums_by_filename[orig_fname]

        file_sha256sum = self.get_sha256sum(context, orig_fname)
        bareosfd.DebugMessage(
            context,
            100,
            "set_file_attributes() orig_fname: %s restoreobject_sha256sum: %s file_sha256sum: %s\n"
            % (orig_fname, repr(restoreobject_sha256sum), repr(file_sha256sum)),
        )
        if file_sha256sum != restoreobject_sha256sum:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_ERROR"],
                "bad restoreobject orig_fname: %s restoreobject_sha256sum: %s file_sha256sum: %s\n"
                % (orig_fname, repr(restoreobject_sha256sum), repr(file_sha256sum)),
            )

        return bRCs["bRC_OK"]

    def end_restore_file(self, context):
        bareosfd.DebugMessage(
            context, 100, "end_restore_file() self.FNAME: %s\n" % self.FNAME
        )
        return bRCs["bRC_OK"]

    def restore_object_data(self, context, ROP):
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
            context,
            100,
            "BareosFdPluginLocalFilesetWithRestoreObjects:restore_object_data() called with ROP:%s\n"
            % (ROP),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.object_name(%s): %s\n" % (type(ROP.object_name), ROP.object_name),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.plugin_name(%s): %s\n" % (type(ROP.plugin_name), ROP.plugin_name),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.object_len(%s): %s\n" % (type(ROP.object_len), ROP.object_len),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.object_full_len(%s): %s\n"
            % (type(ROP.object_full_len), ROP.object_full_len),
        )
        bareosfd.DebugMessage(
            context, 100, "ROP.object(%s): %s\n" % (type(ROP.object), repr(ROP.object))
        )
        orig_filename = os.path.splitext(ROP.object_name)[0]
        if ROP.object_name.endswith(".sha256sum"):
            self.sha256sums_by_filename[orig_filename] = str(ROP.object)
        elif ROP.object_name.endswith(".abspath"):
            if str(ROP.object) != orig_filename:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_ERROR"],
                    "bad restoreobject orig_fname: %s restoreobject_fname: %s\n"
                    % (orig_filename, repr(str(ROP.object))),
                )
        elif ROP.object_name.endswith(".longrestoreobject"):
            stored_length = int(os.path.splitext(ROP.object_name)[0])
            if str(ROP.object) != "a" * stored_length:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_ERROR"],
                    "bad long restoreobject %s does not match stored object\n"
                    % (ROP.object_name),
                )
        else:
            bareosfd.DebugMessage(
                context,
                100,
                "not checking restoreobject: %s\n" % (type(ROP.object_name)),
            )
        return bRCs["bRC_OK"]

    def get_sha256sum(self, context, filename):
        f = open(filename, "rb")
        m = hashlib.sha256()
        while True:
            d = f.read(8096)
            if not d:
                break
            m.update(d)
        f.close()
        return m.hexdigest()


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
