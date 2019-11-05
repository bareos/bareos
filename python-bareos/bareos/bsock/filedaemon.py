"""
Communicates with the bareos-fd
"""

from   bareos.bsock.connectiontype  import ConnectionType
from   bareos.bsock.lowlevel import LowLevel
from   bareos.bsock.protocolmessageids import ProtocolMessageIds
import bareos.exceptions
import shlex


class FileDaemon(LowLevel):
    '''use to send and receive the response to Bareos File Daemon'''

    def __init__(self,
                 address="localhost",
                 port=9102,
                 dirname=None,
                 name=None,
                 password=None,
                 tls_psk_enable=True,
                 tls_psk_require=False
                 ):
        super(FileDaemon, self).__init__()
        self.tls_psk_enable=tls_psk_enable
        self.tls_psk_require=tls_psk_require
        # Well, we are not really a Director,
        # but using the interface provided for Directors.
        self.identity_prefix = u'R_DIRECTOR'
        self.connect(address, port, dirname, ConnectionType.FILEDAEMON, name, password)
        self.auth(name=name, password=password)
        self._init_connection()


    def get_and_evaluate_auth_responses(self):
        code, text = self.receive_and_evaluate_response_message()

        self.logger.debug(u'code: {0}'.format(code))


        #
        # Test if authentication has been accepted.
        #
        if code == ProtocolMessageIds.FdOk:
            self.logger.info(u'Authentication: {0}'.format(text))
            self.auth_credentials_valid = True
        else:
            raise bareos.exceptions.AuthenticationError("Received unexcepted message: {0} {1} (expecting auth ok)".format(code, text))


    def call(self, command):
        '''
        Replace spaces by char(1) in quoted arguments
        and then call the original function.
        '''
        if isinstance(command, list):
            cmdlist=command
        else:
            cmdlist=shlex.split(command)
        command0 = []
        for arg in cmdlist:
            command0.append(arg.replace(" ", "\x01"))
        return super(FileDaemon, self).call(command0)
