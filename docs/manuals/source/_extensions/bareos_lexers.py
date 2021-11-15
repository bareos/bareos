#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2020 Bareos GmbH & Co. KG
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

from pygments.lexer import RegexLexer, inherit, bygroups
from pygments.lexers.shell import BashLexer
from pygments.token import *

#
# http://pygments.org/docs/lexerdevelopment/
# http://pygments.org/docs/tokens/
#

# Test:
#  cat consolidate.cfg | pygmentize -l extensions/bareos_lexers.py:BareosConfigLexer -x
#


class BareosBaseLexer(BashLexer):
    name = "BareosBase"

    tokens = {
        "root": [
            # (r'(<input>)(.*)(</input>)', bygroups(None, Generic.Emph, None)),
            # (r'(<input>)(.*)(</input>)', bygroups(None, Generic.Strong, None)),
            (r"(<input>)(.*)(</input>)", bygroups(None, Generic.Heading, None)),
            (r"(<strong>)(.*)(</strong>)", bygroups(None, Generic.Strong, None)),
            inherit,
        ]
    }


class BareosConfigLexer(BareosBaseLexer):
    name = "BareosConfig"
    aliases = ["bareosconfig", "bconfig"]
    filenames = ["*.cfg"]

    tokens = {"root": [inherit]}


class BareosConsoleLexer(BareosBaseLexer):
    name = "BareosConsole"
    aliases = ["bareosconsole", "bconsole"]
    # filenames = ['*.cfg']

    tokens = {"root": [inherit]}


class BareosLogLexer(BareosBaseLexer):
    name = "BareosLog"
    aliases = ["bareoslog"]
    filenames = ["*.log"]

    tokens = {"root": [inherit]}


class BareosMessageLexer(BareosBaseLexer):
    name = "BareosMessage"
    aliases = ["bareosmessage", "bmessage"]
    # filenames = ['*.log']

    tokens = {"root": [inherit]}
