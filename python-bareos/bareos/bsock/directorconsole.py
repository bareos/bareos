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
                 password=None):
        super(DirectorConsole, self).__init__()
        self.connect(address, port, dirname, ConnectionType.DIRECTOR)
        self.auth(name=name, password=password, auth_success_regex=b'^1000 OK.*$')
        self._init_connection()


    def _init_connection(self):
        self.call("autodisplay off")


    def get_to_prompt(self):
        self.send(b".")
        return super(DirectorConsole, self).get_to_prompt()
