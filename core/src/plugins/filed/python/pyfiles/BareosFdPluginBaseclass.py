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
# Baseclass for Bareos python plugins
# Functions taken and adapted from bareos-fd.py

import bareosfd
from bareosfd import *
import os
import stat
import time


class BareosFdPluginBaseclass(object):

    """ Bareos python plugin base class """

    def __init__(self, plugindef, mandatory_options=None):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        events = []
        events.append(bEventJobEnd)
        events.append(bEventEndBackupJob)
        events.append(bEventEndFileSet)
        events.append(bEventHandleBackupFile)
        events.append(bEventStartBackupJob)
        events.append(bEventStartRestoreJob)
        bareosfd.RegisterEvents(events)
        # get some static Bareos values
        self.fdname = bareosfd.GetValue(bVarFDName)
        self.jobId = bareosfd.GetValue(bVarJobId)
        self.client = bareosfd.GetValue(bVarClient)
        self.since = bareosfd.GetValue(bVarSinceTime)
        self.level = bareosfd.GetValue(bVarLevel)
        self.jobName = bareosfd.GetValue(bVarJobName)
        # jobName is of format myName.2020-05-12_11.35.27_05
        # short Name is everything left of the third point seen from the right
        self.shortName = self.jobName.rsplit(".", 3)[0]
        self.workingdir = bareosfd.GetValue(bVarWorkingDir)
        self.startTime = int(time.time())
        self.FNAME = "undef"
        self.filetype = "undef"
        self.file = None
        bareosfd.DebugMessage(
            100, "FDName = %s - BareosFdPluginBaseclass\n" % (self.fdname)
        )
        bareosfd.DebugMessage(
            100, "WorkingDir: %s JobId: %s\n" % (self.workingdir, self.jobId)
        )
        self.mandatory_options = mandatory_options

    def __str__(self):
        return "<%s:fdname=%s jobId=%s client=%s since=%d level=%c jobName=%s workingDir=%s>" % (
            self.__class__,
            self.fdname,
            self.jobId,
            self.client,
            self.since,
            self.level,
            self.jobName,
            self.workingdir,
        )

    def parse_plugin_definition(self, plugindef):
        bareosfd.DebugMessage(100, 'plugin def parser called with "%s"\n' % (plugindef))
        # Parse plugin options into a dict
        if not hasattr(self, "options"):
            self.options = dict()
        plugin_options = plugindef.split(":")
        while plugin_options:
            current_option = plugin_options.pop(0)
            key, sep, val = current_option.partition("=")
            # See if the last character is a escape of the value string
            while val[-1:] == "\\":
                val = val[:-1] + ":" + plugin_options.pop(0)
            # Replace all '\\'
            val = val.replace("\\", "")
            bareosfd.DebugMessage(100, 'key:val = "%s:%s"\n' % (key, val))
            if val == "":
                continue
            else:
                if key not in self.options:
                    self.options[key] = val
        # after we've parsed all arguments, we check the options for mandatory settings
        return self.check_options(self.mandatory_options)

    def check_options(self, mandatory_options=None):
        """
        Check Plugin options
        Here we just verify that eventual mandatory options are set.
        If you have more to veriy, just overwrite ths method in your class
        """
        if mandatory_options is None:
            return bRC_OK
        for option in mandatory_options:
            if option not in self.options:
                bareosfd.DebugMessage(
                    100, "Mandatory option '%s' not defined.\n" % option
                )
                bareosfd.JobMessage(
                    M_FATAL,
                    "Mandatory option '%s' not defined.\n" % (option),
                )
                return bRC_Error
            bareosfd.DebugMessage(
                100, "Using Option %s=%s\n" % (option, self.options[option])
            )
        return bRC_OK

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
            return bRC_OK
        elif os.path.islink(self.FNAME):
            self.fileType = "FT_LNK"
            bareosfd.DebugMessage(
                100,
                "Did not open file %s of type %s\n" % (self.FNAME, self.fileType),
            )
            return bRC_OK
        elif os.path.exists(self.FNAME) and stat.S_ISFIFO(os.stat(self.FNAME).st_mode):
            self.fileType = "FT_FIFO"
            bareosfd.DebugMessage(
                100,
                "Did not open file %s of type %s\n" % (self.FNAME, self.fileType),
            )
            return bRC_OK
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
        except:
            IOP.status = -1
            return bRC_Error
        return bRC_OK

    def plugin_io_close(self, IOP):
        bareosfd.DebugMessage(100, "Closing file " + "\n")
        if self.fileType == "FT_REG":
            self.file.close()
        return bRC_OK

    def plugin_io_seek(self, IOP):
        return bRC_OK

    def plugin_io_read(self, IOP):
        if self.fileType == "FT_REG":
            bareosfd.DebugMessage(
                200, "Reading %d from file %s\n" % (IOP.count, self.FNAME)
            )
            IOP.buf = bytearray(IOP.count)
            try:
                IOP.status = self.file.readinto(IOP.buf)
                IOP.io_errno = 0
            except Exception as e:
                bareosfd.JobMessage(
                    M_ERROR,
                    'Could net read %d bytes from file %s. "%s"'
                    % (IOP.count, file_to_backup, e.message),
                )
                IOP.io_errno = e.errno
                return bRC_Error
        else:
            bareosfd.DebugMessage(
                100,
                "Did not read from file %s of type %s\n" % (self.FNAME, self.fileType),
            )
            IOP.buf = bytearray()
            IOP.status = 0
            IOP.io_errno = 0
        return bRC_OK

    def plugin_io_write(self, IOP):
        bareosfd.DebugMessage(200, "Writing buffer to file %s\n" % (self.FNAME))
        try:
            self.file.write(IOP.buf)
        except Exception as e:
            bareosfd.JobMessage(
                M_ERROR,
                'Could net write to file %s. "%s"' % (file_to_backup, e.message),
            )
            IOP.io_errno = e.errno
            IOP.status = 0
            return bRC_Error
        IOP.status = IOP.count
        IOP.io_errno = 0
        return bRC_OK

    def plugin_io(self, IOP):
        """
        Basic IO operations. Some tweaks here: IOP.fname is only set on file-open
        We need to capture it on open and keep it for the remaining procedures
        Now (since 2020) separated into sub-methods to ease overloading in derived classes
        """
        bareosfd.DebugMessage(
            250,
            "plugin_io called with function %s filename %s\n" % (IOP.func, IOP.fname),
        )
        bareosfd.DebugMessage(250, "self.FNAME is set to %s\n" % (self.FNAME))
        if IOP.func == IO_OPEN:
            return self.plugin_io_open(IOP)
        elif IOP.func == IO_CLOSE:
            return self.plugin_io_close(IOP)
        elif IOP.func == IO_SEEK:
            return self.plugin_io_seek(IOP)
        elif IOP.func == IO_READ:
            return self.plugin_io_read(IOP)
        elif IOP.func == IO_WRITE:
            return self.plugin_io_write(IOP)

    def handle_plugin_event(self, event):
        if event == bEventJobEnd:
            bareosfd.DebugMessage(100, "handle_plugin_event called with bEventJobEnd\n")
            return self.end_job()

        elif event == bEventEndBackupJob:
            bareosfd.DebugMessage(
                100, "handle_plugin_event called with bEventEndBackupJob\n"
            )
            return self.end_backup_job()

        elif event == bEventEndFileSet:
            bareosfd.DebugMessage(
                100, "handle_plugin_event called with bEventEndFileSet\n"
            )
            return self.end_fileset()

        elif event == bEventStartBackupJob:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with bEventStartBackupJob\n"
            )
            return self.start_backup_job()

        elif event == bEventStartRestoreJob:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event() called with bEventStartRestoreJob\n",
            )
            return self.start_restore_job()

        else:
            bareosfd.DebugMessage(
                100, "handle_plugin_event called with event %s\n" % (event)
            )
        return bRC_OK

    def start_backup_job(self):
        """
        Start of Backup Job. Called just before backup job really start.
        Overload this to arrange whatever you have to do at this time.
        """
        return bRC_OK

    def end_job(self):
        """
        Called if job ends regularyly (not for cancelled jobs)
        Overload this to arrange whatever you have to do at this time.
        """
        return bRC_OK

    def end_backup_job(self):
        """
        Called if backup job ends, before ClientAfterJob
        Overload this to arrange whatever you have to do at this time.
        """
        return bRC_OK

    def start_backup_file(self, savepkt):
        """
        Base method, we do not add anything, overload this method with your
        implementation to add files to backup fileset
        """
        bareosfd.DebugMessage(100, "start_backup called\n")
        return bRC_Skip

    def end_backup_file(self):
        bareosfd.DebugMessage(100, "end_backup_file() entry point in Python called\n")
        return bRC_OK

    def end_fileset(self):
        bareosfd.DebugMessage(100, "end_fileset() entry point in Python called\n")
        return bRC_OK

    def start_restore_job(self):
        """
        Start of Restore Job. Called , if you have Restore objects.
        Overload this to handle restore objects, if applicable
        """
        return bRC_OK

    def start_restore_file(self, cmd):
        bareosfd.DebugMessage(
            100,
            "start_restore_file() entry point in Python called with %s\n" % (cmd),
        )
        return bRC_OK

    def end_restore_file(self):
        bareosfd.DebugMessage(100, "end_restore_file() entry point in Python called\n")
        return bRC_OK

    def restore_object_data(self, ROP):
        bareosfd.DebugMessage(100, "restore_object_data called with " + str(ROP) + "\n")
        return bRC_OK

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
        # We leave file creation up to the core for the default case
        restorepkt.create_status = CF_CORE
        return bRC_OK

    def set_file_attributes(self, restorepkt):
        bareosfd.DebugMessage(
            100,
            "set_file_attributes() entry point in Python called with %s\n"
            % (str(restorepkt)),
        )
        return bRC_OK

    def check_file(self, fname):
        bareosfd.DebugMessage(
            100,
            "check_file() entry point in Python called with %s\n" % (fname),
        )
        return bRC_OK

    def get_acl(self, acl):
        bareosfd.DebugMessage(
            100, "get_acl() entry point in Python called with %s\n" % (acl)
        )
        return bRC_OK

    def set_acl(self, acl):
        bareosfd.DebugMessage(
            100, "set_acl() entry point in Python called with %s\n" % (acl)
        )
        return bRC_OK

    def get_xattr(self, xattr):
        bareosfd.DebugMessage(
            100, "get_xattr() entry point in Python called with %s\n" % (xattr)
        )
        return bRC_OK

    def set_xattr(self, xattr):
        bareosfd.DebugMessage(
            100, "set_xattr() entry point in Python called with %s\n" % (xattr)
        )
        return bRC_OK

    def handle_backup_file(self, savepkt):
        bareosfd.DebugMessage(
            100,
            "handle_backup_file() entry point in Python called with %s\n" % (savepkt),
        )
        return bRC_OK


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
