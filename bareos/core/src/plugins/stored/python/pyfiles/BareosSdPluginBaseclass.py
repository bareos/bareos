#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2021 Bareos GmbH & Co. KG
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
# Functions taken and adapted from bareos-sd.py

import bareossd
import time


class BareosSdPluginBaseclass(object):

    """ Bareos storage python plugin base class """

    def __init__(self, plugindef):
        bareossd.DebugMessage(100, "Constructor called in module %s\n" % (__name__))
        events = []

        events.append(bareossd.bsdEventJobStart)
        events.append(bareossd.bsdEventJobEnd)
        bareossd.RegisterEvents(events)

        # get some static Bareos values
        self.jobName = bareossd.GetValue(bareossd.bsdVarJobName)
        self.jobLevel = chr(bareossd.GetValue(bareossd.bsdVarLevel))
        self.jobId = int(bareossd.GetValue(bareossd.bsdVarJobId))

        bareossd.DebugMessage(
            100,
            "JobName = %s - Level = %s - Id = %s - BareosSdPluginBaseclass\n"
            % (self.jobName, self.jobLevel, self.jobId),
        )

    def __str__(self):
        return "<$%s:jobName=%s jobId=%s Level=%s>" % (
            self.__class__,
            self.jobName,
            self.jobId,
            self.jobLevel,
        )

    def parse_plugin_definition(self, plugindef):
        """
        Called with the plugin options from the bareos configfiles
        You should overload this method with your own and do option checking
        here, return bRCs['bRC_Error'], if options are not ok
        or better call super.parse_plugin_definition in your own class and
        make sanity check on self.options afterwards
        """
        bareossd.DebugMessage(100, "plugin def parser called with %s\n" % (plugindef))
        # Parse plugin options into a dict
        self.options = dict()
        plugin_options = plugindef.split(":")
        for current_option in plugin_options:
            key, sep, val = current_option.partition("=")
            bareossd.DebugMessage(100, "key:val = %s:%s" % (key, val))
            if val == "":
                continue
            else:
                self.options[key] = val
        return bareossd.bRC_OK

    def handle_plugin_event(self, event):
        """
        This method is called for each of the above registered events
        Overload this method to implement your actions for the events,
        You may first call this method in your derived class to get the
        job attributes read and then only adjust where useful.
        """
        if event == bareossd.bsdEventJobStart:
            self.jobStartTime = time.time()
            bareossd.DebugMessage(
                100,
                "bsdEventJobStart event triggered at Unix time %s\n"
                % (self.jobStartTime),
            )

        elif event == bareossd.bsdEventJobEnd:
            self.jobEndTime = time.time()
            bareossd.DebugMessage(
                100,
                "bsdEventJobEnd event triggered at Unix time %s\n" % (self.jobEndTime),
            )
            self.jobBytes = int(bareossd.GetValue(bareossd.bsdVarJobBytes))
            self.jobFiles = int(bareossd.GetValue(bareossd.bsdVarJobFiles))
            self.jobRunningTime = self.jobEndTime - self.jobStartTime
            self.throughput = 0
            if self.jobRunningTime > 0:
                self.throughput = self.jobBytes / self.jobRunningTime
                bareossd.DebugMessage(
                    100,
                    "jobRunningTime: %s s, Throughput: %s Bytes/s\n"
                    % (self.jobRunningTime, self.throughput),
                )

        return bareossd.bRC_OK


# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
