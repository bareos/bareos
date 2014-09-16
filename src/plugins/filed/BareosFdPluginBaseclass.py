#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Baseclass for Bareos python plugins
# Functions taken and adapted from bareos-fd.py
# (c) Bareos GmbH & Co. KG, Maik Aussendorf
# AGPL v.3

from bareosfd import *
from bareos_fd_consts import *
from os import O_WRONLY, O_CREAT
import os

class BareosFdPluginBaseclass(object):
    ''' Bareos python plugin base class '''
    def __init__(self, context, plugindef):
        DebugMessage(context, 100, "Constructor called in module " + __name__ + "\n");
        events = [];
        events.append(bEventType['bEventJobEnd']);
        events.append(bEventType['bEventEndBackupJob']);
        events.append(bEventType['bEventEndFileSet']);
        events.append(bEventType['bEventHandleBackupFile']);
        RegisterEvents(context, events);
        # get some static Bareos values
        self.fdname = GetValue(context, bVariable['bVarFDName']);
        self.jobId = GetValue(context, bVariable['bVarJobId']);
        self.client = GetValue(context, bVariable['bVarClient']);
        self.since = GetValue(context, bVariable['bVarSinceTime']);
        self.level = GetValue(context, bVariable['bVarLevel']);
        self.jobName = GetValue(context, bVariable['bVarJobName']);
        self.workingdir = GetValue(context, bVariable['bVarWorkingDir']);
        self.FNAME = "undef";
        DebugMessage(context, 100, "FDName = " + self.fdname + " - BareosFdPluginBaseclass\n");
        DebugMessage(context, 100, "WorkingDir = " + self.workingdir + " jobId: " + str(self.jobId) + "\n");

    def __str__(self):
        return "<%s:fdname=%s jobId=%s client=%s since=%d level=%c jobName=%s workingDir=%s>"%(self.__class__,self.fdname,self.jobId,self.client,self.since,self.level,self.jobName,self.workingdir)

    def parse_plugin_definition(self,context, plugindef):
        DebugMessage(context, 100, "plugin def parser called with " + plugindef + "\n");
        # Parse plugin options into a dict
        self.options = dict();
        plugin_options = plugindef.split(":");
        for current_option in plugin_options:
            key,sep,val = current_option.partition("=");
            DebugMessage(context, 100, "key:val: " + key + ':' + val + "\n");
            if val == '':
                continue;
            else:
                self.options[key] = val;
        # you should overload this method with your own and do option checking here, return bRCs['bRC_Error'], if options are not ok
        # or better call super.parse_plugin_definition in your own class and make sanity check on self.options afterwards
        return bRCs['bRC_OK'];


    def plugin_io(self, context, IOP):
        DebugMessage(context, 100, "plugin_io called with function " + str(IOP.func) + "\n");
        DebugMessage(context, 100, "FNAME is set to " + self.FNAME + "\n");

        if IOP.func == bIOPS['IO_OPEN']:
            self.FNAME = IOP.fname;
            try:
                if IOP.flags & (O_CREAT | O_WRONLY):
                    DebugMessage(context, 100, "Open file " + str(self.FNAME) + " for writing with " + str(IOP) + "\n");
                    dirname = os.path.dirname (self.FNAME);
                    if not os.path.exists(dirname):
                        DebugMessage(context, 100, "Directory %s does not exist, creating it now" %dirname);
                        os.makedirs(dirname);
                    self.file = open(self.FNAME, 'wb');
                else:
                    DebugMessage(context, 100, "Open file " + str(self.FNAME) + " for reading with " + str(IOP) + "\n");
                    self.file = open(self.FNAME, 'rb');
            except:
                IOP.status = -1;
                return bRCs['bRC_Error'];

            return bRCs['bRC_OK'];

        elif IOP.func == bIOPS['IO_CLOSE']:
            DebugMessage(context, 100, "Closing file " + "\n");
            self.file.close();
            return bRCs['bRC_OK'];

        elif IOP.func == bIOPS['IO_SEEK']:
            return bRCs['bRC_OK'];

        elif IOP.func == bIOPS['IO_READ']:
            DebugMessage(context, 200, "Reading %d from file %s" %(IOP.count,self.FNAME));
            IOP.buf = bytearray(IOP.count);
            IOP.status = self.file.readinto(IOP.buf);
            IOP.io_errno = 0
            return bRCs['bRC_OK'];

        elif IOP.func == bIOPS['IO_WRITE']:
            DebugMessage(context, 200, "Writing buffer to file " + str(self.FNAME) + "\n");
            self.file.write(IOP.buf);
            IOP.status = IOP.count;
            IOP.io_errno = 0
            return bRCs['bRC_OK'];


    def handle_plugin_event(self, context, event):
        if event == bEventType['bEventJobEnd']:
            DebugMessage(context, 100, "handle_plugin_event called with bEventJobEnd\n");

        elif event == bEventType['bEventEndBackupJob']:
            DebugMessage(context, 100, "handle_plugin_event called with bEventEndBackupJob\n");

        elif event == bEventType['bEventEndFileSet']:
            DebugMessage(context, 100, "handle_plugin_event called with bEventEndFileSet\n");

        else:
            DebugMessage(context, 100, "handle_plugin_event called with event" + str(event) + "\n");

        return bRCs['bRC_OK'];


    def start_backup_file(self,context, savepkt):
        '''
        Base method, we do not add anything, overload this method with your implementation to add files to backup fileset
        '''
        DebugMessage(context, 100, "start_backup called\n");
        return bRCs['bRC_Skip'];


    def end_backup_file(self, context):
        DebugMessage(context, 100, "end_backup_file() entry point in Python called\n")
        return bRCs['bRC_OK'];

    def start_restore_file(self, context, cmd):
        DebugMessage(context, 100, "start_restore_file() entry point in Python called with" + str(cmd) + "\n")
        return bRCs['bRC_OK'];

    def end_restore_file(self,context):
        DebugMessage(context, 100, "end_restore_file() entry point in Python called\n")
        return bRCs['bRC_OK'];

    def restore_object_data(self, context, ROP):
        DebugMessage(context, 100, "restore_object_data called with " + str(ROP) + "\n");
        return bRCs['bRC_OK'];

    def create_file(self,context, restorepkt):
        '''
        Creates the file to be restored and directory structure, if needed.
        Adapt this in your derived class, if you need modifications for virtual files or similar
        '''
        DebugMessage(context, 100, "create_file() entry point in Python called with" + str(restorepkt) + "\n")
        FNAME = restorepkt.ofname;
        dirname = os.path.dirname (FNAME);
        if not os.path.exists(dirname):
            DebugMessage(context, 200, "Directory %s does not exist, creating it now" %dirname);
            os.makedirs(dirname);
        # open creates the file, if not yet existing, we close it again right aways
        # it will be opened again in plugin_io
        open (FNAME,'wb').close();
        restorepkt.create_status = bCFs['CF_EXTRACT'];
        return bRCs['bRC_OK'];

    def check_file(self,context, fname):
        DebugMessage(context, 100, "check_file() entry point in Python called with" + str(fname) + "\n")
        return bRCs['bRC_OK'];

    def handle_backup_file(self,context, savepkt):
        DebugMessage(context, 100, "handle_backup_file called with " + str(savepkt) + "\n");
        return bRCs['bRC_OK'];

# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
