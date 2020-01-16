#!/usr/bin/env python

"""
This script is intended to be integrated with pam_exec into the Linux authentication process.
It can be used to auto-create users that do already exist as PAM users as Bareos users.

Requires Bareos >= 19.2.4.
"""

from __future__ import print_function
import argparse
import bareos.bsock
import bareos.exceptions
import logging
import os
from pprint import pformat
import random
import string
import sys


def check_requirements(director):
    min_version = "19.2.4"
    logger = logging.getLogger()
    logger.debug("checking requirements: start")
    try:
        result = director.call(".users")
    except (bareos.exceptions.Error) as e:
        logger.error(
            "The .users command is required, but not available: {}".format(str(e))
        )
        sys.exit(1)
    try:
        result = director.call("version")
    except (bareos.exceptions.Error) as e:
        logger.error(
            "The version command is required, but not available: {}".format(str(e))
        )
        sys.exit(1)
    version = result["version"]["version"]
    if version < min_version:
        logger.error(
            "Bareos has version {}. However Bareos >= {} is required.".format(
                version, min_version
            )
        )
        sys.exit(1)
    logger.debug("checking requirements: finish")


def get_user_names(director):
    result = director.call(".users")["users"]
    users = [i["name"] for i in result]
    return users


def does_user_exists(director, username):
    return username in get_user_names(director)


def add_user(director, username, profile):

    result = director.call(
        'configure add user="{username}" profile="{profile}"'.format(
            username=username, profile=profile
        )
    )

    try:
        if result["configure"]["add"]["name"] != username:
            logger.error("Failed to create user {}.".format(username))
            # logger.debug(str(result))
            return False
    except KeyError:
        logger.debug("result: {}".format(pformat(result)))
        errormessage = pformat(result)
        try:
            errormessage = "".join(result["error"]["data"]["messages"]["error"])
        except KeyError:
            pass
        print("Failed to add user {}:\n".format(username))
        print("{}".format(errormessage))
        return False

    return True


def getArguments():
    argparser = argparse.ArgumentParser(
        description="Add a PAM user to Bareos Director."
    )
    argparser.add_argument(
        "-d", "--debug", action="store_true", help="enable debugging output"
    )
    bareos.bsock.DirectorConsole.argparser_add_default_command_line_arguments(argparser)
    argparser.add_argument(
        "--username", help="Name of the user to add. Default: content of ENV(PAM_USER)"
    )
    argparser.add_argument(
        "--profile",
        default="webui-admin",
        help="Bareos Profile for the newly generated user",
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

    bareos_args = bareos.bsock.DirectorConsole.argparser_get_bareos_parameter(args)
    logger.debug("options: %s" % (bareos_args))

    try:
        director = bareos.bsock.DirectorConsoleJson(**bareos_args)
    except (bareos.exceptions.Error) as e:
        print(str(e))
        sys.exit(1)
    logger.debug("authentication successful")

    check_requirements(director)

    username = os.getenv("PAM_USER", args.username)
    profile = getattr(args, "profile", "webui-admin")

    if username is None:
        logger.error("Failed: username not given.")
        sys.exit(1)

    if does_user_exists(director, username):
        print("Skipped. User {} already exists.".format(username))
        sys.exit(0)

    if not add_user(director, username, profile):
        logger.error("Failed to add user {}.".format(username))
        sys.exit(1)

    print("Added user {} (with profile {}) to Bareos.".format(username, profile))
