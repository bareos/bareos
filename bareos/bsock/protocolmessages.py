#!/usr/bin/env python

class ProtocolMessages():
    """
    strings defined by the protocol to talk to the Bareos Director.
    """
    @staticmethod
    def hello(name):
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
