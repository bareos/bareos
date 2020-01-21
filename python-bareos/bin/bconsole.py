#!/usr/bin/python

from __future__ import print_function
import argparse
import bareos.bsock
import bareos.exceptions
import logging
import sys


def getArguments():
    argparser = argparse.ArgumentParser(description="Console to Bareos Director.")
    argparser.add_argument(
        "-d", "--debug", action="store_true", help="enable debugging output"
    )
    bareos.bsock.DirectorConsole.argparser_add_default_command_line_arguments(argparser)
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

    bareos_args = bareos.bsock.DirectorConsole.argparser_get_bareos_parameter(args)
    logger.debug("options: %s" % (bareos_args))
    try:
        director = bareos.bsock.DirectorConsole(**bareos_args)
    except (bareos.exceptions.Error) as e:
        print(str(e))
        sys.exit(1)

    logger.debug("authentication successful")
    director.interactive()
