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
# Baseclass for Bareos python plugins
# Functions taken and adapted from bareos-fd.py

import bareosfd
from bareos_fd_consts import bVariable, bFileType, bRCs, bCFs
from bareos_fd_consts import bEventType, bIOPS, bJobMessageType
import os


class BareosFdPluginBaseclass(object):

    ''' Bareos python plugin base class '''

    def __init__(self, context, plugindef, mandatory_options=None):
        bareosfd.DebugMessage(
            context, 100,
            "Constructor called in module %s with plugindef=%s\n" %
            (__name__, plugindef))
        events = []
        events.append(bEventType['bEventJobEnd'])
        events.append(bEventType['bEventEndBackupJob'])
        events.append(bEventType['bEventEndFileSet'])
        events.append(bEventType['bEventHandleBackupFile'])
        events.append(bEventType['bEventStartBackupJob'])
        events.append(bEventType['bEventStartRestoreJob'])
        bareosfd.RegisterEvents(context, events)
        # get some static Bareos values
        self.fdname = bareosfd.GetValue(context, bVariable['bVarFDName'])
        self.jobId = bareosfd.GetValue(context, bVariable['bVarJobId'])
        self.client = bareosfd.GetValue(context, bVariable['bVarClient'])
        self.since = bareosfd.GetValue(context, bVariable['bVarSinceTime'])
        self.level = bareosfd.GetValue(context, bVariable['bVarLevel'])
        self.jobName = bareosfd.GetValue(context, bVariable['bVarJobName'])
        self.workingdir = bareosfd.GetValue(context, bVariable['bVarWorkingDir'])
        self.FNAME = "undef"
        self.file = None
        bareosfd.DebugMessage(
            context, 100,
            "FDName = %s - BareosFdPluginBaseclass\n" %
            (self.fdname))
        bareosfd.DebugMessage(
            context, 100,
            "WorkingDir: %s JobId: %s\n" %
            (self.workingdir, self.jobId))
        self.mandatory_options = mandatory_options

    def __str__(self):
        return "<%s:fdname=%s jobId=%s client=%s since=%d level=%c jobName=%s workingDir=%s>" % \
            (self.__class__, self.fdname, self.jobId, self.client,
             self.since, self.level, self.jobName, self.workingdir)

    def parse_plugin_definition(self, context, plugindef):
        bareosfd.DebugMessage(
            context, 100,
            "plugin def parser called with \"%s\"\n" %
            (plugindef))
        # Parse plugin options into a dict
        if not hasattr (self, 'options'):
            self.options = dict()
        plugin_options = plugindef.split(":")
        while plugin_options:
            current_option = plugin_options.pop(0)
            key, sep, val = current_option.partition("=")
            # See if the last character is a escape of the value string
            while val[-1:] == '\\':
                val = val[:-1] + ':' + plugin_options.pop(0)
            # Replace all '\\'
            val = val.replace("\\", "")
            bareosfd.DebugMessage(
                context, 100,
                "key:val = \"%s:%s\"\n" %
                (key, val))
            if val == '':
                continue
            else:
                if key not in self.options:
                    self.options[key] = val
        # after we've parsed all arguments, we check the options for mandatory settings
        return self.check_options(context, self.mandatory_options)

    def check_options(self, context, mandatory_options=None):
        '''
        Check Plugin options
        Here we just verify that eventual mandatory options are set.
        If you have more to veriy, just overwrite ths method in your class
        '''

        if mandatory_options is None:
            return bRCs['bRC_OK']

        for option in mandatory_options:
            if (option not in self.options):
                bareosfd.DebugMessage(
                    context, 100,
                    "Mandatory option \'%s\' not defined.\n" %
                    option)
                bareosfd.JobMessage(
                    context, bJobMessageType['M_FATAL'],
                    "Mandatory option \'%s\' not defined.\n" %
                    (option))
                return bRCs['bRC_Error']

            bareosfd.DebugMessage(
                context, 100,
                "Using Option %s=%s\n" %
                (option, self.options[option]))

        return bRCs['bRC_OK']

    def plugin_io(self, context, IOP):
        bareosfd.DebugMessage(
            context, 100,
            "plugin_io called with function %s\n" %
            (IOP.func))
        bareosfd.DebugMessage(
            context, 100,
            "FNAME is set to %s\n" %
            (self.FNAME))

        if IOP.func == bIOPS['IO_OPEN']:
            self.FNAME = IOP.fname
            try:
                if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                    bareosfd.DebugMessage(
                        context, 100,
                        "Open file %s for writing with %s\n" %
                        (self.FNAME, IOP))

                    dirname = os.path.dirname(self.FNAME)
                    if not os.path.exists(dirname):
                        bareosfd.DebugMessage(
                            context, 100,
                            "Directory %s does not exist, creating it now\n" %
                            (dirname))
                        os.makedirs(dirname)
                    self.file = open(self.FNAME, 'wb')
                else:
                    bareosfd.DebugMessage(
                        context, 100,
                        "Open file %s for reading with %s\n" %
                        (self.FNAME, IOP))
                    self.file = open(self.FNAME, 'rb')
            except:
                IOP.status = -1
                return bRCs['bRC_Error']

            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_CLOSE']:
            bareosfd.DebugMessage(
                context, 100,
                "Closing file " + "\n")
            self.file.close()
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_SEEK']:
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_READ']:
            bareosfd.DebugMessage(
                context, 200,
                "Reading %d from file %s\n" %
                (IOP.count, self.FNAME))
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.file.readinto(IOP.buf)
            IOP.io_errno = 0
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_WRITE']:
            bareosfd.DebugMessage(
                context, 200,
                "Writing buffer to file %s\n" %
                (self.FNAME))
            self.file.write(IOP.buf)
            IOP.status = IOP.count
            IOP.io_errno = 0
            return bRCs['bRC_OK']

    def handle_plugin_event(self, context, event):
        if event == bEventType['bEventJobEnd']:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event called with bEventJobEnd\n")

        elif event == bEventType['bEventEndBackupJob']:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event called with bEventEndBackupJob\n")

        elif event == bEventType['bEventEndFileSet']:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event called with bEventEndFileSet\n")

        elif event == bEventType['bEventStartBackupJob']:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event() called with bEventStartBackupJob\n")

            return self.start_backup_job(context)

        elif event == bEventType['bEventStartRestoreJob']:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event() called with bEventStartRestoreJob\n")

            return self.start_restore_job(context)

        else:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event called with event %s\n" % (event))

        return bRCs['bRC_OK']

    def start_backup_job(self, context):
        '''
        Start of Backup Job. Called just before backup job really start.
        Overload this to arrange whatever you have to do at this time.
        '''
        return bRCs['bRC_OK']

    def start_backup_file(self, context, savepkt):
        '''
        Base method, we do not add anything, overload this method with your
        implementation to add files to backup fileset
        '''
        bareosfd.DebugMessage(
            context, 100,
            "start_backup called\n")
        return bRCs['bRC_Skip']

    def end_backup_file(self, context):
        bareosfd.DebugMessage(
            context, 100,
            "end_backup_file() entry point in Python called\n")
        return bRCs['bRC_OK']

    def start_restore_job(self, context):
        '''
        Start of Restore Job. Called , if you have Restore objects.
        Overload this to handle restore objects, if applicable
        '''
        return bRCs['bRC_OK']

    def start_restore_file(self, context, cmd):
        bareosfd.DebugMessage(
            context, 100,
            "start_restore_file() entry point in Python called with %s\n" % (cmd))
        return bRCs['bRC_OK']

    def end_restore_file(self, context):
        bareosfd.DebugMessage(
            context, 100,
            "end_restore_file() entry point in Python called\n")
        return bRCs['bRC_OK']

    def restore_object_data(self, context, ROP):
        bareosfd.DebugMessage(
            context, 100,
            "restore_object_data called with " + str(ROP) + "\n")
        return bRCs['bRC_OK']

    def create_file(self, context, restorepkt):
        '''
        Creates the file to be restored and directory structure, if needed.
        Adapt this in your derived class, if you need modifications for
        virtual files or similar
        '''
        bareosfd.DebugMessage(
            context, 100,
            "create_file() entry point in Python called with %s\n" %
            (restorepkt))
        FNAME = restorepkt.ofname
        dirname = os.path.dirname(FNAME)
        if not os.path.exists(dirname):
            bareosfd.DebugMessage(
                context, 200,
                "Directory %s does not exist, creating it now\n" %
                dirname)
            os.makedirs(dirname)
        # open creates the file, if not yet existing, we close it again right
        # aways it will be opened again in plugin_io.
        # But: only do this for regular files, prevent from
        # IOError: (21, 'Is a directory', '/tmp/bareos-restores/my/dir/')
        # if it's a directory
        if restorepkt.type == bFileType['FT_REG']:
            open(FNAME, 'wb').close()
            restorepkt.create_status = bCFs['CF_EXTRACT']
        return bRCs['bRC_OK']

    def set_file_attributes(self, context, restorepkt):
        bareosfd.DebugMessage(
            context, 100,
            "set_file_attributes() entry point in Python called with %s\n" %
            (str(restorepkt)))
        return bRCs['bRC_OK']

    def check_file(self, context, fname):
        bareosfd.DebugMessage(
            context, 100,
            "check_file() entry point in Python called with %s\n" %
            (fname))
        return bRCs['bRC_OK']

    def get_acl(self, context, acl):
        bareosfd.DebugMessage(
            context, 100,
            "get_acl() entry point in Python called with %s\n" %
            (acl))
        return bRCs['bRC_OK']

    def set_acl(self, context, acl):
        bareosfd.DebugMessage(
            context, 100,
            "set_acl() entry point in Python called with %s\n" %
            (acl))
        return bRCs['bRC_OK']

    def get_xattr(self, context, xattr):
        bareosfd.DebugMessage(
            context, 100,
            "get_xattr() entry point in Python called with %s\n" %
            (xattr))
        return bRCs['bRC_OK']

    def set_xattr(self, context, xattr):
        bareosfd.DebugMessage(
            context, 100,
            "set_xattr() entry point in Python called with %s\n" %
            (xattr))
        return bRCs['bRC_OK']

    def handle_backup_file(self, context, savepkt):
        bareosfd.DebugMessage(
            context, 100,
            "handle_backup_file() entry point in Python called with %s\n" %
            (savepkt))
        return bRCs['bRC_OK']

# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
