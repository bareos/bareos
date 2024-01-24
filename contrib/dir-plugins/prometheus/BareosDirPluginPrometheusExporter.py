#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
# Author: Benedikt Braunger
#
# Prometheus Exporter as Bareos python plugin
# Functions taken and adapted from BareosDirPluginGraphiteSender.py

import bareosdir
import BareosDirPluginBaseclass
from prometheus_client import (
    CollectorRegistry,
    Gauge,
    Enum,
    Histogram,
    Info,
    push_to_gateway,
)
from prometheus_client.exposition import basic_auth_handler


class BareosDirPluginPrometheusExporter(
    BareosDirPluginBaseclass.BareosDirPluginBaseclass
):
    """Bareos Python plugin to send data about finished backups to a Prometheus Pushgateway"""

    # maybe this can go to consts
    job_levels = {
        "F": "Full",
        "I": "Incremental",
        "D": "Differential",
        "S": "Unknown",
        "C": "Verify catalog",
        "V": "Verify init",
        "O": "Verfiy volume to catalog",
        "d": "Verify disk to catalog",
        "A": "Verify Data",
        "B": "Base",
        " ": "None",
        "f": "Virtual full",
    }
    job_status = {
        "A": "Canceled",
        "B": "Blocked",
        "C": "Created",
        "D": "Differences",
        "E": "Error",
        "F": "Wait FD",
        "I": "Incomplete",
        "L": "Commiting data",
        "M": "Wait mount",
        "R": "Running",
        "S": "Wait SD",
        "T": "Terminated",
        "W": "Warning",
        "a": "SD despooling",
        "c": "Wait FD resource",
        "d": "Wait max jobs",
        "e": "Non-fatal error",
        "f": "Fatal error",
        "i": "Attr insertion",
        "j": "Wait job resource",
        "l": "Despooling",
        "m": "Wait media",
        "p": "Wait priority",
        "q": "Wait device",
        "s": "Wait SD resource",
        "t": "Wait start time",
    }

    job_types = {
        "B": "Backup",
        "M": "Backup migrated",
        "V": "Verify",
        "R": "Restore",
        "U": "Console",
        "I": "Internal",
        "D": "Admin",
        "A": "Archive",
        "C": "Copy of a job",
        "c": "Copy job",
        "g": "Migration",
        "S": "Scan",
        "O": "Consolidate",
    }

    def authentication_handler(self, url, method, timeout, headers, data):
        """This function handles the basic auth credtials for prometheus-client"""
        return basic_auth_handler(
            url, method, timeout, headers, data, self.username, self.password
        )

    def parse_plugin_definition(self, plugindef):
        """
        Check, if mandatory gateway is set and set default for other unset parameters
        """
        super(BareosDirPluginPrometheusExporter, self).parse_plugin_definition(
            plugindef
        )
        # monitoring Host is mandatory
        if "gateway_host" not in self.options:
            self.gateway_host = "localhost"
        else:
            self.gateway_host = self.options["gateway_host"]

        if "gateway_port" not in self.options:
            self.gateway_port = 9091
        else:
            self.gateway_port = int(self.options["gateway_port"])

        if "username" in self.options and "password" in self.options:
            self.username = self.options["username"]
            self.password = self.options["password"]
            self.use_basic_auth = True
            bareosdir.DebugMessage(
                100, "Using Basic auth with username={}\n".format(self.username)
            )
        else:
            self.use_basic_auth = False
            bareosdir.DebugMessage(
                100, "Username/password missing, disabling basic auth\n"
            )

        if "use_tls" not in self.options:
            self.use_tls = False
        else:
            self.use_tls = self.options["use_tls"]

        if "report_failed" not in self.options:
            self.report_failed = False
        else:
            self.report_failed = bool(self.options["report_failed"])

        # we return OK in anyway, we do not want to produce Bareos errors just because of
        # a failing metric exporter
        return bareosdir.bRC_OK

    def handle_plugin_event(self, event):
        """
        This method is called for every registered event
        """

        # We first call the method from our superclass to get job attributes read
        super(BareosDirPluginPrometheusExporter, self).handle_plugin_event(event)

        if event == bareosdir.bDirEventJobEnd:
            # This event is called at the end of a job, here evaluate the results
            self.push_job_information()

        return bareosdir.bRC_OK

    def push_job_information(self):
        """
        Process Bareos job data and send it to the prometheus pushgateway
        """
        registry = CollectorRegistry()

        TIME_BUCKETS = (6, 60, 600, 1800, 3600, 10800, 18000, 28800, 86400)

        bareos_job_status = Enum(
            "bareos_job_status",
            "Backup Status",
            states=list(self.job_status.values()),
            labelnames=["instance", "jobid"],
            registry=registry,
        )
        # see https://github.com/bareos/bareos/blob/master/core/src/include/job_level.h
        bareos_job_level = Enum(
            "bareos_job_level",
            "Backup Level",
            states=list(self.job_levels.values()),
            labelnames=["instance", "jobid"],
            registry=registry,
        )
        bareos_job_running_time = Histogram(
            "bareos_job_running_time",
            "Job running time",
            labelnames=["instance", "jobid"],
            registry=registry,
            buckets=TIME_BUCKETS,
        )
        bareos_job_files = Gauge(
            "bareos_job_files",
            "Backed up files",
            labelnames=["instance", "jobid"],
            registry=registry,
        )
        bareos_job_bytes = Gauge(
            "bareos_job_bytes",
            "Backed up bytes",
            labelnames=["instance", "jobid"],
            registry=registry,
        )
        bareos_job_throughput = Gauge(
            "bareos_job_throughtput",
            "Backup throughtput",
            registry=registry,
            labelnames=["instance", "jobid"],
        )
        # see https://github.com/bareos/bareos/blob/master/core/src/include/job_types.h
        bareos_job_type = Enum(
            "bareos_job_type",
            "Job Type",
            states=list(self.job_types.values()),
            registry=registry,
            labelnames=["instance", "jobid"],
        )
        bareos_job_client = Info(
            "bareos_job_client",
            "Client",
            registry=registry,
            labelnames=["instance", "jobid"],
        )
        bareos_job_priority = Gauge(
            "bareos_job_priority",
            "Job Priority",
            registry=registry,
            labelnames=["instance", "jobid"],
        )

        bareos_job_name = "_".join(self.jobName.split(".")[:-3])
        bareos_job_id = self.jobId

        if (
            self.jobStatus == "E" or self.jobStatus == "f" or self.jobStatus == "A"
        ) and self.report_failed is False:
            return

        bareos_job_status.labels(instance=bareos_job_name, jobid=bareos_job_id).state(
            self.job_status[self.jobStatus]
        )
        bareos_job_running_time.labels(
            instance=bareos_job_name, jobid=bareos_job_id
        ).observe(self.jobRunningTime)
        bareos_job_files.labels(instance=bareos_job_name, jobid=bareos_job_id).set(
            self.jobFiles
        )
        bareos_job_bytes.labels(instance=bareos_job_name, jobid=bareos_job_id).set(
            self.jobBytes
        )
        bareos_job_throughput.labels(instance=bareos_job_name, jobid=bareos_job_id).set(
            self.throughput
        )
        bareos_job_priority.labels(instance=bareos_job_name, jobid=bareos_job_id).set(
            self.Priority
        )
        bareos_job_level.labels(instance=bareos_job_name, jobid=bareos_job_id).state(
            self.job_levels[self.jobLevel]
        )
        bareos_job_type.labels(instance=bareos_job_name, jobid=bareos_job_id).state(
            self.job_types[chr(self.jobType)]
        )
        bareos_job_client.labels(instance=bareos_job_name, jobid=bareos_job_id).info(
            {"client": self.jobClient}
        )

        if self.use_tls is True or self.use_tls == "yes":
            gateway = "https://{}:{}".format(self.gateway_host, self.gateway_port)
        else:
            gateway = "{}:{}".format(self.gateway_host, self.gateway_port)

        bareosdir.DebugMessage(100, "Submitting metrics to {}\n".format(gateway))
        try:
            if self.use_basic_auth:
                push_to_gateway(
                    "{}".format(gateway),
                    job="bareos",
                    registry=registry,
                    handler=self.authentication_handler,
                )
            else:
                push_to_gateway("{}".format(gateway), job="bareos", registry=registry)
        except Exception as excp:
            bareosdir.DebugMessage(
                100,
                "Error: Submitting metrics to pushgateway '{}' failed.\n".format(
                    gateway
                ),
            )
            bareosdir.DebugMessage(100, "python error was: {}\n".format(excp))
            bareosdir.JobMessage(
                bareosdir.M_INFO, "Failed to submit metrics to pushgateway\n"
            )


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
