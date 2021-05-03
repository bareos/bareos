#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

import logging

PLUGINS = list()
CHECKERS = dict()
MODIFIERS = dict()


def get_matching_plugins(file_, plugins):
    matched_plugins = []
    for pattern, pattern_plugins in plugins.items():
        if file_.match(pattern):
            logging.debug("File '{}' matches pattern '{}'".format(file_, pattern))
            matched_plugins += pattern_plugins
    return matched_plugins


def get_checkers(file_):
    return get_matching_plugins(file_, CHECKERS)


def get_modifiers(file_):
    return get_matching_plugins(file_, MODIFIERS)


def register_checker(*args, name=None):
    def decorator(_func):
        for pattern in args:
            if pattern not in CHECKERS:
                CHECKERS[pattern] = list()
            CHECKERS[pattern].append((name, _func))
            PLUGINS.append(_func)
        return _func

    return decorator


def register_modifier(*args, name=None):
    def decorator(_func):
        for pattern in args:
            if pattern not in MODIFIERS:
                MODIFIERS[pattern] = list()
            MODIFIERS[pattern].append((name, _func))
            PLUGINS.append(_func)
        return _func

    return decorator
