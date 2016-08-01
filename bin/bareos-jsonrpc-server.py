#!/usr/bin/python

import argparse
import bareos.bsock
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
    def __init__(self, bconsole ):
        self.logger = logging.getLogger()
        self.logger.debug("init")
        self.conn = bconsole

    def call( self, command ):
        self.logger.debug( command )
        return self.conn.call_fullresult( command )

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
    parser.add_argument('-d', '--debug', action='store_true', help="enable debugging output")
    parser.add_argument('--name', default="*UserAgent*", help="use this to access a specific Bareos director named console. Otherwise it connects to the default console (\"*UserAgent*\")")
    parser.add_argument('-p', '--password', help="password to authenticate to a Bareos Director console", required=True)
    parser.add_argument('--port', default=9101, help="Bareos Director network port")
    parser.add_argument('--dirname', help="Bareos Director name")
    parser.add_argument('address', nargs='?', default="localhost", help="Bareos Director network address")
    args = parser.parse_args()
    return args


if __name__ == '__main__':
    logging.basicConfig(format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    args=getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    try:
        options = [ 'address', 'port', 'dirname', 'name' ]
        parameter = {}
        for i in options:
            if hasattr(args, i) and getattr(args,i) != None:
                logger.debug( "%s: %s" %(i, getattr(args,i)))
                parameter[i] = getattr(args, i)
            else:
                logger.debug( '%s: ""' %(i))
        logger.debug('options: %s' % (parameter))
        password = bareos.bsock.Password(args.password)
        parameter['password']=password
        director = bareos.bsock.DirectorConsoleJson(**parameter)
    except RuntimeError as e:
        print(str(e))
        sys.exit(1)
    logger.debug( "authentication successful" )

    bconsole_methods = BconsoleMethods( director )

    bconsole_methods_to_jsonrpc( bconsole_methods )

    print bconsole_methods.call("list jobs last")

    # Threading HTTP-Server
    http_server = pyjsonrpc.ThreadingHttpServer(
        server_address = ('localhost', 8080),
        RequestHandlerClass = RequestHandler
    )
    print "Starting HTTP server ..."
    print "URL: http://localhost:8080"
    http_server.serve_forever()
