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


class BareosLibcloudApi(object):
    @staticmethod
    def probe_driver(options):
        driver = get_driver(options)
        if driver != None:
            return "success"
        return "failed"

    def __init__(self, options, last_run, tmp_dir_path):
        self.tmp_dir_path = tmp_dir_path
        self.count_worker_ready = 0
        self.count_bucket_explorer_ready = 0

        self.number_of_worker = options["nb_worker"] if options["nb_worker"] > 0 else 1

        queue_size = options["queue_size"] if options["queue_size"] > 0 else 1000
        self.discovered_objects_queue = Queue(queue_size)
        self.downloaded_objects_queue = Queue(queue_size)
        self.message_queue = Queue()

        self.__create_tmp_dir()

        self.bucket_explorer = BucketExplorer(
            options,
            last_run,
            self.message_queue,
            self.discovered_objects_queue,
            self.number_of_worker,
        )

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

        self.bucket_explorer.start()

        for w in self.worker:
            w.start()

    def worker_ready(self):
        return self.count_worker_ready == self.number_of_worker

    def bucket_explorer_ready(self):
        return self.count_bucket_explorer_ready == 1

    def check_worker_messages(self):
        while not self.message_queue.empty():
            try:
                message = self.message_queue.get_nowait()
                if message.type == MESSAGE_TYPE.INFO_MESSAGE:
                    jobmessage("M_INFO", message.message_string)
                elif message.type == MESSAGE_TYPE.ERROR_MESSAGE:
                    jobmessage("M_ERROR", message.message_string)
                elif message.type == MESSAGE_TYPE.READY_MESSAGE:
                    if message.worker_id == 0:
                        self.count_bucket_explorer_ready += 1
                    else:
                        self.count_worker_ready += 1
                elif message.type == MESSAGE_TYPE.ABORT_MESSAGE:
                    return ERROR
                elif message.type == MESSAGE_TYPE.DEBUG_MESSAGE:
                    debugmessage(message.level, message.message_string)
                else:
                    raise Exception(value="Unknown message type")
            except Exception as e:
                jobmessage("M_INFO", "check_worker_messages exception: %s" % e)
        return SUCCESS

    def get_next_task(self):
        if not self.downloaded_objects_queue.empty():
            return self.downloaded_objects_queue.get_nowait()
        return None

    def shutdown(self):
        debugmessage(100, "Shut down worker")
        self.bucket_explorer.shutdown()
        for w in self.worker:
            w.shutdown()

        debugmessage(100, "Wait for worker")
        while not self.bucket_explorer_ready():
            self.check_worker_messages()
        while not self.worker_ready():
            self.check_worker_messages()
        debugmessage(100, "Worker finished")

        while not self.discovered_objects_queue.empty():
            self.discovered_objects_queue.get()

        while not self.downloaded_objects_queue.empty():
            self.downloaded_objects_queue.get()

        while not self.message_queue.empty():
            self.message_queue.get()

        debugmessage(100, "Join worker processes")

        for w in self.worker:
            w.join(timeout=0.3)

        self.bucket_explorer.join(timeout=0.3)

        for w in self.worker:
            if w.is_alive():
                w.terminate()

        if self.bucket_explorer.is_alive():
            self.bucket_explorer.terminate()

        try:
            self.__remove_tmp_dir()
        except:
            pass

        jobmessage("M_INFO", "Shutdown of worker processes is ready")

    def __create_tmp_dir(self):
        try:
            self.__remove_tmp_dir()
        except:
            pass
        os.mkdir(self.tmp_dir_path)

    def __remove_tmp_dir(self):
        try:
            files = glob.glob(self.tmp_dir_path + "/*")
            for f in files:
                os.remove(f)
            os.rmdir(self.tmp_dir_path)
        except:
            pass

    def __del__(self):
        try:
            self.__remove_tmp_dir()
        except:
            pass
