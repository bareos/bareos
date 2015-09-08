#!/usr/bin/python

import argparse
#import bareos
import bareos.bsock
import logging
import sys

def getArguments():
    parser = argparse.ArgumentParser(description='Console to Bareos Director.' )
    parser.add_argument('-d', '--debug', action='store_true', help="enable debugging output")
    parser.add_argument('--name', default="*UserAgent*", help="use this to access a specific Bareos director named console. Otherwise it connects to the default console (\"*UserAgent*\")", dest="clientname")
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
        password = bareos.bsock.Password(args.password)
        director = bareos.bsock.BSock(address=args.address, dirname=args.dirname, port=args.port, password=password)
    except RuntimeError as e:
        print str(e)
        sys.exit(1)
    logger.debug( "authentication successful" )
    director.interactive()
