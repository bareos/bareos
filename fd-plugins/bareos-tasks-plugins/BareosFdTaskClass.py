#!/usr/bin/env python
# -*- Mode: Python; tab-width: 4 -*-
#
# Bareos FileDaemon Task plugin
# Copyright (C) 2018 Marco Lertora <marco.lertora@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import subprocess
from StringIO import StringIO
from fcntl import fcntl, F_GETFL, F_SETFL
from pwd import getpwnam

from bareosfd import JobMessage, DebugMessage, StatPacket, GetValue
from bareos_fd_consts import bRCs, bIOPS, bJobMessageType, bFileType, bVariable
from BareosFdPluginBaseclass import BareosFdPluginBaseclass


bJobType = dict(
    BACKUP=66,
    PREVIOUS=77,
    VERIFY=86,
    RESTORE=82,
    CONSOLE=99,
    COPY=67,
    INTERNAL=73,
    ADMIN=68,
    ARCHIVE=65,
    MIGRATION=103,
    SCAN=83,
)


class TaskException(Exception):
    pass


class Task(object):
    task_name = 'unknown'
    file_extension = 'dump'
    block_size = 65536
    unknown_size = 0
    pool_counter = 0
    pool_interval = 100

    def get_name(self):
        return self.task_name

    def get_details(self):
        return self.get_name()

    def get_next_tasks(self):
        return list()

    def get_filename(self):
        return '{0}.{1}'.format(self.get_name(), self.file_extension)

    def get_size(self):
        return self.unknown_size

    def get_block_size(self):
        return self.block_size

    def task_pool(self):
        self.pool_counter = (self.pool_counter % self.pool_interval) + 1
        if self.pool_counter == self.pool_interval:
            self.pool()

    def pool(self):
        return

    def task_open(self):
        return

    def task_read(self, buf):
        raise Exception('need to be override')

    def task_close(self):
        return

    def task_wait(self):
        return 0


class TaskStringIO(Task):
    file_extension = 'log'

    def __init__(self, task_name, data):
        self.task_name = task_name
        self.data = data

    def task_open(self):
        self.data.seek(0)

    def task_read(self, buf):
        chunk = self.data.read(len(buf))
        buf[:len(chunk)] = chunk
        return len(chunk)


class TaskProcess(Task):
    process = None
    run_as_user = None
    run_environ = dict()
    command = list()
    use_stdout = True
    use_stderr = True
    stderr_buffer = StringIO()

    def get_details(self):
        sep = ' '
        return '{0} {1}'.format(self.get_name(), sep.join(self.command))

    def get_next_tasks(self):
        tasks = list()
        if self.use_stderr:
            tasks.append(TaskStringIO(self.get_name() + '-stderr', self.stderr_buffer))
        return tasks

    def pre_run_execute(self):
        if self.run_as_user:
            item = getpwnam(self.run_as_user)
            os.chdir(item.pw_dir)
            os.setgid(item.pw_gid)
            os.setuid(item.pw_uid)

        for key, value in self.run_environ.items():
            os.environ[key] = value

    def execute_command(self, command):
        try:
            return subprocess.check_output(command, shell=False, bufsize=-1,
                                           preexec_fn=self.pre_run_execute)
        except (subprocess.CalledProcessError, OSError, ValueError), e:
            raise TaskException(e)

    def pool(self):
        if self.use_stderr:
            try:
                self.stderr_buffer.write(self.process.stderr.read())
            except IOError:
                pass

    def task_open(self):
        try:
            self.process = subprocess.Popen(self.command, shell=False, bufsize=-1,
                                            stdout=subprocess.PIPE if self.use_stdout else None,
                                            stderr=subprocess.PIPE if self.use_stderr else None,
                                            preexec_fn=self.pre_run_execute)
            if self.use_stderr:
                fcntl(self.process.stderr, F_SETFL, fcntl(self.process.stderr, F_GETFL) | os.O_NONBLOCK)
        except (subprocess.CalledProcessError, OSError, ValueError), e:
            raise TaskException('invalid command: {0} {1}'.format(self.command, e))

    def task_read(self, buf):
        if not self.process:
            raise TaskException('pipe closed')
        try:
            return self.process.stdout.readinto(buf)
        except IOError, e:
            raise TaskException(e)

    def task_wait(self):
        self.process.wait()
        code = self.process.poll()

        if code is None:
            return '[X] unknown error'

        if not code == 0:
            stdout, stderr = self.process.communicate()
            return '[{0}] {1}'.format(code, stderr.strip())


class TaskProcessFIFO(TaskProcess):
    fifo = None
    fifo_path = '/var/lib/bareos/task.fifo'
    use_stdout = False

    def make_fifo(self):
        if os.path.exists(self.fifo_path):
            os.remove(self.fifo_path)

        os.mkfifo(self.fifo_path)

        if self.run_as_user:
            item = getpwnam(self.run_as_user)
            os.chown(self.fifo_path, item.pw_uid, item.pw_gid)

    def task_open(self):
        self.make_fifo()
        super(TaskProcessFIFO, self).task_open()
        self.fifo = open(self.fifo_path, 'rb')

    def task_read(self, buf):
        if not self.fifo:
            raise TaskException('pipe closed')

        try:
            return self.fifo.readinto(buf)
        except IOError, e:
            raise TaskException(e)

    def task_close(self):
        super(TaskProcessFIFO, self).task_close()
        self.fifo.close()


class PluginConfig(dict):

    def get_boolean(self, key, default=False):
        value = self.get(key)
        if value is None:
            return default
        return value == 'yes'

    def get_list(self, key, default=list()):
        value = self.get(key)
        if value is None:
            return default
        return value.split(':')


class BareosFdTaskClass(BareosFdPluginBaseclass):
    debug_level = 100
    plugin_name = 'unknown'

    def __init__(self, context, plugin_def):
        BareosFdPluginBaseclass.__init__(self, context, plugin_def)
        self.config = None
        self.folder = None
        self.job_type = None
        self.task = None
        self.tasks = list()

    def log_format(self, message):
        return '{0}: {1}\n'.format(self.plugin_name, message.lower())

    def job_message(self, context, level, message):
        self.debug_message(context, message)
        JobMessage(context, level, self.log_format(message))

    def debug_message(self, context, message):
        DebugMessage(context, self.debug_level, self.log_format(message))

    def prepare_tasks(self):
        raise Exception('need to be override')

    def parse_plugin_definition(self, context, plugin_def):
        BareosFdPluginBaseclass.parse_plugin_definition(self, context, plugin_def)
        self.job_type = GetValue(context, bVariable['bVarType'])
        self.config = PluginConfig(**self.options)
        self.folder = self.config.get('folder', '@{0}'.format(self.plugin_name.upper()))
        try:
            self.prepare_tasks()
        except TaskException, _:
            return bRCs['bRC_Error']

        self.debug_message(context, '{0} task created'.format(len(self.tasks)))
        return bRCs['bRC_OK']

    def start_backup_file(self, context, save_pkt):

        if not len(self.tasks):
            self.job_message(context, bJobMessageType['M_WARNING'], 'no tasks defined')
            return bRCs['bRC_Skip']

        self.task = self.tasks.pop()
        stat_pkt = StatPacket()
        stat_pkt.size = self.task.get_size()
        stat_pkt.blksize = self.task.get_block_size()
        save_pkt.statp = stat_pkt
        save_pkt.fname = os.path.join('/', self.folder, self.task.get_filename())
        save_pkt.type = bFileType['FT_REG']

        return bRCs['bRC_OK']

    def plugin_io(self, context, iop):
        if self.job_type == bJobType['BACKUP']:

            if iop.func == bIOPS['IO_OPEN']:
                try:
                    self.job_message(context, bJobMessageType['M_INFO'], '{0} started'.format(self.task.get_name()))
                    self.task.task_pool()
                    self.task.task_open()
                except TaskException, e:
                    self.job_message(context, bJobMessageType['M_ERROR'], '{0} {1}'.format(self.task.get_name(), e))
                    return bRCs['bRC_Error']
                return bRCs['bRC_OK']

            elif iop.func == bIOPS['IO_CLOSE']:
                try:
                    self.job_message(context, bJobMessageType['M_INFO'], '{0} done'.format(self.task.get_name()))
                    self.task.task_pool()
                    self.task.task_close()
                except TaskException, e:
                    self.job_message(context, bJobMessageType['M_ERROR'], '{0} {1}'.format(self.task.get_name(), e))
                    return bRCs['bRC_Error']
                return bRCs['bRC_OK']

            elif iop.func == bIOPS['IO_READ']:
                try:
                    self.task.task_pool()
                    iop.io_errno = 0
                    iop.buf = bytearray(iop.count)
                    iop.status = self.task.task_read(iop.buf)
                except TaskException, e:
                    self.job_message(context, bJobMessageType['M_ERROR'], '{0} {1}'.format(self.task.get_name(), e))
                    return bRCs['bRC_Error']
                return bRCs['bRC_OK']

        return BareosFdPluginBaseclass.plugin_io(self, context, iop)

    def end_backup_file(self, context):
        result = self.task.task_wait()

        if result:
            self.job_message(context, bJobMessageType['M_ERROR'], '{0} {1}'.format(self.task.get_details(), result))
            return bRCs['bRC_Error']

        self.tasks.extend(self.task.get_next_tasks())
        return bRCs['bRC_More'] if len(self.tasks) else bRCs['bRC_OK']
