#!/usr/bin/env python
# -*- Mode: Python; tab-width: 4 -*-
#
# Bareos FileDaemon Task plugin
# Copyright (C) 2018 Marco Lertora <marco.lertora@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import shlex

import bareosfd
from bareos_tasks.BareosFdTaskClass import TaskProcess, BareosFdTaskClass


class TaskQueryDatabase(TaskProcess):

    def __init__(self, psql='psql', pg_host=None, pg_port=None, pg_user=None):
        self.run_as_user = pg_user
        psql_options = '--expanded --no-align --no-psqlrc'
        self.command = [ psql ]
        if pg_host:
            self.command.append("--host={}".format(pg_host))
        if pg_port:
            self.command.append("--port={}".format(pg_port))
        self.command += shlex.split(psql_options)
        super(TaskQueryDatabase, self).__init__()

    def execute_query(self, query):
        items = list()
        data = self.execute_command(self.command + ['--command=' + query]).decode('utf-8')
        for record in data.split('\n\n'):
            item = dict(map(lambda x: x.split('|', 1), record.splitlines()))
            items.append(item)
        return items

    def get_database_size(self, database):
        query = 'SELECT pg_database_size(\'{0}\');'.format(database)
        items = self.execute_query(query)
        size = int(items[0]['pg_database_size'])
        return size

    def get_databases(self):
        query = 'SELECT datname FROM pg_database;'
        exclude = ['postgres', 'template1', 'template0']
        items = self.execute_query(query)
        items = map(lambda x: x['datname'], items)
        items = filter(lambda x: x not in exclude, items)
        return items


class TaskDumpDatabase(TaskProcess):
    task_name = 'dump-database'
    file_extension = 'sql'

    def __init__(self, database, psql='psql', pg_dump='pg_dump', pg_host=None, pg_port=None, pg_user=None, pg_dump_options=''):
        self.database = database
        self.psql = psql
        self.pg_host = pg_host
        self.pg_port = pg_port
        self.run_as_user = pg_user
        self.command = [ pg_dump ]
        if pg_host:
            self.command.append("--host={}".format(pg_host))
        if pg_port:
            self.command.append("--port={}".format(pg_port))
        self.command += shlex.split(pg_dump_options)
        self.command.append(self.database)
        super(TaskDumpDatabase, self).__init__()

    def get_name(self):
        return '{0}-{1}'.format(self.task_name, self.database)

    def get_size(self):
        # This does not return the size of the dump file, but the current required disk space.
        # However, as the need the size before the dump command is executed,
        # this is the best estimation we got.
        # However, there will be warnings about incorrect sizes on restore.
        return TaskQueryDatabase(self.psql, self.pg_host, self.pg_port, self.run_as_user).get_database_size(self.database)


class BareosFdPgSQLClass(BareosFdTaskClass):
    plugin_name = 'pgsql'

    def prepare_tasks(self):
        self.tasks = list()

        psql = self.options.get('psql', 'psql')
        pg_dump = self.options.get('pg_dump', 'pg_dump')
        pg_dump_options = self.options.get('pg_dump_options', '--no-owner --no-acl')
        pg_host = self.options.get('pg_host')
        pg_port = self.options.get('pg_port')
        pg_user = self.options.get('pg_user', 'postgres')
        # allow user to specify an empty string,
        # to run the commands as the some user as bareos-fd.
        if pg_user == "''":
            pg_user = None

        databases = self.config.get_list('databases', TaskQueryDatabase(psql, pg_host, pg_port, pg_user).get_databases())

        if 'exclude' in self.config:
            exclude = self.config.get_list('exclude')
            databases = filter(lambda x: x not in exclude, databases)

        for database in databases:
            self.tasks.append(TaskDumpDatabase(database, psql, pg_dump, pg_host, pg_port, pg_user, pg_dump_options))
