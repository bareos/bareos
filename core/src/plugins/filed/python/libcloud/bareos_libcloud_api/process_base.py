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

from multiprocessing import Process, Queue, Event
from time import sleep
from bareos_libcloud_api.queue_message import InfoMessage
from bareos_libcloud_api.queue_message import ReadyMessage
from bareos_libcloud_api.queue_message import ErrorMessage
from bareos_libcloud_api.queue_message import AbortMessage
from bareos_libcloud_api.queue_message import DebugMessage
from bareos_libcloud_api.queue_message import MESSAGE_TYPE

try:
    import Queue as Q
except ImportError:
    import queue as Q


class ProcessBase(Process):
    def __init__(
        self, worker_id, message_queue,
    ):
        super(ProcessBase, self).__init__()
        self.shutdown_event = Event()
        self.message_queue = message_queue
        self.worker_id = worker_id

    def run_process(self):
        # implementation of derived class
        pass

    def run(self):
        self.run_process()
        self.ready_message()
        self.wait_for_shutdown()

    def shutdown(self):
        self.shutdown_event.set()

    def wait_for_shutdown(self):
        self.shutdown_event.wait()

    def info_message(self, message):
        self.message_queue.put(InfoMessage(self.worker_id, message))

    def error_message(self, message):
        self.message_queue.put(ErrorMessage(self.worker_id, message))

    def debug_message(self, level, message):
        self.message_queue.put(DebugMessage(self.worker_id, level, message))

    def ready_message(self):
        self.message_queue.put(ReadyMessage(self.worker_id))

    def abort_message(self):
        self.message_queue.put(AbortMessage(self.worker_id))

    def queue_try_put(self, queue, obj):
        while not self.shutdown_event.is_set():
            try:
                queue.put(obj, block=True, timeout=0.5)
                return
            except Q.Full:
                self.debug_message(400, "Queue %s is full, trying again.." % queue)
                continue
