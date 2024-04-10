#   BAREOS - Backup Archiving REcovery Open Sourced
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
ArgumentParser wrapper.

Uses configargparse, if available,
otherwise, falls back to argparse.
"""

from pprint import pformat

HAVE_CONFIG_ARG_PARSE_MODULE = False
try:
    import configargparse as argparse

    HAVE_CONFIG_ARG_PARSE_MODULE = True
except ImportError:
    import argparse


class ArgumentParser(argparse.ArgumentParser):
    """ArgumentParser wrapper"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if HAVE_CONFIG_ARG_PARSE_MODULE:
            self.add_argument(
                "-c", "--config", is_config_file=True, help="Config file path."
            )

    def format_values(self):
        try:
            return super().format_values()
        except AttributeError:
            return pformat(vars(self.parse_args()))
