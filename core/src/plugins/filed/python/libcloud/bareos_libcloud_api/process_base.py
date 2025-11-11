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

import traceback
from enum import Enum
from threading import Thread, Event
from bareos_libcloud_api.queue_message import (
    InfoMessage,
    ReadyMessage,
    ErrorMessage,
    AbortMessage,
    DebugMessage,
)

try:
    import Queue as queue
except ImportError:
    import queue


class ThreadType(Enum):
    """
    Enumeration for Thread Types.

    Attributes:
        BUCKET_EXPLORER: Bucket Explorer thread.
        STREAMER: Streamer thread.
        WORKER: Worker Thread.
    """

    BUCKET_EXPLORER = 0
    STREAMER = 1
    WORKER = 2


class ProcessBase(Thread):
    """
    Threaded base class replacing multiprocessing.Process.
    Provides the same public API (run, shutdown, info_message, etc.)
    so existing Worker/BucketExplorer code can inherit unchanged.
    """

    def __init__(
        self,
        worker_id,
        thread_type,
        message_queue,
    ):
        """
        Initialize the ProcessBase thread.

        Args:
            worker_id (int): The ID of the worker.
            message_queue (queue.Queue): The queue for sending messages.
        """
        super().__init__()
        self.shutdown_event = Event()
        self.message_queue = message_queue
        self.worker_id = worker_id
        self.thread_type = thread_type
        self.driver = None

    def run_process(self):
        """
        Main process logic to be implemented by derived classes.
        This method is called by the run() method.
        """

    def run(self):
        """
        The main entry point for the thread's activity.
        """
        try:
            self.run_process()
        except Exception as e:
            self.error_message(
                f"Exception in {self.thread_type.name} thread {self.worker_id}: {e}\n"
                f"{traceback.format_exc()}"
            )
            self.abort_message()
        self.ready_message()
        self.wait_for_shutdown()

    def shutdown(self):
        """
        Signal the thread to shut down.
        """
        self.shutdown_event.set()

    def wait_for_shutdown(self):
        """
        Block until the shutdown event is set.
        """
        self.shutdown_event.wait()

    def info_message(self, message):
        """
        Put an informational message into the message queue.

        Args:
            message (str): The message to send.
        """
        self.message_queue.put(InfoMessage(self.thread_type, self.worker_id, message))

    def error_message(self, message, exception=None):
        """
        Put an error message into the message queue.

        Args:
            message (str): The error message.
            exception (Exception, optional): The exception that occurred. Defaults to None.
        """
        s = self._format_exception_string(exception)
        self.message_queue.put(
            ErrorMessage(self.thread_type, self.worker_id, message + s)
        )

    def debug_message(self, level, message):
        """
        Put a debug message into the message queue.

        Args:
            level (int): The debug level.
            message (str): The debug message.
        """
        self.message_queue.put(
            DebugMessage(self.thread_type, self.worker_id, level, message)
        )

    def ready_message(self):
        """
        Put a ready message into the message queue, indicating the worker is finished.
        """
        self.message_queue.put(ReadyMessage(self.thread_type, self.worker_id))

    def abort_message(self):
        """
        Put an abort message into the message queue to signal a fatal error.
        """
        self.message_queue.put(AbortMessage(self.thread_type, self.worker_id))

    def queue_try_put(self, queue_obj, item):
        """
        Try to put an item into a queue, retrying if it is full.

        This method will block until the item is put into the queue or
        the shutdown event is set.

        Args:
            queue_obj (queue.Queue): The queue to put the item into.
            item: The item to put into the queue.
        """
        while not self.shutdown_event.is_set():
            try:
                queue_obj.put(item, block=True, timeout=0.5)
                return
            except queue.Full:
                self.debug_message(400, f"Queue {queue_obj} is full, trying again..")
                continue

    @staticmethod
    def _format_exception_string(exception):
        """
        Format an exception object into a string.

        Args:
            exception (Exception): The exception to format.

        Returns:
            str: The formatted exception string.
        """
        return f" ({exception})" if exception is not None else ""
