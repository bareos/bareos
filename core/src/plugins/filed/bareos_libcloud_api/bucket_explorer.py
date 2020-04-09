#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

from bareos_libcloud_api.process_base import ProcessBase
from bareos_libcloud_api.get_libcloud_driver import get_driver
from bareos_libcloud_api.mtime import ModificationTime


class TASK_TYPE(object):
    UNDEFINED = 0
    DOWNLOADED = 1
    TEMP_FILE = 2
    STREAM = 3

    def __setattr__(self, *_):
        raise Exception("class JOB_TYPE is read only")


def parse_options_bucket(name, options):
    if name not in options:
        return None
    else:
        buckets = list()
        for bucket in options[name].split(","):
            buckets.append(bucket)
        return buckets


class BucketExplorer(ProcessBase):
    def __init__(
        self,
        options,
        last_run,
        message_queue,
        discovered_objects_queue,
        number_of_workers,
    ):
        super(BucketExplorer, self).__init__(0, message_queue)
        self.options = options
        self.last_run = last_run
        self.discovered_objects_queue = discovered_objects_queue
        self.buckets_include = parse_options_bucket("buckets_include", options)
        self.buckets_exclude = parse_options_bucket("buckets_exclude", options)
        self.number_of_workers = number_of_workers

    def run_process(self):
        self.driver = get_driver(self.options)

        if self.driver == None:
            self.error_message("Could not load driver")
            self.abort_message()
            return

        try:
            if not self.shutdown_event.is_set():
                self.__iterate_over_buckets()
        except Exception:
            self.error_message("Error while iterating buckets")
            self.abort_message()

        for _ in range(self.number_of_workers):
            self.discovered_objects_queue.put(None)

    def __iterate_over_buckets(self):
        for bucket in self.driver.iterate_containers():
            if self.shutdown_event.is_set():
                break

            if self.buckets_include is not None:
                if bucket.name not in self.buckets_include:
                    continue

            if self.buckets_exclude is not None:
                if bucket.name in self.buckets_exclude:
                    continue

            self.info_message('Exploring bucket "%s"' % (bucket.name,))

            self.__generate_jobs_for_bucket_objects(
                self.driver.iterate_container_objects(bucket)
            )

    def __generate_jobs_for_bucket_objects(self, object_iterator):
        for obj in object_iterator:
            if self.shutdown_event.is_set():
                break

            mtime, mtime_ts = ModificationTime().get_mtime(obj)

            task = {
                "name": obj.name,
                "bucket": obj.container.name,
                "data": None,
                "index": None,
                "size": obj.size,
                "mtime": mtime_ts,
                "type": TASK_TYPE.UNDEFINED,
            }

            object_name = "%s/%s" % (obj.container.name, obj.name)

            if self.last_run > mtime:
                self.info_message(
                    "File %s not changed, skipped (%s > %s)"
                    % (object_name, self.last_run, mtime),
                )

                # This object was present on our last backup
                # Here, we push it directly to bareos, it will not be backed up
                # again but remembered as "still here" (for accurate mode)
                # If accurate mode is off, we can simply skip that object
                if self.options["accurate"] is True:
                    self.queue_try_put(self.discovered_objects_queue, task)

                continue

            self.info_message(
                "File %s was changed or is new, put to queue (%s < %s)"
                % (object_name, self.last_run, mtime),
            )

            self.queue_try_put(self.discovered_objects_queue, task)
