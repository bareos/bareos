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
import BareosFdPluginBaseclass
import stat


class BareosFdPluginLocalFileset(
    BareosFdPluginBaseclass.BareosFdPluginBaseclass
):  # noqa
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
        if mandatory_options is None:
            mandatory_options = ["filename"]
        # Last argument of super constructor is a list of mandatory arguments
        super(BareosFdPluginLocalFileset, self).__init__(
            plugindef, mandatory_options
        )
        self.files_to_backup = []
        self.file_to_backup = ""
        # We need to get the stat-packet in set_file_attributes
        # and use it again in end_restore_file, and this may be mixed up
        # with different files
        self.statp = {}
        self.allow = None
        self.deny = None

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
            bareosfd.DebugMessage(
                100, "File %s denied by configuration\n" % (filename)
            )
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "File %s denied by configuration\n" % (filename),
            )
            return False
        else:
            return True

    def append_file_to_backup(self, filename):
        """
        Basically add filename to list of files to backup in
        files_to_backup
        Overload this, if you want to check for extra stuff
        of do other things with files to backup
        """
        self.files_to_backup.append(filename)

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
                            self.append_file_to_backup(
                                os.path.join(topdir, fileName)
                            )
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

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt. In this example
        only files (no directories) are allowed
        """
        bareosfd.DebugMessage(100, "start_backup_file() called\n")
        if not self.files_to_backup:
            bareosfd.DebugMessage(100, "No files to backup\n")
            return bareosfd.bRC_Skip

        self.file_to_backup = self.files_to_backup.pop()
        bareosfd.DebugMessage(100, "file: " + self.file_to_backup + "\n")

        mystatp = bareosfd.StatPacket()
        try:
            if os.path.islink(self.file_to_backup):
                statp = os.lstat(self.file_to_backup)
            else:
                statp = os.stat(self.file_to_backup)
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                'Could net get stat-info for file %s: "%s"'
                % (self.file_to_backup, e),
            )
        # As of Bareos 19.2.7 attribute names in bareosfd.StatPacket differ from os.stat
        # In this case we have to translate names
        # For future releases consistent names are planned, allowing to assign the
        # complete stat object in one rush
        # if hasattr(mystatp, "st_uid"):
        #     mystatp = statp
        # else:

        mystatp.st_mode = statp.st_mode
        mystatp.st_ino = statp.st_ino
        mystatp.st_dev = statp.st_dev
        mystatp.st_nlink = statp.st_nlink
        mystatp.st_uid = statp.st_uid
        mystatp.st_gid = statp.st_gid
        mystatp.st_size = statp.st_size
        mystatp.st_atime = statp.st_atime
        mystatp.st_mtime = statp.st_mtime
        mystatp.st_ctime = statp.st_ctime
        #bareosfd.JobMessage( bareosfd.M_ERROR, '\nmystatp: %s\nstatp: %s\n' % (mystatp,statp))

        savepkt.fname = self.file_to_backup
        # os.islink will detect links to directories only when
        # there is no trailing slash - we need to perform checks
        # on the stripped name but use it with trailing / for the backup itself
        if os.path.islink(self.file_to_backup.rstrip("/")):
            savepkt.type = bareosfd.FT_LNK
            savepkt.link = os.readlink(
                self.file_to_backup.rstrip("/")
            )
            bareosfd.DebugMessage(150, "file type is: FT_LNK\n")
        elif os.path.isfile(self.file_to_backup):
            savepkt.type = bareosfd.FT_REG
            bareosfd.DebugMessage(150, "file type is: FT_REG\n")
        elif os.path.isdir(self.file_to_backup):
            savepkt.type = bareosfd.FT_DIREND
            savepkt.link = self.file_to_backup
            bareosfd.DebugMessage(
                150, "file %s type is: FT_DIREND\n" % self.file_to_backup
            )
        elif stat.S_ISFIFO(os.stat(self.file_to_backup).st_mode):
            savepkt.type = bareosfd.FT_FIFO
            bareosfd.DebugMessage(150, "file type is: FT_FIFO\n")
        else:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                "File %s of unknown type" % (self.file_to_backup),
            )
            return bareosfd.bRC_Skip

        savepkt.statp = mystatp
        bareosfd.DebugMessage(150, "file statpx " + str(savepkt.statp) + "\n")

        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """
        Creates the file to be restored and directory structure, if needed.
        Adapt this in your derived class, if you need modifications for
        virtual files or similar
        """
        bareosfd.DebugMessage(
            100,
            "create_file() entry point in Python called with %s\n" % (restorepkt),
        )
        FNAME = restorepkt.ofname
        if not FNAME:
            return bareosfd.bRC_Error
        dirname = os.path.dirname(FNAME.rstrip("/"))
        if not os.path.exists(dirname):
            bareosfd.DebugMessage(
                200, "Directory %s does not exist, creating it now\n" % dirname
            )
            os.makedirs(dirname)
        # open creates the file, if not yet existing, we close it again right
        # aways it will be opened again in plugin_io.
        if restorepkt.type == bareosfd.FT_REG:
            open(FNAME, "wb").close()
            restorepkt.create_status = bareosfd.CF_EXTRACT
        elif restorepkt.type == bareosfd.FT_LNK:
            linkNameEnc = restorepkt.olname
            linkNameClear = linkNameEnc
            if not os.path.islink(FNAME.rstrip("/")):
                # if not os.path.exists(linkNameClear):
                os.symlink(linkNameClear, FNAME.rstrip("/"))
            restorepkt.create_status = bareosfd.CF_CREATED
        elif restorepkt.type == bareosfd.FT_LNKSAVED:
            linkNameEnc = restorepkt.olname
            linkNameClear = linkNameEnc
            if not os.path.exists(linkNameClear):
                os.link(linkNameClear, FNAME.rstrip("/"))
            restorepkt.create_status = bareosfd.CF_CREATED
        elif restorepkt.type == bareosfd.FT_DIREND:
            if not os.path.exists(FNAME):
                os.makedirs(FNAME)
            restorepkt.create_status = bareosfd.CF_CREATED
        elif restorepkt.type == bareosfd.FT_FIFO:
            if not os.path.exists(FNAME):
                try:
                    os.mkfifo(FNAME, 0o600)
                except Exception as e:
                    bareosfd.JobMessage(
                        bareosfd.M_ERROR,
                        'Could net create fifo %s: "%s"' % (FNAME, e),
                    )
            restorepkt.create_status = bareosfd.CF_CREATED
        else:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "Unknown type %s of file %s" % (restorepkt.type, FNAME),
            )
        return bareosfd.bRC_OK

    def set_file_attributes(self, restorepkt):
        """
        Need to verify: we set attributes here but on plugin_io close
        the mtime will be modified again.
        approach: save stat-packet here and set it on
        end_restore_file
        """
        # restorepkt.create_status = bareosfd.CF_CORE
        # return bareosfd.bRC_OK

        # Python attribute setting does not work properly with links
        #        if restorepkt.type in [bareosfd.FT_LNK,bareosfd.FT_LNKSAVED]:
        #            return bareosfd.bRC_OK
        file_name = restorepkt.ofname
        file_attr = restorepkt.statp
        self.statp[file_name] = file_attr

        bareosfd.DebugMessage(
            150,
            "Set file attributes " + file_name + " with stat " + str(file_attr) + "\n",
        )
        try:
            os.chown(file_name, file_attr.st_uid, file_attr.st_gid)
            os.chmod(file_name, file_attr.st_mode)
            os.utime(file_name, (file_attr.st_atime, file_attr.st_mtime))
            newStat = os.stat(file_name)
            bareosfd.DebugMessage(
                150,
                "Verified file attributes "
                + file_name
                + " with stat "
                + str(newStat)
                + "\n",
            )

        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                'Could net set attributes for file %s: "%s"' % (file_name, e),
            )

        return bareosfd.bRC_OK

    def end_restore_file(self):
        bareosfd.DebugMessage(
            100,
            "end_restore_file() entry point in Python called FNAME: %s\n" % self.FNAME,
        )
        bareosfd.DebugMessage(
            150,
            "end_restore_file set file attributes "
            + self.FNAME
            + " with stat "
            + str(self.statp[self.FNAME])
            + "\n",
        )
        try:
            os.chown(self.FNAME, self.statp[self.FNAME].st_uid, self.statp[self.FNAME].st_gid)
            os.chmod(self.FNAME, self.statp[self.FNAME].st_mode)
            os.utime(
                self.FNAME, (self.statp[self.FNAME].st_atime, self.statp[self.FNAME].st_mtime)
            )
            # del sometimes leads to no-key errors, it seams that end_restore_file is sometimes called
            # multipl times.
            # del self.statp[self.FNAME]
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                'Could net set attributes for file %s: "%s"' % (self.FNAME, e),
            )
        return bareosfd.bRC_OK

    def end_backup_file(self):
        """
        Here we return 'bRC_More' as long as our list files_to_backup is not
        empty and bRC_OK when we are done
        """
        bareosfd.DebugMessage(
            100, "end_backup_file() entry point in Python called\n"
        )
        if self.files_to_backup:
            return bareosfd.bRC_More
        else:
            return bareosfd.bRC_OK


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
