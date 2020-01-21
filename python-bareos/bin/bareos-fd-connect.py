#!/usr/bin/python

import argparse
import bareos.bsock
from bareos.bsock.filedaemon import FileDaemon
import logging
import sys


def getArguments():
    argparser = argparse.ArgumentParser(description="Connect to Bareos File Daemon.")
    argparser.add_argument(
        "-d", "--debug", action="store_true", help="enable debugging output"
    )
    bareos.bsock.FileDaemon.argparser_add_default_command_line_arguments(argparser)
    argparser.add_argument(
        "command", nargs="*", help="Command to send to the Bareos File Daemon"
    )
    args = argparser.parse_args()
    return args


if __name__ == "__main__":
    logging.basicConfig(
        format="%(levelname)s %(module)s.%(funcName)s: %(message)s", level=logging.INFO
    )
    logger = logging.getLogger()

    args = getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    bareos_args = bareos.bsock.FileDaemon.argparser_get_bareos_parameter(args)
    logger.debug("options: %s" % (bareos_args))
    try:
        bsock = FileDaemon(**bareos_args)
    except (bareos.exceptions.Error) as e:
        print(str(e))
        sys.exit(1)
    logger.debug("authentication successful")
    if args.command:
        print(bsock.call(args.command))
    else:
        bsock.interactive()
