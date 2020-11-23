#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2015-2020 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

"""
Protocol messages between bareos-director and user-agent.
"""

from bareos import __version__
from bareos.bsock.connectiontype import ConnectionType
from bareos.bsock.constants import Constants
from bareos.bsock.protocolmessageids import ProtocolMessageIds
from bareos.bsock.protocolversions import ProtocolVersions


class ProtocolMessages:
    """
    strings defined by the protocol to talk to the Bareos Director.
    """

    def __init__(self, protocolversion=ProtocolVersions.last):
        self.set_version(protocolversion)

    def set_version(self, protocolversion):
        self.protocolversion = protocolversion

    def get_version(self):
        return self.protocolversion

    def hello(self, name, type=ConnectionType.DIRECTOR):
        if type == ConnectionType.FILEDAEMON:
            return bytearray("Hello Director %s calling\n" % (name), "utf-8")
        else:
            if self.protocolversion < ProtocolVersions.bareos_18_2:
                return bytearray("Hello %s calling\n" % (name), "utf-8")
            else:
                return bytearray(
                    "Hello %s calling version %s\n" % (name, __version__), "utf-8"
                )

    # @staticmethod
    # def ok():
    # return "1000 OK:"

    @staticmethod
    def auth_ok():
        return b"1000 OK auth\n"

    @staticmethod
    def auth_failed():
        return b"1999 Authorization failed.\n"

    @staticmethod
    def not_authorized():
        return b"1999 You are not authorized.\n"

    @staticmethod
    def is_auth_ok(msg):
        return msg == ProtocolMessages.auth_ok()

    @staticmethod
    def is_not_authorized(msg):
        return msg == ProtocolMessages.not_authorized()

    @staticmethod
    def pam_user_credentials(pam_username, pam_password):
        """
        Returns a string similar to:
        4002 USERNAME PASSWORD
        """
        return bytearray(
            "{id}{s}{username}{s}{password}".format(
                id=ProtocolMessageIds.PamUserCredentials,
                username=pam_username,
                password=pam_password,
                s=Constants.record_separator,
            ),
            "utf-8",
        )
