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
            return bytearray("Hello Director %s calling\n" % (name), 'utf-8')
        else:
            return bytearray("Hello %s calling\n" % (name), 'utf-8')

    #@staticmethod
    #def ok():
        #return "1000 OK:"

    @staticmethod
    def auth_ok():
        return b"1000 OK auth\n"

    @staticmethod
    def auth_failed():
        return b"1999 Authorization failed.\n"

    @staticmethod
    def not_authorized():
        return b"1999 You are not authorized.\n"

    @staticmethod
    def is_auth_ok(msg):
        return msg == ProtocolMessages.auth_ok()

    @staticmethod
    def is_not_authorized(msg):
        return msg == ProtocolMessages.not_authorized()