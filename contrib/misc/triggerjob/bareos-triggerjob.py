#!/usr/bin/env python

from __future__ import print_function
import argparse
import bareos.bsock
import logging
import sys



def get_job_names(director):
    result = director.call('.jobs')['jobs']
    jobs = [job['name'] for job in result]
    return jobs


def get_connected_clients(director):
    result = director.call('status director')['client-connection']
    clients = [client['name'] for client in result]
    return clients


def trigger(director, jobnames, clients, hours):
    for client in clients:
        jobname = 'backup-{}'.format(client)
        if not jobname in jobnames:
            print('{} skipped, no matching job ({}) found'.format(client, jobname))
        else:
            jobs = director.call(
                ('list jobs client={} hours={}').format(client, hours))['jobs']
            if jobs:
                job = director.call(
                    ('list jobs client={} hours={} last').format(client, hours))['jobs'][0]
                jobinfo = '{starttime}: jobid={jobid}, level={level}, status={jobstatus}'.format(
                    **job)
                print('{}: skipped, recent backups available ({})'.format(
                    jobname, jobinfo))
            else:
                jobid = director.call('run {} yes'.format(jobname))[
                    'run']['jobid']
                print('{}: backup triggered, jobid={}'.format(jobname, jobid))


def getArguments():
    epilog = """
    bareos-triggerjob is a Python script that allows you to perform a backup for a connected client if a definable time has passed since the last backup.

    It will look for all clients connected to the Director. If it finds a job named "backup-{clientname}" that did not successfully run during the specified time period, it will trigger this job. This way, clients that are not regularly connected to the director, such as notebooks, can be reliably backed up.

    bareos-triggerjob should be executed regularly to detect newly connected clients. To do so, a cronjob should run the script repeatedly.

    Note: bareos-triggerjob requires a connection between director and client. Therefore, activate Client Initiated Connections (https://docs.bareos.org/TasksAndConcepts/NetworkSetup.html#client-initiated-connection) to automatically establish a connection whenever possible. Otherwise no jobs will be started.

    """
    
    argparser = argparse.ArgumentParser(description=u"Trigger Bareos jobs.", epilog=epilog)
    argparser.add_argument('-d', '--debug', action='store_true',
                        help="enable debugging output")
    bareos.bsock.DirectorConsole.argparser_add_default_command_line_arguments(argparser)
    args = argparser.parse_args()
    return args


if __name__ == '__main__':
    logging.basicConfig(
        format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    args = getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    try:
        options = ['address', 'port', 'dirname', 'name']
        parameter = {}
        for i in options:
            if hasattr(args, i) and getattr(args, i) != None:
                logger.debug("%s: %s" % (i, getattr(args, i)))
                parameter[i] = getattr(args, i)
            else:
                logger.debug('%s: ""' % (i))
        logger.debug('options: %s' % (parameter))
        password = bareos.bsock.Password(args.password)
        parameter['password'] = password
        director = bareos.bsock.DirectorConsoleJson(**parameter)
    except RuntimeError as e:
        print(str(e))
        sys.exit(1)
    logger.debug("authentication successful")

    hours = str(args.hours)

    jobs = get_job_names(director)
    clients = get_connected_clients(director)
    trigger(director, jobs, clients, hours)
