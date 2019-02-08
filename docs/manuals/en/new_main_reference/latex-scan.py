#!/usr/bin/env python

# -*- coding: utf-8 -*-

from   __future__ import print_function
import argparse
import logging
#import fileinput
from   pandocfilters import toJSONFilter, Code, Str, Para, Plain
#from pyparsing import Word, alphas, alphanums, Literal, restOfLine, OneOrMore, \
#    empty, Suppress, replaceWith, nestedExpr
from   pyparsing import *
import re
import sys


#class LatexParserEod(Exception):
#    """Base class for exceptions in this module."""
#    pass

class PData(object):
    def __init__(self, text, parsed = None, replace = None, keep = None):
        self.data = {
            'source': text,
            'parsed': parsed,
            'replace': replace,
            'keep': keep
        }

    def get(self):
        if self.data['replace']:
            return str(self.data['replace'])
        else:
            return str(self.data['source'])

    def getText(self):
        if self.data['replace'] is not None:
            return str(self.data['replace'])
        else:
            return str(self.data['source'])

    def replace(self, text):
        self.data['replace'] = text

class PString(PData):
    pass

class PRegex(PData):
    def get(self):
        if self.data['replace']:
            return str(self.data['replace'])
        else:
            return str(self.data['parsed'])

class PFunction(PData):
    def get(self):
        if self.data['replace']:
            return str(self.data['replace'])
        else:
            return str(self.data['parsed'])

    def getName(self):
        return self.data['parsed']['name']

    def getParameters(self):
        return self.data['parsed']['parameters']

    def getNotEmptyParameters(self):
        return list(filter(None, self.data['parsed']['parameters']))


class ParsedResults(object):
    def __init__(self):
        self.data = []

    def __iter__(self):
        return self.data.__iter__()

    def __next__(self): # Python 3:
        return self.data.__next__()

    def next(self):
        return self.__next__()

    def append(self, item):
        self.data.append(item)

    def getTranslated(self):
        result = ''
        for item in self.data:
            result += item.getText()
        return result

    def getDump(self):
        result = ''
        for item in self.data:
            result += str(type(item)) + ":\n"
            result += str(item.data) + "\n"
            #result += item.get()
            result += "\n---\n\n"
        return result


class RegexDefs(object):
    def __init__(self):

        #elif self.parseRegex(r'$\geq$', r'>='):
        #    pass
        # done in pre_conversion_changes.sh, as it has been easier there.
        #elif self.parseRegex(r'{\textbar}', r'|'):
        #    pass
        #elif self.parseRegex(r'{\textless}', r'<'):
        #    pass
        #elif self.parseRegex(r'{\textgreater}', r'>'):
        #    pass

        self.regexOpts = re.MULTILINE | re.DOTALL | re.VERBOSE

        self.regex = {
            'Path': {
                'pattern': r'``path:(.*?)``',
                'flags':   re.VERBOSE,
                'replace': r':file:`\1`'
            },
            'VerbPath': {
                'pattern': r'\\verb\|path:(.*?)\|',
                'flags':   re.VERBOSE,
                'replace': r':file:`\1`'
            },
            'AtHash': {
                'pattern': r'@\\\#',
                'flags':   re.VERBOSE,
                'replace': r'@#'
            },
            'idir': {
                'pattern': r'\\idir\ *(.*?)\n',
                'flags':   re.VERBOSE,
                'replace': r'/_static/images/\1.*\n'
            },
            'EnvBareosConfigResource': {
                'pattern': r'::\n\n(\s*)\\begin{bareosConfigResource}{(.*?)}{(.*?)}{(.*?)}\s*\n(.*?)\n\s*\\end{bareosConfigResource}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2.d/\3/\4.conf\n\n\5'
            },
            #${PERL} 's#\{bareosConfigResource\}\{(.*?)\}\{(.*?)\}\{(.*?)\}#\n.. code-block:: sh\n    :caption: \1 \2 \3\n#g'   ${DESTFILE}
            'EnvBconfig0':  {
                'pattern': r'::\n\n\s*\\begin{bconfig}{}\s*\n(.*?)\n\s*\\end{bconfig}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\n\1'
            },
            'EnvBconfig':  {
                'pattern': r'::\n\n(\s*)\\begin{bconfig}{([^}]+?)}\s*\n(.*?)\n\s*\\end{bconfig}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            #${PERL} 's#\{bconfig\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
            'EnvBconsole0': {
                'pattern': r'::\n\n\s*\\begin{bconsole}{}\s*\n(.*?)\n\s*\\end{bconsole}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\n\1'
            },
            'EnvBconsole': {
                'pattern': r'::\n\n(\s*)\\begin{bconsole}{([^}]+?)}\s*\n(.*?)\n\s*\\end{bconsole}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            #${PERL} 's#\{bconsole\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
            'EnvBmessage0': {
                'pattern': r'::\n\n\s*\\begin{bmessage}{}\s*\n(.*?)\n\s*\\end{bmessage}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\n\1'
            },
            'EnvBmessage': {
                'pattern': r'::\n\n(\s*)\\begin{bmessage}{([^}]+?)}\s*\n(.*?)\n\s*\\end{bmessage}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            'EnvCommands0': {
                'pattern': r'::\n\n\s*\\begin{commands}{}\s*\n(.*?)\n\s*\\end{commands}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\n\1'
            },
            'EnvCommands': {
                'pattern': r'::\n\n(\s*)\\begin{commands}{([^}]+?)}\s*\n(.*?)\n\s*\\end{commands}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            #${PERL} 's#\{commands\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}d
            'EnvCommandOut': {
                'pattern': r'::\n\n(\s*)\\begin{commandOut}{([^}]+?)}\s*\n(.*?)\n\s*\\end{commandOut}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            #${PERL} 's#\{commandOut\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
            'EnvConfig0': {
                'pattern': r'::\n\n\s*\\begin{config}{}\s*\n(.*?)\n\s*\\end{config}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\n\1'
            },
            'EnvConfig': {
                'pattern': r'::\n\n(\s*)\\begin{config}{([^}]+?)}\s*\n(.*?)\n\s*\\end{config}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            #${PERL} 's#\{config\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
            'EnvLogging': {
                'pattern': r'::\n\n(\s*)\\begin{logging}{([^}]+?)}\s*\n(.*?)\n\s*\\end{logging}',
                'flags':   self.regexOpts, 
                'replace': r'.. code-block:: sh\n\1:caption: \2\n\n\3'
            },
            #${PERL} 's#\{logging\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
            'ImageReference': {
                'pattern': r'\ *\|image\|',
                'flags':   self.regexOpts,
                #'replace': None,
                'extraHandling': True
            },
            'ImageBlock': {
                'pattern': r'\.\.\s\|image\|\simage::\s\\idir\s(.*?)$(.*?)(?=\n[^\s]|^$)',
                'flags':   self.regexOpts, 
                'replace': r'',
                'return':  r'.. image:: /_static/images/\1.*\2\n\n',
                'extraHandling': True
            },
        }

        for key in self.regex.keys():
            self.regex[key]['compiled'] = re.compile(self.regex[key]['pattern'], self.regex[key]['flags'])


    def __iter__(self):
        return self.regex.__iter__()

    def getPattern(self, key):
        return self.regex[key]['compiled']

    def getPatternString(self, key):
        return self.regex[key]['pattern']

    def getReplaceTemplate(self, key):
        return self.regex[key]['replace']

    def getMatch(self, key, text):
        return self.getPattern(key).match(text)

    def getReturnValue(self, key, match):
        result = None
        if 'return' in self.regex[key]:
            result = match.expand(self.regex[key]['return'])
        return result

    def needsSpecialTreatment(self, key):
        return 'extraHandling' in self.regex[key]


class Parser(object):

    def __init__(self):
        self.data = None
        self.start = 0
        self.pos = 0
        self.length = 0
        self.regex = RegexDefs()
        self.images = []
        self.result = ParsedResults()


    def getChar(self):
        if not self.isEod():
            ch = self.data[self.pos]
            #print(ch)
            return ch

    def nextChar(self):
        if self.isEod():
            return False
        self.pos += 1
        return True

    def isEod(self):
        if self.pos + 1 >= self.length:
            self.pos = self.length
            return True
        return False

    def skipWhitespaces(self):
        # skip whitespaces
        while (not self.isEod()) and self.getChar().isspace():
            self.nextChar()

    def skipWhitespacesUntil(self, expected):
        start = self.pos
        # skip whitespaces
        while (not self.isEod()) and self.getChar().isspace():
            self.nextChar()
        if self.getChar() != expected:
            # don't skip whitespaces
            self.pos = start
            return False
        return True

    def getSourceItem(self, start = None, modifier = 0):
        logger = logging.getLogger()
        if start is None:
            start = self.start
        if self.isEod():
            if modifier:
                modifier -= 1
            result = self.data[start:(self.pos + modifier)]
            #logger.debug('eod, mod: {}, {}, full: {} ({}, {}, {})'.format(modifier, result, self.data[start:], start, self.pos, self.length))
        else:
            result = self.data[start:(self.pos + modifier)]
            #logger.debug('mod: {}, {}'.format(modifier, result))

        return result

    def isEqual(self, string):
        return string == self.data[self.pos:self.pos+len(string)]

    def appendPriorText(self):
        priorText = self.getSourceItem()
        if len(priorText) > 0:
            self.result.append(PString(priorText))

    def getFunctionName(self):
        # skip \
        self.nextChar()
        start = self.pos

        # TODO: appendix-e/verify: \normalsize\n as last line is converted to 'normalsize\n' function name.
        ch = self.getChar()
        while (not self.isEod()) and ch != ' ' and ch != '\n' and ch != '{' and ch != '}' and ch != '[' and ch != '*' and ch != '`':
            self.nextChar()
            ch = self.getChar()
        return self.getSourceItem(start)

    def getOptionalParameters(self):
        # Are multiple optional parameter allowed?
        # Maybe * and optional parameter?
        # Currently only one is returned.
        if self.isEod():
            return None
        if self.getChar() == '*':
            result = self.getSourceItem(self.pos, +1)
            self.nextChar()
            return [ result ]
        if not self.skipWhitespacesUntil(r'['):
            return None
        # skip [
        self.nextChar()
        start = self.pos
        nested = 1
        while nested > 0:
            while self.getChar() != ']':
                if self.getChar() == '[':
                    nested += 1
                self.nextChar()
            nested -= 1
            self.nextChar()
        return [ self.getSourceItem(start, -1) ]


    def getParameter(self):
        if self.isEod():
            return None
        if not self.skipWhitespacesUntil(r'{'):
            return None
        # skip {
        self.nextChar()
        start = self.pos
        nested = 1
        while nested > 0:
            while (not self.isEod()) and self.getChar() != '}':
                if self.getChar() == '{':
                    nested += 1
                self.nextChar()
            nested -= 1
            self.nextChar()
        result = self.getSourceItem(start, -1)
        if result.startswith(u'%\n'):
            result = result[2:]
        return result


    def getParameters(self):
        result = []
        parameter = self.getParameter()
        while parameter is not None:
            result.append(parameter)
            parameter = self.getParameter()
        return result


    def parseFunction(self):
        logger = logging.getLogger()
        self.start = self.pos
        result = {
            'name': None,
            'optional': [],
            'parameters': []
        }
        result['name'] = self.getFunctionName()
        #logger.debug("function: {}, {} {}".format(result['name'], self.start, self.data[self.start:self.start+30]))
        result['optional'] = self.getOptionalParameters()
        result['parameters'] = self.getParameters()
        self.result.append(PFunction(self.getSourceItem(), result))
        self.start = self.pos
        if self.isEod():
            self.start = self.length
        #logger.debug("start: {}, length: {}".format(self.start, self.length))


    def parseRawLatex(self):
        """
        :raw-latex:`\index[general]{Bareos Console}`
        """
        logger = logging.getLogger()
        # TODO: remove ':raw-latex:`...` only if function gets translated
        compareString = u':raw-latex:`'
        if not self.isEqual(compareString):
            return False
        self.appendPriorText()
        self.pos += len(compareString)
        self.parseFunction()
        if self.getChar().isspace():
            self.result.append(PString(r' '))
        if not self.skipWhitespacesUntil(r'`'):
            # matching backtick not found
            return False
        self.nextChar()
        self.start = self.pos
        return True


    def parseRawLatexBlock(self):
        """
        :raw-latex:`\index[general]{Bareos Console}`
        """
        logger = logging.getLogger()
        # TODO: remove ':raw-latex:`...` only if function gets translated
        compareString = u'.. raw:: latex\n'
        if not self.isEqual(compareString):
            return False
        self.appendPriorText()
        self.pos += len(compareString)
        self.skipWhitespaces()
        self.parseFunction()
        return True


    def parseRegex(self, key):
        logger = logging.getLogger()

        match = self.regex.getMatch(key, self.data[self.pos:])
        if not match:
            return False
        self.appendPriorText()
        self.start = self.pos
        result = {
            'regex': self.regex.getPatternString(key),
            'parameters': match.groups()
        }
        #logger.debug('pos: {}, end: {}'.format(self.pos, match.end()))
        replace = match.expand(self.regex.getReplaceTemplate(key))
        self.pos += match.end()
        self.result.append(PRegex(self.getSourceItem(), result, replace))
        self.start = self.pos
        return True

    def parseImageReferenceRegex(self):
        logger = logging.getLogger()
        key = 'ImageReference'

        if len(self.images) == 0:
            return False

        match = self.regex.getMatch(key, self.data[self.pos:])
        if not match:
            return False
        self.appendPriorText()
        self.start = self.pos
        result = {
            'regex': self.regex.getPatternString(key),
            'parameters': match.groups()
        }
        #logger.debug('pos: {}, end: {}'.format(self.pos, match.end()))
        replace = match.expand(self.images.pop(0))
        self.pos += match.end()
        self.result.append(PRegex(self.getSourceItem(), result, replace))
        self.start = self.pos
        return True


    def parseRegexReturnMatch(self, key):
        logger = logging.getLogger()

        match = self.regex.getMatch(key, self.data[self.pos:])
        if not match:
            return False
        self.appendPriorText()
        self.start = self.pos
        result = {
            'regex': self.regex.getPatternString(key),
            'parameters': match.groups()
        }
        #logger.debug('pos: {}, end: {}'.format(self.pos, match.end()))
        replace = match.expand(self.regex.getReplaceTemplate(key))
        result = self.regex.getReturnValue(key, match)
        self.pos += match.end()
        self.result.append(PRegex(self.getSourceItem(), result, replace, result))
        self.start = self.pos
        return self.regex.getReturnValue(key, match)



    def parseAllRegex(self):
        logger = logging.getLogger()

        for key in self.regex:
            if not self.regex.needsSpecialTreatment(key):
                if self.parseRegex(key):
                    return True

        value = self.parseRegexReturnMatch('ImageBlock')
        if value:
            logger.debug('adding image:\n{}'.format(value))
            self.images.append(value)
            return True

        if self.parseImageReferenceRegex():
            return True

        return False


    def parse(self, string):

        self.data = string
        self.start = 0
        self.pos = 0
        self.length = len(self.data)
        self.result = ParsedResults()


        while not self.isEod():
            if self.parseRawLatexBlock():
                pass
            elif self.parseRawLatex():
                pass
            elif self.parseAllRegex():
                pass
            elif self.getChar() == '\\':
                self.appendPriorText()
                self.parseFunction()
            else:
                self.nextChar()
        self.appendPriorText()
        return self.result



class Translate(object):

    @staticmethod
    def getDaemonName(source):
        logger = logging.getLogger()

        if source.lower() == "fd":
            return "File Daemon"
        if source.lower() == "sd":
            return "Storage Daemon"
        if source.lower() == "dir":
            return "Director"
        if source.lower() == "console":
            return "Console"
        if source.lower() == "general":
            # used by index, ignore it.
            return None
        logger.warning("Unknown daemon type {}".format(source))


    @staticmethod
    def getRstPath(path):
        # replace all single '\' (written as '\\' in Python) by '\\' (written as '\\\\' in Python), as required by RST
        newpath = re.sub(r'\\(?=[^\\])', r'\\\\', path)
        return r':file:`{0}`'.format(newpath)


    #
    # a
    #
    @staticmethod
    def argument(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def at(item):
        item.replace(b'@')

    #
    # b
    #
    @staticmethod
    def bareosWhitepaperTapeSpeedTuning(item):
        item.replace(b'\\elink{Bareos Whitepaper Tape Speed Tuning}{http://www.bareos.org/en/Whitepapers/articles/Speed_Tuning_of_Tape_Drives.html}')

    @staticmethod
    def bareosMigrateConfigSh(item):
        item.replace(b'\\elink{bareos-migrate-config.sh}{https://github.com/bareos/bareos-contrib/blob/master/misc/bareos-migrate-config/bareos-migrate-config.sh}')

    @staticmethod
    def bareosTlsConfigurationExample(item):
        item.replace(b'\\elink{Bareos Regression Testing Base Configuration}{https://github.com/bareos/bareos-regress/tree/master/configs/BASE/}')

    @staticmethod
    def bcheckmark(item):
        # REMARK: should show a Checkmark symbol.
        # As this has not worked in Latex, a 'x' is shown instead.
        # Could be adapted to show the checkmark.
        item.replace(r'x'.format(*item.getParameters()))

    @staticmethod
    def bconfigInput(item):
        item.replace(b'\n\n.. literalinclude:: /_static/{0}\n\n'.format(*item.getParameters()))

    @staticmethod
    def bcommand(item):
        logger = logging.getLogger()

        parameter = item.getNotEmptyParameters()
        if len(parameter) >= 2:
            item.replace(b':strong:`{0} {1}`'.format(*parameter))
        else:
            logger.warn("bcommand should use 2 parameter.")
            item.replace(b':strong:`{0}`'.format(*parameter))
        #${PERL} 's#:raw-latex:`\\bcommand\{(\w+)\}\{([\w/ ]+)}`#:strong:`\1 \2`#g' ${DESTFILE}
        #${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}\{}`#:strong:`\1`#g'  ${DESTFILE}
        #${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}
        #${PERL} 's#\\bcommand\{(.*?)\}\{}#:strong:`\1`#g'  ${DESTFILE}

    @staticmethod
    def bquote(item):
        item.replace(b':emphasis:`{0}`'.format(*item.getParameters()))


    #
    # c
    #
    @staticmethod
    def cmlink(item):
        # % check mark internal link
        # \newcommand{\cmlink}[1]{\ilink{\bcheckmark}{#1}}
        item.replace(r'\ilink{{\bcheckmark}}{{{0}}}'.format(*item.getParameters()))

    @staticmethod
    def configline(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))
        #${PERL} "s#:raw-latex:\`\\configline${BRACKET}\`#:strong:\`\1\`#g"  ${DESTFILE}

    @staticmethod
    def configdirective(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def configresource(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def contribDownloadBareosOrg(item):
        item.replace(r'`<http://download.bareos.org/bareos/contrib/>`_'.format(*item.getParameters()))

    @staticmethod
    def command(item):
        item.replace(r':command:`{0}`'.format(*item.getParameters()))


    #
    # d
    #
    @staticmethod
    def dbtable(item):
        item.replace(b'**{0}**'.format(*item.getParameters()))

    @staticmethod
    def developerGuide(item):
        item.replace(u'\elink{{Bareos Developer Guide ({0})}}{{http://doc.bareos.org/master/html/bareos-developer-guide.html#{0}}}'.format(*item.getParameters()))

    @staticmethod
    def bareosDeveloperGuideApiModeJson(item):
        item.replace(u'\developerGuide{{dot-commands}}'.format(*item.getParameters()))

    @staticmethod
    def bareosDeveloperGuideApiModeJson(item):
        item.replace(u'\developerGuide{{api-mode-2-json}}'.format(*item.getParameters()))

    @staticmethod
    def directory(item):
        item.replace(Translate.getRstPath(item.getParameters()[0]))

    @staticmethod
    def dt(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def dtYesNo(item):
        item.replace(b':strong:`Yes|No`'.format(*item.getParameters()))


    #
    # e
    #
    @staticmethod
    def elink(item):
        item.replace(r'`{0} <{1}>`_'.format(*item.getParameters()))

    @staticmethod
    def externalReferenceDroplet(item):
        item.replace(b'\\url{https://github.com/scality/Droplet}')

    @staticmethod
    def externalReferenceIsilonNdmpEnvironmentVariables(item):
        item.replace(b'\elink{Isilon OneFS 7.2.0 CLI Administration Guide}{https://www.emc.com/collateral/TechnicalDocument/docu56048.pdf}, section \bquote{NDMP environment variables}')


    #
    # f
    #
    @staticmethod
    def file(item):
        item.replace(Translate.getRstPath(item.getParameters()[0]))

    @staticmethod
    def fileset(item):
        item.replace(b'**{0}**'.format(*item.getParameters()))


    #
    # g
    #

    #
    # h
    #
    @staticmethod
    def hfill(item):
        item.replace(r'')

    @staticmethod
    def hide(item):
        item.replace(r''.format(*item.getParameters()))

    @staticmethod
    def host(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))


    #
    # i
    #
    @staticmethod
    def index(item):
        '''
        Uses
        :index:
        inline role instead of the

        .. index::

        block level markup.
        The inline role index links to the text parameter,
        while the block level markup should be used before (!) the relevant section.

        The LaTex optional parameter (general, dir, sd or fd) is currently ignored.

        In contrast to a LaTex index, where mulitple items describe a hierachie of indexes,
        here multiple paramter cross-link to each other.

        Example:
        \index[general]{Console!Command!. commands}
        leads to 3 indexes:
        . commands
            <Console Command, [1]>
        Command
            <. commands, Console, [1]>
        Console
            <Command . commands, [1]>

        See: http://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#index-generating-markup
        '''

        logger = logging.getLogger()
        #logger.debug(str(item.get()))
        par = item.getParameters()[0]
        items = par.split('!')
        nr = len(items)
        if nr == 1:
            #item.replace(b'\n.. index::\n   {0}   single: {0}\n'.format(*items))
            item.replace(r':index:`[TAG={0}] <single: {0}>`'.format(*items))
        elif nr == 2:
            #item.replace(b'\n.. index::\n   {0}   pair: {0}; {1}\n'.format(*items))
            item.replace(r':index:`[TAG={0}->{1}] <pair: {0}; {1}>`'.format(*items))
        elif nr == 3:
            #item.replace(b'\n.. index::\n   {0}   triple: {0}; {1}; {2}\n'.format(*items))
            item.replace(r':index:`[TAG={0}->{1}->{2}] <triple: {0}; {1}; {2}>`'.format(*items))

    #
    # j
    #
    @staticmethod
    def job(item):
        item.replace(b'**{0}**:sup:`Dir`:sub:`job`\ '.format(*item.getParameters()))

    #
    # k
    #

    #
    # l
    #
    @staticmethod
    def linkResourceDirectiveValue(item):
        item.replace(b'**{2}**:sup:`{0}`:sub:`{1}`\ = **{3}**'.format(*item.getParameters()))

    ## TODO: add link
    @staticmethod
    def linkResourceDirective(item):
        item.replace(b'**{2}**:sup:`{0}`:sub:`{1}`\ '.format(*item.getParameters()))
        #${PERL} 's#:raw-latex:`\\linkResourceDirective\*\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g' ${DESTFILE}
        #${PERL} 's#:raw-latex:`\\resourceDirectiveValue\{(.*?)\}\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ = **\4**#g' ${DESTFILE}

    @staticmethod
    def limitation(item):
        # \newcommand{\limitationNostar}[3]{%
        # \paragraph{Limitation: #1: #2.}
        # #3
        # \index[general]{Limitation!#1!#2}%
        # \index[general]{#1!Limitation!#2}%
        # }
        component, summary, text = item.getParameters()
        item.replace(b':index:`{0}: {1}. <triple: Limitation; {0}; {1}>`\n{2}\n'.format(component, summary, text))


    #
    # n
    #
    @staticmethod
    def name(item):
        item.replace(b'**{0}**'.format(*item.getParameters()))

    @staticmethod
    def NdmpBareos(item):
        # \ilink{NDMP\_BAREOS}{sec:NdmpBareos}
        item.replace(r'\ilink{NDMP_BAREOS}{section-NdmpBareos}')

    @staticmethod
    def NdmpNative(item):
        # \ilink{NDMP\_BAREOS}{sec:NdmpBareos}
        item.replace(r'\ilink{NDMP_NATIVE}{section-NdmpNative}')

    # NDMP
    @staticmethod
    def DataManagementAgent(item):
        item.replace(r'Data Management Agent')

    @staticmethod
    def DataAgent(item):
        item.replace(r'Data Agent')

    @staticmethod
    def TapeAgent(item):
        item.replace(r'Tape Agent')

    @staticmethod
    def RobotAgent(item):
        item.replace(r'Robot Agent')

    #
    # m
    #
    @staticmethod
    def multicolumn(item):
        # TODO: is there a better solution?
        item.replace(b':strong:`{2}` '.format(*item.getParameters()))
        ##${PERL} 's#:raw-latex:`\\multicolumn\{[^}]*\}\{[^}]+\}\{([^}]*)\}`#:strong:`\1`\ #g' ${DESTFILE}


    #
    # o
    #
    @staticmethod
    def os(item):
        dist, version = item.getParameters()
        if version:
            item.replace(b':strong:`{0} {1}`'.format(dist, version))
        else:
            item.replace(b':strong:`{0}`'.format(dist))
        #${PERL} 's#\\os\{(.*?)\}\{}#:strong:`\1`#g' ${DESTFILE}
        #${PERL} 's#\\os\{(.*?)\}\{(.*?)}#:strong:`\1 \2`#g' ${DESTFILE}

    #
    # p
    #
    @staticmethod
    def package(item):
        item.replace(b'**{0}**'.format(*item.getParameters()))
        #${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}

    @staticmethod
    def pagebreak(item):
        item.replace(r'')

    @staticmethod
    def parameter(item):
        # don't use :option:, as this has a special behavior, that we don't use.
        item.replace(b'``{0}``'.format(*item.getParameters()))
        #${PERL} 's#(\s+)\\parameter\{(.*?)\}(\s*)#\1                  :option:`\2`\3#g' ${DESTFILE}

    @staticmethod
    def pluginevent(item):
        item.replace(b':strong:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def pool(item):
        item.replace(b'**{0}**:sup:`Dir`:sub:`pool`\ '.format(*item.getParameters()))


    #
    # r
    #
    @staticmethod
    def registrykey(item):
        item.replace(r'``{0}``'.format(*item.getParameters()))

    @staticmethod
    def releaseUrlDownloadBareosOrg(item):
        item.replace(r'`<http://download.bareos.org/bareos/release/{0}/>`_'.format(*item.getParameters()))

    @staticmethod
    def releaseUrlDownloadBareosCom(item):
        item.replace(r'`<http://download.bareos.com/bareos/release/{0}/>`_'.format(*item.getParameters()))

    @staticmethod
    def resourcename(item):
        item.replace(b'**{2}**:sup:`{0}`:sub:`{1}` '.format(*item.getParameters()))

    @staticmethod
    def resourcetype(item):
        item.replace(b':sup:`{0}`\ :strong:`{1}`'.format(*item.getParameters()))


    #
    # s
    #
    @staticmethod
    def sdBackend(item):    
        #\newcommand{\sdBackend}[2]{%
        #\ifthenelse{\isempty{#2}}{%
        #\path|#1|%
        #\index[general]{#1}%
        #\index[sd]{Backend!#1}%
        #}{%
        #\path|#1| (#2)%
        #\index[general]{#1 (#2)}%
        #\index[general]{#2!#1}%
        #\index[sd]{Backend!#1 (#2)}%
        #}%
        #}
        name, backend = item.getParameters()
        if backend:
            item.replace(b'**{0}** ({1})'.format(name, backend))
        else:
            item.replace(b'**{0}**'.format(name))

    @staticmethod
    def sinceVersion(item):
        #% 1: daemon (dir|sd|fd),
        #% 2: item,
        #% 3: version
        #\edef\pv{#3}%
        #\ifcsvoid{pv}{}{%
        #Version $>=$ #3%
        #%\index[#1]{#2}%
        #% expand variables
        #\edef\temp{\noexpand\index[general]{bareos-#3!#2}}%
        #\temp%
        #}%

        # TODO: use versionadded:: ?

        logger = logging.getLogger()

        par = item.getParameters()

        newitems = []
        version = par[2]
        newitems.append(version)
        newitems.append('bareos-{}'.format(version))
        daemon = Translate.getDaemonName(par[0])
        #if daemon is not None:
        #    newitems.append(daemon)

        # Parameter 1 can contain additional index separators
        items = par[1].split('!')
        newitems.extend(items)

        nr = len(newitems)-1
        if nr == 2:
            item.replace(r':index:`Version >= {0} <pair: {1}; {2}>`'.format(*newitems))
        elif nr >= 3:
            item.replace(r':index:`Version >= {0} <triple: {1}; {2}; {3}>`'.format(*newitems))
            if nr > 3:
                logger.warning('sinceVersion: only using the first 3 items of {}. Given: {}'.format(nr, str(par)))
        ##              raw-latex:`\sinceVersion{dir}{requires!jansson}{15.2.0}`
        ##sed  's#:raw-latex:`\\sinceVersion\{(.*)\}\{(.*)\}\{(.*)\}`#\n\n.. versionadded:: \3\n   \1 \2\n#g'   ${DESTFILE}
        #${PERL} 's|:raw-latex:`\\sinceVersion\{(.*)\}\{(.*)\}\{(.*)\}`|\3|g'   ${DESTFILE}

    # moved to preconversion, to avoid errors.
    #@staticmethod
    #def subsubsubsection(item):
    #    name = item.getParameters()[0]
    #    item.replace(u'{0}\n{1}\n'.format(name, len(name)*'"'))

    #
    # t
    #
    @staticmethod
    def textbar(item):
        item.replace(b'|')

    @staticmethod
    def textbf(item):
        item.replace(r':strong:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def textrightarrow(item):
        item.replace(b'->')
        #${PERL} 's#:raw-latex:`\\textrightarrow`#->#g' ${DESTFILE}
        #${PERL} 's#:raw-latex:`\\textrightarrow `#->#g' ${DESTFILE}

    @staticmethod
    def ticket(item):
        item.replace(r':issue:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def TODO(item):
        item.replace(r'.. TODO: {0}'.format(*item.getParameters()))

    @staticmethod
    def trademark(item):
        # TODO: fix this
        item.replace(r'\ :sup:`(TM)`')

    #
    # u
    #
    @staticmethod
    def url(item):
        item.replace(r'`<{0}>`_'.format(*item.getParameters()))
        ##``URL:http://download.bareos.org/bareos/release/latest/``
        #${PERL} 's#``URL:(.*?)``#`\1 <\1>`_#g'   ${DESTFILE}
        #${PERL} 's#``\\url\{(.*?)\}#`\1 \1>`_#g'   ${DESTFILE}

    @staticmethod
    def user(item):
        item.replace(b'**{0}**'.format(*item.getParameters()))
        #${PERL} "s#:raw-latex:\`\\user${BRACKET}\`#**\1**#g"   ${DESTFILE}
        ##${PERL} 's#\\user\{(.*)\}#**\1**#g'   ${DESTFILE}


    #
    # v
    #
    @staticmethod
    def volumestatus(item):
        item.replace(b'**{0}**'.format(*item.getParameters()))

    @staticmethod
    def volumeparameter(item):
        parameter, value = item.getParameters()
        if value:
            item.replace(b'**{0} = {1}**'.format(parameter, value))
        else:
            item.replace(b'**{0}**'.format(parameter))
        #\newcommand{\volumeparameter}[2]{\ifthenelse{\isempty{#2}}{%
        #  \path|#1|%
        # }{%
        #  \path|#1 = #2|%
        #}}

    @staticmethod
    def verbatiminput(item):
        # file must be in the source/ directory (at least there have to be a link)
        item.replace(b'\n\n.. literalinclude:: /_static/{0}\n\n'.format(*item.getParameters()))


    #
    # w
    #
    @staticmethod
    def warning(item):
        # TODO: prepand indention level (\s*, \s*|\s, arbitrary text)
        #item.replace(b'\n.. warning::\n   {0}'.format(*item.getParameters()))
        text = item.getParameters()[0]
        indenttext = '   '
        # get indention of 2. line
        match = re.match(r'.*?\n(\s*).*', text)
        if match:
            indenttext = match.expand(r'\1')
        indentwarning = ''
        if len(indenttext) >= 3:
            indentwarning = ' ' * (len(indenttext) - 3)
        item.replace(b'\n\n{0}.. warning::\n{1}{2}'.format(indentwarning, indenttext, text))
        ## warning
        #${PERL0} 's#:raw-latex:`\\warning${BRACKET}`#\n.. warning:: \n  \1#ims' ${DESTFILE}
        ##${PERL0} 's#\\warning\{(.*?)\}#\n.. warning:: \n  \1#ims'  ${DESTFILE}

        ## clean empty :: at the start of line
        #${PERL} 's/\s*::$//g' ${DESTFILE}

    #
    # y
    #
    @staticmethod
    def yesno(item):
        item.replace(b'yes\\|no`')

    #
    ## link targets
    #
    @staticmethod
    def ilink(item):
        item.replace(b':ref:`{0} <{1}>`'.format(*item.getParameters()))
        #${PERL} 's#:raw-latex:`\\ilink\{(.*?)\}\{(.*?)\}`#:ref:`\1 <\2>`#g' ${DESTFILE}

    @staticmethod
    def nameref(item):
        item.replace(b':ref:`{0}`'.format(*item.getParameters()))

    @staticmethod
    def label(item):
        item.replace(b'\n\n.. _{0}:\n\n'.format(*item.getParameters()))
        #${PERL} 's#:raw-latex:`\\label\{(.*)\}`#\n\n.. _`\1`: \1#g'   ${DESTFILE}


    #
    ## abbreviations
    #
    @staticmethod
    def bareosDir(item):
        item.replace(r'|bareosDir|')

    @staticmethod
    def bareosFd(item):
        item.replace(r'|bareosFd|')

    @staticmethod
    def bareosSd(item):
        item.replace(r'|bareosSd|')

    @staticmethod
    def bareosTrayMonitor(item):
        item.replace(b'|bareosTrayMonitor|')

    @staticmethod
    def bareosWebui(item):
        item.replace(b'|bareosWebui|')

    @staticmethod
    def mysql(item):
        item.replace(b'|mysql|')

    @staticmethod
    def postgresql(item):
        item.replace(b'|postgresql|')

    @staticmethod
    def sqlite(item):
        item.replace(b'|sqlite|')

    @staticmethod
    def vmware(item):
        item.replace(b'|vmware|')

    @staticmethod
    def vSphere(item):
        item.replace(b'|vsphere|')


    #
    # Paths
    #

    ## TODO: check if |xxxx| replace can be used
    @staticmethod
    def configFileDirUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-dir.conf`')

    @staticmethod
    def fileStoragePath(item):
        item.replace(b':file:`/var/lib/bareos/storage/`')

    @staticmethod
    def scriptPathUnix(item):
        item.replace(b':file:`/usr/lib/bareos/scripts/`')

    @staticmethod
    def configPathUnix(item):
        item.replace(b':file:`/etc/bareos/`')

    @staticmethod
    def configFileDirUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-dir.conf`')

    @staticmethod
    def configFileSdUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-sd.conf`')

    @staticmethod
    def configFileFdUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-fd.conf`')

    @staticmethod
    def configFileBconsoleUnix(item):
        item.replace(b':file:`/etc/bareos/bconsole.conf`')

    @staticmethod
    def configFileTrayMonitorUnix(item):
        item.replace(b':file:`/etc/bareos/tray-monitor.conf`')

    @staticmethod
    def configDirectoryDirUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-dir.d/`')

    @staticmethod
    def configDirectorySdUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-sd.d/`')

    @staticmethod
    def configDirectoryFdUnix(item):
        item.replace(b':file:`/etc/bareos/bareos-fd.d/`')

    @staticmethod
    def configDirectoryTrayMonitorUnix(item):
        item.replace(b':file:`/etc/bareos/tray-monitor.d/`')

    @staticmethod
    def logfileUnix(item):
        item.replace(b':file:`/var/log/bareos/bareos.log`')

    #${PERL} 's#:raw-latex:`\\configFileBatUnix`#:file:`/etc/bareos/bat.conf`#g'   ${DESTFILE}
    #${PERL} 's#:raw-latex:`\\configDirectoryBconsoleUnix`#:file:`/etc/bareos/bconsole.d/`#g'   ${DESTFILE}


    ## remove stuff that has no meaning anymore
    @staticmethod
    def small(item):
        if item.getParameters():
            item.replace(r'{0}'.format(*item.getParameters()))
        else:
            item.replace(r'')

    @staticmethod
    def footnotesize(item):
        if item.getParameters():
            item.replace(b'{0}'.format(*item.getParameters()))
        else:
            item.replace(b'')

    @staticmethod
    def normalsize(item):
        if item.getParameters():
            item.replace(b'{0}'.format(*item.getParameters()))
        else:
            item.replace(b'')

    #
    # not required
    #

    #${PERL} 's#:math:`\ldots`#...#g'   ${DESTFILE}

    ## remove nonsense lines
    #${PERL} 's#   .. raw:: latex##g'   ${DESTFILE}

    #${PERL} 's#:raw-latex:`\\ref\{(.*?)\}`#`<\1>`_#g' ${DESTFILE}

    #
    # TODO
    #

    # images:
    # \includegraphics[width=0.8\textwidth]{\idir win-install-1}
    # =>
    # .. image:: images/win-install-1.*
    # :width: 80%
    # currently done by incomplete substition. Fails if more than 1 image is defined per file.

    ## highlighting in listings/code blocks

    #${PERL} 's#\<input\>##g'  ${DESTFILE}
    #${PERL} 's#\</input\>##g'  ${DESTFILE}
    #${PERL} 's#\<command\>##g'  ${DESTFILE}
    #${PERL} 's#\</command\>##g'  ${DESTFILE}
    # <command>
    # <parameter>

    ## rename sec: to section- as : has special meaning in sphinx (done in pre)
    #${PERL} 's#sec:#section-#g'   ${DESTFILE}
    #${PERL} 's#item:#item-#g'   ${DESTFILE}



def parseAndTranslate(data):
    logger = logging.getLogger()

    inputdata = ''
    outputdata = data
    counter = 1
    parser = Parser()

    while inputdata != outputdata:
        inputdata = outputdata
        try:
            parsedResult = parser.parse(inputdata)
        except IndexError:
            print(parser.result.getDump())
            raise

        for item in parsedResult:
           if type(item) == PFunction:
               # if Translate class has a method with the name of the latex function,
               # call it.
               if hasattr(Translate, item.getName()) and callable(getattr(Translate, item.getName())):
                   getattr(Translate, item.getName())(item)
               else:
                   logger.warning("WARNING: no translation found for {}".format(item.get()))

        outputdata = parsedResult.getTranslated()

        logger.debug('DUMP {}:\n{}'.format(counter, parsedResult.getDump()))
        #print("RESULT {}:".format(counter))
        #print(outputdata)

        counter += 1

    return outputdata


def pandocfilter(key, value, xformat, meta):
    '''
    {"t":"RawInline","c":["latex","\\index[general]{Command!bconsole}"]}]}
    '''
    logger = logging.getLogger()
    if key == "RawInline" or key == 'RawBlock':
        logger.debug('value {}'.format(value))
        logger.debug('format {}'.format(xformat))
        logger.debug('meta {}'.format(meta))
        result = parseAndTranslate(value[1])
        logger.debug('result: {}'.format(result))
        if key == "RawInline":
            return Str(result)
            #return Code(None, result)
            #return Code(('', [], []), result)
        elif key == 'RawBlock':
            return Plain([Str(result)])
    return None


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    logger = logging.getLogger()

    parser = argparse.ArgumentParser(description='Convert LaTex to RST.')
    parser.add_argument('-s', '--standalone', action='store_true', help='Run as standalone program, not Pandoc filter.')
    parser.add_argument('extra', nargs='*')
    args = parser.parse_args()

    if args.standalone:
        data = []
        #filename = sys.stdin
        #textfile = open(filename, 'r')
        #data = textfile.read()
        #textfile.close()
        #for line in fileinput.input():
        #    data.append(line)
        #fileinput.close()
        data = ''.join(sys.stdin.readlines())
        result = parseAndTranslate(data)
        print(result)
    else:
        # called as Pandoc filter
        toJSONFilter(pandocfilter)
