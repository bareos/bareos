#!/usr/bin/env python
# -*- Mode: Python; tab-width: 4 -*-
#
# Bareos FileDaemon PgSQL plugin
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

import shlex

from BareosFdTaskClass import TaskProcess, BareosFdTaskClass


class TaskQueryDatabase(TaskProcess):

    def __init__(self, psql=None, pg_user=None):
        self.run_as_user = pg_user
        psql_options = '--expanded --no-align'
        self.command = [psql if psql else 'psql'] + shlex.split(psql_options)
        super(TaskQueryDatabase, self).__init__()

    def execute_query(self, query):
        items = list()
        data = self.execute_command(self.command + ['--command=' + query])
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

    def __init__(self, database, psql=None, pg_dump=None, pg_user=None, pg_dump_options=''):
        self.database = database
        self.psql = psql
        self.run_as_user = pg_user
        self.command = [pg_dump if pg_dump else 'pg_dump'] + shlex.split(pg_dump_options) + [self.database]
        super(TaskDumpDatabase, self).__init__()

    def get_name(self):
        return '{0}-{1}'.format(self.task_name, self.database)

    def get_size(self):
        return TaskQueryDatabase(self.psql, self.run_as_user).get_database_size(self.database)


class BareosFdPgSQLClass(BareosFdTaskClass):
    plugin_name = 'pgsql'

    def prepare_tasks(self):
        self.tasks = list()

        psql = self.options.get('psql', 'psql')
        pg_dump = self.options.get('pg_dump', 'pg_dump')
        pg_dump_options = self.options.get('pg_dump_options', '--no-owner --no-acl')
        pg_user = self.options.get('pg_user', 'postgres')

        databases = self.config.get_list('databases', TaskQueryDatabase(psql, pg_user).get_databases())

        if 'exclude' in self.config:
            exclude = self.config.get_list('exclude')
            databases = filter(lambda x: x not in exclude, databases)

        for database in databases:
            self.tasks.append(TaskDumpDatabase(database, psql, pg_dump, pg_user, pg_dump_options))
