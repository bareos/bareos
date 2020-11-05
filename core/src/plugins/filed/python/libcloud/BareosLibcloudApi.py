#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

from bareosfd import M_INFO, M_ERROR
from bareos_libcloud_api.bucket_explorer import BucketExplorer
from bareos_libcloud_api.bucket_explorer import TASK_TYPE
from bareos_libcloud_api.debug import debugmessage, jobmessage
from bareos_libcloud_api.get_libcloud_driver import get_driver
from bareos_libcloud_api.get_libcloud_driver import options
from bareos_libcloud_api.mtime import ModificationTime
from bareos_libcloud_api.queue_message import MESSAGE_TYPE
from bareos_libcloud_api.worker import Worker
from libcloud.common.types import LibcloudError
from multiprocessing import Queue, Event
from time import sleep

import glob
import io
import libcloud
import os
import sys
import uuid

SUCCESS = 0
ERROR = 1
ABORT = 2


class BareosLibcloudApi(object):
    @staticmethod
    def probe_driver(options):
        driver = get_driver(options)
        if driver != None:
            return "success"
        return "failed"

    def __init__(self, options, last_run, tmp_dir_path):
        self.tmp_dir_path = tmp_dir_path + "/" + str(uuid.uuid4())
        self.count_worker_ready = 0
        self.count_bucket_explorer_ready = 0

        self.number_of_worker = options["nb_worker"] if options["nb_worker"] > 0 else 1

        queue_size = options["queue_size"] if options["queue_size"] > 0 else 1000
        self.discovered_objects_queue = Queue(queue_size)
        self.downloaded_objects_queue = Queue(queue_size)
        self.message_queue = Queue()

        self._create_tmp_dir()

        jobmessage(M_INFO, "Initialize BucketExplorer")

        self.bucket_explorer = BucketExplorer(
            options,
            last_run,
            self.message_queue,
            self.discovered_objects_queue,
            self.number_of_worker,
        )

        jobmessage(M_INFO, "Initialize %d Workers" % self.number_of_worker)

        self.worker = [
            Worker(
                options,
                i + 1,
                self.tmp_dir_path,
                self.message_queue,
                self.discovered_objects_queue,
                self.downloaded_objects_queue,
            )
            for i in range(self.number_of_worker)
        ]

        jobmessage(M_INFO, "Starting BucketExplorer")
        try:
            self.bucket_explorer.start()
        except Exception as e:
            jobmessage(M_ERROR, "Start BucketExplorer failed: %s" % (str(e)))
            raise

        jobmessage(M_INFO, "Starting Workers")
        try:
            for w in self.worker:
                w.start()
        except Exception as e:
            jobmessage(M_ERROR, "Start Worker failed: %s" % (str(e)))
            raise

    def worker_ready(self):
        return self.count_worker_ready == self.number_of_worker

    def bucket_explorer_ready(self):
        return self.count_bucket_explorer_ready == 1

    def check_worker_messages(self):
        while not self.message_queue.empty():
            try:
                message = self.message_queue.get_nowait()
                message_text = ("%3d: %s") % (
                    message.worker_id,
                    message.message_string,
                )
                if message.type == MESSAGE_TYPE.INFO_MESSAGE:
                    jobmessage(M_INFO, message_text)
                elif message.type == MESSAGE_TYPE.ERROR_MESSAGE:
                    jobmessage(M_ERROR, message_text)
                elif message.type == MESSAGE_TYPE.READY_MESSAGE:
                    if message.worker_id == 0:
                        self.count_bucket_explorer_ready += 1
                    else:
                        self.count_worker_ready += 1
                elif message.type == MESSAGE_TYPE.ABORT_MESSAGE:
                    return ABORT
                elif message.type == MESSAGE_TYPE.DEBUG_MESSAGE:
                    debugmessage(message.level, message_text)
                else:
                    raise Exception(value="Unknown message type")
            except Exception as e:
                jobmessage(M_INFO, "check_worker_messages exception: %s" % str(e))
        return SUCCESS

    def get_next_task(self):
        if not self.downloaded_objects_queue.empty():
            return self.downloaded_objects_queue.get_nowait()
        return None

    def shutdown(self):
        jobmessage(M_INFO, "Shut down bucket explorer")
        self.bucket_explorer.shutdown()
        jobmessage(M_INFO, "Shut down worker")
        try:
            for w in self.worker:
                w.shutdown()
        except:
            jobmessage(M_INFO, "Shut down worker failed")

        jobmessage(M_INFO, "Wait for worker")
        while not self.bucket_explorer_ready():
            self.check_worker_messages()
        while not self.worker_ready():
            self.check_worker_messages()
        jobmessage(M_INFO, "Worker finished")

        while not self.discovered_objects_queue.empty():
            self.discovered_objects_queue.get()

        while not self.downloaded_objects_queue.empty():
            self.downloaded_objects_queue.get()

        while not self.message_queue.empty():
            self.message_queue.get()

        jobmessage(M_INFO, "Join worker processes")

        for w in self.worker:
            w.join(timeout=0.3)

        self.bucket_explorer.join(timeout=0.3)

        for w in self.worker:
            if w.is_alive():
                w.terminate()

        if self.bucket_explorer.is_alive():
            self.bucket_explorer.terminate()

        try:
            self._remove_tmp_dir()
        except:
            pass

        jobmessage(M_INFO, "Finished shutdown of worker processes")

    def _create_tmp_dir(self):
        debugmessage(100, "Try to create temporary directory: %s" % (self.tmp_dir_path))
        os.makedirs(self.tmp_dir_path)
        debugmessage(100, "Created temporary directory: %s" % (self.tmp_dir_path))

    def _remove_tmp_dir(self):
        debugmessage(100, "Try to remove leftover files from: %s" % (self.tmp_dir_path))
        try:
            files = glob.glob(self.tmp_dir_path + "/*")
            cnt = 0
            for f in files:
                os.remove(f)
                cnt += 1
            if cnt != 0:
                debugmessage(
                    100,
                    "Removed %d leftover files from: %s" % (cnt, self.tmp_dir_path),
                )
            else:
                debugmessage(
                    100,
                    "No leftover files to remove from: %s" % (self.tmp_dir_path),
                )
            os.rmdir(self.tmp_dir_path)
            debugmessage(100, "Removed temporary directory: %s" % (self.tmp_dir_path))
        except:
            pass

    def __del__(self):
        try:
            self._remove_tmp_dir()
        except:
            pass
