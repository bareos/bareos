"""
Bareos specific exceptions
"""

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

class AuthenticationError(Error):
    """
    error during Authentication
    """
    pass
