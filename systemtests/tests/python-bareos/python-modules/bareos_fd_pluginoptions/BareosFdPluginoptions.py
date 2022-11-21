#!/usr/bin/env python
# -*- coding: utf-8 -*-

# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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
# Bareos python plugins class that adds files from a local list to
# the backup fileset

import bareosfd
from bareosfd import *
from BareosFdPluginLocalFileset import BareosFdPluginLocalFileset
import json


class BareosFdPluginoptions(BareosFdPluginLocalFileset):
    """
    This Bareos-FD-Plugin-Class was created for automated test purposes only.
    """

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )

        self.current_xattr_number = 0
        # Last argument of super constructor is a list of mandatory arguments
        super(BareosFdPluginLocalFileset, self).__init__(plugindef, ["filename"])

    def parse_plugin_definition(self, plugindef):
        result = super(BareosFdPluginLocalFileset, self).parse_plugin_definition(
            plugindef
        )
        if result is not bRC_OK:
            return result

        DebugMessage(100, "parse_plugin_definition: {}\n".format(self.options))

        # When the option dumpoptions is given,
        # dump the plugin options into a JSON file.
        try:
            dumpfile = self.options["dumpoptions"]
            with open(dumpfile, "w") as fp:
                json.dump(self.options, fp)
        except KeyError:
            pass

        return bRC_OK


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
