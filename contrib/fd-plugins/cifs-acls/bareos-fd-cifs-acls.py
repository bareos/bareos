#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
#
# Author: Olivier Gagnon
#

import bareosfd
import BareosFdWrapper
from BareosFdWrapper import *  # noqa


import BareosFdPluginCifsAcls


def load_bareos_plugin(plugindef):
    BareosFdWrapper.bareos_fd_plugin_object = (
        BareosFdPluginCifsAcls.BareosFdPluginCifsAcls(plugindef)
    )
    return bareosfd.bRC_OK
