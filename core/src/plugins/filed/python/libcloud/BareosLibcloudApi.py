#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

import os
import sys
import uuid
import pathlib
import time

from enum import Enum
from queue import Queue
from threading import Event


import bareosfd
from bareos_libcloud_api.bucket_explorer import BucketExplorer
from bareos_libcloud_api.debug import debugmessage, jobmessage

from bareos_libcloud_api.queue_message import (
    InfoMessage,
    ReadyMessage,
    ErrorMessage,
    AbortMessage,
    DebugMessage,
)

from bareos_libcloud_api.worker import Worker
from bareos_libcloud_api.streamer import Streamer
from bareos_libcloud_api.process_base import ThreadType


class WorkerState(Enum):
    """
    Enumeration for possible worker states.

    Attributes:
        SUCCESS: Worker in successful state.
        ERROR: Worker encountered an error.
        ABORT: Worker requested to be aborted.
    """

    SUCCESS = 0
    ERROR = 1
    ABORT = 2


class BareosLibcloudApi:
    """
    API class for the Bareos libcloud plugin.
    Handles worker management, communication with the Libcloud API and temporary data cleanup.
    """

    @staticmethod
    def redirect_stdout_stderr():
        """
        Redirect sys.stdout and sys.stderr to /dev/null.
        """
        if sys.stdout is None:
            sys.stdout.close()
        if sys.stderr is None:
            sys.stderr.close()
        sys.stdout = open("/dev/null", "w", encoding="utf-8")
        sys.stderr = open("/dev/null", "w", encoding="utf-8")

    def __init__(self, options, last_run, tmp_dir_path):
        """
        Initialize the BareosLibcloudApi instance, set up queues, workers, and temporary directory.
        """
        self.options = options
        self.tmp_dir_path = tmp_dir_path + "/" + str(uuid.uuid4())
        self.count_worker_ready = 0
        self.count_bucket_explorer_ready = 0
        self.count_streamer_ready = 0
        self.last_worker_message_time = int(time.time())

        self.number_of_worker = options["nb_worker"] if options["nb_worker"] > 0 else 1

        queue_size = options["queue_size"] if options["queue_size"] > 0 else 1000
        self.discovered_objects_queue = Queue(maxsize=queue_size)
        self.downloaded_objects_queue = Queue(maxsize=queue_size)
        self.message_queue = Queue()
        self.bucket_explorer_queue_filled_event = Event()

        self.stream_pipe_thread = None

        self._create_tmp_dir()

        jobmessage(bareosfd.M_INFO, "Initialize BucketExplorer")

        self.bucket_explorer = BucketExplorer(
            options,
            last_run,
            self.message_queue,
            self.discovered_objects_queue,
            self.number_of_worker,
            self.bucket_explorer_queue_filled_event,
        )

        jobmessage(bareosfd.M_INFO, f"Initialize {self.number_of_worker} Workers")

        self.worker = [
            Worker(
                options,
                i,
                self.tmp_dir_path,
                self.message_queue,
                self.discovered_objects_queue,
                self.downloaded_objects_queue,
                self.bucket_explorer_queue_filled_event,
            )
            for i in range(self.number_of_worker)
        ]

        try:
            BareosLibcloudApi.redirect_stdout_stderr()
        except Exception as e:
            jobmessage(
                bareosfd.M_ERROR,
                f"Could not redirect stdout and/or stderr: {str(e)}",
            )
            raise

        jobmessage(bareosfd.M_INFO, "Starting BucketExplorer")
        try:
            self.bucket_explorer.start()
        except Exception as e:
            jobmessage(bareosfd.M_ERROR, f"Start BucketExplorer failed: {str(e)}")
            raise

        jobmessage(bareosfd.M_INFO, "Starting Workers")
        try:
            for w in self.worker:
                w.start()
        except Exception as e:
            jobmessage(bareosfd.M_ERROR, f"Start Worker failed: {str(e)}")
            raise

    def worker_ready(self):
        """
        Return True if all workers are ready.
        """
        debugmessage(
            120,
            f"worker_ready(): count_worker_ready: {self.count_worker_ready}, "
            f"number_of_worker: {self.number_of_worker}",
        )
        return self.count_worker_ready == self.number_of_worker

    def bucket_explorer_ready(self):
        """
        Return True if the bucket explorer is ready.
        """
        return self.count_bucket_explorer_ready == 1

    def check_worker_messages(self):
        """
        Process and handle messages from workers and the bucket explorer.
        Returns WorkerState.SUCCESS or WorkerState.ABORT.
        """
        while not self.message_queue.empty():
            self.last_worker_message_time = int(time.time())
            try:
                message = self.message_queue.get_nowait()
                message_worker_id = ""
                if message.thread_type is ThreadType.WORKER:
                    message_worker_id = f"({message.worker_id})"
                message_prefix = f"{message.thread_type.name}{message_worker_id}"
                message_text = f"{message_prefix}: {message.message_string}"
                if isinstance(message, InfoMessage):
                    jobmessage(bareosfd.M_INFO, message_text)
                elif isinstance(message, ErrorMessage):
                    jobmessage(bareosfd.M_ERROR, message_text)
                elif isinstance(message, ReadyMessage):
                    if message.thread_type is ThreadType.BUCKET_EXPLORER:
                        self.count_bucket_explorer_ready += 1
                    elif message.thread_type is ThreadType.STREAMER:
                        self.count_streamer_ready += 1
                    else:
                        self.count_worker_ready += 1
                elif isinstance(message, AbortMessage):
                    return WorkerState.ABORT
                elif isinstance(message, DebugMessage):
                    debugmessage(message.level, message_text)
                else:
                    raise Exception(value="Unknown message type")
            except Exception as e:
                jobmessage(
                    bareosfd.M_INFO, f"check_worker_messages exception: {str(e)}"
                )
        return WorkerState.SUCCESS

    def get_next_task(self):
        """
        Get the next downloaded object from the queue and return it, or return None if the
        queue is empty.
        """
        if not self.downloaded_objects_queue.empty():
            return self.downloaded_objects_queue.get_nowait()
        return None

    def start_stream_pipe(self, storage_obj):
        """
        Start streaming pipe for the given storage object.

        Creates a pipe and starts a thread for streaming download of the object
        writing to the pipe. Returns the file handle of the read end of the pipe.
        """
        pipe_read_fd, pipe_write_fd = os.pipe()
        self.stream_pipe_thread = Streamer(
            self.options,
            storage_obj,
            pipe_write_fd,
            self.message_queue,
        )
        self.stream_pipe_thread.start()
        read_file_handle = os.fdopen(pipe_read_fd, "rb")
        return read_file_handle

    def end_stream_pipe(self):
        """
        End streaming pipe.
        """
        self.stream_pipe_thread.shutdown()
        self.stream_pipe_thread.join()
        self.stream_pipe_thread = None

    def shutdown(self):
        """
        Shut down all workers and the bucket explorer, clean up queues and temporary files.
        """
        jobmessage(bareosfd.M_INFO, "Shut down bucket explorer")
        self.bucket_explorer.shutdown()
        jobmessage(bareosfd.M_INFO, "Shut down worker")
        try:
            for w in self.worker:
                w.shutdown()
        except Exception as e:
            jobmessage(bareosfd.M_INFO, f"Shut down worker failed: {str(e)}")

        jobmessage(bareosfd.M_INFO, "Wait for worker")
        worker_wait_start = time.time()
        while not self.bucket_explorer_ready():
            self.check_worker_messages()
        while not self.worker_ready():
            self.check_worker_messages()
            # with very bad network connection, workers can get stuck in libcloud calls
            if time.time() - worker_wait_start > 60:
                debugmessage(
                    100, "Shut down worker timeout waiting for workers to finish"
                )
                break

        jobmessage(bareosfd.M_INFO, "Workers finished")

        while not self.discovered_objects_queue.empty():
            self.discovered_objects_queue.get()

        while not self.downloaded_objects_queue.empty():
            self.downloaded_objects_queue.get()

        while not self.message_queue.empty():
            self.message_queue.get()

        jobmessage(bareosfd.M_INFO, "Shutdown worker threads")

        for w in self.worker:
            w.join(timeout=0.3)

        self.bucket_explorer.join(timeout=0.3)

        for w in self.worker:
            if w.is_alive():
                debugmessage(
                    100, f"Worker {w.worker_id} still alive, trying to join again"
                )
                w.join(timeout=1)

        if self.bucket_explorer.is_alive():
            debugmessage(100, "BucketExplorer still alive, trying to join again")
            self.bucket_explorer.join(timeout=10.0)

        try:
            self._remove_tmp_dir()
        except Exception as e:
            debugmessage(100, f"Failed to remove temporary directory: {str(e)}")

        jobmessage(bareosfd.M_INFO, "Finished shutdown of worker threads")

    def _create_tmp_dir(self):
        """
        Create the temporary directory for worker files.

        If the directory already exists, no exception is raised due to exist_ok=True.
        Any exceptions raised by os.makedirs (other than directory exists) will emit
        a jobmessage of type M_FATAL.
        """
        debugmessage(100, f"Try to create temporary directory: {self.tmp_dir_path}")
        try:
            os.makedirs(self.tmp_dir_path, exist_ok=True)
            debugmessage(100, f"Created temporary directory: {self.tmp_dir_path}")
        except Exception as e:
            jobmessage(
                bareosfd.M_FATAL,
                f"Failed to create temporary directory {self.tmp_dir_path}: {e}",
            )

    def _remove_tmp_dir(self):
        """
        Remove all files in the temporary directory and the directory itself.
        Emits a warning jobmessage if any files could not be removed.
        """
        debugmessage(100, f"Try to remove leftover files from: {self.tmp_dir_path}")
        tmp_path = pathlib.Path(self.tmp_dir_path)
        if not tmp_path.exists():
            debugmessage(
                100, f"Temporary directory does not exist: {self.tmp_dir_path}"
            )
            return

        cnt = 0
        had_exception = False
        for file in tmp_path.iterdir():
            if file.is_file():
                try:
                    file.unlink()
                    cnt += 1
                except Exception as e:
                    debugmessage(100, f"Failed to remove file {file}: {e}")
                    had_exception = True
        if cnt != 0:
            debugmessage(
                100,
                f"Removed {cnt} leftover files from: {self.tmp_dir_path}",
            )
        else:
            debugmessage(
                100,
                f"No leftover files to remove from: {self.tmp_dir_path}",
            )

        if had_exception:
            jobmessage(
                bareosfd.M_WARNING,
                f"Some files could not be removed from temporary directory: {self.tmp_dir_path}",
            )

        try:
            tmp_path.rmdir()
            debugmessage(100, f"Removed temporary directory: {self.tmp_dir_path}")
        except Exception as e:
            debugmessage(
                100, f"Failed to remove temporary directory {self.tmp_dir_path}: {e}"
            )
            jobmessage(
                bareosfd.M_WARNING,
                f"Could not remove temporary directory {self.tmp_dir_path}: {str(e)}",
            )

    def disbled__del__(self):
        """
        Destructor: close sys.stdout/sys.stderr.
        """
        debugmessage(100, "Entering BareosLibcloudApi.__del__")
        # Note that certain operations like self._remove_tmp_dir() again from this destructor can cause
        # exceptions like this:
        # ImportError: sys.meta_path is None, Python is likely shutting down
        try:
            if sys.stdout is not None:
                sys.stdout.close()
            if sys.stderr is not None:
                sys.stderr.close()
        except Exception as e:
            debugmessage(
                100,
                f"Exception {type(e).__name__} in "
                f"BareosLibcloudApi.__del__: {str(e)}",
            )
