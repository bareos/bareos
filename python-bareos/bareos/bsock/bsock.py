"""
Communicates with the bareos-director

Legacy, use DirectorConsole instead.
"""

from bareos.bsock.directorconsole import DirectorConsole


class BSock(DirectorConsole):
    def __init__(self, *args, **kwargs):
        super(BSock, self).__init__(*args, **kwargs)
