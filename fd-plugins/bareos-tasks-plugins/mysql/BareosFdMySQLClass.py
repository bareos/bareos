#!/usr/bin/env python
# -*- Mode: Python; tab-width: 4 -*-
#
# Bareos FileDaemon MySQL plugin
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

    def __init__(self, mysql=None, mysql_user=None):
        self.run_as_user = mysql_user
        mysql_options = '--batch --skip-column-names'
        self.command = [mysql if mysql else 'mysql'] + shlex.split(mysql_options)
        super(TaskQueryDatabase, self).__init__()

    def execute_query(self, query):
        data = self.execute_command(self.command + ['--execute=' + query])
        return list(map(lambda x: x.split('\t'), data.splitlines()))

    def get_database_size(self, database):
        query = 'SELECT SUM(DATA_LENGTH + INDEX_LENGTH) FROM information_schema.TABLES WHERE TABLE_SCHEMA = \"{0}\";'.format(database)
        items = self.execute_query(query)
        size = int(items[0][0])
        return size

    def get_databases(self):
        query = 'SHOW DATABASES;'
        exclude = ['performance_schema', 'information_schema', 'mysql']
        items = self.execute_query(query)
        items = map(lambda x: x[0], items)
        items = filter(lambda x: x not in exclude, items)
        return items


class TaskDumpDatabase(TaskProcess):
    task_name = 'dump-database'
    file_extension = 'sql'

    def __init__(self, database, mysql=None, mysql_dump=None, mysql_user=None, mysql_dump_options=''):
        self.database = database
        self.mysql = mysql
        self.run_as_user = mysql_user
        self.command = [mysql_dump if mysql_dump else 'mysqldump'] + shlex.split(mysql_dump_options) + [self.database]
        super(TaskDumpDatabase, self).__init__()

    def get_name(self):
        return '{0}-{1}'.format(self.task_name, self.database)

    def get_size(self):
        return TaskQueryDatabase(self.mysql, self.run_as_user).get_database_size(self.database)


class BareosFdMySQLClass(BareosFdTaskClass):
    plugin_name = 'mysql'

    def prepare_tasks(self):
        self.tasks = list()

        mysql = self.options.get('mysql', 'mysql')
        mysql_dump = self.options.get('mysql_dump', 'mysqldump')
        mysql_dump_options = self.options.get('mysql_dump_options', '--events --single-transaction --add-drop-database')
        mysql_user = self.options.get('mysql_user')

        databases = self.config.get_list('databases', TaskQueryDatabase(mysql, mysql_user).get_databases())

        if 'exclude' in self.config:
            exclude = self.config.get_list('exclude')
            databases = filter(lambda x: x not in exclude, databases)

        for database in databases:
            self.tasks.append(TaskDumpDatabase(database, mysql, mysql_dump, mysql_user, mysql_dump_options))
