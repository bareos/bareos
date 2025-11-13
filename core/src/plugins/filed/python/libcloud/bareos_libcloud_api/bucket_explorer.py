#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

import time
from enum import Enum
from bareos_libcloud_api.process_base import ThreadType
from bareos_libcloud_api.process_base import ProcessBase
from bareos_libcloud_api.get_libcloud_driver import get_driver
from bareos_libcloud_api.mtime import ModificationTime


class TaskType(Enum):
    """
    Enumeration for possible task types in BucketExplorer.

    Attributes:
        UNDEFINED: Task type is not defined.
        DOWNLOADED: Task represents a downloaded object.
        TEMP_FILE: Task represents a temporary file.
        STREAM: Task represents a streamed object.
        ACCURATE: Task used for accurate mode processing.
    """

    UNDEFINED = 0
    DOWNLOADED = 1
    TEMP_FILE = 2
    STREAM = 3
    ACCURATE = 4


def parse_options_bucket(name, options):
    """
    Parse comma-separated bucket names from options.

    Args:
        name (str): The option key to look for.
        options (dict): The options dictionary.

    Returns:
        list or None: List of bucket names, or None if the option is not present.
    """
    if name not in options:
        return None

    buckets = []
    for bucket in options[name].split(","):
        buckets.append(bucket)
    return buckets


class BucketExplorer(ProcessBase):
    """
    Class for exploring buckets using a libcloud driver.

    Attributes:
        options (dict): Configuration options.
        last_run (int): Timestamp of the last run.
        discovered_objects_queue (Queue): Queue for discovered objects.
        buckets_include (list or None): Buckets to include.
        buckets_exclude (list or None): Buckets to exclude.
        number_of_workers (int): Number of worker threads.
        queue_filled_event (Event): Event to set when the queue is filled.
    """

    def __init__(
        self,
        options,
        last_run,
        message_queue,
        discovered_objects_queue,
        number_of_workers,
        queue_filled_event,
    ):
        """
        Initialize BucketExplorer.

        Args:
            options (dict): Configuration options.
            last_run (int): Timestamp of the last run.
            message_queue (Queue): Queue for messages.
            discovered_objects_queue (Queue): Queue for discovered objects.
            number_of_workers (int): Number of worker processes.
            queue_filled_event (Event): Event to set when the queue is filled.
        """
        super().__init__(0, ThreadType.BUCKET_EXPLORER, message_queue)
        self.options = options
        self.last_run = last_run
        self.discovered_objects_queue = discovered_objects_queue
        self.buckets_include = parse_options_bucket("buckets_include", options)
        self.buckets_exclude = parse_options_bucket("buckets_exclude", options)
        self.number_of_workers = number_of_workers
        self.queue_filled_event = queue_filled_event
        self.fail_on_download_error = bool(
            options["fail_on_download_error"]
            if "fail_on_download_error" in options
            else 0
        )

    def run_process(self):
        """
        Main process loop for BucketExplorer.
        Initializes the libcloud driver and iterates over buckets,
        generating tasks for discovered objects.
        """
        error = ""
        try:
            self.driver = get_driver(self.options)
        except Exception as e:
            error = f"({str(e)})"

        if self.driver is None:
            self.error_message(f"Could not load driver {error}")
            self.abort_message()
            return

        while not self.shutdown_event.is_set():
            try:
                self._iterate_over_buckets()
                self.shutdown_event.set()
            except Exception as e:
                self.error_message(f"Error while iterating buckets ({str(e)})")
                if self.fail_on_download_error:
                    self.abort_message()

        self.queue_filled_event.set()

        for _ in range(self.number_of_workers):
            self.discovered_objects_queue.put(None)

    def _iterate_over_buckets(self):
        """
        Iterate over buckets using the libcloud driver,
        apply include/exclude filters, and generate tasks for bucket objects.
        """
        for bucket in self.driver.iterate_containers():
            if self.shutdown_event.is_set():
                break

            if self.buckets_include is not None:
                if bucket.name not in self.buckets_include:
                    continue

            if self.buckets_exclude is not None:
                if bucket.name in self.buckets_exclude:
                    continue

            self.debug_message(100, f'Exploring bucket "{bucket.name}"')
            self.info_message(f'Exploring bucket "{bucket.name}"')

            container_objects_iterator = self.driver.iterate_container_objects(bucket)
            self.debug_message(100, f'Generating tasks for bucket "{bucket.name}"')
            self._generate_tasks_for_bucket_objects(container_objects_iterator)

    def _generate_tasks_for_bucket_objects(self, object_iterator):
        """
        Generate tasks for objects in a bucket.

        Args:
            object_iterator: Iterator over bucket objects.
        """
        obj_count = 0
        task_count = 0
        start_time = time.time()
        for obj in object_iterator:
            if obj_count == 0:
                iteration_start_time = time.time()
                iterator_init_time = iteration_start_time - start_time
                self.debug_message(
                    100,
                    f"Bucket object iterator initialization took {iterator_init_time:.2f} s",
                )

            obj_count += 1

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
                "type": TaskType.UNDEFINED,
            }

            object_name = f"{obj.container.name}/{obj.name}"

            if self.last_run > mtime:
                self.debug_message(
                    110,
                    f"File {object_name} not changed, skipped ({self.last_run} > {mtime})",
                )

                # This object was present on our last backup
                # Here, we push it directly to bareos, it will not be backed up
                # again but remembered as "still here" (for accurate mode)
                # If accurate mode is off, we can simply skip that object
                if self.options["accurate"] is True:
                    task_count += 1
                    task["type"] = TaskType.ACCURATE
                    self.queue_try_put(self.discovered_objects_queue, task)

                continue

            self.debug_message(
                110,
                f"File {object_name} was changed or is new, put to queue "
                f"({self.last_run} < {mtime})",
            )

            task_count += 1
            if task_count >= (self.discovered_objects_queue.maxsize - 1):
                self.queue_filled_event.set()

            self.queue_try_put(self.discovered_objects_queue, task)

        self.debug_message(
            100,
            f"Generating {task_count} tasks took {time.time() - iteration_start_time:.2f} s",
        )
