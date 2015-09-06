#!/usr/bin/python

import argparse
import bareos.bareosfuse
import bareos.bsock
import fuse
import logging
import sys

LOG_FILENAME        = '/tmp/bareos-fuse-bsock.log'

#def getArguments():
    #parser = argparse.ArgumentParser(description='Console to Bareos Director.' )
    #parser.add_argument( '-d', '--debug', action='store_true', help="enable debugging output" )
    #parser.add_argument( '-p', '--password', help="password to authenticate at Bareos Director" )
    #parser.add_argument( '--port', default=9101, help="Bareos Director network port" )
    #parser.add_argument( 'address', default="localhost", help="Bareos Director host" )
    #parser.add_argument( 'dirname', nargs='?', default=None, help="Bareos Director name" )
    #args = parser.parse_args()
    #return args

if __name__ == '__main__':
    # initialize logging
    #logging.basicConfig(filename=LOG_FILENAME,level=logging.DEBUG,format="%(asctime)s %(process)5d(%(threadName)s) %(levelname)-7s %(module)s %(funcName)s( %(message)s )")    
    if sys.argv[1] == "--test":
        logging.basicConfig(level=logging.DEBUG,format="%(levelname)-7s %(module)s %(funcName)s( %(message)s )")    
    else:
        logging.basicConfig(filename=LOG_FILENAME,level=logging.DEBUG,format="%(asctime)s  %(levelname)-7s %(module)s %(funcName)s( %(message)s )")    
    logger = logging.getLogger()
    
    #args=getArguments()
    #if args.debug:
    #logger.setLevel(logging.DEBUG)
        
    usage = """
    Bareos Fuse filesystem: displays files from Bareos backups as a (userspace) filesystem.
                       Internaly, it uses the Bareos bconsole.

    """ + fuse.Fuse.fusage

    args = {
        'address': "localhost",
        'port': 8001,
        'password': "RGd7GuCtX+5osHzRoGzJYXkwUpevs2OEMozPOZvbpi4f",
    }

    password = bareos.bsock.Password(args['password'])
    director = bareos.bsock.BSockJson(address=args['address'], port=args['port'], password=password)

    if sys.argv[1] == "--test":
        test = bareos.bareosfuse.BareosWrapper(bsock=director)
        print test.readdir("", 0)
        test.getattr("")
        print test.readdir("/jobs", 0)
        test.getattr("/jobs")
        print test.readdir("/NOT", 0)
        test.getattr("/NOT")
    else:
        fs = bareos.bareosfuse.BareosFuse(
            #version="%prog: " + BAREOS_FUSE_VERSION,
            usage = usage,
            # required to let "-s" set single-threaded execution
            dash_s_do = 'setsingle',
            bsockjson = director,
        )

        #server.parser.add_option(mountopt="root", metavar="PATH", default='/',help="mirror filesystem from under PATH [default: %default]")
        #server.parse(values=server, errex=1)
        fs.parse()

        fs.main()
