#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2020 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

"""
Low Level socket methods to communication with the bareos-director.
"""

# Authentication code is taken from
# https://github.com/hanxiangduo/bacula-console-python

import hashlib
import hmac
import logging
import random
import re
from select import select
import socket
import ssl
import struct
import sys
import time
import warnings

from bareos.bsock.constants import Constants
from bareos.bsock.connectiontype import ConnectionType
from bareos.bsock.protocolmessageids import ProtocolMessageIds
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.util.bareosbase64 import BareosBase64
from bareos.util.password import Password
import bareos.exceptions

# Try to load the sslpsk module,
# with implement TLS-PSK (Transport Layer Security - Pre-Shared-Key)
# on top of the ssl module.
# If it is not available, we continue anyway,
# but don't use TLS-PSK.
try:
    import sslpsk
except ImportError:
    warnings.warn(
        u"Connection encryption via TLS-PSK is not available, as the module sslpsk is not installed."
    )


class LowLevel(object):
    """
    Low Level socket methods to communicate with the bareos-director.
    """

    @staticmethod
    def argparser_get_bareos_parameter(args):
        """
        This method is usally used together with the method argparser_add_default_command_line_arguments.

        @param args: Arguments retrieved by ArgumentParser.parse_args()
        @type args:  ArgParser.Namespace

        @return: returns the relevant parameter from args to initialize a connection.
        @rtype: dict
        """
        result = {}
        for key, value in vars(args).items():
            if value is not None:
                if key.startswith("BAREOS_"):
                    bareoskey = key.split("BAREOS_", 1)[1]
                    result[bareoskey] = value
        return result

    def __init__(self):
        self.logger = logging.getLogger()
        self.logger.debug("init")
        self.status = None
        self.address = None
        self.password = None
        self.pam_username = None
        self.pam_password = None
        self.port = None
        self.dirname = None
        self.socket = None
        self.auth_credentials_valid = False
        self.max_reconnects = 0
        self.tls_psk_enable = True
        self.tls_psk_require = False
        try:
            self.tls_version = ssl.PROTOCOL_TLS
        except AttributeError:
            self.tls_version = ssl.PROTOCOL_SSLv23
        self.connection_type = None
        self.requested_protocol_version = None
        self.protocol_messages = ProtocolMessages()
        # identity_prefix have to be set in each class
        self.identity_prefix = u"R_NONE"
        self.receive_buffer = b""

    def __del__(self):
        self.close()

    def connect(
        self, address, port, dirname, connection_type, name=None, password=None
    ):
        self.address = address
        self.port = int(port)
        if dirname:
            self.dirname = dirname
        else:
            self.dirname = address
        self.connection_type = connection_type
        self.name = name
        if password is None:
            raise bareos.exceptions.ConnectionError(u"Parameter 'password' is required.")
        if isinstance(password, Password):
            self.password = password
        else:
            self.password = Password(password)

        return self.__connect()

    def __connect(self):
        connected = False
        connected_plain = False
        auth = False
        if self.tls_psk_require:
            if not self.is_tls_psk_available():
                raise bareos.exceptions.ConnectionError(
                    u"TLS-PSK is required, but sslpsk module not loaded/available."
                )
            if not self.tls_psk_enable:
                raise bareos.exceptions.ConnectionError(
                    u"TLS-PSK is required, but not enabled."
                )

        if self.tls_psk_enable and self.is_tls_psk_available():
            try:
                self.__connect_tls_psk()
            except (bareos.exceptions.ConnectionError, ssl.SSLError) as e:
                self._handleSocketError(e)
                if self.tls_psk_require:
                    raise
                else:
                    self.logger.warning(
                        u"Failed to connect via TLS-PSK. Trying plain connection."
                    )
            else:
                connected = True
                self.logger.debug("Encryption: {0}".format(self.socket.cipher()))

        if not connected:
            self.__connect_plain()
            connected = True
            connected_plain = True
            self.logger.debug("Encryption: None")

        if connected:
            try:
                auth = self.auth()
            except bareos.exceptions.PamAuthenticationError:
                raise
            except bareos.exceptions.AuthenticationError:
                if (
                    self.connection_type == ConnectionType.DIRECTOR
                    and self.requested_protocol_version is None
                    and self.get_protocol_version() > ProtocolVersions.bareos_12_4
                ):
                    # reconnect and try old protocol
                    self.logger.warning(
                        "Failed to connect using protocol version {0}. Trying protocol version {1}. ".format(
                            self.get_protocol_version(), ProtocolVersions.bareos_12_4
                        )
                    )
                    self.close()
                    self.__connect_plain()
                    self.protocol_messages.set_version(ProtocolVersions.bareos_12_4)
                    auth = self.auth()
                else:
                    raise

        return auth

    def __connect_plain(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # initialize
        try:
            self.socket.connect((self.address, self.port))
        except (socket.error, socket.gaierror) as e:
            self._handleSocketError(e)
            raise bareos.exceptions.ConnectionError(
                "Failed to connect to host {0}, port {1}: {2}".format(
                    self.address, self.port, str(e)
                )
            )

        self.logger.debug("connected to {0}:{1}".format(self.address, self.port))

        return True

    def __connect_tls_psk(self):
        """
        Connect and establish a TLS-PSK connection on top of the connection.
        """
        self.__connect_plain()
        # wrap socket with TLS-PSK
        client_socket = self.socket
        identity = self.get_tls_psk_identity()
        if isinstance(self.password, Password):
            password = self.password.md5()
        else:
            raise bareos.exceptions.ConnectionError(u"No password provided.")
        self.logger.debug("identity = {0}, password = {1}".format(identity, password))
        try:
            self.socket = sslpsk.wrap_socket(
                client_socket,
                ssl_version=self.tls_version,
                ciphers="ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
                psk=(password, identity),
                server_side=False,
            )
        except ssl.SSLError as e:
            # raise ConnectionError(
            #     "failed to connect to host {0}, port {1}: {2}".format(self.address, self.port, str(e)))
            # Using a general raise keep more information about the type of error.
            raise
        return True

    def get_tls_psk_identity(self):
        """Bareos TLS-PSK excepts the identiy is a specific format."""
        name = str(self.name)
        if isinstance(self.name, bytes):
            name = self.name.decode("utf-8")
        result = u"{0}{1}{2}".format(self.identity_prefix, Constants.record_separator, name)
        return bytes(bytearray(result, "utf-8"))


    @staticmethod
    def is_tls_psk_available():
        """Checks if we have all required modules for TLS-PSK."""
        return "sslpsk" in sys.modules

    def get_protocol_version(self):
        return self.protocol_messages.get_version()

    def get_cipher(self):
        if hasattr(self.socket, "cipher"):
            return self.socket.cipher()
        else:
            return None

    def auth(self):
        """
        Login to a Bareos Daemon.

        @return: True, if the authentication succeeds.
                 In earlier versions, authentication failures returned False.
                 However, now an authentication failure raises an exception.
        @rtype: bool

        @raise bareos.exceptions.AuthenticationError: if authentication fails.
        """

        bashed_name = self.protocol_messages.hello(self.name, type=self.connection_type)
        # send the bash to the director
        self.send(bashed_name)

        try:
            (ssl, result_compatible, result) = self._cram_md5_respond(
                password=self.password.md5(), tls_remote_need=0
            )
        except bareos.exceptions.SignalReceivedException as e:
            self._handleSocketError(e)
            raise bareos.exceptions.AuthenticationError(
                "Received unexcepted signal: {0}".format(str(e))
            )
        if not result:
            raise bareos.exceptions.AuthenticationError("failed (in response)")
        if not self._cram_md5_challenge(
            clientname=self.name,
            password=self.password.md5(),
            tls_local_need=0,
            compatible=True,
        ):
            raise bareos.exceptions.AuthenticationError("failed (in challenge)")

        self.finalize_authentication()

        return self.auth_credentials_valid

    def receive_and_evaluate_response_message(self):
        regex_str = r"^(\d\d\d\d){0}(.*)$".format(
            Constants.record_separator_compat_regex
        )
        regex = bytes(bytearray(regex_str, "utf8"))
        incoming_message = self.recv_msg(regex)
        match = re.search(regex, incoming_message, re.DOTALL)
        code = int(match.group(1))
        text = match.group(2)

        return (code, text)

    def _init_connection(self):
        pass

    def close(self):
        """disconnect"""
        if self.socket is not None:
            self.socket.close()
        self.socket = None

    def reconnect(self):
        result = False
        if self.max_reconnects > 0:
            try:
                self.max_reconnects -= 1
                if self.__connect() and self._init_connection():
                    result = True
            except (socket.error, bareos.exceptions.ConnectionLostError):
                self.logger.warning("failed to reconnect")
        return result

    def call(self, command):
        """
        call a bareos-director user agent command
        """
        if isinstance(command, list):
            command = " ".join(command)
        return self._send_a_command_and_receive_result(command)

    def _send_a_command_and_receive_result(self, command):
        """
        Send a command and receive the result.
        If connection is lost, try to reconnect.
        """
        result = b""
        try:
            self.send(bytearray(command, "utf-8"))
            result = self.recv_msg()
        except (
            bareos.exceptions.SocketEmptyHeader,
            bareos.exceptions.ConnectionLostError,
        ) as e:
            self.logger.error(
                "connection problem (%s): %s" % (type(e).__name__, str(e))
            )
            if self.reconnect():
                return self._send_a_command_and_receive_result(command, count + 1)
            else:
                raise
        return result

    def send_command(self, command):
        return self.call(command)

    def send(self, msg=None):
        """use socket to send request to director"""
        self.__check_socket_connection()
        msg_len = len(msg)  # plus the msglen info

        try:
            # convert to network flow
            self.logger.debug("{0}".format(msg.rstrip()))
            self.socket.sendall(struct.pack("!i", msg_len) + msg)
        except socket.error as e:
            self._handleSocketError(e)

    def recv_bytes(self, length, timeout=10):
        """
        Receive a number of bytes.

        @raise bareos.exceptions.ConnectionLostError:
               is raised, if the socket connection gets lost.
        @raise socket.timeout:
               is raised, if a timeout occurs on the socket connection,
               meaning no data received.
        """
        self.socket.settimeout(timeout)
        msg = b""
        # get the message
        while length > 0:
            self.logger.debug("expecting {0} bytes.".format(length))
            submsg = self.socket.recv(length)
            if len(submsg) == 0:
                errormsg = u"Failed to retrieve data. Assuming the connection is lost."
                self._handleSocketError(errormsg)
                raise bareos.exceptions.ConnectionLostError(errormsg)
            length -= len(submsg)
            msg += submsg
        return msg

    def recv(self):
        """
        Receive a single message.
        This is,
        header (4 bytes): if
            > 0: length of the following message
            < 0: Bareos signal
        msg: of the length descriped in the header.

        @raise bareos.exceptions.SignalReceivedException:
               is raised, if a Bareos signal is received.
        """
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

    def recv_msg(self, regex=b"^\d\d\d\d OK.*$"):
        """
        Receive a full Director message.

        It retrieves Director messages (header + message text),
        until
          1. the message matches the specified regex or
          2. the header indicates a signal.

        @raise bareos.exceptions.SignalReceivedException:
               is raised, if a Bareos signal is received.
        """
        self.__check_socket_connection()
        try:
            timeouts = 0
            while True:
                # get the message header
                try:
                    header = self.__get_header()
                except (socket.timeout, ssl.SSLError) as exception:
                    # When using a SSL connection,
                    # a timeout is raised as
                    # ssl.SSLError exception with message: 'The read operation timed out'.
                    # ssl.SSLError is inherited from socket.error.
                    # Because we can't be sure,
                    # that it is really a timeout, we log it.
                    if isinstance(exception, ssl.SSLError) and self.logger.isEnabledFor(
                        logging.DEBUG
                    ):
                        # self.logger.exception('On SSL connections, timeout are raised as ssl.SSLError exceptions:')
                        self.logger.debug("{0}".format(repr(exception)))
                    self.logger.debug("timeout (%i) on receiving header" % (timeouts))
                    timeouts += 1
                else:
                    if header <= 0:
                        # header is a signal
                        self.__set_status(header)
                        if self.is_end_of_message(header):
                            result = self.receive_buffer
                            self.receive_buffer = b""
                            return result
                    else:
                        # header is the length of the next message
                        length = header
                        submsg = self.recv_submsg(length)
                        # check for regex in new submsg
                        # and last line in old message,
                        # which might have been incomplete without new submsg.
                        lastlineindex = self.receive_buffer.rfind(b"\n") + 1
                        self.receive_buffer += submsg
                        match = re.search(
                            regex, self.receive_buffer[lastlineindex:], re.DOTALL
                        )
                        # Bareos indicates end of command result by line starting with 4 digits
                        if match:
                            self.logger.debug(
                                'msg "{0}" matches regex "{1}"'.format(
                                    self.receive_buffer.strip(), regex
                                )
                            )
                            result = self.receive_buffer[
                                0 : lastlineindex + match.end()
                            ]
                            self.receive_buffer = self.receive_buffer[
                                lastlineindex + match.end() + 1 :
                            ]
                            return result
        except socket.error as e:
            self._handleSocketError(e)

    def recv_submsg(self, length):
        # get the message
        msg = self.recv_bytes(length)
        if type(msg) is str:
            msg = bytearray(msg.decode("utf-8"), "utf-8")
        if type(msg) is bytes:
            msg = bytearray(msg)
        self.logger.debug(str(msg))
        return msg

    def interactive(self):
        """
        Enter the interactive mode.
        Exit via typing "exit" or "quit".
        """
        command = ""
        while command != "exit" and command != "quit" and self.is_connected():
            try:
                command = self._get_input()
            except EOFError:
                return False
            try:
                if command == "exit" or command == "quit":
                    return True
                resultmsg = self.call(command)
                self._show_result(resultmsg)
            except bareos.exceptions.JsonRpcErrorReceivedException as exp:
                print(str(exp))
                # print(str(exp.jsondata))

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
        # print(msg.decode('utf-8'))
        sys.stdout.write(msg.decode("utf-8"))
        # add a linefeed, if there isn't one already
        if len(msg) >= 2:
            if msg[-2] != ord(b"\n"):
                sys.stdout.write("\n")

    def __get_header(self, timeout=10):
        header = self.recv_bytes(4, timeout)
        return self.__get_header_data(header)

    def __get_header_data(self, header):
        # struct.unpack:
        #   !: network (big/little endian conversion)
        #   i: integer (4 bytes)
        data = struct.unpack("!i", header)[0]
        return data

    def is_end_of_message(self, data):
        return (
            (not self.is_connected())
            or data == Constants.BNET_EOD
            or data == Constants.BNET_TERMINATE
            or data == Constants.BNET_MAIN_PROMPT
            or data == Constants.BNET_SUB_PROMPT
        )

    def is_connected(self):
        return self.status != Constants.BNET_TERMINATE

    def _cram_md5_challenge(
        self, clientname, password, tls_local_need=0, compatible=True
    ):
        """
        client launch the challenge,
        client confirm the dir is the correct director
        """

        # get the timestamp
        # here is the console
        # to confirm the director so can do this on bconsole`way
        rand = random.randint(1000000000, 9999999999)
        # chal = "<%u.%u@%s>" %(rand, int(time.time()), self.dirname)
        chal = "<%u.%u@%s>" % (rand, int(time.time()), clientname)
        msg = bytearray("auth cram-md5 %s ssl=%d\n" % (chal, tls_local_need), "utf-8")
        # send the confirmation
        self.send(msg)
        # get the response
        msg = self.recv()
        if msg[-1] == 0:
            del msg[-1]
        self.logger.debug("received: " + str(msg))

        # hash with password
        hmac_md5 = hmac.new(password, None, hashlib.md5)
        hmac_md5.update(bytes(bytearray(chal, "utf-8")))
        bbase64compatible = BareosBase64().string_to_base64(
            bytearray(hmac_md5.digest()), True
        )
        bbase64notcompatible = BareosBase64().string_to_base64(
            bytearray(hmac_md5.digest()), False
        )
        self.logger.debug("string_to_base64, compatible:     " + str(bbase64compatible))
        self.logger.debug(
            "string_to_base64, not compatible: " + str(bbase64notcompatible)
        )

        is_correct = (msg == bbase64compatible) or (msg == bbase64notcompatible)
        # check against compatible base64 and Bareos specific base64
        if is_correct:
            self.send(ProtocolMessages.auth_ok())
        else:
            self.logger.error(
                "expected result: %s or %s, but get %s"
                % (bbase64compatible, bbase64notcompatible, msg)
            )
            self.send(ProtocolMessages.auth_failed())

        # check the response is equal to base64
        return is_correct

    def _cram_md5_respond(self, password, tls_remote_need=0, compatible=True):
        """
        client connect to dir,
        the dir confirm the password and the config is correct
        """
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

        msg_list = msg.split(b" ")
        chal = msg_list[2]
        # get th timestamp and the tle info from director response
        ssl = int(msg_list[3][4])
        compatible = True
        # hmac chal and the password
        hmac_md5 = hmac.new((password), None, hashlib.md5)
        hmac_md5.update(bytes(chal))

        # base64 encoding
        msg = BareosBase64().string_to_base64(bytearray(hmac_md5.digest()))

        # send the base64 encoding to director
        self.send(msg)
        received = self.recv()
        if ProtocolMessages.is_auth_ok(received):
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
        if self.socket is None:
            result = False
            if self.auth_credentials_valid:
                # connection have worked before, but now it is gone
                raise bareos.exceptions.ConnectionLostError(
                    "currently no network connection"
                )
            else:
                raise RuntimeError("should connect to director first before send data")
        return result

    def _handleSocketError(self, exception):
        self.logger.warning("socket error: {0}".format(str(exception)))
        self.close()
