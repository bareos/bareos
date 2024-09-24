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
from glob import glob
from tempfile import TemporaryDirectory
from typing import Optional
import os
import re
import subprocess
import sys

from git import Repo
import yaml
import infer_license

LICENSE_FILENAME = "LICENSE.txt"
LICENSE_TEMPLATE = "devtools/template/LICENSE.txt"


class LicenseDiscoveryError(Exception):
    pass


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
    parser.add_argument(
        "--skip-cpm",
        action="store_true",
        help="Skip CMake/CPM license discovery",
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
    base_dir: str = Repo(template_filename, search_parent_directories=True).working_dir
    translations = {
        "year": f"{date.today().year}",
    }
    translations['include("core/LICENSE")'] = get_include_file_content(
        os.path.join(base_dir, "core/LICENSE")
    )
    return translations


def generate_license_file(template_filename, target_filename, cpm_licenses=None):
    template = get_file_content(template_filename)
    translations = get_translations(template_filename)
    fragments = [template.format_map(translations)]
    if cpm_licenses:
        fragments.append("\n".join(cpm_licenses))

    if target_filename != "-":
        with open(target_filename, "w", encoding="utf-8") as file:
            file.write("\n".join(fragments))
    else:
        sys.stdout.write("\n".join(fragments))


def infer_copyright(license_text: str) -> Optional[str]:
    start_markers = ("Copyright (C) ", "Copyright (c) ")
    end_markers = ("All rights reserved", "\n\n")
    start_pos = -1
    for marker in start_markers:
        pos = license_text.find(marker)
        if pos >= 0:
            start_pos = pos + len(marker)
            break
    if start_pos == -1:
        return None

    # find the end position with the shortest match
    # we need to filter >= 0, as find will return -1 when no match is found
    end_pos = min(
        filter(
            lambda x: x >= 0,
            [license_text.find(marker, start_pos) for marker in end_markers],
        )
    )

    # sanity check: if extracted copyright is longer than 100 chars, probably
    #               something went wrong.
    if end_pos - start_pos > 100:
        return None

    return license_text[start_pos:end_pos].replace("\n", " ").strip()


def reformat_license(license_text: str, *, file_spec: list[str]) -> str:
    """Reformat a license text into Debian license file format"""
    # file list
    out: list[str] = ["Files:"]
    if len(file_spec) > 0:
        for file in file_spec:
            out.append(f"    {file}")
    else:
        out.append("    *")

    # copyright field
    copy = infer_copyright(license_text)
    if copy:
        out.append(f"Copyright: {copy}")
    else:
        out.append("Copyright: see license text")

    # license field
    inferred_license = infer_license.guess_text(license_text)
    if inferred_license:
        license_name = inferred_license.shortname
    else:
        license_name = "Other"
    out.append(f"License: {license_name}")

    # the license text itself
    for line in license_text.strip("\n").split("\n"):
        if len(line) == 0:
            out.append("    .")
        else:
            out.append(f"    {line}")

    out.append("")  # add newline at the end
    return "\n".join(out)


def check_cpm_only_support(source_dir="."):
    with open(f"{source_dir}/CMakeLists.txt", "r", encoding="utf-8") as fp:
        for line in fp:
            if line.startswith("option(CPM_ONLY"):
                return True
    return False


def read_license_from_directory(directory, *, file_spec: list[str]):
    files = []
    for pattern in ("LICENSE*", "LICENCE*", "COPYING*", "Copyright*"):
        files.extend(glob(pattern, root_dir=directory))
    if len(files) == 0:
        raise LicenseDiscoveryError(f"directory {directory} contains no license-file")
    if len(files) > 1:
        raise LicenseDiscoveryError(
            f"directory {directory} contains more than one license-file"
        )
    return reformat_license(
        get_file_content(os.path.join(directory, files[0])), file_spec=file_spec
    )


def get_cpm_licenses(source_dir=".") -> list[str]:
    if not check_cpm_only_support(source_dir):
        return []
    with TemporaryDirectory() as build_dir:
        res = subprocess.run(
            [
                "cmake",
                "-S",
                source_dir,
                "-B",
                build_dir,
                "-DCPM_ONLY=ON",
                "-DCPM_USE_LOCAL_PACKAGES=OFF",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if res.returncode != 0:
            print(f"CMake returned {res.returncode}:\n{res.stderr}", file=sys.stderr)
            sys.exit(1)

        with open(
            os.path.join(build_dir, "cpm-packages.yaml"), "r", encoding="utf-8"
        ) as fp:
            licenses = []
            for pkg, meta in yaml.safe_load(fp).items():
                licenses.append(
                    read_license_from_directory(
                        meta["source_dir"], file_spec=[f"third-party/{pkg}/*"]
                    )
                )
    return licenses


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

    if args.skip_cpm:
        cpm_licenses = None
    else:
        cpm_licenses = get_cpm_licenses()
    try:
        generate_license_file(template_filename, target_filename, cpm_licenses)
    except FileNotFoundError:
        print(f"Could not find file {template_filename}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
