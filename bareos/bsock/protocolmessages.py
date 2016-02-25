"""
Protocol messages between bareos-director and user-agent.
"""

from   bareos.bsock.connectiontype import ConnectionType

class ProtocolMessages():
    """
    strings defined by the protocol to talk to the Bareos Director.
    """
    @staticmethod
    def hello(name, type=ConnectionType.DIRECTOR):
        if type == ConnectionType.FILEDAEMON:
            return "Hello Director %s calling\n" % (name)
        else:
            return "Hello %s calling\n" % (name)

    #@staticmethod
    #def ok():
        #return "1000 OK:"

    @staticmethod
    def auth_ok():
        return "1000 OK auth\n"

    @staticmethod
    def auth_failed():
        return "1999 Authorization failed.\n"

    @staticmethod
    def is_auth_ok(msg):
        return msg == ProtocolMessages.auth_ok()
