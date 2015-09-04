#!/usr/bin/env python

"""
Communicates with the bareos-director
"""

from    bareos.bsock.lowlevel import LowLevel
import  sys

class BSock(LowLevel):
    '''use to send and receive the response from director'''

    def __init__(self,
                 address="localhost",
                 port=9101,
                 dirname=None,
                 clientname="*UserAgent*",
                 password=None):
        super(BSock, self).__init__()
        self.connect(address, port, dirname)
        self.auth(clientname=clientname, password=password)
        self._set_state_director_prompt()


    def call(self, command):
        '''
        call a bareos-director user agent command
        '''
        self.send(command)
        return self.recv_msg()


    def send_command(self, commamd):
        return self.call(command)


    def interactive(self):
        """
        Enter the interactive mode.
        Exit via typing "exit" or "quit".
        """
        self._set_state_director_prompt()
        command = ""
        while command != "exit" and command != "quit":
            command = raw_input(">>")
            resultmsg = self.call(command)
            sys.stdout.write(resultmsg)
        return True
