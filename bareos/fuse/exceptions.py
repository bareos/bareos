"""
Bareos-Fuse specific exceptions
"""

class ParameterMissing(Exception):
    """parameter missing"""
    pass

class SocketConnectionRefused(Exception):
    """network socket connection refused"""
    pass

class RestoreClientUnknown(Exception):
    """given restore client is not known"""
    pass

class RestoreJobUnknown(Exception):
    """given restore job is not known"""
    pass

class RestorePathInvalid(Exception):
    """restore path is invalid"""
    pass
