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

class AuthenticationError(Error):
    """
    error during Authentication
    """
    pass
