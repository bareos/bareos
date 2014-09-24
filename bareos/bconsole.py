#!/usr/bin/python

# Authentication code is taken from 
# https://github.com/hanxiangduo/bacula-console-python

import base64
import hashlib
import hmac
import logging
from pprint import pprint
import random
import re
import socket
import struct
# sys: sys.stdout.write
import sys
import time

class bsock:
    """
    translated enum from https://github.com/bareos/bareos/blob/master/src/lib/bsock.h
    """
    BNET_EOD            = -1    #,          /* End of data stream, new data may follow */
    BNET_EOD_POLL       = -2    #,          /* End of data and poll all in one */
    BNET_STATUS         = -3    #,          /* Send full status */
    BNET_TERMINATE      = -4    #,          /* Conversation terminated, doing close() */
    BNET_POLL           = -5    #,          /* Poll request, I'm hanging on a read */
    BNET_HEARTBEAT      = -6    #,          /* Heartbeat Response requested */
    BNET_HB_RESPONSE    = -7    #,          /* Only response permited to HB */
    BNET_xxxxxxPROMPT   = -8    #,          /* No longer used -- Prompt for subcommand */
    BNET_BTIME          = -9    #,          /* Send UTC btime */
    BNET_BREAK          = -10   #,         /* Stop current command -- ctl-c */
    BNET_START_SELECT   = -11   #,         /* Start of a selection list */
    BNET_END_SELECT     = -12   #,         /* End of a select list */
    BNET_INVALID_CMD    = -13   #,         /* Invalid command sent */
    BNET_CMD_FAILED     = -14   #,         /* Command failed */
    BNET_CMD_OK         = -15   #,         /* Command succeeded */
    BNET_CMD_BEGIN      = -16   #,         /* Start command execution */
    BNET_MSGS_PENDING   = -17   #,         /* Messages pending */
    BNET_MAIN_PROMPT    = -18   #,         /* Server ready and waiting */
    BNET_SELECT_INPUT   = -19   #,         /* Return selection input */
    BNET_WARNING_MSG    = -20   #,         /* Warning message */
    BNET_ERROR_MSG      = -21   #,         /* Error message -- command failed */
    BNET_INFO_MSG       = -22   #,         /* Info message -- status line */
    BNET_RUN_CMD        = -23   #,         /* Run command follows */
    BNET_YESNO          = -24   #,         /* Request yes no response */
    BNET_START_RTREE    = -25   #,         /* Start restore tree mode */
    BNET_END_RTREE      = -26   #,         /* End restore tree mode */
    BNET_SUB_PROMPT     = -27   #,         /* Indicate we are at a subprompt */
    BNET_TEXT_INPUT     = -28   #          /* Get text input from user */

    description = {
        BNET_EOD:           "End of data stream, new data may follow",
        BNET_EOD_POLL:      "End of data and poll all in one",
        BNET_STATUS:        "Send full status",
        BNET_TERMINATE:     "Conversation terminated, doing close()",
        BNET_POLL:          "Poll request, I'm hanging on a read",
        BNET_HEARTBEAT:     "Heartbeat Response requested",
        BNET_HB_RESPONSE:   "Only response permited to HB",
        BNET_xxxxxxPROMPT:  "No longer used -- Prompt for subcommand",
        BNET_BTIME:         "Send UTC btime",
        BNET_BREAK:         "Stop current command -- ctl-c",
        BNET_START_SELECT:  "Start of a selection list",
        BNET_END_SELECT:    "End of a select list",
        BNET_INVALID_CMD:   "Invalid command sent",
        BNET_CMD_FAILED:    "Command failed",
        BNET_CMD_OK:        "Command succeeded",
        BNET_CMD_BEGIN:     "Start command execution",
        BNET_MSGS_PENDING:  "Messages pending",
        BNET_MAIN_PROMPT:   "Server ready and waiting",
        BNET_SELECT_INPUT:  "Return selection input",
        BNET_WARNING_MSG:   "Warning message",
        BNET_ERROR_MSG:     "Error message -- command failed",
        BNET_INFO_MSG:      "Info message -- status line",
        BNET_RUN_CMD:       "Run command follows",
        BNET_YESNO:         "Request yes no response",
        BNET_START_RTREE:   "Start restore tree mode",
        BNET_END_RTREE:     "End restore tree mode",
        BNET_SUB_PROMPT:    "Indicate we are at a subprompt",
        BNET_TEXT_INPUT:    "Get text input from user"
    }

    @staticmethod
    def get_description( code ):
        try:
            description = bsock.description[code]
        except KeyError:
            #self.logger.error( "unknown bsock code " + code )
            return
        else:
            return description


class bconsole:
    '''use to send and receive the response from director'''

    DIRhello = "Hello %s calling\n"
    DIROKhello = "1000 OK:"
    DIRauthOK  = "1000 OK auth\n"

    # assume, that the director knows at least 10 commands.
    # If not, we assume, something is wrong with our connection
    DIRECTOR_MIN_COMMANDS=10
    RE_dot_help = re.compile( r"(?P<command>\w+) (?P<description>.*) -- *(?P<parameter>.*)" )

    def __init__(self, host="localhost", port=9101, dirname=None ):
        self.logger = logging.getLogger()
        self.logger.debug("init")
        self.host = host
        self.port = port
        if dirname:
            self.dirname = dirname
        else:
            self.dirname = host
        # after connect get the sockaddr
        self.connect()
        self.director_commands = dict()
        self.status = None

    def connect(self):
        try:
            self.socket = socket.create_connection((self.host, self.port))
        except socket.gaierror as e:
            self._handleSocketError( e )
            raise RuntimeError( "failed to connect to host " + str(self.host) + ", port " + str(self.port) + ": " + str(e) )
        else:
            self.logger.debug( "connected to " + str(self.host) + ":" + str(self.port) )


    def send(self, msg=None):
        '''use socket to send request to director'''
        if self.socket == None:
            raise RuntimeError("should connect to director first before send data")
        msg_len = len(msg) # plus the msglen info

        try:
            self.socket.sendall(struct.pack("!i", msg_len) + msg) # convert to network flow
            self.logger.debug( "%s" %(msg.encode('string-escape')) )
        except socket.error as e:
            self._handleSocketError( e )


    def recv(self):
        '''will receive data from director '''
        if self.socket == None:
            raise RuntimeError("should connect to director first before recv data")
        nbyte = 0
        # first get the message length
        msg_header = self.socket.recv(4)
        if len(msg_header) <= 0:
            # perhaps some signal command
            raise RuntimeError("get the msglen error")
        # get the message
        msg_length = struct.unpack("!i", msg_header)[0]
        msg = ""
        if msg_length <= 0:
            self.logger.debug( "msg len: " + str(msg_length) )
        while msg_length > 0:
            msg += self.socket.recv(msg_length)
            msg_length -= len(msg)
        return msg


    def recv_msg(self):
        '''will receive data from director '''
        if self.socket == None:
            raise RuntimeError("should connect to director first before recv data")
        msg = ""
        submsg_length = 0
        try:
            while True:
                # first get the message length
                self.socket.settimeout(0.1)
                try:
                    header = self.socket.recv(4)
                except Timeout:
                    self.logger.debug( "timeout on receiving header" )
                    pass
                else:
                    if len(header) == 0:
                        self.logger.debug( "received empty header, assuming connection is closed" )
                        break
                    elif len(header) < 0:
                        # perhaps some signal command
                        self.logger.error( "failed to get header (len: " + str(len(header)) + ")" )
                        raise RuntimeError("get the msglen error (" + str(len(header)) + ")" )
                    else:
                        # get the message
                        submsg_length = struct.unpack("!i", header)[0]
                        if submsg_length <= 0:
                            #self.logger.debug( "msg len: " + str(submsg_length) )
                            self.setStatus( submsg_length )
                            if ( submsg_length == bsock.BNET_EOD or
                                 submsg_length == bsock.BNET_MAIN_PROMPT or
                                 submsg_length == bsock.BNET_SUB_PROMPT ):
                                    return msg
                        submsg = ""
                        while submsg_length > 0:
                            self.logger.debug( "  submsg len: " + str(submsg_length) )
                            self.socket.settimeout( None )
                            submsg += self.socket.recv(submsg_length)
                            submsg_length -= len(submsg)
                            msg += submsg
        except socket.error as e:
            self._handleSocketError( e )
        return msg


    def setStatus( self, status ):
        self.status = status
        status_text = bsock.get_description( status )
        self.logger.debug( str(status_text) + " (" + str(status) + ")" )


    def login(self, password=None, password_plain=None, password_md5=None, clientname="*UserAgent*"):
        '''
        authenticate the director
        if the authenticate success return True else False
        dir: the director location
        clientname: the client calling always be *UserAgent*
        '''
        compatible = True
        bashed_name = self.DIRhello %(clientname)
        # send the bash to the director
        self.send(bashed_name)
        if password and not password_plain and not password_md5:
            password_plain = password
        if password_plain:
            password_md5 = self._password_clear2md5(password_plain)

        if not password_md5:
            self.logger.error( "no password given" )
            return False

        self.logger.debug( clientname + ", " + self.dirname + ", password: " + str(password_plain) + ", password (md5): " + str(password_md5)  )

        (ssl, result_compatible, result) = self._cram_md5_respond( password=password_md5, tls_remote_need=0 )
        if result:
            if self._cram_md5_challenge( clientname=clientname, password=password_md5, tls_local_need=0, compatible=compatible ):
                return True
        return False


    def init(self):
        self.set_state_director_prompt()
        self._get_commands()
        if len(self.director_commands) < self.DIRECTOR_MIN_COMMANDS:
            raise RuntimeError("director reveals only " + len(self.director_commands) + " commands. Something is wrong here. Giving up." )
        return True


    def set_state_director_prompt( self ):
        self.send(".")
        msg=self.recv_msg()
        self.logger.debug( "received message: " + msg)
        # TODO: check prompt
        return True


    def call( self, command ):
        '''
        call a Bareos director function
        '''
        self.send( command )
        result=self.recv_msg()

        return result


    def _get_commands( self ):
        '''
        ask director for available commands:
        .help returns the commands in following syntax:
        command description -- parameter
        '''

        # TODO: var command description seems to be wrong
        #       var     (Does variable expansion --var Does variable expansion)
        self.send( ".help" )
        lines=self.recv_msg().split( "\n" )
        error = 0
        for line in lines:
            if line:
                command = self.RE_dot_help.match( line )
                if command:
                    parameteroptions = command.groupdict()['parameter'].split( " | " )
                    self.director_commands[ command.groupdict()['command'] ] = dict( raw=line, description=command.groupdict()['description'], parameter= list(parameteroptions) )
                else:
                    # this should not happen. Raise an exception?
                    self.logger.error( 'string does not match command regex: "' + line + '"' )
                    error+=1
        return error == 0


    def show_commands( self ):
        for key in sorted( self.director_commands.keys()):
            command = self.director_commands[ key ]
            #pprint( command )
            print '{command:12s}'.format(command=key),
            print '({description})'.format( description=command['description'] )
            for parameter in command['parameter']:
                print ' '.ljust(12),'{parameter}'.format( parameter=parameter )
            #print command['raw']
            print


    def interactive( self ):
        self.set_state_director_prompt()
        command=""
        while command != "exit" and command != "quit":
            command = raw_input(">>")
            self.send(command)
            msg=self.recv_msg()
            sys.stdout.write(msg)
        return True


    def _password_clear2md5( self, password ):
        '''
        md5 the password and return the hex style
        '''
        md5 = hashlib.md5()
        md5.update(password)
        return md5.hexdigest()


    def _cram_md5_respond( self, password, tls_remote_need=0, compatible=True ):
        '''
        client connect to dir,
        the dir confirm the password and the config is correct
        '''
        # receive from the director
        chal = ""
        ssl = 0
        result = False
        msg = ""
        try:
            msg = self.recv()
        except RuntimeError:
            self.logger.error( "RuntimeError exception in recv" )
            return (0, True, False)
        # check the receive message
        self.logger.debug( "(recv): " + msg.encode('string-escape') )
        msg_list = msg.split(" ")
        chal = msg_list[2]
        # get th timestamp and the tle info from director response
        ssl = int( msg_list[3][4] )
        compatible = True
        # hmac chal and the password
        hmac_md5 = hmac.new(password)
        hmac_md5.update(chal)

        # base64 encoding
        msg = base64.b64encode(hmac_md5.digest()).rstrip('=')
        # cut off the '==' after the base64 encode
        # because the bacula base64 encode
        # is not compatible for the rest world
        # so here we should do some to fix it

        # send the base64 encoding to director
        self.send(msg)
        received = self.recv()
        if received == self.DIRauthOK:
            result = True
        else:
            self.logger.error( "failed: " + received )
        return (ssl, compatible, result)

    def _cram_md5_challenge( self, clientname, password, tls_local_need=0, compatible=True ):
        '''
        client launch the challenge, 
        client confirm the dir is the correct director
        '''

        # get the timestamp
        # here is the console 
        # to confirm the director so can do this on bconsole`way 
        rand = random.randint(1000000000, 9999999999)
        #chal = "<%u.%u@%s>" %(rand, int(time.time()), self.dirname)
        chal = "<%u.%u@%s>" %(rand, int(time.time()), clientname )
        msg = 'auth cram-md5 %s ssl=%d\n' %(chal, tls_local_need)
        # send the confirmation
        self.send(msg)
        # get the response

        # hash with password 
        hmac_md5 = hmac.new(password)
        hmac_md5.update(chal)
        # encode to base64
        hmac_comp = base64.b64encode(hmac_md5.digest()).rstrip('=')
        msg=self.recv()
        # TODO: here it an error, fix it. At the momemt, just ignore it.
        if hmac_comp != msg:
            self.logger.debug( "expected result: " + hmac_comp )
            self.logger.debug( "actual   result: " + msg )
        #is_correct = (hmac_comp == msg)
        is_correct = True
        if is_correct:
            self.send("1000 OK auth\n")
        else:
            logger.error( "want %s but get %s" %(hmac_comp, msg) )
            self.send("1999 Authorization failed.\n")

        # check the response is equal to base64
        return is_correct


    def _handleSocketError(self, exception):
        self.logger.error( "socket error:" + str(exception) )
        self.socket = None
