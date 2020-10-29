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

FINISH = 0
CONTINUE = 1


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
            worker_id, message_queue,
        )
        self.options = options
        self.tmp_dir_path = tmp_dir_path
        self.input_queue = discovered_objects_queue
        self.output_queue = downloaded_objects_queue

    def run_process(self):
        self.driver = get_driver(self.options)
        if self.driver == None:
            self.error_message("Could not load driver")
            self.abort_message()
            return

        status = CONTINUE
        while status != FINISH and not self.shutdown_event.is_set():
            status = self.__iterate_input_queue()

    def __iterate_input_queue(self):
        while not self.input_queue.empty():
            if self.shutdown_event.is_set():
                return FINISH
            task = self.input_queue.get()
            if task == None:  # poison pill
                return FINISH
            if self.__run_download_task(task) == FINISH:
                return FINISH
        # try again
        return CONTINUE

    def __run_download_task(self, task):
        try:
            obj = self.driver.get_object(task["bucket"], task["name"])

        except ObjectDoesNotExistError:
            self.error_message(
                "Could not get file object, skipping: %s"
                % (task_object_full_name(task))
            )
            return CONTINUE
        except requests.exceptions.ConnectionError as e:
            self.error_message(
                "Connection error while getting file object, %s (%s)"
                % (task_object_full_name(task), str(e))
            )
            return CONTINUE
        except Exception as e:
            self.error_message(
                "Exception while getting file object, %s (%s)"
                % (task_object_full_name(task), str(e))
            )
            return CONTINUE

        action = CONTINUE

        if task["size"] < 1024 * 10:
            action = self.__prefetch_file_into_queue(task, obj)
        elif task["size"] < self.options["prefetch_size"]:
            action = self.__download_object_into_tempfile(task, obj)
        else:  # files larger than self.options["prefetch_size"]
            action = self.__put_stream_object_into_queue(task, obj)

        return action

    def __prefetch_file_into_queue(self, task, obj):
        try:
            self.debug_message(
                110,
                "%3d: Put complete file %s into queue"
                % (self.worker_id, task_object_full_name(task)),
            )
            stream = obj.as_stream()
            content = b"".join(list(stream))

            size_of_fetched_object = len(content)
            if size_of_fetched_object != task["size"]:
                self.error_message(
                    "prefetched file %s: got %s bytes, not the real size (%s bytes)"
                    % (
                        task_object_full_name(task),
                        size_of_fetched_object,
                        task["size"],
                    ),
                )
                return CONTINUE

            task["data"] = io.BytesIO(content)
            task["type"] = TASK_TYPE.DOWNLOADED

            self.queue_try_put(self.output_queue, task)
            return CONTINUE

        except (LibcloudError, Exception) as e:
            self.error_message(
                "Could not download file %s (%s)"
                % (task_object_full_name(task), str(e),)
            )
            return CONTINUE

    def __download_object_into_tempfile(self, task, obj):
        try:
            self.debug_message(
                110,
                "%3d: Prefetch file %s" % (self.worker_id, task_object_full_name(task)),
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
                return CONTINUE
        except (
            ConnectionError,
            TimeoutError,
            ConnectionResetError,
            ConnectionAbortedError,
        ) as e:
            self.error_message(
                "Connection error while downloading %s (%s)"
                % (task_object_full_name(task), str(e))
            )
            return CONTINUE
        except OSError as e:
            self.error_message(
                "Could not download to temporary file %s (%s)"
                % (task_object_full_name(task), str(e))
            )
            return CONTINUE
        except ObjectDoesNotExistError as e:
            silentremove(tmpfilename)
            self.error_message("Could not open object, skipping: %s" % e.object_name)
            return CONTINUE
        except LibcloudError:
            silentremove(tmpfilename)
            self.error_message(
                "Error downloading object, skipping: %s" % (task_object_full_name(task))
            )
            return CONTINUE
        except Exception:
            silentremove(tmpfilename)
            self.error_message(
                "Error using temporary file, skipping: %s"
                % (task_object_full_name(task))
            )
            return CONTINUE

    def __put_stream_object_into_queue(self, task, obj):
        try:
            self.debug_message(
                110,
                "%3d: Prepare file as stream for download %s"
                % (self.worker_id, task_object_full_name(task)),
            )
            task["data"] = obj
            task["type"] = TASK_TYPE.STREAM
            self.queue_try_put(self.output_queue, task)
            return CONTINUE
        except LibcloudError:
            self.error_message(
                "Libcloud error preparing stream object, skipping: %s"
                % (task_object_full_name(task))
            )
            return CONTINUE
        except Exception:
            self.error_message(
                "Error preparing stream object, skipping: %s"
                % (task_object_full_name(task))
            )
            return CONTINUE
