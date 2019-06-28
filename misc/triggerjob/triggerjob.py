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


def trigger(director, jobnames, clients):
    for client in clients:
        jobname = 'backup-{}'.format(client)
        if not jobname in jobnames:
            print('{} skipped, no matching job ({}) found'.format(client, jobname))
        else:
            jobs = director.call(
                'list jobs client={} hours=24'.format(client))['jobs']
            if jobs:
                job = director.call('list jobs client={} hours=24 last'.format(client))[
                    'jobs'][0]
                jobinfo = '{starttime}: jobid={jobid}, level={level}, status={jobstatus}'.format(
                    **job)
                print('{}: skipped, recent backups available ({})'.format(
                    jobname, jobinfo))
            else:
                # TODO: check type=backup and failed
                jobid = director.call('run {} yes'.format(jobname))[
                    'run']['jobid']
                print('{}: backup triggered, jobid={}'.format(jobname, jobid))


def getArguments():
    parser = argparse.ArgumentParser(description='Console to Bareos Director.')
    parser.add_argument('-d', '--debug', action='store_true',
                        help="enable debugging output")
    parser.add_argument('--name', default="*UserAgent*",
                        help="use this to access a specific Bareos director named console. Otherwise it connects to the default console (\"*UserAgent*\")")
    parser.add_argument(
        '-p', '--password', help="password to authenticate to a Bareos Director console", required=True)
    parser.add_argument('--port', default=9101,
                        help="Bareos Director network port")
    parser.add_argument('--dirname', help="Bareos Director name")
    parser.add_argument('address', nargs='?', default="localhost",
                        help="Bareos Director network address")
    args = parser.parse_args()
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
    jobs = get_job_names(director)
    clients = get_connected_clients(director)
    trigger(director, jobs, clients)

    # director.interactive()
