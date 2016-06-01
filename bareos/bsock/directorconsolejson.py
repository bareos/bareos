"""
Reimplementation of the bconsole program in python.
"""

from   bareos.bsock.directorconsole import DirectorConsole
from   pprint import pformat, pprint
import json

class DirectorConsoleJson(DirectorConsole):
    """
    use to send and receive the response from director
    """

    def __init__(self, *args, **kwargs):
        super(DirectorConsoleJson, self).__init__(*args, **kwargs)

    def _init_connection(self):
        # older version did not support compact mode,
        # therfore first set api mode to json (which should always work in bareos >= 15.2.0)
        # and then set api mode json compact (which should work with bareos >= 15.2.2)
        self.logger.debug(self.call(".api json"))
        self.logger.debug(self.call(".api json compact=yes"))


    def call(self, command):
        json = self.call_fullresult(command)
        if json == None:
            return
        if 'result' in json:
            result = json['result']
        else:
            # TODO: or raise an exception?
            result = json
        return result


    def call_fullresult(self, command):
        resultstring = super(DirectorConsoleJson, self).call(command)
        data = None
        if resultstring:
            try:
                data = json.loads(resultstring.decode('utf-8'))
            except ValueError as e:
                # in case result is not valid json,
                # create a JSON-RPC wrapper
                data = {
                    'error': {
                        'code': 2,
                        'message': str(e),
                        'data': resultstring
                    },
                }
        return data


    def _show_result(self, msg):
        pprint(msg)
