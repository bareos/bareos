#!/usr/bin/env python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

from base64 import b85encode
from sys import argv, stdin, stderr

usage = f"""Usage: {argv[0]} [VALUE]
Encode VALUE so it can be used as encoded value in Bareos plugin configuration.
If VALUE is omitted, it will be read from stdin."""


def encode(val):
    return b85encode(val.encode("utf-8")).decode("ascii")


def main():
    if len(argv) == 1:
        if stdin.isatty():
            print("Value to encode: ", file=stderr, end="")
        val = input()
    elif len(argv) == 2:
        val = argv[1]
    else:
        print(usage, file=stderr)
        exit(1)

    print(encode(val))
    exit(0)


if __name__ == "__main__":
    main()
