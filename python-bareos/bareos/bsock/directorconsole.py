"""
Communicates with the bareos-dir console
"""

from   bareos.bsock.connectiontype import ConnectionType
from   bareos.bsock.constants import Constants
from   bareos.bsock.lowlevel import LowLevel
from   bareos.bsock.protocolmessageids import ProtocolMessageIds
from   bareos.bsock.protocolmessages import ProtocolMessages
from   bareos.bsock.protocolversions import ProtocolVersions
import bareos.exceptions

class DirectorConsole(LowLevel):
    '''use to send and receive the response to Bareos File Daemon'''

    def __init__(self,
                 address="localhost",
                 port=9101,
                 dirname=None,
                 name="*UserAgent*",
                 password=None,
                 protocolversion=ProtocolVersions.last,
                 pam_username=None,
                 pam_password=None,
                 tls_psk_enable=True,
                 tls_psk_require=False
                 ):
        super(DirectorConsole, self).__init__()
        self.pam_username=pam_username
        self.pam_password=pam_password
        self.tls_psk_enable=tls_psk_enable
        self.tls_psk_require=tls_psk_require
        self.identity_prefix = u'R_CONSOLE'
        self.protocolversion = protocolversion
        if self.protocolversion:
            self.protocol_messages.set_version(self.protocolversion)
        self.connect(address, port, dirname, ConnectionType.DIRECTOR, name, password)
        self.auth(name=name, password=password)
        self._init_connection()


    def get_and_evaluate_auth_responses(self):
        code, text = self.receive_and_evaluate_response_message()

        self.logger.debug(u'code: {0}'.format(code))

        #
        # Test if PAM is requested.
        # If yes, handle PAM messages.
        #
        # Is is only available, with protocolversion >= ProtocolVersions.bareos_18_2,
        # however as it is optional,
        # it will be evaluated with all protocol versions.
        #
        if code == ProtocolMessageIds.PamRequired:
            self.logger.debug(u'PAM request: {0}'.format(text))
            if (not self.pam_username) or (not self.pam_password):
                raise bareos.exceptions.AuthenticationError("PAM authentication is requested, but no PAM credentials given. Giving up.\n");
            self.send(ProtocolMessages.pam_user_credentials(self.pam_username, self.pam_password))
            try:
                code, text = self.receive_and_evaluate_response_message()
            except bareos.exceptions.ConnectionLostError as e:
                raise bareos.exceptions.AuthenticationError(u'PAM authentication failed.')
        else:
            if (self.pam_username) or (self.pam_password):
                raise bareos.exceptions.AuthenticationError("PAM credentials provided, but this Director console does not offer PAM login. Giving up.\n");


        #
        # Test if authentication has been accepted.
        #
        if code == ProtocolMessageIds.Ok:
            self.logger.info(u'Authentication: {0}'.format(text))
            self.auth_credentials_valid = True
        else:
            raise bareos.exceptions.AuthenticationError("Received unexcepted message: {0} {1} (expecting auth ok)".format(code, text))


        if self.protocolversion >= ProtocolVersions.bareos_18_2:
            #
            # Handle info message.
            #
            code, text = self.receive_and_evaluate_response_message()
            if code == ProtocolMessageIds.InfoMessage:
                self.logger.debug(u'Info: {0}'.format(text))
            else:
                raise bareos.exceptions.AuthenticationError("Received unexcepted message: {0} {1} (expecting info message)".format(code, text))


    def _init_connection(self):
        self.call("autodisplay off")


    def get_to_prompt(self):
        self.send(b".")
        return super(DirectorConsole, self).get_to_prompt()
