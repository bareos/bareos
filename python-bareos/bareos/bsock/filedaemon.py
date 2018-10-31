"""
Communicates with the bareos-fd
"""

from   bareos.bsock.connectiontype  import ConnectionType
from   bareos.bsock.lowlevel import LowLevel
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
        self.auth(name=name, password=password, auth_success_regex=b'^2000 OK Hello.*$')
        self._init_connection()

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
