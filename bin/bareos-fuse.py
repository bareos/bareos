#!/usr/bin/python

import argparse
import bareos.fuse
import bareos.bsock
import fuse
import logging
import sys

LOG_FILENAME        = '/tmp/bareos-fuse-bsock.log'

if __name__ == '__main__':
    # initialize logging
    #logging.basicConfig(filename=LOG_FILENAME,level=logging.DEBUG,format="%(asctime)s %(process)5d(%(threadName)s) %(levelname)-7s %(module)s %(funcName)s( %(message)s )")    
    if sys.argv[1] == "--test":
        logging.basicConfig(level=logging.DEBUG,format="%(levelname)-7s %(module)s %(funcName)s( %(message)s )")    
    else:
        logging.basicConfig(filename=LOG_FILENAME,level=logging.DEBUG,format="%(asctime)s  %(levelname)-7s %(module)s %(funcName)s( %(message)s )")    
    logger = logging.getLogger()
    
    usage = """
    Bareos Fuse filesystem: displays files from Bareos backups as a (userspace) filesystem.

    """ + fuse.Fuse.fusage

    if sys.argv[1] == "--test":
        test = bareos.bareosfuse.BareosWrapper(bsock=director)
        print test.readdir("", 0)
        test.getattr("")
        print test.readdir("/jobs", 0)
        test.getattr("/jobs")
        print test.readdir("/NOT", 0)
        test.getattr("/NOT")
    else:
        fs = bareos.fuse.BareosFuse(
            #version="%prog: " + BAREOS_FUSE_VERSION,
            usage = usage,
            # required to let "-s" set single-threaded execution
            dash_s_do = 'setsingle',
        )

        fs.parser.add_option(
            mountopt="address",
            metavar="BAREOS_DIRECTOR",
            default='localhost',
            help="address of the Bareos Director to connect [default: \"%default\"]")
        fs.parser.add_option(
            mountopt="port",
            metavar="PORT",
            default='9101',
            help="address of the Bareos Director to connect [default: \"%default\"]")
        fs.parser.add_option(
            mountopt="dirname",
            metavar="NAME",
            default='',
            help="name of the Bareos Director to connect [default: \"%default\"]")
        fs.parser.add_option(
            mountopt="name",
            metavar="NAME",
            help="name of the Bareos Named Console")
        fs.parser.add_option(
            mountopt="password",
            metavar="PASSWORD",
            default='',
            help="password to authenticate at Bareos Director [default: \"%default\"]")

        fs.parse(values=fs, errex=1)

        fs.main()
