#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2015-2021 Bareos GmbH & Co. KG
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
Bareos specific exceptions.
"""

from bareos.bsock.constants import Constants


class Error(Exception):
    """Base Bareos error exception."""

    pass


class ConnectionError(Error):
    """Error with the connection."""

    pass


class ConnectionLostError(Error):
    """Connection lost error."""

    pass


class SocketEmptyHeader(Error):
    """Socket connection received an empty header. Connection lost?"""

    pass


class AuthenticationError(ConnectionError):
    """Error during authentication."""

    pass


class PamAuthenticationError(AuthenticationError):
    """Error during PAM authentication."""

    pass


class SignalReceivedException(Error):
    """Received a Bareos signal during a connection."""

    def __init__(self, signal):
        # Call the base class constructor with the parameters it needs
        message = Constants.get_description(signal)
        super(SignalReceivedException, self).__init__(
            "{0} ({1})".format(message, signal)
        )

        # Now for your custom code...
        self.signal = signal


class JsonRpcErrorReceivedException(Error):
    """Received JSON-RPC error object.

    This exception is raised,
    if a JSON-RPC error object is received.
    """

    def __init__(self, jsondata):
        # Call the base class constructor with the parameters it needs

        # Expected result will look like this:
        #
        # {u'jsonrpc': u'2.0', u'id': None, u'error': {u'message': u'failed', u'code': 1, u'data': {u'messages': {u'error': [u'INVALIDCOMMAND: is an invalid command.\n']}, u'result': {}}}}

        try:
            message = jsondata["error"]["message"]
        except KeyError:
            message = ""
        try:
            errormessages = jsondata["error"]["data"]["messages"]["error"]
            error = "".join(errormessages)
        except (KeyError, TypeError):
            error = str(jsondata)

        super(JsonRpcErrorReceivedException, self).__init__(
            "{0}: {1}".format(message, error)
        )

        # Now for your custom code...
        self.jsondata = jsondata


class JsonRpcInvalidJsonReceivedException(JsonRpcErrorReceivedException):
    """Received invalid JSON-RPC object.

    This exception is raised,
    if a invalid JSON-RPC object is received (e.g. data is not a valid JSON structure).
    """

    def __init__(self, jsondata):
        # Call the base class constructor with the parameters it needs

        # Expected result will look like this:
        #
        # {'error': {'message': 'No JSON object could be decoded', 'code': 2, 'data': bytearray(b'Client {\n  Name = "bareos-fd"\n  Description = "Client resource of the Director itself."\n  Address = "localhost"\n  Password = "****************"\n}\n\n{"jsonrpc":"2.0","id":null,"result":{}}')}}

        try:
            message = jsondata["error"]["message"]
        except KeyError:
            message = ""
        try:
            origdata = str(jsondata["error"]["data"])
        except (KeyError, TypeError):
            origdata = ""

        super(JsonRpcErrorReceivedException, self).__init__(
            "{0}: {1}".format(message, origdata)
        )

        # Now for your custom code...
        self.jsondata = jsondata
