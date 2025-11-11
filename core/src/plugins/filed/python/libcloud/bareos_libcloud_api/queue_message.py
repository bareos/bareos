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


class QueueMessageBase:
    """
    Base class for queue messages.
    """

    def __init__(self, thread_type, worker_id, message):
        self.thread_type = thread_type
        self.worker_id = worker_id
        self.message_string = message


class ErrorMessage(QueueMessageBase):
    """
    Error message class.
    """

    def __init__(self, thread_type, worker_id, message):
        QueueMessageBase.__init__(self, thread_type, worker_id, message)


class InfoMessage(QueueMessageBase):
    """
    Info message class.
    """

    def __init__(self, thread_type, worker_id, message):
        QueueMessageBase.__init__(self, thread_type, worker_id, message)


class DebugMessage(QueueMessageBase):
    """
    Debug message class.
    """

    def __init__(self, thread_type, worker_id, level, message):
        QueueMessageBase.__init__(self, thread_type, worker_id, message)
        self.level = level


class ReadyMessage(QueueMessageBase):
    """
    Ready message class.
    """

    def __init__(self, thread_type, worker_id, message=None):
        QueueMessageBase.__init__(self, thread_type, worker_id, message)


class AbortMessage(QueueMessageBase):
    """
    Abort message class.
    """

    def __init__(self, thread_type, worker_id):
        QueueMessageBase.__init__(self, thread_type, worker_id, None)
