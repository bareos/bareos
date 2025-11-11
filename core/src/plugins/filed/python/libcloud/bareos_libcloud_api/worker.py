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

import io
import uuid
import os
import pathlib
import requests.exceptions

import time

from libcloud.common.types import LibcloudError
from libcloud.storage.types import ObjectDoesNotExistError

from bareos_libcloud_api.bucket_explorer import TaskType
from bareos_libcloud_api.process_base import ThreadType
from bareos_libcloud_api.process_base import ProcessBase
from bareos_libcloud_api.get_libcloud_driver import get_driver

CONTINUE = 0
FINISH_ON_ERROR = 1
FINISH_ON_REQUEST = 2


def task_object_full_name(task):
    """
    Generate the full name of a task object, joining bucket and object name with slash.

    Args:
        task (dict): A task dictionary containing 'bucket' and 'name'.

    Returns:
        str: The full name in 'bucket/name' format.
    """
    return task["bucket"] + "/" + task["name"]


class Worker(ProcessBase):
    """
    Worker thread for processing download tasks from a queue.
    It first retrieves object metadata from cloud storage based on task details.
    Depending on object size, very small objects are prefetched to memory, small
    objects are prefetched to temporary files, for large objects, only the stream
    handle will be used. In any case the processed task is put into an output queue
    for further processing by the main thread.
    """

    def __init__(
        self,
        options,
        worker_id,
        tmp_dir_path,
        message_queue,
        discovered_objects_queue,
        downloaded_objects_queue,
        bucket_explorer_queue_filled_event,
    ):
        """
        Initialize the Worker thread.

        Args:
            options (dict): Configuration options for the worker and libcloud driver.
            worker_id (int): The ID of the worker.
            tmp_dir_path (str): Path to the temporary directory for downloads.
            message_queue (Queue): Queue for sending status/error messages.
            discovered_objects_queue (Queue): Input queue for tasks to be processed.
            downloaded_objects_queue (Queue): Output queue for completed tasks.
        """
        super().__init__(worker_id, ThreadType.WORKER, message_queue)
        self.options = options
        self.tmp_dir_path = tmp_dir_path
        self.input_queue = discovered_objects_queue
        self.output_queue = downloaded_objects_queue
        self.bucket_explorer_queue_filled_event = bucket_explorer_queue_filled_event
        self.fail_on_download_error = bool(
            options["fail_on_download_error"]
            if "fail_on_download_error" in options
            else 0
        )
        self.debug_message(
            100,
            f"Init WORKER {worker_id}: fail_on_download_error: "
            f"{self.fail_on_download_error}",
        )

    def run_process(self):
        """
        Main process for the worker.
        Initializes the libcloud driver and processes tasks from the input queue
        until a shutdown is requested or a fatal error occurs.
        """
        if not self._initialize_driver():
            return

        # wait for bucket explorer to initially fill the queue to prevent
        # from useless looping of _iterate_input_queue with an empty queue
        self.bucket_explorer_queue_filled_event.wait()

        status = CONTINUE
        while status == CONTINUE and not self.shutdown_event.is_set():
            status = self._iterate_input_queue()

        if status == FINISH_ON_ERROR:
            self.debug_message(100, "finishing on error")
            self.abort_message()

    def _initialize_driver(self):
        """
        Initializes the libcloud driver for the worker.
        If the driver cannot be loaded, an error message is sent, and an abort message is
        queued.
        """
        self.driver = None
        error = ""
        try:
            self.driver = get_driver(self.options)
        except Exception as e:
            error = f"({e})"

        if self.driver is None:
            self.error_message(f"Could not load driver {error}")
            self.abort_message()
            return False

        return True

    def _iterate_input_queue(self):
        """
        Continuously fetch and process tasks from the input queue.

        Returns:
            int: A status code indicating whether to continue processing,
                 finish due to a request, or finish due to an error.
                 (CONTINUE, FINISH_ON_REQUEST, FINISH_ON_ERROR)
        """
        while not self.input_queue.empty():
            if self.shutdown_event.is_set():
                self.debug_message(100, "got shutdown event, finishing")
                return FINISH_ON_REQUEST
            self.debug_message(110, "getting element from input_queue")
            task = self.input_queue.get()
            self.debug_message(110, "got element from input_queue")
            if task is None:  # poison pill
                self.debug_message(
                    100, "got poison pill (end of input queue), finishing"
                )
                return FINISH_ON_REQUEST
            task_result = self._run_download_task(task)
            if task_result == FINISH_ON_ERROR:
                return FINISH_ON_ERROR
        # try again
        self.debug_message(
            110, "worker _iterate_input_queue() called with empty input queue"
        )
        return CONTINUE

    def _run_download_task(self, task):
        """
        Execute a single download task.

        This method retrieves the object from storage and decides on the download
        strategy (in-memory, temp file, or stream) based on the object's size.

        Args:
            task (dict): The task to be processed.
        """
        self.debug_message(
            110,
            f"_run_download_task(): trying to get object data for {task['bucket']}:{task['name']}",
        )
        try:
            obj = self.driver.get_object(task["bucket"], task["name"])
            self.debug_message(
                110,
                f"_run_download_task(): successfully got object data for {task['bucket']}:{task['name']}",
            )

        except ObjectDoesNotExistError as e:
            self.error_message(
                f"Could not get file object, skipping: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()
        except requests.exceptions.ConnectionError as e:
            self.error_message(
                f"Connection error while getting file object: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self.error_message(
                f"Exception while getting file object: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()

        action = CONTINUE

        if task["type"] == TaskType.ACCURATE:
            action = self._put_accurate_object_into_queue(task, obj)
        elif task["size"] < self.options["prefetch_inmemory_size"]:
            action = self._download_object_into_queue(task, obj)
        elif task["size"] < self.options["prefetch_size"]:
            action = self._download_object_into_tempfile(task, obj)
        else:  # files larger than self.options["prefetch_size"]
            action = self._put_stream_object_into_queue(task, obj)

        return action

    def _download_object_into_queue(self, task, obj):
        """
        Download a small object entirely into memory and put it in the output queue.

        Args:
            task (dict): The task details.
            obj (StorageObject): The libcloud storage object to download.

        Returns:
            int: A status code (CONTINUE on success, on error depending on the
            fail_on_download_error option returns FINISH_ON_ERROR or CONTINUE).
        """
        try:
            self.debug_message(
                110,
                f"Put complete file {task_object_full_name(task)} into queue",
            )
            content = b"".join(obj.as_stream())
            self.debug_message(
                110,
                f"Successfully downloaded file {task_object_full_name(task)} into memory",
            )

            size_of_fetched_object = len(content)
            if size_of_fetched_object != task["size"]:
                self.error_message(
                    f"skipping prefetched object {task_object_full_name(task)} that has "
                    f"{size_of_fetched_object} bytes, not {task['size']} bytes as stated before",
                )
                return self._fail_job_or_continue_running()

            task["data"] = io.BytesIO(content)
            task["type"] = TaskType.DOWNLOADED

            self.queue_try_put(self.output_queue, task)
            return CONTINUE

        except (LibcloudError, Exception) as e:
            self.error_message(
                f"Could not download file {task_object_full_name(task)}", e
            )
            return self._fail_job_or_continue_running()

    def _download_object_into_tempfile(self, task, obj):
        """
        Download a medium-sized object to a temporary file.

        Args:
            task (dict): The task details.
            obj (StorageObject): The libcloud storage object to download.

        Returns:
            int: A status code (CONTINUE on success, on error depending on the
            fail_on_download_error option returns FINISH_ON_ERROR or CONTINUE).
        """
        try:
            self.debug_message(
                110,
                f"Prefetch object to temp file {task_object_full_name(task)}",
            )
            tmpfilename = self.tmp_dir_path + "/" + str(uuid.uuid4())
            obj.download(tmpfilename)
            self.debug_message(
                110,
                f"Successfully downloaded file {task_object_full_name(task)} to temporary file",
            )

            task["data"] = None
            task["tmpfile"] = tmpfilename
            task["type"] = TaskType.TEMP_FILE
            if os.path.isfile(tmpfilename):
                self.queue_try_put(self.output_queue, task)
                return CONTINUE

        except OSError as e:
            self._silentremove(tmpfilename)
            self.error_message(
                f"Could not download to temporary file {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()
        except ObjectDoesNotExistError as e:
            self._silentremove(tmpfilename)
            self.error_message(f"Could not open object, skipping: {e.object_name}")
            return self._fail_job_or_continue_running()
        except LibcloudError as e:
            self._silentremove(tmpfilename)
            self.error_message(
                f"Error downloading object, skipping: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self._silentremove(tmpfilename)
            self.error_message(
                f"Error using temporary file, skipping: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()

    def _put_stream_object_into_queue(self, task, obj):
        """
        Prepare a large object for streaming and put it in the output queue.
        The object data itself is not downloaded here.

        Args:
            task (dict): The task details.
            obj (StorageObject): The libcloud storage object to be streamed.

        Returns:
            int: A status code (CONTINUE on success, on error depending on the
            fail_on_download_error option returns FINISH_ON_ERROR or CONTINUE).
        """
        try:
            self.debug_message(
                110,
                f"Prepare object as stream for download {task_object_full_name(task)}",
            )
            task["data"] = obj
            task["type"] = TaskType.STREAM
            self.queue_try_put(self.output_queue, task)
            return CONTINUE
        except LibcloudError as e:
            self.error_message(
                f"Libcloud error preparing stream object, skipping: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self.error_message(
                f"Error preparing stream object, skipping: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()

    def _put_accurate_object_into_queue(self, task, obj):
        """
        Put an object into the queue for 'accurate' mode processing.
        The object data is not downloaded.

        Args:
            task (dict): The task details.
            obj (StorageObject): The libcloud storage object.

        Returns:
            int: A status code (CONTINUE on success, on error depending on the
            fail_on_download_error option returns FINISH_ON_ERROR or CONTINUE).
        """
        try:
            self.debug_message(
                110,
                f"Prepare object as accurate for download {task_object_full_name(task)}",
            )
            task["data"] = obj
            task["type"] = TaskType.ACCURATE
            self.queue_try_put(self.output_queue, task)
            return CONTINUE
        except LibcloudError as e:
            self.error_message(
                f"Libcloud error preparing accurate object, skipping: "
                f"{task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self.error_message(
                f"Error preparing accurate object, skipping: {task_object_full_name(task)}",
                e,
            )
            return self._fail_job_or_continue_running()

    def _fail_job_or_continue_running(self):
        """
        Decide whether to terminate the job or continue based on the
        'fail_on_download_error' option.

        Returns:
            int: FINISH_ON_ERROR if the job should fail, otherwise CONTINUE.
        """
        if self.fail_on_download_error:
            return FINISH_ON_ERROR

        return CONTINUE

    def _silentremove(self, filename):
        """
        Silently remove a file.

        Args:
            filename (str): The path to the file to be removed.
        """
        try:
            pathlib.Path(filename).unlink(missing_ok=True)
        except Exception as e:
            self.debug_message(
                100,
                f"Unexpected {type(e).__name__} exception when removing {filename}: {e}",
            )
