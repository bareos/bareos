"""
Low Level socket methods to communication with the bareos-director.
"""

# Authentication code is taken from
# https://github.com/hanxiangduo/bacula-console-python

from   bareos.exceptions import *
from   bareos.util.bareosbase64 import BareosBase64
from   bareos.util.password import Password
from   bareos.bsock.constants import Constants
from   bareos.bsock.protocolmessages import ProtocolMessages
import hmac
import logging
import random
import socket
import struct
import time

class LowLevel(object):
    """
    Low Level socket methods to communicate with the bareos-director.
    """

    def __init__(self):
        self.logger = logging.getLogger()
        self.logger.debug("init")
        self.status = None
        self.address = None
        self.password = None
        self.port = None
        self.dirname = None
        self.socket = None
        self.auth_credentials_valid = False


    def connect(self, address="localhost", port=9101, dirname=None):
        '''
        connect to bareos-director
        '''
        self.address = address
        self.port = port
        if dirname:
            self.dirname = dirname
        else:
            self.dirname = address
        return self.__connect()


    def __connect(self):
        try:
            self.socket = socket.create_connection((self.address, self.port))
        except socket.gaierror as e:
            self._handleSocketError(e)
            raise ConnectionError(
                "failed to connect to host " + str(self.address) + ", port " + str(self.port) + ": " + str(e))
        else:
            self.logger.debug("connected to " + str(self.address) + ":" + str(self.port))
        return True


    def auth(self, password, name="*UserAgent*"):
        '''
        login to the bareos-director
        if the authenticate success return True else False
        dir: the director location
        name: own name. Default is *UserAgent*
        '''
        if not isinstance(password, Password):
            raise AuthenticationError("password must by of type bareos.Password() not %s" % (type(password)))
        self.password = password
        self.name = name
        return self.__auth()


    def __auth(self):
        bashed_name = ProtocolMessages.hello(self.name)
        # send the bash to the director
        self.send(bashed_name)

        (ssl, result_compatible, result) = self._cram_md5_respond(password=self.password.md5(), tls_remote_need=0)
        if not result:
            raise AuthenticationError("failed (in response)")
        if not self._cram_md5_challenge(clientname=self.name, password=self.password.md5(), tls_local_need=0, compatible=True):
            raise AuthenticationError("failed (in challenge)")
        self.auth_credentials_valid = True
        return True


    def disconnect(self):
        ''' disconnect '''
        # TODO
        pass


    def reconnect(self):
        result = False
        if self.auth_credentials_valid:
            try:
                if self.__connect() and self.__auth() and self._set_state_director_prompt():
                    result = True
            except socket.error:
                self.logger.warning("failed to reconnect")
        return result


    def send(self, msg=None):
        '''use socket to send request to director'''
        self.__check_socket_connection()
        msg_len = len(msg) # plus the msglen info

        try:
            # convert to network flow
            self.socket.sendall(struct.pack("!i", msg_len) + msg)
            self.logger.debug("%s" %(msg.encode('string-escape')))
        except socket.error as e:
            self._handleSocketError(e)


    def recv(self):
        '''will receive data from director '''
        self.__check_socket_connection()
        # get the message header
        header = self.__get_header()
        if header <= 0:
            self.logger.debug("header: " + str(header))
        # get the message
        length = header
        msg = self.recv_submsg(length)
        return msg


    def recv_msg(self):
        '''will receive data from director '''
        self.__check_socket_connection()
        msg = ""
        try:
            timeouts = 0
            while True:
                # get the message header
                self.socket.settimeout(0.1)
                try:
                    header = self.__get_header()
                except socket.timeout:
                    # only log every 100 timeouts
                    if timeouts % 100 == 0:
                        self.logger.debug("timeout (%i) on receiving header" % (timeouts))
                    timeouts+=1
                else:
                    if header <= 0:
                        # header is a signal
                        self.__set_status(header)
                        if self.is_end_of_message(header):
                            return msg
                    else:
                        # header is the length of the next message
                        length = header
                        msg += self.recv_submsg(length)
        except socket.error as e:
            self._handleSocketError(e)
        return msg


    def recv_submsg(self, length):
        # get the message
        msg = ""
        while length > 0:
            self.logger.debug("  submsg len: " + str(length))
            # TODO
            self.socket.settimeout(10)
            submsg = self.socket.recv(length)
            length -= len(submsg)
            #self.logger.debug(submsg)
            msg += submsg
        return msg


    def __get_header(self):
        self.__check_socket_connection()
        header = self.socket.recv(4)
        if len(header) == 0:
            self.logger.debug("received empty header, assuming connection is closed")
            raise SocketEmptyHeader()
        else:
            return self.__get_header_data(header)


    def __get_header_data(self, header):
        # struct.unpack:
        #   !: network (big/little endian conversion)
        #   i: integer (4 bytes)
        data = struct.unpack("!i", header)[0]
        return data


    def is_end_of_message(self, data):
        return (data == Constants.BNET_EOD or
                data == Constants.BNET_MAIN_PROMPT or
                data == Constants.BNET_SUB_PROMPT)


    def _cram_md5_challenge(self, clientname, password, tls_local_need=0, compatible=True):
        '''
        client launch the challenge,
        client confirm the dir is the correct director
        '''

        # get the timestamp
        # here is the console
        # to confirm the director so can do this on bconsole`way
        rand = random.randint(1000000000, 9999999999)
        #chal = "<%u.%u@%s>" %(rand, int(time.time()), self.dirname)
        chal = "<%u.%u@%s>" %(rand, int(time.time()), clientname)
        msg = 'auth cram-md5 %s ssl=%d\n' %(chal, tls_local_need)
        # send the confirmation
        self.send(msg)
        # get the response
        msg = self.recv().strip(chr(0))
        self.logger.debug("received: " + msg)

        # hash with password
        hmac_md5 = hmac.new(password)
        hmac_md5.update(chal)
        bbase64compatible = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()), True)
        bbase64notcompatible = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()), False)
        self.logger.debug("string_to_base64, compatible:     " + bbase64compatible)
        self.logger.debug("string_to_base64, not compatible: " + bbase64notcompatible)

        is_correct = ((msg == bbase64compatible) or (msg == bbase64notcompatible))
        # check against compatible base64 and Bareos specific base64
        if is_correct:
            self.send(ProtocolMessages.auth_ok())
        else:
            self.logger.error("expected result: %s or %s, but get %s" %(bbase64compatible, bbase64notcompatible, msg))
            self.send(ProtocolMessages.auth_failed())

        # check the response is equal to base64
        return is_correct

    def _cram_md5_respond(self, password, tls_remote_need=0, compatible=True):
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
            self.logger.error("RuntimeError exception in recv")
            return (0, True, False)
        # check the receive message
        self.logger.debug("(recv): " + msg.encode('string-escape'))
        msg_list = msg.split(" ")
        chal = msg_list[2]
        # get th timestamp and the tle info from director response
        ssl = int(msg_list[3][4])
        compatible = True
        # hmac chal and the password
        hmac_md5 = hmac.new(password)
        hmac_md5.update(chal)

        # base64 encoding
        msg = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()))

        # send the base64 encoding to director
        self.send(msg)
        received = self.recv()
        if  ProtocolMessages.is_auth_ok(received):
            result = True
        else:
            self.logger.error("failed: " + received)
        return (ssl, compatible, result)


    def __set_status(self, status):
        self.status = status
        status_text = Constants.get_description(status)
        self.logger.debug(str(status_text) + " (" + str(status) + ")")


    def _set_state_director_prompt(self):
        self.send(".")
        msg = self.recv_msg()
        self.logger.debug("received message: " + msg)
        # TODO: check prompt
        return True


    def __check_socket_connection(self):
        result = True
        if self.socket == None:
            result = False
            if self.auth_credentials_valid:
                # connection have worked before, but now it is gone
                raise ConnectionLostError("currently no network connection")
            else:
                raise RuntimeError("should connect to director first before send data")
        return result


    def _handleSocketError(self, exception):
        self.logger.error("socket error:" + str(exception))
        self.socket = None
