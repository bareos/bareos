#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

from bareos_libcloud_api.bucket_explorer import TASK_TYPE
from bareos_libcloud_api.process_base import ProcessBase
from bareos_libcloud_api.get_libcloud_driver import get_driver
import io
from libcloud.common.types import LibcloudError
from libcloud.storage.types import ObjectDoesNotExistError
from bareos_libcloud_api.utils import silentremove
import requests.exceptions
from time import sleep
import uuid
import os

CONTINUE = 0
FINISH_ON_ERROR = 1
FINISH_ON_REQUEST = 2


def task_object_full_name(task):
    return task["bucket"] + "/" + task["name"]


class Worker(ProcessBase):
    def __init__(
        self,
        options,
        worker_id,
        tmp_dir_path,
        message_queue,
        discovered_objects_queue,
        downloaded_objects_queue,
    ):
        super(Worker, self).__init__(
            worker_id,
            message_queue,
        )
        self.options = options
        self.tmp_dir_path = tmp_dir_path
        self.input_queue = discovered_objects_queue
        self.output_queue = downloaded_objects_queue
        self.fail_on_download_error = bool(
            options["fail_on_download_error"]
            if "fail_on_download_error" in options
            else 0
        )

    def run_process(self):
        error = ""
        try:
            self.driver = get_driver(self.options)
        except Exception as e:
            error = "(%s)" % str(e)
            pass

        if self.driver == None:
            self.error_message("Could not load driver %s" % error)
            self.abort_message()
            return

        status = CONTINUE
        while status == CONTINUE and not self.shutdown_event.is_set():
            status = self._iterate_input_queue()
        if status == FINISH_ON_ERROR:
            self.abort_message()

    def _iterate_input_queue(self):
        while not self.input_queue.empty():
            if self.shutdown_event.is_set():
                return FINISH_ON_REQUEST
            task = self.input_queue.get()
            if task == None:  # poison pill
                return FINISH_ON_REQUEST
            if self._run_download_task(task) == FINISH_ON_ERROR:
                return FINISH_ON_ERROR
        # try again
        return CONTINUE

    def _run_download_task(self, task):
        try:
            obj = self.driver.get_object(task["bucket"], task["name"])

        except ObjectDoesNotExistError as e:
            self.error_message(
                "Could not get file object, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()
        except requests.exceptions.ConnectionError as e:
            self.error_message(
                "Connection error while getting file object, %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self.error_message(
                "Exception while getting file object, %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()

        action = CONTINUE

        if task["type"] == TASK_TYPE.ACCURATE:
            action = self._put_accurate_object_into_queue(task, obj)
        elif task["size"] < 1024 * 10:
            action = self._download_object_into_queue(task, obj)
        elif task["size"] < self.options["prefetch_size"]:
            action = self._download_object_into_tempfile(task, obj)
        else:  # files larger than self.options["prefetch_size"]
            action = self._put_stream_object_into_queue(task, obj)

        return action

    def _download_object_into_queue(self, task, obj):
        try:
            self.debug_message(
                110,
                "Put complete file %s into queue" % (task_object_full_name(task)),
            )
            stream = obj.as_stream()
            content = b"".join(list(stream))

            size_of_fetched_object = len(content)
            if size_of_fetched_object != task["size"]:
                self.error_message(
                    "skipping prefetched object %s that has %d bytes, "
                    "not %d bytes as stated before"
                    % (
                        task_object_full_name(task),
                        size_of_fetched_object,
                        task["size"],
                    ),
                )
                return self._fail_job_or_continue_running()

            task["data"] = io.BytesIO(content)
            task["type"] = TASK_TYPE.DOWNLOADED

            self.queue_try_put(self.output_queue, task)
            return CONTINUE

        except (LibcloudError, Exception) as e:
            self.error_message(
                "Could not download file %s" % (task_object_full_name(task)), e
            )
            return self._fail_job_or_continue_running()

    def _download_object_into_tempfile(self, task, obj):
        try:
            self.debug_message(
                110,
                "Prefetch object to temp file %s" % (task_object_full_name(task)),
            )
            tmpfilename = self.tmp_dir_path + "/" + str(uuid.uuid4())
            obj.download(tmpfilename)
            task["data"] = None
            task["tmpfile"] = tmpfilename
            task["type"] = TASK_TYPE.TEMP_FILE
            if os.path.isfile(tmpfilename):
                self.queue_try_put(self.output_queue, task)
                return CONTINUE
            else:
                self.error_message(
                    "Error downloading object, skipping: %s"
                    % (task_object_full_name(task))
                )
                return self._fail_job_or_continue_running()
        except OSError as e:
            silentremove(tmpfilename)
            self.error_message(
                "Could not download to temporary file %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()
        except ObjectDoesNotExistError as e:
            silentremove(tmpfilename)
            self.error_message("Could not open object, skipping: %s" % e.object_name)
            return self._fail_job_or_continue_running()
        except LibcloudError as e:
            silentremove(tmpfilename)
            self.error_message(
                "Error downloading object, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            silentremove(tmpfilename)
            self.error_message(
                "Error using temporary file, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()

    def _put_stream_object_into_queue(self, task, obj):
        try:
            self.debug_message(
                110,
                "Prepare object as stream for download %s"
                % (task_object_full_name(task)),
            )
            task["data"] = obj
            task["type"] = TASK_TYPE.STREAM
            self.queue_try_put(self.output_queue, task)
            return CONTINUE
        except LibcloudError as e:
            self.error_message(
                "Libcloud error preparing stream object, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self.error_message(
                "Error preparing stream object, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()

    def _put_accurate_object_into_queue(self, task, obj):
        try:
            self.debug_message(
                110,
                "Prepare object as accurate for download %s"
                % (task_object_full_name(task)),
            )
            task["data"] = obj
            task["type"] = TASK_TYPE.ACCURATE
            self.queue_try_put(self.output_queue, task)
            return CONTINUE
        except LibcloudError as e:
            self.error_message(
                "Libcloud error preparing accurate object, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()
        except Exception as e:
            self.error_message(
                "Error preparing accurate object, skipping: %s"
                % (task_object_full_name(task)),
                e,
            )
            return self._fail_job_or_continue_running()

    def _fail_job_or_continue_running(self):
        if self.fail_on_download_error:
            return FINISH_ON_ERROR
        return CONTINUE
