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
from time import sleep
import uuid

FINISH = 0
CONTINUE = 1


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
        while status != FINISH:
            status = self.__iterate_input_queue()

    def __iterate_input_queue(self):
        while not self.input_queue.empty():
            if self.shutdown_event.is_set():
                return FINISH
            job = self.input_queue.get()
            if job == None:  # poison pill
                return FINISH
            if self.__run_job(job) == FINISH:
                return FINISH
        # try again
        return CONTINUE

    def __run_job(self, job):
        success = False
        try:
            obj = self.driver.get_object(job["bucket"], job["name"])
        except ObjectDoesNotExistError:
            self.error_message(
                "Could not get file object, skipping: %s/%s"
                % (job["bucket"], job["name"])
            )
            return CONTINUE

        if job["size"] < 1024 * 10:
            try:
                self.debug_message(
                    110,
                    "%3d: Put complete file %s into queue"
                    % (self.worker_id, job["bucket"] + job["name"]),
                )
                stream = obj.as_stream()
                content = b"".join(list(stream))

                size_of_fetched_object = len(content)
                if size_of_fetched_object != job["size"]:
                    self.error_message(
                        "prefetched file %s: got %s bytes, not the real size (%s bytes)"
                        % (job["name"], size_of_fetched_object, job["size"]),
                    )
                    return CONTINUE

                job["data"] = io.BytesIO(content)
                job["type"] = TASK_TYPE.DOWNLOADED
                success = True
            except LibcloudError:
                self.error_message("Libcloud error, could not download file")
                return CONTINUE
            except Exception:
                self.error_message("Could not download file")
                return CONTINUE
        elif job["size"] < self.options["prefetch_size"]:
            try:
                self.debug_message(
                    110,
                    "%3d: Prefetch file %s"
                    % (self.worker_id, job["bucket"] + job["name"]),
                )
                tmpfilename = self.tmp_dir_path + "/" + str(uuid.uuid4())
                obj.download(tmpfilename)
                job["data"] = None
                job["tmpfile"] = tmpfilename
                job["type"] = TASK_TYPE.TEMP_FILE
                success = True
            except OSError as e:
                self.error_message("Could not open temporary file %s" % e.filename)
                self.abort_message()
                return FINISH
            except ObjectDoesNotExistError as e:
                silentremove(tmpfilename)
                self.error_message(
                    "Could not open object, skipping: %s" % e.object_name
                )
                return CONTINUE
            except LibcloudError:
                silentremove(tmpfilename)
                self.error_message(
                    "Error downloading object, skipping: %s/%s"
                    % (job["bucket"], job["name"])
                )
                return CONTINUE
            except Exception:
                silentremove(tmpfilename)
                self.error_message(
                    "Error using temporary file for, skipping: %s/%s"
                    % (job["bucket"], job["name"])
                )
                return CONTINUE
        else:
            try:
                self.debug_message(
                    110,
                    "%3d: Prepare file as stream for download %s"
                    % (self.worker_id, job["bucket"] + job["name"]),
                )
                job["data"] = obj
                job["type"] = TASK_TYPE.STREAM
                success = True
            except LibcloudError:
                self.error_message(
                    "Libcloud error preparing stream object, skipping: %s/%s"
                    % (job["bucket"], job["name"])
                )
                return CONTINUE
            except Exception:
                self.error_message(
                    "Error preparing stream object, skipping: %s/%s"
                    % (job["bucket"], job["name"])
                )
                return CONTINUE

        if success == True:
            self.queue_try_put(self.output_queue, job)

        return CONTINUE
