"""
Communicates with the bareos-director using JSON results

Legacy, use DirectorConsoleJson instead.
"""

from bareos.bsock.directorconsolejson import DirectorConsoleJson


class BSockJson(DirectorConsoleJson):
    def __init__(self, *args, **kwargs):
        super(BSockJson, self).__init__(*args, **kwargs)
