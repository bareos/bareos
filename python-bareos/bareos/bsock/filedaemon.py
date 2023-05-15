#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2016-2023 Bareos GmbH & Co. KG
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
Send and receive the response to Bareos File Daemon (bareos-fd).
"""

from bareos.bsock.connectiontype import ConnectionType
from bareos.bsock.lowlevel import LowLevel
from bareos.bsock.protocolmessageids import ProtocolMessageIds
from bareos.bsock.tlsversionparser import TlsVersionParser
import bareos.exceptions
import shlex


class FileDaemon(LowLevel):
    """Send and receive the response to Bareos File Daemon (bareos-fd)."""

    @staticmethod
    def argparser_add_default_command_line_arguments(argparser):
        """Extend argparser with :py:class:`FileDaemon` specific parameter.

        Every command line program must offer a similar set of parameter
        to connect to a Bareos File Daemon.
        This method adds the required parameter to an existing ArgParser instance.
        Parameter required to initialize a FileDaemon class
        are stored in variables prefixed with ``BAREOS_``.

        Use :py:func:`bareos.bsock.lowlevel.LowLevel.argparser_get_bareos_parameter` to retrieve the relevant parameter
        (with the ``BAREOS_`` prefix removed).

        Example:

           .. code-block:: python

              argparser = argparse.ArgumentParser(description='Connect to  Bareos File Daemon.')
              FileDaemon.argparser_add_default_command_line_arguments(argparser)
              args = argparser.parse_args()
              bareos_args = DirectorConsole.argparser_get_bareos_parameter(args)
              fd = FileDaemon(**bareos_args)

        Args:
          argparser (ArgParser): ArgParser instance.
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
        """\

        Args:
           address (str): Address of the Bareos File Daemon (hostname or IP).

           port (int): Port number of the Bareos File Daemon.

           name (str):
              Name of the File Daemon.

           password  (str, bareos.util.Password):
              Password, in cleartext or as Password object.

           tls_psk_enable (boolean): Enable TLS-PSK.

           tls_psk_require (boolean): Enforce using TLS-PSK.

           tls_version (None, ssl.PROTOCOL_TLS, ssl.PROTOCOL_TLSv1, ssl.PROTOCOL_TLSv1_1, ssl.PROTOCOL_TLSv1_2):
              TLS protocol version to use.

        Raises:
          bareos.exceptions.ConnectionError: On connections errors.
        """
        super(FileDaemon, self).__init__()
        self.tls_psk_enable = tls_psk_enable
        self.tls_psk_require = tls_psk_require
        if tls_version is not None:
            self.tls_version = tls_version
        # Well, we are not really a Director,
        # but using the interface provided for Directors.
        self.identity_prefix = "R_DIRECTOR"
        self.connect(address, port, dirname, ConnectionType.FILEDAEMON, name, password)
        self._init_connection()

    def _finalize_authentication(self):
        code, text = self.receive_and_evaluate_response_message()

        self.logger.debug("code: {0}".format(code))

        #
        # Test if authentication has been accepted.
        #
        if code == ProtocolMessageIds.FdOk:
            self.logger.info("Authentication: {0}".format(text))
            self.auth_credentials_valid = True
        else:
            raise bareos.exceptions.AuthenticationError(
                "Received unexcepted message: {0} {1} (expecting auth ok)".format(
                    code, text
                )
            )

    def call(self, command):
        """Calls a command on the Bareos File Daemon and returns its result.

        Args:
           command (str or list): Command to execute. Best provided as a list.

        Returns:
            bytes: Result received from the File Daemon.
        """
        if isinstance(command, list):
            cmdlist = command
        else:
            cmdlist = shlex.split(command)
        command0 = []
        for arg in cmdlist:
            command0.append(arg.replace(" ", "\x01"))
        return super(FileDaemon, self).call(command0)
