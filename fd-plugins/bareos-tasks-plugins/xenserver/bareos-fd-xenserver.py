#!/usr/bin/env python
# -*- Mode: Python; tab-width: 4 -*-
#
# Bareos FileDaemon XenServer plugin
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

import BareosFdWrapper
from bareos_fd_consts import bRCs
from BareosFdWrapper import *
from BareosFdXenServerClass import BareosFdXenServerClass


def load_bareos_plugin(context, plugin_def):
    BareosFdWrapper.bareos_fd_plugin_object = BareosFdXenServerClass(context, plugin_def)
    return bRCs['bRC_OK']
