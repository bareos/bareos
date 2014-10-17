#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Baseclass for Bareos python plugins
# Functions taken and adapted from bareos-fd.py
# (c) Bareos GmbH & Co. KG, Maik Aussendorf
# AGPL v.3

import bareosfd
from bareos_fd_consts import bVariable, bFileType, bRCs, bCFs
from bareos_fd_consts import bEventType, bIOPS
import os


class BareosFdPluginBaseclass(object):

    ''' Bareos python plugin base class '''

    def __init__(self, context, plugindef):
        bareosfd.DebugMessage(
            context, 100, "Constructor called in module %s\n" %
            (__name__))
        events = []
        events.append(bEventType['bEventJobEnd'])
        events.append(bEventType['bEventEndBackupJob'])
        events.append(bEventType['bEventEndFileSet'])
        events.append(bEventType['bEventHandleBackupFile'])
        bareosfd.RegisterEvents(context, events)
        # get some static Bareos values
        self.fdname = bareosfd.GetValue(context, bVariable['bVarFDName'])
        self.jobId = bareosfd.GetValue(context, bVariable['bVarJobId'])
        self.client = bareosfd.GetValue(context, bVariable['bVarClient'])
        self.since = bareosfd.GetValue(context, bVariable['bVarSinceTime'])
        self.level = bareosfd.GetValue(context, bVariable['bVarLevel'])
        self.jobName = bareosfd.GetValue(context, bVariable['bVarJobName'])
        self.workingdir = bareosfd.GetValue(
            context,
            bVariable['bVarWorkingDir'])
        self.FNAME = "undef"
        bareosfd.DebugMessage(
            context, 100, "FDName = %s - BareosFdPluginBaseclass\n" %
            (self.fdname))
        bareosfd.DebugMessage(
            context, 100, "WorkingDir: %s JobId: %s\n" %
            (self.workingdir, self.jobId))

    def __str__(self):
        return "<%s:fdname=%s jobId=%s client=%s since=%d level=%c jobName=%s workingDir=%s>" % \
            (self.__class__, self.fdname, self.jobId, self.client,
             self.since, self.level, self.jobName, self.workingdir)

    def parse_plugin_definition(self, context, plugindef):
        bareosfd.DebugMessage(
            context, 100, "plugin def parser called with %s\n" %
            (plugindef))
        # Parse plugin options into a dict
        self.options = dict()
        plugin_options = plugindef.split(":")
        for current_option in plugin_options:
            key, sep, val = current_option.partition("=")
            bareosfd.DebugMessage(context, 100, "key:val = %s:%s" % (key, val))
            if val == '':
                continue
            else:
                self.options[key] = val
        # you should overload this method with your own and do option checking
        # here, return bRCs['bRC_Error'], if options are not ok
        # or better call super.parse_plugin_definition in your own class and
        # make sanity check on self.options afterwards
        return bRCs['bRC_OK']

    def plugin_io(self, context, IOP):
        bareosfd.DebugMessage(
            context, 100, "plugin_io called with function %s\n" % (IOP.func))
        bareosfd.DebugMessage(
            context, 100, "FNAME is set to %s\n" % (self.FNAME))

        if IOP.func == bIOPS['IO_OPEN']:
            self.FNAME = IOP.fname
            try:
                if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                    bareosfd.DebugMessage(
                        context, 100,
                        "Open file %s for writing with %s\n"
                        % (self.FNAME, IOP))

                    dirname = os.path.dirname(self.FNAME)
                    if not os.path.exists(dirname):
                        bareosfd.DebugMessage(
                            context, 100,
                            "Directory %s does not exist, creating it now\n"
                            % (dirname))
                        os.makedirs(dirname)
                    self.file = open(self.FNAME, 'wb')
                else:
                    bareosfd.DebugMessage(
                        context, 100,
                        "Open file %s for reading with %s\n"
                        % (self.FNAME, IOP))
                    self.file = open(self.FNAME, 'rb')
            except:
                IOP.status = -1
                return bRCs['bRC_Error']

            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_CLOSE']:
            bareosfd.DebugMessage(context, 100, "Closing file " + "\n")
            self.file.close()
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_SEEK']:
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_READ']:
            bareosfd.DebugMessage(
                context, 200, "Reading %d from file %s\n" %
                (IOP.count, self.FNAME))
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.file.readinto(IOP.buf)
            IOP.io_errno = 0
            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_WRITE']:
            bareosfd.DebugMessage(
                context, 200, "Writing buffer to file %s\n" % (self.FNAME))
            self.file.write(IOP.buf)
            IOP.status = IOP.count
            IOP.io_errno = 0
            return bRCs['bRC_OK']

    def handle_plugin_event(self, context, event):
        if event == bEventType['bEventJobEnd']:
            bareosfd.DebugMessage(
                context, 100, "handle_plugin_event called with bEventJobEnd\n")

        elif event == bEventType['bEventEndBackupJob']:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event called with bEventEndBackupJob\n")

        elif event == bEventType['bEventEndFileSet']:
            bareosfd.DebugMessage(
                context,
                100,
                "handle_plugin_event called with bEventEndFileSet\n")

        else:
            bareosfd.DebugMessage(
                context, 100,
                "handle_plugin_event called with event %s\n" % (event))

        return bRCs['bRC_OK']

    def start_backup_file(self, context, savepkt):
        '''
        Base method, we do not add anything, overload this method with your
        implementation to add files to backup fileset
        '''
        bareosfd.DebugMessage(context, 100, "start_backup called\n")
        return bRCs['bRC_Skip']

    def end_backup_file(self, context):
        bareosfd.DebugMessage(
            context,
            100,
            "end_backup_file() entry point in Python called\n")
        return bRCs['bRC_OK']

    def start_restore_file(self, context, cmd):
        bareosfd.DebugMessage(
            context, 100,
            "start_restore_file() entry point in Python called with %s\n"
            % (cmd))
        return bRCs['bRC_OK']

    def end_restore_file(self, context):
        bareosfd.DebugMessage(
            context,
            100,
            "end_restore_file() entry point in Python called\n")
        return bRCs['bRC_OK']

    def restore_object_data(self, context, ROP):
        bareosfd.DebugMessage(
            context,
            100,
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
            "create_file() entry point in Python called with %s\n"
            % (restorepkt))
        FNAME = restorepkt.ofname
        dirname = os.path.dirname(FNAME)
        if not os.path.exists(dirname):
            bareosfd.DebugMessage(
                context,
                200,
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

    def check_file(self, context, fname):
        bareosfd.DebugMessage(
            context, 100,
            "check_file() entry point in Python called with %s\n" % (fname))
        return bRCs['bRC_OK']

    def handle_backup_file(self, context, savepkt):
        bareosfd.DebugMessage(
            context, 100, "handle_backup_file called with %s\n" % (savepkt))
        return bRCs['bRC_OK']

# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
