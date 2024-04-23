#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

from setuptools import setup, find_packages

# this will not work for an installation outside of pipenv, because
# dependencies are missing.

setup(
    name="check-sources",
    version="0.0.1",
    packages=find_packages(),
    entry_points={
        "console_scripts": [
            "check-sources=check_sources.__main__:main",
            "bareos-check-sources=check_sources.__main__:main",
            "add-copyright-header=check_sources.add_copyright_header:main",
            "pr-tool=pr_tool.main:main",
            "update-changelog-links=changelog_utils.update_links:main",
            "add-changelog-entry=changelog_utils.add_entry:main",
            "update-license=license_utils.update_license:main",
        ]
    },
)
