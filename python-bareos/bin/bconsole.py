#!/usr/bin/python

from __future__ import print_function
import argparse
import bareos.bsock
import bareos.exceptions
from   bareos.bsock.protocolversions import ProtocolVersions
import logging
import sys

def getArguments():
    parser = argparse.ArgumentParser(description='Console to Bareos Director.' )
    parser.add_argument('-d', '--debug', action='store_true', help="enable debugging output")
    parser.add_argument('--name', default="*UserAgent*", help="use this to access a specific Bareos director named console. Otherwise it connects to the default console (\"*UserAgent*\")")
    parser.add_argument('-p', '--password', help="password to authenticate to a Bareos Director console", required=True)
    parser.add_argument('--port', default=9101, help="Bareos Director network port")
    parser.add_argument('--dirname', help="Bareos Director name")
    parser.add_argument('--protocolversion',
                        default=ProtocolVersions.last,
                        help=u'Specify the protocol version to use. Default: {} (current)'.format(ProtocolVersions.last))
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
                logger.debug("%s: %s" %(i, getattr(args,i)))
                parameter[i] = getattr(args,i)
            else:
                logger.debug( '%s: ""' %(i))
        logger.debug('options: %s' % (parameter))
        password = bareos.bsock.Password(args.password)
        parameter['password']=password
        try:
            director = bareos.bsock.DirectorConsole(**parameter)
        except (bareos.exceptions.ConnectionError) as e:
            print(str(e))
            sys.exit(1)
    except RuntimeError as e:
        print(str(e))
        sys.exit(1)
    logger.debug( "authentication successful" )
    director.interactive()
