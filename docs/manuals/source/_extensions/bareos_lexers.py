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
