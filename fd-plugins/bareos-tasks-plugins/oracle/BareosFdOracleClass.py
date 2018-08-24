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

import os
import shlex

from BareosFdTaskClass import BareosFdTaskClass, TaskProcessFIFO


class TaskDumpDatabase(TaskProcessFIFO):
    task_name = 'dump-database'
    file_extension = 'dump'

    def __init__(self, db_sid, db_user, db_password, ora_home, ora_exp, ora_user, ora_exp_options):
        self.database = db_user + '-' + db_sid
        self.run_as_user = ora_user
        self.run_environ = dict(ORACLE_SID=db_sid, ORACLE_HOME=ora_home)
        ora_exp = os.path.join(ora_home, 'bin/', ora_exp)
        db_credential = db_user + '/' + db_password
        self.command = [ora_exp, db_credential] + shlex.split(ora_exp_options) + ['file=' + self.fifo_path]
        super(TaskDumpDatabase, self).__init__()

    def get_name(self):
        return '{0}-{1}'.format(self.task_name, self.database)


class BareosFdOracleClass(BareosFdTaskClass):
    plugin_name = 'oracle'

    def prepare_tasks(self):
        self.tasks = list()

        ora_exp = self.options.get('ora_exp', 'exp')
        ora_home = self.options.get('ora_home', '/opt/oracle/app/oracle/product/10.2.0')
        ora_user = self.options.get('ora_user', 'oracle')
        ora_exp_options = self.options.get('ora_exp_options', 'full=yes')  # 'tables=TABLE_NAME'
        ora_exp_options += ' statistics=none compress=N direct=yes recordlength={0}'.format(TaskDumpDatabase.block_size)
        db_sid = self.options.get('db_sid')
        db_user = self.options.get('db_user')
        db_password = self.options.get('db_password')

        self.tasks.append(TaskDumpDatabase(db_sid, db_user, db_password, ora_home, ora_exp, ora_user, ora_exp_options))
