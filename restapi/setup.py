#!/usr/bin/python
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2023 Bareos GmbH & Co. KG
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


import os
import re
import glob
from setuptools import find_packages, setup


def get_version():
    base_dir = os.path.abspath(os.path.dirname(__file__))

    try:
        with open(
            os.path.join(base_dir, "bareos_restapi", "VERSION.txt")
        ) as version_file:
            # read version
            # and adapt it according to
            # https://www.python.org/dev/peps/pep-0440/.
            # Local version identifiers
            # <public version identifier>[+<local version label>]
            # are not supported by PyPI and therefore not included.
            fullversion = version_file.read().strip()
            __version__ = re.sub(r"~pre([0-9]+)\..*", r".dev\1", fullversion)
    except IOError:
        # Fallback version.
        # First protocol implemented
        # has been introduced with this version.
        __version__ = "20.0.4"

    return __version__


setup(
    name="bareos-restapi",
    version=get_version(),
    license="AGPLv3",
    author="Bareos Team",
    author_email="packager@bareos.com",
    packages=find_packages(),
    package_data={
        "bareos_restapi": [
            "metatags.yaml",
            "openapi.json",
            "api.ini.example",
            "VERSION.txt",
        ],
    },
    # data_files=[('etc/bareos/restapi',['etc/restapi.ini'])],
    url="https://github.com/bareos/bareos/",
    # What does your project relate to?
    keywords="bareos, REST API",
    description="REST API for Bareos console access.",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    python_requires=">=3.6",
    install_requires=[
        "fastapi",
        "packaging",
        "passlib",
        "pydantic",
        "python-bareos",
        "python-jose",
        # python-multipart: used by fastapi
        "python-multipart",
        "pyyaml",
        "uvicorn",
    ],
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: GNU Affero General Public License v3",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Topic :: System :: Archiving :: Backup",
    ],
)
