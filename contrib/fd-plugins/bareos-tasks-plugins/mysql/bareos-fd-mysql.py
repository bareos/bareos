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

import BareosFdWrapper
from bareos_fd_consts import bRCs
from BareosFdWrapper import *
from BareosFdMySQLClass import BareosFdMySQLClass


def load_bareos_plugin(context, plugin_def):
    BareosFdWrapper.bareos_fd_plugin_object = BareosFdMySQLClass(context, plugin_def)
    return bRCs['bRC_OK']
