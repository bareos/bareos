#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2020 Bareos GmbH & Co. KG
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
Communicates with the bareos-fd
"""

from bareos.bsock.connectiontype import ConnectionType
from bareos.bsock.lowlevel import LowLevel
from bareos.bsock.protocolmessageids import ProtocolMessageIds
from bareos.bsock.tlsversionparser import TlsVersionParser
import bareos.exceptions
import shlex


class FileDaemon(LowLevel):
    """use to send and receive the response to Bareos File Daemon"""

    @staticmethod
    def argparser_add_default_command_line_arguments(argparser):
        """
        Every command line program must offer a similar set of parameter
        to connect to a Bareos File Daemon.
        This method adds the required parameter to an existing ArgParser object.
        Parameter required to initialize a FileDaemon class
        are stored in variables prefixed with BAREOS_.

        Use the argparser_get_bareos_parameter method to retrieve the relevant parameter
        (with the BAREOS_ prefix removed).

        Example:
        argparser = argparse.ArgumentParser(description='Console to Bareos Director.')
        DirectorConsole.argparser_add_default_command_line_arguments(argparser)
        args = argparser.parse_args()
        bareos_args = DirectorConsole.argparser_get_bareos_parameter(args)
        director = DirectorConsole(**bareos_args)

        @param argparser: ArgParser
        @type name: ArgParser
        """
        argparser.add_argument(
            "--name",
            help="Name of the Director resource in the File Daemon.",
            required=True,
            dest="BAREOS_name",
        )

        argparser.add_argument(
            "-p",
            "--password",
            help="Password to authenticate to a Bareos File Daemon.",
            required=True,
            dest="BAREOS_password",
        )

        argparser.add_argument(
            "--port",
            default=9102,
            help="Bareos File Daemon network port.",
            dest="BAREOS_port",
        )

        argparser.add_argument(
            "--address",
            default="localhost",
            help="Bareos File Daemon network address.",
            dest="BAREOS_address",
        )

        argparser.add_argument(
            "--tls-psk-require",
            help="Allow only encrypted connections. Default: False.",
            action="store_true",
            dest="BAREOS_tls_psk_require",
        )

        TlsVersionParser().add_argument(argparser)

    def __init__(
        self,
        address="localhost",
        port=9102,
        dirname=None,
        name=None,
        password=None,
        tls_psk_enable=True,
        tls_psk_require=False,
        tls_version=None,
    ):
        super(FileDaemon, self).__init__()
        self.tls_psk_enable = tls_psk_enable
        self.tls_psk_require = tls_psk_require
        if tls_version is not None:
            self.tls_version = tls_version
        # Well, we are not really a Director,
        # but using the interface provided for Directors.
        self.identity_prefix = u"R_DIRECTOR"
        self.connect(address, port, dirname, ConnectionType.FILEDAEMON, name, password)
        self._init_connection()

    def finalize_authentication(self):
        code, text = self.receive_and_evaluate_response_message()

        self.logger.debug(u"code: {0}".format(code))

        #
        # Test if authentication has been accepted.
        #
        if code == ProtocolMessageIds.FdOk:
            self.logger.info(u"Authentication: {0}".format(text))
            self.auth_credentials_valid = True
        else:
            raise bareos.exceptions.AuthenticationError(
                "Received unexcepted message: {0} {1} (expecting auth ok)".format(
                    code, text
                )
            )

    def call(self, command):
        """
        Replace spaces by char(1) in quoted arguments
        and then call the original function.
        """
        if isinstance(command, list):
            cmdlist = command
        else:
            cmdlist = shlex.split(command)
        command0 = []
        for arg in cmdlist:
            command0.append(arg.replace(" ", "\x01"))
        return super(FileDaemon, self).call(command0)
