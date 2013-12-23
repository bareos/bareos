#!/usr/bin/python

import argparse
import bareos
import inspect
import logging
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
    def __init__(self, bconsole ):
        self.logger = logging.getLogger()
        self.logger.debug("init")
        self.conn = bconsole

    def call( self, command ):
        self.logger.debug( command )
        raw=self.conn.call( command )
        result = {
            'raw': raw
        }
        return result

def bconsole_methods_to_jsonrpc( bconsole_methods ):
    tuples = inspect.getmembers( bconsole_methods, predicate=inspect.ismethod )
    methods = RequestHandler.methods
    for i in tuples:
        methods[i[0]] = getattr( bconsole_methods, i[0] )
        print i[0]
    print methods
    RequestHandler.methods=methods


def getArguments():
    parser = argparse.ArgumentParser(description='Run Bareos Director JSON-RPC proxy.' )
    parser.add_argument( '-d', '--debug', action='store_true', help="enable debugging output" )
    parser.add_argument( '-p', '--password', help="password to authenticate at Bareos Director" )
    parser.add_argument( 'host', default="localhost", help="Bareos Director host" )
    parser.add_argument( 'dirname', nargs='?', default=None, help="Bareos Director name" )
    args = parser.parse_args()
    return args


if __name__ == '__main__':
    logging.basicConfig(format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    args=getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    try:
        director = bareos.bconsole( host=args.host, dirname=args.dirname )
    except RuntimeError as e:
        print str(e)
        sys.exit(1)
    if not director.login( password=args.password ):
        print "failed to authenticate"
        sys.exit(1)
    logger.debug( "authentication successful" )

    director.init()
    director.show_commands()

    bconsole_methods = BconsoleMethods( director )

    bconsole_methods_to_jsonrpc( bconsole_methods )

    print bconsole_methods.call( "status director" )

    # Threading HTTP-Server
    http_server = pyjsonrpc.ThreadingHttpServer(
        server_address = ('localhost', 8080),
        RequestHandlerClass = RequestHandler
    )
    print "Starting HTTP server ..."
    print "URL: http://localhost:8080"
    http_server.serve_forever()
