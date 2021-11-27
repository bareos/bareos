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
import xattr as eattr

from BareosFdPluginBaseclass import BareosFdPluginBaseclass

class BareosFdPluginCifsAcls(BareosFdPluginBaseclass):
    plugin_name = "bareos-fd-cifs-acls"
    mandatory_options = None
    
    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        super(BareosFdPluginCifsAcls, self).__init__(plugindef, self.mandatory_options)
    
    def get_xattr(self, xattr: bareosfd.XattrPacket):
        bname = str("system.cifs_acl")
        bvalue = None
        has_xattr = False

        try:
            bvalue = eattr.getxattr(xattr.fname, bname)
            has_xattr = True
            xattr.name = bytearray(bname, 'ascii')
            xattr.value = bytearray(bvalue)
            
        except OSError as e:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                "Unable to retrieve CIFS ACLs for %s: %s\n" % (xattr.fname, e.strerror))
            pass

        bareosfd.DebugMessage(100, "%s GETXATTR: %s\n" % (self.plugin_name, xattr))
            
        return bareosfd.bRC_OK

    def set_xattr(self, xattr: bareosfd.XattrPacket):
        bname = str("system.cifs_acl")

        if xattr.name.decode() == bname:
            try:
                eattr.setxattr(xattr.fname, bname, bytes(xattr.value))
            except OSError as e:
                bareosfd.JobMessage(
                    bareosfd.M_WARNING,
                    "Unable to restore CIFS ACLs for %s: %s\n" % (xattr.fname, e.strerror))
                pass
            bareosfd.DebugMessage(100, "%s SETXATTR: %s\n" % (self.plugin_name, xattr))
        return bareosfd.bRC_OK
