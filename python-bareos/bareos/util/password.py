#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2015-2021 Bareos GmbH & Co. KG
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
Handle Bareos passwords.
"""

import hashlib


class Password(object):
    """
    Handle Bareos passwords.

    Instead of working with plain text passwords,
    Bareos often uses the MD5 hash of the password.

    This class make a password available in both forms.
    """

    def __init__(self, password=None):
        """\

        Args:
           password (str, optional): Password as plain text.
        """
        self.password_md5 = None
        self.set_plaintext(password)

    def set_plaintext(self, password):
        """Set new password.

        Args:
           password (str): Password as plain text.
        """
        self.password_plaintext = bytearray(password, "utf-8")
        self.set_md5(self.__plaintext2md5(password))

    def set_md5(self, password):
        """Set the MD5 form of the password.

        If the password is initialized by this method,
        it is only available as MD5 hash, not as plain text password.

        Args:
           password (bytes): Password as MD5 hash.
        """
        self.password_md5 = password

    def plaintext(self):
        """Get password as plain text.

        Returns:
            str: Password as plain text.
        """
        return self.password_plaintext.decode("utf-8")

    def md5(self):
        """Get password as MD5 hash.

        Returns:
            bytes: Password as MD5 hash.
        """
        return self.password_md5

    @staticmethod
    def __plaintext2md5(password):
        """
        md5 the password and return the hex style
        """
        md5 = hashlib.md5()
        md5.update(bytes(bytearray(password, "utf-8")))
        return bytes(bytearray(md5.hexdigest(), "utf-8"))
