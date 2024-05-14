#!/usr/bin/env python3

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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
Create a license file from a license template.
The format of the license file is according to
https://www.debian.org/doc/packaging-manuals/copyright-format/
"""

from argparse import ArgumentParser
from datetime import date
import os
import re
import sys

from git import Repo

LICENSE_FILENAME = "LICENSE.txt"
LICENSE_TEMPLATE = "devtools/template/LICENSE.txt"


def parse_cmdline_args():
    parser = ArgumentParser()
    parser.add_argument(
        "--template",
        "-t",
        default=LICENSE_TEMPLATE,
        help="License file template. Default: %(default)s",
    )
    parser.add_argument(
        "--out",
        "-o",
        default="-",
        help="Target file name. Default: stdout",
    )
    return parser.parse_args()


def get_include_file_content(path, indent="    "):
    """Reads a file a transforms it as section
    in Debian packaging copyright-format
    (indented and empty lines replaced by ".").
    """
    emptyline = re.compile(r"^\s*$")
    content = ""
    with open(path, "r", encoding="utf-8") as file:
        for line in file:
            if emptyline.fullmatch(line):
                line = ".\n"
            content += indent + line
    return content


def get_file_content(filename):
    with open(filename, "r", encoding="utf-8") as file:
        content = file.read()
    return content


def get_translations(template_filename):
    """Reads the template file,
    replaces the variables
    and returns the resulting text.
    """
    base_dir = Repo(template_filename, search_parent_directories=True).working_dir
    translations = {
        "year": date.today().year,
    }
    translations['include("core/LICENSE")'] = get_include_file_content(
        os.path.join(base_dir, "core/LICENSE")
    )
    return translations


def generate_license_file(template_filename, target_filename):
    template = get_file_content(template_filename)
    translations = get_translations(template_filename)
    if target_filename != "-":
        with open(target_filename, "w", encoding="utf-8") as file:
            file.write(template.format_map(translations))
    else:
        sys.stdout.write(template.format_map(translations))


def main():
    args = parse_cmdline_args()

    if "/" in args.template:
        template_filename = args.template
    else:
        template_filename = f"{os.path.curdir}/{args.template}"

    if args.out == "-" or "/" in args.out:
        target_filename = args.out
    else:
        target_filename = f"{os.path.curdir}/{args.out}"

    try:
        generate_license_file(template_filename, target_filename)
    except FileNotFoundError:
        print(f"Could not find file {template_filename}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
