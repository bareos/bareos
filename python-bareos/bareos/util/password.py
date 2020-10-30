#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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
Class to handle Bareos passwords.
"""

import hashlib


class Password(object):
    def __init__(self, password=None):
        self.password_md5 = None
        self.set_plaintext(password)

    def set_plaintext(self, password):
        self.password_plaintext = bytearray(password, "utf-8")
        self.set_md5(self.__plaintext2md5(password))

    def set_md5(self, password):
        self.password_md5 = password

    def plaintext(self):
        return self.password_plaintext

    def md5(self):
        return self.password_md5

    @staticmethod
    def __plaintext2md5(password):
        """
        md5 the password and return the hex style
        """
        md5 = hashlib.md5()
        md5.update(bytes(bytearray(password, "utf-8")))
        return bytes(bytearray(md5.hexdigest(), "utf-8"))
