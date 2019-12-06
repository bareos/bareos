#!/usr/bin/python

from __future__ import print_function
import argparse
import bareos.bsock
import bareos.exceptions
import inspect
import logging
# pip install python-jsonrpc
import pyjsonrpc
import sys
from   types import MethodType

def add(a, b):
    """Test function"""
    return a + b

class RequestHandler(pyjsonrpc.HttpRequestHandler):
    # Register public JSON-RPC methods
    methods = {
        "add": add
    }

class BconsoleMethods:
   def __init__(self, bconsole):
      self.logger = logging.getLogger()
      self.logger.debug("init")
      self.conn = bconsole

   def execute(self, command):
      """
      Generic function to call any bareos console command.
      """
      self.logger.debug(command)
      return self.conn.call(command)

   def execute_fullresult(self, command):
      """
      Generic function to call any bareos console commands,
      and return the full result (also the pseudo jsonrpc header, not required here).
      """
      self.logger.debug(command)
      return self.conn.call_fullresult(command)

   def list(self, command):
      """
      Interface to the Bareos console list command.
      """
      return self.execute("list " + command)

   def call(self, command):
      """
      legacy function, as call is a suboptimal name.
      It is used internally by python-jsonrpc.
      Use execute() instead.
      """
      return self.execute(command)



def bconsole_methods_to_jsonrpc(bconsole_methods):
    tuples = inspect.getmembers(bconsole_methods, predicate=inspect.ismethod)
    methods = RequestHandler.methods
    for i in tuples:
        methods[i[0]] = getattr(bconsole_methods, i[0])
        print(i[0])
    print(methods)
    RequestHandler.methods=methods


def getArguments():
    argparser = argparse.ArgumentParser(description='Run Bareos Director JSON-RPC proxy.' )
    argparser.add_argument('-d', '--debug', action='store_true', help="enable debugging output")
    bareos.bsock.DirectorConsoleJson.argparser_add_default_command_line_arguments(argparser)
    args = argparser.parse_args()
    return args


if __name__ == '__main__':
    logging.basicConfig(format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    args=getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    bareos_args = bareos.bsock.DirectorConsoleJson.argparser_get_bareos_parameter(args)
    logger.debug('options: %s' % (bareos_args))
    try:
        director = bareos.bsock.DirectorConsoleJson(**bareos_args)
    except (bareos.exceptions.ConnectionError) as e:
        print(str(e))
        sys.exit(1)
    logger.debug( "authentication successful" )

    bconsole_methods = BconsoleMethods( director )

    bconsole_methods_to_jsonrpc( bconsole_methods )

    print(bconsole_methods.call("list jobs last"))

    # Threading HTTP-Server
    http_server = pyjsonrpc.ThreadingHttpServer(
        server_address = ('localhost', 8080),
        RequestHandlerClass = RequestHandler
    )
    print("Starting HTTP server ...")
    print("URL: http://localhost:8080")
    http_server.serve_forever()
