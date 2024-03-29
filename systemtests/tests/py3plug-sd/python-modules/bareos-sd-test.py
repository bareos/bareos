# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2023 Bareos GmbH & Co. KG
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
# Author: Tobias Plum
#
import bareossd
import BareosSdWrapper
from BareosSdWrapper import *  # noqa
import BareosSdTest


def load_bareos_plugin(plugindef):
    """
    This function is called by the Bareos-Sd to load the plugin
    We use it to instantiate the plugin class
    """
    # BareosSdWrapper.bareos_sd_plugin_object is the module attribute that
    # holds the plugin class object
    BareosSdWrapper.bareos_sd_plugin_object = BareosSdTest.BareosSdTest(plugindef)
    return bareossd.bRC_OK
