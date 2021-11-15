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
# NscaSender for Bareos python plugins
# Functions taken and adapted from bareos-dir.py

from bareosdir import * 
from bareos_dir_consts import * 
import BareosDirPluginBaseclass
import pynsca

class BareosDirPluginNscaSender(BareosDirPluginBaseclass.BareosDirPluginBaseclass):
    ''' Bareos DIR python plugin Nagios / Icinga  sender class '''

    def parse_plugin_definition(self, context, plugindef):
        '''
        Check, if mandatory monitoringHost is set and set default for other unset parameters
        '''
        super(BareosDirPluginNscaSender, self).parse_plugin_definition(
            context, plugindef)
        # monitoring Host is mandatory
        if not 'monitorHost' in self.options:
            JobMessage(context, bJobMessageType['M_WARNING'], "Plugin %s needs parameter 'monitorHost' specified\n" %(self.__class__))
            self.monitorHost = ""
        else:
            self.monitorHost = self.options['monitorHost']
        if not 'encryption' in self.options:
            DebugMessage(context, 100, "Paraemter encryption not set for plugin %s. Setting to 1 (XOR nsca default)" %(self.__class__))
            self.encryption= 1; # XOR is nsca default 
        else:
            self.encryption = int(self.options['encryption'])
        if not 'monitorPort' in self.options:
            self.monitorPort = 5667
        else:
            self.monitorPort = int(self.options['monitorPort'])
        if not 'checkHost' in self.options:
            self.checkHost = 'bareos'
        else:
            self.checkHost = self.options['checkHost']
        if not 'checkService' in self.options:
            self.checkService = 'backup_job'
        else:
            self.checkService = self.options['checkService']
        # we return OK in anyway, we do not want to produce Bareos errors just because of failing
        # Nagios notifications
        return bRCs['bRC_OK']

    def handle_plugin_event(self, context, event):
        '''
        This method is calle for every registered event
        '''

        # We first call the method from our superclass to get job attributes read
        super(BareosDirPluginNscaSender, self).handle_plugin_event(context, event)

        if event == bDirEventType['bDirEventJobEnd']:
            # This event is called at the end of a job, here evaluate the results
            self.evaluateJobStatus (context)
            self.transmitResult (context)

        return bRCs['bRC_OK'];

       
    def evaluateJobStatus(self,context):
        '''
        Depending on the jobStatus we compute monitoring status and monitoring message
        You may overload this method to adjust messages
        '''
        self.nagiosMessage = "";
        coreMessage = "Bareos job %s on %s with id %d level %s, %d errors, %d jobBytes, %d files terminated with status %s" \
            %(self.jobName, self.jobClient, self.jobId, self.jobLevel, self.jobErrors, self.jobBytes, self.jobFiles, self.jobStatus)
        if (self.jobStatus == 'E' or self.jobStatus == 'f'):
            self.nagiosResult = 2; # critical
            self.nagiosMessage = "CRITICAL: %s" %coreMessage
        elif (self.jobStatus == 'W'):
            self.nagiosResult = 1; # warning
            self.nagiosMessage = "WARNING: %s" %coreMessage
        elif (self.jobStatus == 'A'):
            self.nagiosResult = 1; # warning
            self.nagiosMessage = "WARNING: %s CANCELED" %coreMessage
        elif (self.jobStatus == 'T'):
            self.nagiosResult = 0; # OK
            self.nagiosMessage = "OK: %s" %coreMessage 
        else:
            self.nagiosResult = 3; # unknown
            self.nagiosMessage = "UNKNOWN: %s" %coreMessage

        # Performance data according Nagios developer guide
        self.perfstring = "|Errors=%d;;;; Bytes=%d;;;; Files=%d;;;; Throughput=%dB/s;;;; jobRuntime=%ds;;;; jobTotalTime=%ds;;;;" \
            % (self.jobErrors, self.jobBytes, self.jobFiles, self.throughput, self.jobRunningTime, self.jobTotalTime)
        
        DebugMessage(context, 100, "Nagios Code: %d, NagiosMessage: %s\n" %(self.nagiosResult,self.nagiosMessage));
        DebugMessage(context, 100, "Performance data: %s\n" %self.perfstring)


    def transmitResult(self,context):
        '''
        Here we send the result to the Icinga / Nagios server using NSCA
        Overload this method if you want ot submit your changes on a different way
        '''
        DebugMessage(context, 100, "Submitting check result to host %s, encryption: %d, Port: %d\n" \
            %(self.monitorHost,self.encryption, self.monitorPort))
        try:
            notif = pynsca.NSCANotifier(self.monitorHost,self.monitorPort, self.encryption)
            notif.svc_result(self.checkHost, self.checkService, self.nagiosResult, self.nagiosMessage + self.perfstring)
        except:
            JobMessage(context, bJobMessageType['M_WARNING'], "Plugin %s could not transmit check result to host %s, port %d\n"\
                 %(self.__class__, self.monitorHost, self.monitorPort))

# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
