#!/usr/bin/python
# -*- coding: utf-8 -*-
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright(C) 2015-2015 Bareos GmbH & Co. KG
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
# PoC code to implement a Bareos commandline tool that allows to trigger
# common tasks from a Bareos client.
# bsmc stands for "Bareos simple management cli"
#
# Implemented are show scheduler, trigger incremental backup and restore a
# single given file.
#


import ConfigParser
import argparse
import bareos.bsock.bsockjson
import logging
import time
import sys
import datetime
import requests
import json
import hashlib
import os
import os.path
from socket import socket
from django.contrib.messages.context_processors import messages


def sentItemsToCarbon(messages, carbonHost, carbonPort):
    '''
    Send messages to carbon via socket
    '''
    field_separator = ' '
    line_separator = '\n'
    sock = socket()
    try:
        sock.connect((carbonHost, carbonPort))
    except:
        print "Could not connect to carbonHost: %s, port: %d\n" % (carbonHost, carbonPort)
        return False
    carbonMessage = ""
    while messages:
        carbonMessage += field_separator.join(map(str, messages.pop())) + line_separator
    try:
        sock.sendall(carbonMessage)
    except:
        print "Error could not transmit data to carbon host\n"


def sendEvent(url, eventPayload):
    '''
    Send a single event to Carbon via http/json
    Parameters
    url: the carbon api url to receive events
    eventPayLoad: event data in json format
    '''
    logger.debug("send eventdata %s to %s" % (json.dumps(eventPayload), url))
    r = requests.post(url, data=json.dumps(eventPayload))
    if r.status_code != 200:
        logger.error("http post for event failed with code %d and reason: %s" % (r.statuscode, r.reason))
        return 1
    logger.debug(r.text)
    return 0


def sendEventsToCarbon(url, events):
    '''
    Send a list of events to carbon
    '''
    if not events:
        logger.info("No events to send")
        return None
    for line in events:
        sendEvent(url, line)


def getJobStats(where):
    '''
    Get Job Statistics.
    Returns a list of of stat items, a stat item is a list (triplet)
    of identifier (metric name), value, timestamp.
    '''
    messages = []
    statement = """select * from JobStats %s""" % where
    logger.debug("Query db: %s" % statement)
    response = director.call(".sql query=\"%s\"" % statement)
    if not 'query' in response:
        return []
    for row in response['query']:
        jobId = int(row['jobid'])
        if not jobId in jobIdMapping.keys():
            statement = """select clientid,name from Job where jobid=%s""" % jobId
            logger.debug("Query db: %s" % statement)
            jobDetails = director.call(".sql query=\"%s\"" % statement)
            jobIdMapping[jobId] = jobDetails['query'][0]['clientid']
            jobNameMapping[jobId] = jobDetails['query'][0]['name']
        clientName = clientIdMapping[jobIdMapping[jobId]]
        jobName = jobNameMapping[jobId]
        timeObject = datetime.datetime.strptime(row['sampletime'], "%Y-%m-%d %H:%M:%S")
        timestamp = int(timeObject.strftime("%s"))
        logger.info("%s.clients.%s.jobs.%s.jobbytes %s at %d" %
                    (metricPrefix, clientName, jobName, row['jobbytes'], timestamp))
        messages.append(["%s.clients.%s.jobs.%s.jobbytes" %
                         (metricPrefix, clientName, jobName), int(row['jobbytes']), timestamp])
        messages.append(["%s.clients.%s.jobs.%s.jobfiles" %
                         (metricPrefix, clientName, jobName), int(row['jobfiles']), timestamp])
    return messages


def getDeviceStats(where):
    '''
    Get DeviceStats in timeframe specified by where.
    Returns a list of of stat items, a stat item is a list (triplet)
    of identifier (metric name), value, timestamp.
    '''
    messages = []
    statement = """select * from DeviceStats %s""" % where
    logger.debug("Query db: %s" % statement)
    response = director.call(".sql query=\"%s\"" % statement)
    if not 'query' in response:
        return []
    for row in response['query']:
        deviceId = row['deviceid']
        if not deviceId in deviceIdMapping.keys():
            logger.error("Device ID: %s unknown. Ignoring" % deviceId)
            continue
        deviceName = deviceIdMapping[deviceId]
        timeObject = datetime.datetime.strptime(row['sampletime'], "%Y-%m-%d %H:%M:%S")
        timestamp = int(timeObject.strftime("%s"))
        for dataItem in deviceStatKeys:
            logger.info("%s.devices.%s.%s %d at %d"
                        % (metricPrefix, deviceName, dataItem, int(row[dataItem]), timestamp))
            messages.append(["%s.devices.%s.%s" % (metricPrefix, deviceName, dataItem), int(row[dataItem]), timestamp])
    return messages


def getJobTotals():
    '''
    Get total bytes / files per job
    '''
    messages = []
    now = int(time.time())
    response = director.call("list jobtotals")
    totalFiles = response['jobtotals']['files']
    totalBytes = response['jobtotals']['bytes']
    totalJobs = response['jobtotals']['jobs']
    messages.append([metricPrefix+".totalFiles", int(totalFiles), now])
    messages.append([metricPrefix+".totalBytes", int(totalBytes), now])
    messages.append([metricPrefix+".totalJobs", int(totalJobs), now])

    for job in response['jobs']:
        jobFiles = job['files']
        jobName = job['job']
        jobBytes = job['bytes']
        jobCount = job['jobs']
        logger.info("job: %s, files: %s, count: %s, bytes: %s" % (jobName, jobFiles, jobCount, jobBytes))
        messages.append(["%s.jobs.%s.totalFiles" % (metricPrefix, jobName), jobFiles, now])
        messages.append(["%s.jobs.%s.totalBytes" % (metricPrefix, jobName), jobBytes, now])
        messages.append(["%s.jobs.%s.jobCount" % (metricPrefix, jobName), jobCount, now])
    return messages


def getPoolStatistics():
    '''
    Get Statisctics regarding all defined pools
    '''
    messages = []
    now = int(time.time())
    response = director.call('list volumes')
    for pool in response['volumes'].keys():
        totalBytes = 0
        numVolumes = 0
        volStatus = dict()
        for volume in response['volumes'][pool]:
            totalBytes += int(volume['volbytes'])
            numVolumes += 1
            status = volume['volstatus']
            if status not in volStatus.keys():
                volStatus[status] = 1
            else:
                volStatus[status] += 1
        logger.info("pool: %s, bytes: %d Volumes: %d" % (pool, totalBytes, numVolumes))
        messages.append(["%s.pools.%s.totalBytes" % (metricPrefix, pool), totalBytes, now])
        messages.append(["%s.pools.%s.totalVolumes" % (metricPrefix, pool), numVolumes, now])
        for status in volStatus.keys():
            messages.append(["%s.pools.%s.volumeStatus.%s" % (metricPrefix, pool, status), volStatus[status], now])
    return messages


def getEvents(whereJobs):
    '''
    Reads job-events (start, stop, scheduled start, real end) from the given timerange
    and returns a list of events. Each event is a dictionary with the fields
    ...
    '''
    events = []
    statement = """select * from Job %s and starttime <> '0000-00-00 00:00:00'""" % whereJobs
    logger.debug("Query db: %s" % statement)
    response = director.call(".sql query=\"%s\"" % statement)
    if 'query' in response.keys():
        rows = response['query']
    else:
        rows = []
        logger.info("No events found")
    for row in rows:
        jobId = row['jobid']
        if not jobId in jobIdMapping.keys():
            statement = """select clientid from Job where jobid=%s""" % jobId
            result = director.call(".sql query=\"%s\"" % statement)
            #print result
            jobIdMapping[jobId] = result['query'][0]['clientid']
        clientName = clientIdMapping[jobIdMapping[jobId]]
        jobLevelTag = "level=%s" % row['level']
        jobStatusTag = "status=%s" % row['jobstatus']
        jobIdTag = "id=%s" % jobId
        clientTag = "client=%s" % clientName
        jobInformation = "JobBytes: %s, JobFiles: %s" % (row['jobbytes'], row['jobfiles'])
        if row['poolid'] in poolIdMapping.keys():
            jobPoolTag = "pool=%s" % poolIdMapping[row['poolid']]
        else:
            jobPoolTag = "pool=%s" % row['poolid']
        timeObject = datetime.datetime.strptime(row['starttime'], "%Y-%m-%d %H:%M:%S")
        jobStartTime = int(timeObject.strftime("%s"))
        timeObject = datetime.datetime.strptime(row['schedtime'], "%Y-%m-%d %H:%M:%S")
        jobSchedTime = int(timeObject.strftime("%s"))
        timeObject = datetime.datetime.strptime(row['endtime'], "%Y-%m-%d %H:%M:%S")
        jobEndTime = int(timeObject.strftime("%s"))
        timeObject = datetime.datetime.strptime(row['realendtime'], "%Y-%m-%d %H:%M:%S")
        jobRealEndTime = int(timeObject.strftime("%s"))
        jobTags = clientTag + ' ' + jobIdTag + ' ' + jobLevelTag + ' ' + jobStatusTag + ' ' + jobPoolTag
        logger.info(jobTags)
        events.append({"what": "Job Starttime", "tags": jobTags + ' JobStart',
                       "when": jobStartTime, "data": jobInformation})
        events.append({"what": "Job Endtime", "tags": jobTags + ' JobEnd',
                       "when": jobEndTime, "data": jobInformation})
        events.append({"what": "Job Schedtime", "tags": jobTags + ' JobScheduled',
                       "when": jobSchedTime, "data": jobInformation})
        events.append({"what": "Job Realendtime", "tags": jobTags + ' JobRealEnd',
                       "when": jobRealEndTime, "data": jobInformation})
        return events


if __name__ == '__main__':

    argParser = argparse.ArgumentParser(description='Graphite poller for Bareos director.',
                                        prog="bareos-graphite-poller.py")
    argParser.add_argument('-d', '--debug', default='error', choices=['error', 'warning', 'info', 'debug'],
                           help="Set debug level to {error,warning,info,debug}")
    argParser.add_argument('-c', '--config', help="Configfile", default="/etc/bareos/graphite-poller.conf")
    argParser.add_argument('-e', '--events', dest='events', help="Import Events (JobStart / JobEnd)",
                           action='store_true')
    argParser.add_argument('-j', '--jobstats', dest='jobstats', help="Import jobstats", action='store_true')
    argParser.add_argument('-v', '--devicestats', dest='devicestats', help="Import devicestats", action='store_true')
    argParser.add_argument('-s', '--start', help="Import entries after this timestamp (only used for stats and events)")
    argParser.add_argument('-t', '--to', help="Import entries before this timestamp (only used for stats and events)")
    args = argParser.parse_args()

    logging.basicConfig(format='%(levelname)s %(module)s.%(funcName)s: %(message)s',
                        level=logging.ERROR)
    logger = logging.getLogger()

    if args.debug == 'info':
        logger.setLevel(logging.INFO)
    elif args.debug == "warning":
        logger.setLevel(logging.WARNING)
    elif args.debug == "debug":
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.ERROR)

    parser = ConfigParser.SafeConfigParser({'port': '9101'})
    parser.readfp(open(args.config, "r"))

    host = parser.get("director", "server")
    dirname = parser.get("director", "name")
    password = parser.get("director", "password")
    dir_port = parser.getint("director", "port")
    carbonHost = parser.get("graphite", "server")
    carbonPort = parser.getint("graphite", "port")
    try:
        prefix = parser.get("graphite", "prefix")
    except ConfigParser.NoOptionError:
        prefix = "bareos."
    eventUrl = "http://%s/events/" % carbonHost
    metricPrefix = prefix + dirname
    timeStampFile = "/var/tmp/bareos-graphite-poller_"
    timeStampFileHandler = None
    # evaluate time-ranges (only needed for events and job/devicestats
    where = ""
    whereJobs = ""
    start = ""
    to = ""
    startJobEnd = ""
    toJobEnd = ""
    now = datetime.datetime.now()
    if args.start or args.to:
        if args.start == 'last':
            # we have a unique timestamp file name per option combination
            timeStampFile += hashlib.sha224(str(args)).hexdigest()
            if os.path.isfile(timeStampFile) and os.access(timeStampFile, os.R_OK):
                timeStampFileHandler = open(timeStampFile, 'r+w')
                startFromFile = timeStampFileHandler.read()
                start = "sampletime>'%s'" % startFromFile
                startJobEnd = "realendtime>'%s' " % startFromFile
                to = " sampletime<'%s'" % now.strftime("%Y-%m-%d %H:%M:%S")
                toJobEnd = "realendtime<'%s'" % now.strftime("%Y-%m-%d %H:%M:%S")
                #where = "where %s AND %s" %(start, to)
                #whereJobs = "where %s AND %s" %(startJobEnd, toJobEnd)
            else:
                logger.warning("No timestamp file found. Create one and exit")
                timeStampFileHandler = open(timeStampFile, 'w')
                timeStampFileHandler.write(now.strftime("%Y-%m-%d %H:%M:%S"))
                timeStampFileHandler.close()
                sys.exit(0)
        elif args.start and args.start != 'last':
            start = "sampletime>'%s' " % args.start
            startJobEnd = "realendtime>'%s' " % args.start
        if args.to:
            to = " sampletime<'%s'" % args.to
            toJobEnd = "realendtime<'%s'" % args.to
        if (args.to or args.start == 'last') and args.start:
            where = "where %s AND %s" % (start, to)
            whereJobs = "where %s AND %s" % (startJobEnd, toJobEnd)
        else:
            where = "where " + start + to
            whereJobs = "where " + startJobEnd + toJobEnd

    # messages is a list of lists to store the performance messages
    # Each entry consists of a triplet metric_identifier, metric_value, timestamp
    messages = []
    # Keys to consider when reading device statistics
    deviceStatKeys = ['readtime', 'writetime', 'readbytes', 'writebytes', 'spoolsize',
                      'numwaiting', 'numwriters', 'volcatbytes', 'volcatfiles', 'volcatblocks']

    try:
        logger.info("Connect to Bareos director %s, host %s, port: %d" % (dirname, host, dir_port))
        # print password
        director = bareos.bsock.BSockJson(address=host, port=dir_port, dirname=dirname,
                                          password=bareos.bsock.Password(password))
    except RuntimeError as e:
        print str(e)
        sys.exit(1)

    # jobid mapping might get big, we only resolve as we need
    jobIdMapping = {}
    jobNameMapping = {}

    # Setup client id mapping
    clientIdMapping = {}
    statement = """select clientid,name from Client"""
    response = director.call(".sql query=\"%s\"" % statement)
    if not 'query' in response.keys():
        logger.error("No clients found in director using sql search")
        sys.exit(1)
    for row in response['query']:
        clientIdMapping[row['clientid']] = row['name']

    # Resolve Pool ID to pool name
    poolIdMapping = {}
    statement = """select poolid,name from Pool"""
    response = director.call(".sql query=\"%s\"" % statement)
    if not 'query' in response.keys():
        logger.error("No pools found in director using sql search")
        sys.exit(1)
    for row in response['query']:
        poolIdMapping[row['poolid']] = "%s-%s" % (row['poolid'], row['name'])

    # resolve deviceid to device name (prepending id, to be unique)
    deviceIdMapping = {}
    statement = """select deviceid,name from Device"""
    response = director.call(".sql query=\"%s\"" % statement)
    if not 'query' in response.keys():
        logger.error("No devices found in director using sql search")
        sys.exit(1)
    for row in response['query']:
        deviceIdMapping[row['deviceid']] = "%s-%s" % (row['deviceid'], row['name'])

    logger.debug("Processing JobTotals")
    messages = getJobTotals()
    sentItemsToCarbon(messages, carbonHost, carbonPort)
    messages = []

    logger.debug("Processing Pools")
    messages = getPoolStatistics()
    sentItemsToCarbon(messages, carbonHost, carbonPort)
    messages = []

    # get number of running jobs
    response = director.call('llist jobs jobstatus=R count')
    numRunningJobs = response['jobs'][0]['count']
    logger.info("runningJobs: %s" % (numRunningJobs))
    messages.append(["%s.jobsRunning" % metricPrefix, numRunningJobs, int(now.strftime("%s"))])

    sentItemsToCarbon(messages, carbonHost, carbonPort)
    messages = []

    if args.events:
        logger.debug("Processing events")
        events = getEvents(whereJobs)
        sendEventsToCarbon(eventUrl, events)
        events = []

    if args.devicestats:
        logger.debug("Processing devicestats")
        messages = getDeviceStats(where)
        sentItemsToCarbon(messages, carbonHost, carbonPort)
        messages = []

    if args.jobstats:
        logger.debug("Processing jobstats")
        messages = getJobStats(where)
        sentItemsToCarbon(messages, carbonHost, carbonPort)
        messages = []

    if timeStampFileHandler:
        logger.debug("Writing timestimp")
        timeStampFileHandler.seek(0)
        timeStampFileHandler.write(now.strftime("%Y-%m-%d %H:%M:%S"))
        timeStampFileHandler.close()
