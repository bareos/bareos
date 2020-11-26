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
Communicates with the bareos-dir console
"""

from bareos.bsock.connectiontype import ConnectionType
from bareos.bsock.constants import Constants
from bareos.bsock.lowlevel import LowLevel
from bareos.bsock.protocolmessageids import ProtocolMessageIds
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.bsock.tlsversionparser import TlsVersionParser
import bareos.exceptions


class DirectorConsole(LowLevel):
    """use to send and receive the response to Bareos Director Daemon"""

    @staticmethod
    def argparser_add_default_command_line_arguments(argparser):
        """
        Every command line program must offer a similar set of parameter
        to connect to a Bareos Director.
        This method adds the required parameter to an existing ArgParser object.
        Parameter required to initialize a DirectorConsole class
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
            default="*UserAgent*",
            help='use this to access a specific Bareos director named console. Otherwise it connects to the default console ("*UserAgent*").',
            dest="BAREOS_name",
        )

        argparser.add_argument(
            "-p",
            "--password",
            help="Password to authenticate to a Bareos Director console.",
            required=True,
            dest="BAREOS_password",
        )

        argparser.add_argument(
            "--port",
            default=9101,
            help="Bareos Director network port.",
            dest="BAREOS_port",
        )

        # argparser.add_argument('--dirname', help="Bareos Director name")
        argparser.add_argument(
            "--address",
            default="localhost",
            help="Bareos Director network address.",
            dest="BAREOS_address",
        )

        argparser.add_argument(
            "--protocolversion",
            default=ProtocolVersions.last,
            type=int,
            help=u"Specify the Bareos console protocol version. Default: {0} (current).".format(
                ProtocolVersions.last
            ),
            dest="BAREOS_protocolversion",
        )

        argparser.add_argument(
            "--pam-username",
            help="Username to authenticate against PAM on top off the normal authentication.",
            dest="BAREOS_pam_username",
        )

        argparser.add_argument(
            "--pam-password",
            help="Password to authenticate against PAM on top off the normal authentication.",
            dest="BAREOS_pam_password",
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
        port=9101,
        dirname=None,
        name="*UserAgent*",
        password=None,
        protocolversion=None,
        pam_username=None,
        pam_password=None,
        tls_psk_enable=True,
        tls_psk_require=False,
        tls_version=None,
    ):
        super(DirectorConsole, self).__init__()
        self.pam_username = pam_username
        self.pam_password = pam_password
        self.tls_psk_enable = tls_psk_enable
        self.tls_psk_require = tls_psk_require
        if tls_version is not None:
            self.tls_version = tls_version
        self.identity_prefix = u"R_CONSOLE"
        if protocolversion is not None:
            self.requested_protocol_version = int(protocolversion)
            self.protocol_messages.set_version(self.requested_protocol_version)
        self.connect(address, port, dirname, ConnectionType.DIRECTOR, name, password)
        self._init_connection()
        self.max_reconnects = 1

    def finalize_authentication(self):
        code, text = self.receive_and_evaluate_response_message()

        self.logger.debug(u"code: {0}".format(code))

        #
        # Test if PAM is requested.
        # If yes, handle PAM messages.
        #
        # Is is only available, with protocolversion >= ProtocolVersions.bareos_18_2,
        # however as it is optional,
        # it will be evaluated with all protocol versions.
        #
        if code == ProtocolMessageIds.PamRequired:
            self.logger.debug(u"PAM request: {0}".format(text))
            if (not self.pam_username) or (not self.pam_password):
                raise bareos.exceptions.PamAuthenticationError(
                    "PAM authentication is requested, but no PAM credentials given. Giving up.\n"
                )
            self.send(
                ProtocolMessages.pam_user_credentials(
                    self.pam_username, self.pam_password
                )
            )
            try:
                code, text = self.receive_and_evaluate_response_message()
            except bareos.exceptions.ConnectionLostError as e:
                raise bareos.exceptions.PamAuthenticationError(
                    u"PAM authentication failed."
                )
        else:
            if (self.pam_username) or (self.pam_password):
                raise bareos.exceptions.PamAuthenticationError(
                    "PAM credentials provided, but this Director console does not offer PAM login. Giving up.\n"
                )

        #
        # Test if authentication has been accepted.
        #
        if code == ProtocolMessageIds.Ok:
            self.logger.info(u"Authentication: {0}".format(text))
            self.auth_credentials_valid = True
        else:
            raise bareos.exceptions.AuthenticationError(
                "Received unexcepted message: {0} {1} (expecting auth ok)".format(
                    code, text
                )
            )

        if self.get_protocol_version() >= ProtocolVersions.bareos_18_2:
            #
            # Handle info message.
            #
            code, text = self.receive_and_evaluate_response_message()
            if code == ProtocolMessageIds.InfoMessage:
                self.logger.debug(u"Info: {0}".format(text))
            else:
                raise bareos.exceptions.AuthenticationError(
                    "Received unexcepted message: {0} {1} (expecting info message)".format(
                        code, text
                    )
                )

    def _init_connection(self):
        self.call("autodisplay off")

    def get_to_prompt(self):
        self.send(b".")
        return super(DirectorConsole, self).get_to_prompt()
