"""
Communicates with the bareos-dir console
"""

from   bareos.bsock.connectiontype  import ConnectionType
from   bareos.bsock.lowlevel import LowLevel

class DirectorConsole(LowLevel):
    '''use to send and receive the response to Bareos File Daemon'''

    def __init__(self,
                 address="localhost",
                 port=9101,
                 dirname=None,
                 name="*UserAgent*",
                 password=None,
                 tls_psk_enable=True,
                 tls_psk_require=False
                 ):
        super(DirectorConsole, self).__init__()
        self.tls_psk_enable=tls_psk_enable
        self.tls_psk_require=tls_psk_require
        self.identity_prefix = u'R_CONSOLE'
        self.connect(address, port, dirname, ConnectionType.DIRECTOR, name, password)
        self.auth(name=name, password=password, auth_success_regex=b'^1000 OK.*$')
        self._init_connection()


    def _init_connection(self):
        self.call("autodisplay off")


    def get_to_prompt(self):
        self.send(b".")
        return super(DirectorConsole, self).get_to_prompt()
