"""
Low Level socket methods to communication with the bareos-director.
"""

# Authentication code is taken from
# https://github.com/hanxiangduo/bacula-console-python

from   bareos.bsock.constants import Constants
from   bareos.bsock.connectiontype import ConnectionType
from   bareos.bsock.protocolmessages import ProtocolMessages
from   bareos.util.bareosbase64 import BareosBase64
from   bareos.util.password import Password
import bareos.exceptions
import hashlib
import hmac
import logging
import random
import re
from   select import select
import socket
import ssl
import struct
import sys
import time
import warnings

# Try to load the sslpsk module,
# with implement TLS-PSK (Transport Layer Security - Pre-Shared-Key)
# on top of the ssl module.
# If it is not available, we continue anyway,
# but don't use TLS-PSK.
try:
    import sslpsk
except ImportError:
    warnings.warn(u'Connection encryption via TLS-PSK is not available, as the module sslpsk is not installed.')

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
        self.tls_psk_enable = True
        self.tls_psk_require = False
        self.connection_type = None
        # identity_prefix have to be set in each class
        self.identity_prefix = u'R_NONE'
        self.receive_buffer = b''


    def connect(self, address, port, dirname, type, name = None, password = None):
        self.address = address
        self.port = int(port)
        if dirname:
            self.dirname = dirname
        else:
            self.dirname = address
        self.connection_type = type
        self.name = name
        self.password = password

        return self.__connect()


    def __connect(self):
        connected = False
        if self.tls_psk_require and not self.is_tls_psk_available():
            raise bareos.exceptions.ConnectionError(u'TLS-PSK is required, but sslpsk module not loaded/available.')

        if self.tls_psk_enable and self.is_tls_psk_available():
            try:
                self.__connect_tls_psk()
            except (bareos.exceptions.ConnectionError, ssl.SSLError) as e:
                if self.tls_psk_require:
                    raise
                else:
                    self.logger.warn(u'Failed to connect via TLS-PSK. Trying plain connection.')
            else:
                connected = True
                self.logger.debug("Encryption: {}".format(self.socket.cipher()))

        if not connected:
            self.__connect_plain()
            connected = True
            self.logger.debug("Encryption: None")

        return connected


    def __connect_plain(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # initialize
        try:
            #self.socket = socket.create_connection((self.address, self.port))
            self.socket.connect((self.address, self.port))
        except socket.gaierror as e:
            self._handleSocketError(e)
            raise bareos.exceptions.ConnectionError(
                "failed to connect to host {}, port {}: {}".format(self.address, self.port, str(e)))
        else:
            self.logger.debug("connected to {}:{}".format(self.address, self.port))

        return True


    def __connect_tls_psk(self):
        '''
        Connect and establish a TLS-PSK connection on top of the connection.
        '''
        self.__connect_plain()
        # wrap socket with TLS-PSK
        client_socket = self.socket
        identity = self.get_tls_psk_identity()
        if isinstance(self.password, Password):
            password = self.password.md5()
        else:
            raise bareos.exceptions.ConnectionError(u'No password provides.')
        self.logger.debug("identity = {}, password = {}".format(identity, password))
        try:
            self.socket = sslpsk.wrap_socket(
                client_socket,
                psk=(password, identity),
                ciphers='ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH',
                #ssl_version=ssl.PROTOCOL_TLSv1_2,
                server_side=False)
        except ssl.SSLError as e:
            # raise ConnectionError(
            #     "failed to connect to host {}, port {}: {}".format(self.address, self.port, str(e)))
            # Using a general raise keep more information about the type of error.
            raise
        return True


    def get_tls_psk_identity(self):
        '''Bareos TLS-PSK excepts the identiy is a specific format.'''
        return u'{}{}{}'.format(self.identity_prefix, chr(0x1e), str(self.name))

    def is_tls_psk_available(self):
        '''Checks if we have all required modules for TLS-PSK.'''
        return 'sslpsk' in sys.modules

    def auth(self, name, password, auth_success_regex):
        '''
        Login to the Bareos Director.
        If the authenticate succeeds return True else False.
        dir: the director name
        name: own name.
        '''
        if not isinstance(password, Password):
            raise bareos.exceptions.AuthenticationError("password must by of type bareos.Password() not %s" % (type(password)))
        self.password = password
        self.name = name
        self.auth_success_regex = auth_success_regex
        return self.__auth()


    def __auth(self):
        bashed_name = ProtocolMessages.hello(self.name, type=self.connection_type)
        # send the bash to the director
        self.send(bashed_name)

        try:
            (ssl, result_compatible, result) = self._cram_md5_respond(password=self.password.md5(), tls_remote_need=0)
        except bareos.exceptions.SignalReceivedException as e:
            raise bareos.exceptions.AuthenticationError('Received unexcepted signal: {}'.format(str(e)))
        if not result:
            raise bareos.exceptions.AuthenticationError("failed (in response)")
        if not self._cram_md5_challenge(clientname=self.name, password=self.password.md5(), tls_local_need=0, compatible=True):
            raise bareos.exceptions.AuthenticationError("failed (in challenge)")
        self.recv_msg(self.auth_success_regex)
        self.auth_credentials_valid = True
        return True


    def _init_connection(self):
        pass


    def disconnect(self):
        '''disconnect'''
        # TODO
        pass


    def reconnect(self):
        result = False
        if self.auth_credentials_valid:
            try:
                if self.__connect() and self.__auth() and self._init_connection():
                    result = True
            except socket.error:
                self.logger.warning("failed to reconnect")
        return result


    def call(self, command):
        '''
        call a bareos-director user agent command
        '''
        if isinstance(command, list):
            command = " ".join(command)
        return self.__call(command, 0)


    def __call(self, command, count):
        '''
        Send a command and receive the result.
        If connection is lost, try to reconnect.
        '''
        result = b''
        try:
            self.send(bytearray(command, 'utf-8'))
            result = self.recv_msg()
        except (SocketEmptyHeader, bareos.exceptions.ConnectionLostError) as e:
            self.logger.error("connection problem (%s): %s" % (type(e).__name__, str(e)))
            if count == 0:
                if self.reconnect():
                    return self.__call(command, count+1)
        return result


    def send_command(self, command):
        return self.call(command)


    def send(self, msg=None):
        '''use socket to send request to director'''
        self.__check_socket_connection()
        msg_len = len(msg) # plus the msglen info

        try:
            # convert to network flow
            self.logger.debug('{}'.format(msg.rstrip()))
            self.socket.sendall(struct.pack("!i", msg_len) + msg)
        except socket.error as e:
            self._handleSocketError(e)


    def recv(self):
        '''will receive data from director '''
        self.__check_socket_connection()
        # get the message header
        header = self.__get_header()
        if header <= 0:
            self.logger.debug("header: " + str(header))
            raise bareos.exceptions.SignalReceivedException(header)
        # get the message
        length = header
        msg = self.recv_submsg(length)
        return msg


    def recv_msg(self, regex = b'^\d\d\d\d OK.*$', timeout = None):
        '''will receive data from director '''
        self.__check_socket_connection()
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
                            result = self.receive_buffer
                            self.receive_buffer = b''
                            return result
                    else:
                        # header is the length of the next message
                        length = header
                        submsg = self.recv_submsg(length)
                        # check for regex in new submsg
                        # and last line in old message,
                        # which might have been incomplete without new submsg.
                        lastlineindex = self.receive_buffer.rfind(b'\n') + 1
                        self.receive_buffer += submsg
                        match = re.search(regex, self.receive_buffer[lastlineindex:], re.MULTILINE)
                        # Bareos indicates end of command result by line starting with 4 digits
                        if match:
                            self.logger.debug("msg \"{0}\" matches regex \"{1}\"".format(self.receive_buffer.strip(), regex))
                            result = self.receive_buffer[0:lastlineindex+match.end()]
                            self.receive_buffer = self.receive_buffer[lastlineindex+match.end()+1:]
                            return result
                        #elif re.search("^\d\d\d\d .*$", msg, re.MULTILINE):
                            #return msg
        except socket.error as e:
            self._handleSocketError(e)


    def recv_submsg(self, length):
        # get the message
        msg = b''
        while length > 0:
            self.logger.debug("  submsg len: " + str(length))
            # TODO
            self.socket.settimeout(10)
            submsg = self.socket.recv(length)
            length -= len(submsg)
            #self.logger.debug(submsg)
            msg += submsg
        if (type(msg) is str):
            msg = bytearray(msg.decode('utf-8'), 'utf-8')
        if (type(msg) is bytes):
            msg = bytearray(msg)
        #self.logger.debug(str(msg))
        return msg


    def interactive(self):
        """
        Enter the interactive mode.
        Exit via typing "exit" or "quit".
        """
        command = ""
        while command != "exit" and command != "quit" and self.is_connected():
            command = self._get_input()
            resultmsg = self.call(command)
            self._show_result(resultmsg)
        return True


    def _get_input(self):
        # Python2: raw_input, Python3: input
        try:
            myinput = raw_input
        except NameError:
            myinput = input
        data = myinput(">>")
        return data


    def _show_result(self, msg):
        #print(msg.decode('utf-8'))
        sys.stdout.write(msg.decode('utf-8'))
        # add a linefeed, if there isn't one already
        if len(msg) >= 2:
            if msg[-2] != ord(b'\n'):
                sys.stdout.write(b'\n')


    def __get_header(self):
        header = b''
        header_length = 4
        while header_length > 0:
            self.logger.debug("  remaining header len: {0}".format(header_length))
            self.__check_socket_connection()
            # TODO
            self.socket.settimeout(10)
            submsg = self.socket.recv(header_length)
            header_length -= len(submsg)
            header += submsg
        if len(header) == 0:
            self.logger.debug("received empty header, assuming connection is closed")
            raise bareos.exceptions.SocketEmptyHeader()
        else:
            return self.__get_header_data(header)


    def __get_header_data(self, header):
        # struct.unpack:
        #   !: network (big/little endian conversion)
        #   i: integer (4 bytes)
        data = struct.unpack("!i", header)[0]
        return data


    def is_end_of_message(self, data):
        return ((not self.is_connected()) or
                data == Constants.BNET_EOD or
                data == Constants.BNET_TERMINATE or
                data == Constants.BNET_MAIN_PROMPT or
                data == Constants.BNET_SUB_PROMPT)


    def is_connected(self):
        return (self.status != Constants.BNET_TERMINATE)


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
        chal = '<%u.%u@%s>' %(rand, int(time.time()), clientname)
        msg = bytearray('auth cram-md5 %s ssl=%d\n' %(chal, tls_local_need), 'utf-8')
        # send the confirmation
        self.send(msg)
        # get the response
        msg = self.recv()
        if msg[-1] == 0:
            del msg[-1]
        self.logger.debug("received: " + str(msg))

        # hash with password
        hmac_md5 = hmac.new(bytes(bytearray(password, 'utf-8')), None, hashlib.md5)
        hmac_md5.update(bytes(bytearray(chal, 'utf-8')))
        bbase64compatible = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()), True)
        bbase64notcompatible = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()), False)
        self.logger.debug("string_to_base64, compatible:     " + str(bbase64compatible))
        self.logger.debug("string_to_base64, not compatible: " + str(bbase64notcompatible))

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
        
        # invalid username
        if ProtocolMessages.is_not_authorized(msg):
            self.logger.error("failed: " + str(msg))
            return (0, True, False)
        
        # check the receive message
        self.logger.debug("(recv): " + str(msg).rstrip())
        
        msg_list = msg.split(b' ')
        chal = msg_list[2]
        # get th timestamp and the tle info from director response
        ssl = int(msg_list[3][4])
        compatible = True
        # hmac chal and the password
        hmac_md5 = hmac.new(bytes(bytearray(password, 'utf-8')), None, hashlib.md5)
        hmac_md5.update(bytes(chal))

        # base64 encoding
        msg = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()))

        # send the base64 encoding to director
        self.send(msg)
        received = self.recv()
        if  ProtocolMessages.is_auth_ok(received):
            result = True
        else:
            self.logger.error("failed: " + str(received))
        return (ssl, compatible, result)


    def __set_status(self, status):
        self.status = status
        status_text = Constants.get_description(status)
        self.logger.debug(str(status_text) + " (" + str(status) + ")")


    def has_data(self):
        self.__check_socket_connection()
        timeout = 0.1
        readable, writable, exceptional = select([self.socket], [], [], timeout)
        return readable


    def get_to_prompt(self):
        time.sleep(0.1)
        if self.has_data():
            msg = self.recv_msg()
            self.logger.debug("received message: " + str(msg))
        # TODO: check prompt
        return True


    def __check_socket_connection(self):
        result = True
        if self.socket == None:
            result = False
            if self.auth_credentials_valid:
                # connection have worked before, but now it is gone
                raise bareos.exceptions.ConnectionLostError("currently no network connection")
            else:
                raise RuntimeError("should connect to director first before send data")
        return result


    def _handleSocketError(self, exception):
        self.logger.error("socket error:" + str(exception))
        self.socket = None
