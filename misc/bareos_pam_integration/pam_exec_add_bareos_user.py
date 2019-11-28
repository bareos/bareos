#!/usr/bin/env python

from __future__ import print_function
import argparse
import bareos.bsock
import logging
import os
from   pprint import pformat
import random
import string
import sys


def get_random_string(length=16, chars=string.ascii_letters + string.digits):
    return ''.join(random.choice(chars) for x in range(length))

def get_user_names(director):
    result = director.call('.consoles')['consoles']
    users = [job['name'] for job in result]
    return users


def does_user_exists(director, username):
    return username in get_user_names(director)


def add_user(director, username, profile):
    # The password should not be used,
    # however is required to create a new console.
    # Therefore we set a random password.
    password = 'PAM_WORKAROUND_' + get_random_string()
    result = director.call('configure add console="{username}" password="{password}" profile="{profile}"'.format(username = username, password = password, profile = profile))
    
    try:
        if result['configure']['add']['name'] != username:
            logger.error('Failed to create user {}.'.format(username))
            #logger.debug(str(result))
            return False
    except KeyError:
        logger.debug('result: {}'.format(pformat(result)))
        errormessage = pformat(result)
        try:
            errormessage = ''.join(result['error']['data']['messages']['error'])
        except KeyError:
            pass
        print('Failed to add user {}:\n'.format(username))
        print('{}'.format(errormessage))
        return False
    
    return True


def getArguments():
    parser = argparse.ArgumentParser(description='Console to Bareos Director.')
    parser.add_argument('-d', '--debug', action='store_true',
                        help="enable debugging output")
    parser.add_argument('--name', default="*UserAgent*",
                        help="use this to access a specific Bareos director named console. Otherwise it connects to the default console (\"*UserAgent*\")")
    parser.add_argument('--password', '-p', required=True,
                        help="password to authenticate to a Bareos Director console")
    parser.add_argument('--port', default=9101,
                        help="Bareos Director network port")
    parser.add_argument('--dirname', help="Bareos Director name")
    parser.add_argument('--profile', default='webui-admin',
                        help="Bareos Profile for the newly generated user")    
    parser.add_argument('--address', default="localhost",
                        help="Bareos Director network address")
    parser.add_argument('--username', help="Name of the user to add. Default: content of ENV(PAM_USER)")
    args = parser.parse_args()
    return args


if __name__ == '__main__':
    logging.basicConfig(
        format='%(levelname)s %(module)s.%(funcName)s: %(message)s', level=logging.INFO)
    logger = logging.getLogger()

    args = getArguments()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    try:
        options = ['address', 'port', 'dirname', 'name']
        parameter = {}
        for i in options:
            if hasattr(args, i) and getattr(args, i) != None:
                logger.debug("%s: %s" % (i, getattr(args, i)))
                parameter[i] = getattr(args, i)
            else:
                logger.debug('%s: ""' % (i))
        logger.debug('options: %s' % (parameter))
        password = bareos.bsock.Password(args.password)
        parameter['password'] = password
        director = bareos.bsock.DirectorConsoleJson(**parameter)
    except RuntimeError as e:
        print(str(e))
        sys.exit(1)
    logger.debug("authentication successful")

    username = os.getenv('PAM_USER', args.username)
    profile = getattr(args, 'profile', 'webui-admin')

    if username is None:
        logger.error('Failed: username not given.')
        sys.exit(1)
        
    if does_user_exists(director, username):
        print('Skipped. User {} already exists.'.format(username))
        sys.exit(0)
    
    if not add_user(director, username, profile):
        logger.error('Failed to add user {}.'.format(username))
        sys.exit(1)
        
    print('Added user {} (with profile {}) to Bareos.'.format(username, profile))
