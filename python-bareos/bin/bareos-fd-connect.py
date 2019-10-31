#!/usr/bin/python

import argparse
import bareos.bsock
from   bareos.bsock.filedaemon import FileDaemon
import logging
import sys

def getArguments():
    parser = argparse.ArgumentParser(description='Connect to Bareos File Daemon.')
    parser.add_argument('-d', '--debug', action='store_true', help="enable debugging output")
    parser.add_argument('--name', help="Name of the Director resource in the File Daemon", required=True)
    parser.add_argument('-p', '--password', help="password to authenticate to a Bareos File Daemon", required=True)
    parser.add_argument('--port', default=9102, help="Bareos File Daemon network port")
    parser.add_argument('address', nargs='?', default="localhost", help="Bareos File Daemon network address")
    parser.add_argument('command', nargs='*', help="Command to send to the Bareos File Daemon")
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    logging.basicConfig(format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    args=getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    try:
        options = [ 'address', 'port', 'name' ]
        parameter = {}
        for i in options:
            if hasattr(args, i) and getattr(args,i) != None:
                logger.debug( "%s: %s" %(i, getattr(args,i)))
                parameter[i] = getattr(args,i)
            else:
                logger.debug( '%s: ""' %(i))
        logger.debug('options: %s' % (parameter))
        password = bareos.bsock.Password(args.password)
        parameter['password']=password
        bsock=FileDaemon(**parameter)
    except RuntimeError as e:
        print(str(e))
        sys.exit(1)
    logger.debug( "authentication successful" )
    if args.command:
        print(bsock.call(args.command))
    else:
        bsock.interactive()
