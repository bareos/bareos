#!/usr/bin/python

import argparse
import bareos
import logging
import sys

def getArguments():
    parser = argparse.ArgumentParser(description='Console to Bareos Director.' )
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
    director.interactive()
