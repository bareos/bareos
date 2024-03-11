#!/usr/bin/env python3

#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2024 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

import logging
import sys
import bareos.bsock
from bareos.util import argparse


def get_job_names(director):
    """Get list of job names"""
    result = director.call(".jobs")["jobs"]
    jobs = [job["name"] for job in result]
    return jobs


def get_connected_clients(director):
    """Get list of connected clients (via client initiated connections)"""
    result = director.call("status director")["client-connection"]
    clients = [client["name"] for client in result]
    return clients


def trigger(director, jobnames, clients, hours):
    """
    trigger all jobs that are named "backup-<CLIENTNAME>"
    for all clients that are connected via client initiated connection
    and did run a successful backup for more than <hours> hours.
    """

    for client in clients:
        jobname = "backup-{}".format(client)
        if not jobname in jobnames:
            print("{} skipped, no matching job ({}) found".format(client, jobname))
        else:
            jobs = director.call(
                ("list jobs client={} hours={}").format(client, hours)
            )["jobs"]
            if jobs:
                job = director.call(
                    ("list jobs client={} hours={} last").format(client, hours)
                )["jobs"][0]
                jobinfo = "{starttime}: jobid={jobid}, level={level}, status={jobstatus}".format(
                    **job
                )
                print(
                    "{}: skipped, recent backups available ({})".format(
                        jobname, jobinfo
                    )
                )
            else:
                jobid = director.call("run {} yes".format(jobname))["run"]["jobid"]
                print("{}: backup triggered, jobid={}".format(jobname, jobid))


def get_arguments():
    """argparse setup"""

    epilog = """
    bareos-triggerjob is a Python script that allows you to perform a backup for a connected client if a definable time has passed since the last backup.

    It will look for all clients connected to the Director. If it finds a job named "backup-{clientname}" that did not successfully run during the specified time period, it will trigger this job. This way, clients that are not regularly connected to the director, such as notebooks, can be reliably backed up.

    bareos-triggerjob should be executed regularly to detect newly connected clients. To do so, a cronjob should run the script repeatedly.

    Note: bareos-triggerjob requires a connection between director and client. Therefore, activate Client Initiated Connections (https://docs.bareos.org/TasksAndConcepts/NetworkSetup.html#client-initiated-connection) to automatically establish a connection whenever possible. Otherwise no jobs will be started.

    """

    argparser = argparse.ArgumentParser(
        description="Trigger Bareos jobs.", epilog=epilog
    )
    argparser.add_argument(
        "-d", "--debug", action="store_true", help="enable debugging output"
    )
    argparser.add_argument(
        "--hours",
        type=int,
        default=24,
        help="Minimum time (in hours) since the last successful backup. Default: %(default)s",
    )
    bareos.bsock.DirectorConsoleJson.argparser_add_default_command_line_arguments(
        argparser
    )
    args = argparser.parse_args()
    return args


if __name__ == "__main__":
    logging.basicConfig(
        format="%(levelname)s %(module)s.%(funcName)s: %(message)s", level=logging.INFO
    )
    logger = logging.getLogger()

    args = get_arguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    bareos_args = bareos.bsock.DirectorConsoleJson.argparser_get_bareos_parameter(args)
    try:
        director = bareos.bsock.DirectorConsoleJson(**bareos_args)
    except bareos.exceptions.Error as e:
        print(str(e))
        sys.exit(1)
    logger.debug("authentication successful")

    hours = str(args.hours)

    jobs = get_job_names(director)
    clients = get_connected_clients(director)
    trigger(director, jobs, clients, hours)
