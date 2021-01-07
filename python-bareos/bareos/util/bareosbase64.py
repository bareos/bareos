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
Bareos specific base64 implementation.

Bacula and therefore Bareos specific implementation of a base64 decoder.
"""


class BareosBase64(object):
    """Bareos specific base64 implementation."""

    base64_digits = list(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    )

    def __init__(self):
        """Initialize the Base 64 conversion routines."""
        self.base64_map = dict(list(zip(self.base64_digits, list(range(0, 64)))))

    @staticmethod
    def twos_comp(val, bits):
        """Compute the 2's compliment of int value val."""
        if (val & (1 << (bits - 1))) != 0:
            val = val - (1 << bits)
        return val

    def base64_to_int(self, base64):
        """Convert a base 64 string to integer.

        Args:
           base64 (str): base 64 string.

        Returns:
           int: Integer value of the base64 string.
        """
        value = 0
        first = 0
        neg = False

        if base64[0] == "-":
            neg = True
            first = 1

        for i in range(first, len(base64)):
            value = value << 6
            try:
                value += self.base64_map[base64[i]]
            except KeyError:
                print("KeyError:", i)

        return -value if neg else value

    def int_to_base64(self, value):
        """Convert an integer to base 64.

        Args:
           value (int): integeer value.

        Returns:
           str: base 64 representation of value.
        """
        result = ""
        if value < 0:
            result = "-"
            value = -value

        while value:
            charnumber = value % 0x3F
            result += self.base64_digits[charnumber]
            value = value >> 6
        return result

    def string_to_base64(self, string, compatible=False):
        """Convert a string to base64.

        Args:
           string (str): string to be converted.
           compatible (bool): If True, generate Baculas broken version of base 64 strings.

        Returns:
           bytearray: base 64 representation of the given string.
        """
        buf = ""
        reg = 0
        rem = 0
        char = 0
        i = 0
        while i < len(string):
            if rem < 6:
                reg <<= 8
                char = string[i]
                if not compatible:
                    if char >= 128:
                        char = self.twos_comp(char, 8)
                reg |= char
                i += 1
                rem += 8

            save = reg
            reg >>= rem - 6
            buf += self.base64_digits[reg & 0x3F]
            reg = save
            rem -= 6

        if rem:
            mask = (1 << rem) - 1
            if compatible:
                buf += self.base64_digits[(reg & mask) << (6 - rem)]
            else:
                buf += self.base64_digits[reg & mask]
        return bytearray(buf, "utf-8")
