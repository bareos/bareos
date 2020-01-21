"""
Bareos specific exceptions
"""

from bareos.bsock.constants import Constants


class Error(Exception):
    """
    general error exception
    """

    pass


class ConnectionError(Error):
    """
    error with the Connection
    """

    pass


class ConnectionLostError(Error):
    """
    error with the Connection
    """

    pass


class SocketEmptyHeader(Error):
    """
    socket connection received an empty header. Connection lost?
    """

    pass


class AuthenticationError(ConnectionError):
    """
    error during Authentication
    """

    pass


class PamAuthenticationError(AuthenticationError):
    """
    error during PAM Authentication
    """

    pass


class SignalReceivedException(Error):
    """
    received a signal during a connection.
    """

    def __init__(self, signal):
        # Call the base class constructor with the parameters it needs
        message = Constants.get_description(signal)
        super(SignalReceivedException, self).__init__(
            "{0} ({1})".format(message, signal)
        )

        # Now for your custom code...
        self.signal = signal


class JsonRpcErrorReceivedException(Error):
    """
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
    """
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
