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
# Author: Kozlov Alexander
#
# GraphiteSender for Bareos python plugins
# Functions taken and adapted from bareos-dir.py

from bareosdir import *
from bareos_dir_consts import *
import BareosDirPluginBaseclass
from socket import socket, AF_INET, SOCK_STREAM
import time


class BareosDirPluginGraphiteSender(BareosDirPluginBaseclass.BareosDirPluginBaseclass):
    ''' Bareos DIR python plugin Nagios / Icinga  sender class '''

    def parse_plugin_definition(self, context, plugindef):
        '''
        Check, if mandatory monitoringHost is set and set default for other unset parameters
        '''
        super(BareosDirPluginGraphiteSender, self).parse_plugin_definition(
            context, plugindef)
        # monitoring Host is mandatory
        if 'collectorHost' not in self.options:
            self.collectorHost = "graphite"
        else:
            self.collectorHost = self.options['collectorHost']
        if 'collectorPort' not in self.options:
            self.collectorPort = 2003
        else:
            self.collectorPort = int(self.options['collectorPort'])
        if 'metricPrefix' not in self.options:
            self.metricPrefix = 'apps'
        else:
            self.metricPrefix = self.options['metricPrefix']
        # we return OK in anyway, we do not want to produce Bareos errors just because of failing
        # Nagios notifications
        return bRCs['bRC_OK']

    def handle_plugin_event(self, context, event):
        '''
        This method is calle for every registered event
        '''

        # We first call the method from our superclass to get job attributes read
        super(BareosDirPluginGraphiteSender, self).handle_plugin_event(context, event)

        if event == bDirEventType['bDirEventJobEnd']:
            # This event is called at the end of a job, here evaluate the results
            self.evaluateJobStatus(context)
            self.transmitResult(context)

        return bRCs['bRC_OK']

    def evaluateJobStatus(self, context):
        '''
        Depending on the jobStatus we compute monitoring status and monitoring message
        You may overload this method to adjust messages
        '''
        self.metrics = {}
        job = '_'.join(self.jobName.split('.')[:-3])
        if (self.jobStatus == 'E' or self.jobStatus == 'f'):
            self.metrics['bareos.jobs.{}.status.error'.format(job)] = 1
        elif (self.jobStatus == 'W'):
            self.metrics['bareos.jobs.{}.status.warning'.format(job)] = 1
        elif (self.jobStatus == 'T'):
            self.metrics['bareos.jobs.{}.status.success'.format(job)] = 1
            self.metrics['bareos.jobs.{}.jobbytes'.format(job)] = self.jobBytes
            self.metrics['bareos.jobs.{}.jobfiles'.format(job)] = self.jobFiles
            self.metrics['bareos.jobs.{}.runningtime'.format(job)] = self.jobRunningTime
            self.metrics['bareos.jobs.{}.throughput'.format(job)] = self.throughput
        else:
            self.metrics['bareos.jobs.{}.status.other'.format(job)] = 1

        DebugMessage(context, 100, "Graphite metrics: {}\n".format(self.metrics))

    def transmitResult(self, context):
        '''
        Here we send the result to the Icinga / Nagios server using NSCA
        Overload this method if you want ot submit your changes on a different way
        '''
        DebugMessage(context, 100, "Submitting metrics to {}:{}".format(self.collectorHost,
                                                                        self.collectorPort))
        try:
            sock = socket(AF_INET, SOCK_STREAM)
            sock.connect((self.collectorHost, self.collectorPort))
            for key, value in self.metrics.items():
                sock.send('{prefix}.{key} {value} {time}\n'.format(prefix=self.metricPrefix,
                                                                   key=key,
                                                                   value=value,
                                                                   time=int(time.time())))
            sock.close()
        except Exception as e:
            JobMessage(context, bJobMessageType['M_WARNING'],
                       "Plugin {} could not transmit check result to {}:{}: {}\n".format(self.__class__,
                                                                                         self.collectorHost,
                                                                                         self.collectorPort,
                                                                                         e.message))

# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
