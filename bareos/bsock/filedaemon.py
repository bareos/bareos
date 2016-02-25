"""
Communicates with the bareos-fd
"""

from   bareos.bsock.connectiontype import ConnectionType
from   bareos.bsock import BSock

class FileDaemon(BSock):
    '''use to send and receive the response to Bareos File Daemon'''

    def __init__(self,
                 address="localhost",
                 port=9102,
                 dirname=None,
                 name=None,
                 password=None,
                 type=ConnectionType.FILEDAEMON):
        super(FileDaemon, self).__init__(address, port, dirname, name, password, type)
