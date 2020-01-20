class ProtocolMessageIds(object):
    """
    From https://github.com/bareos/bareos/blob/master/core//src/lib/bnet.h
    """

    #
    # Director
    #
    Unknown = 0
    ProtokollError = 1
    ReceiveError = 2
    Ok = 1000
    PamRequired = 1001
    InfoMessage = 1002
    PamInteractive = 4001
    PamUserCredentials = 4002

    # Filedaemon
    FdOk = 2000
