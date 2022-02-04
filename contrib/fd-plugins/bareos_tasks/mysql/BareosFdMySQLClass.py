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

from bareos_tasks.BareosFdTaskClass import TaskProcess, BareosFdTaskClass


class TaskQueryDatabase(TaskProcess):

    def __init__(self, mysql=None, system_user=None, defaultsfile=None):
        self.run_as_user = system_user
        mysql_options = [ '--batch', '--skip-column-names' ]
        self.command = [mysql if mysql else 'mysql']
        if defaultsfile:
            # defaults-file must be the first option
            self.command.append('--defaults-file={}'.format(defaultsfile))
        self.command += mysql_options
        super(TaskQueryDatabase, self).__init__()

    def execute_query(self, query):
        data = self.execute_command(self.command + ['--execute=' + query])
        return list(map(lambda x: x.split('\t'), data.decode('utf-8').splitlines()))

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

    def __init__(self, database, mysql=None, mysql_dump=None, system_user=None, defaultsfile=None, mysql_dump_options=''):
        self.database = database
        self.mysql = mysql
        self.run_as_user = system_user
        self.defaultsfile = defaultsfile

        self.command = [mysql_dump if mysql_dump else 'mysqldump']
        if defaultsfile:
            # defaults-file must be the first option
            self.command.append('--defaults-file={}'.format(defaultsfile))
        self.command += shlex.split(mysql_dump_options) + [self.database]
        super(TaskDumpDatabase, self).__init__()

    def get_name(self):
        return '{0}-{1}'.format(self.task_name, self.database)

    def get_size(self):
        return TaskQueryDatabase(self.mysql, self.run_as_user, self.defaultsfile).get_database_size(self.database)


class BareosFdMySQLClass(BareosFdTaskClass):
    plugin_name = 'mysql'

    def prepare_tasks(self):
        self.tasks = list()

        mysql = self.options.get('mysql', 'mysql')
        mysql_dump = self.options.get('mysql_dump', 'mysqldump')
        mysql_dump_options = self.options.get('mysql_dump_options', '--events --single-transaction --add-drop-database --databases')
        defaultsfile = self.options.get('defaultsfile')
        system_user = self.options.get('user')
        if not system_user:
            # fallback to old (Bareos <= 19.2) parameter name
            system_user = self.options.get('mysql_user')

        self.debug_message("defaultsfile={}".format(defaultsfile))

        databases = self.config.get_list('databases', TaskQueryDatabase(mysql, system_user, defaultsfile).get_databases())

        if 'exclude' in self.config:
            exclude = self.config.get_list('exclude')
            databases = filter(lambda x: x not in exclude, databases)

        for database in databases:
            self.tasks.append(TaskDumpDatabase(database, mysql, mysql_dump, system_user, defaultsfile, mysql_dump_options))
