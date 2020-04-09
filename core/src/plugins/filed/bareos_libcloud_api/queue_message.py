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

class MESSAGE_TYPE(object):
    INFO_MESSAGE = 1
    ERROR_MESSAGE = 2
    ABORT_MESSAGE = 3
    READY_MESSAGE = 4
    DEBUG_MESSAGE = 5

    def __setattr__(self, *_):
        raise Exception("class MESSAGE_TYPE is read only")


class QueueMessageBase(object):
    def __init__(self, worker_id, message):
        self.worker_id = worker_id
        self.message_string = message
        self.type = None


class ErrorMessage(QueueMessageBase):
    def __init__(self, worker_id, message):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.ERROR_MESSAGE


class InfoMessage(QueueMessageBase):
    def __init__(self, worker_id, message):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.INFO_MESSAGE


class DebugMessage(QueueMessageBase):
    def __init__(self, worker_id, level, message):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.DEBUG_MESSAGE
        self.level = level


class ReadyMessage(QueueMessageBase):
    def __init__(self, worker_id, message=None):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.READY_MESSAGE


class AbortMessage(QueueMessageBase):
    def __init__(self, worker_id):
        QueueMessageBase.__init__(self, worker_id, None)
        self.type = MESSAGE_TYPE.ABORT_MESSAGE
