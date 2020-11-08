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

import argparse
from collections import OrderedDict
import ssl


class ArgParserTlsVersionAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, getattr(ssl, self.choices.get(values)))


class TlsVersionParser:
    def __init__(self):
        # Add the possibility to specify the TLS protocol version.
        # This is required,
        # as sslpsk (1.0.0), depending on the Python and openssl version
        # is known to fail on various protocol versions,
        # especially with the default (PROTOCOL_TLS).
        # Anyhow, if possible, use the default (PROTOCOL_TLS),
        # as this covers different protocol versions,
        # including all versions >= v1.3.
        # There will be no specific constant TLS >= 1.3.
        self.tls_version_options = {
            # "default": "PROTOCOL_TLS",
            "v1": "PROTOCOL_TLSv1",
            "v1.1": "PROTOCOL_TLSv1_1",
            "v1.2": "PROTOCOL_TLSv1_2",
        }

        # remove invalid options
        for key, value in self.tls_version_options.items():
            if not hasattr(ssl, value):
                del self.tls_version_options[key]

    def add_argument(self, argparser):
        argparser.add_argument(
            "--tls-version",
            help="Use a specific TLS protocol version.",
            action=ArgParserTlsVersionAction,
            choices=OrderedDict(sorted(self.tls_version_options.items())),
            dest="BAREOS_tls_version",
        )

    def get_protocol_version_from_string(self, tls_version):
        if tls_version is None:
            return None
        result = None
        try:
            result = getattr(ssl, self.tls_version_options[tls_version.lower()])
        except (KeyError, AttributeError) as exc:
            pass
        return result

    def get_protocol_versions(self):
        return self.tls_version_options.keys()
