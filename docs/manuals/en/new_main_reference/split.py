#!/usr/bin/env python

# -*- coding: utf-8 -*-


'''
split one LaTex file by part and chapter,
and create an RST index (toctree) file.
'''

from   __future__ import print_function
import argparse
from   docutils.core import publish_doctree, publish_parts
import docutils.nodes
import errno
import logging
import os
from   pprint import pprint
import re
import textwrap

filetext = r'''
\begin{document}

\sloppy
\parindent 0pt

\x1
\part{1 Intro}

Bla1

\chapter{1.1 chap}

\x2
\part{2}

Blu2

\chapter{2.1 chap}

chap21


\appendix
\part{3 Appendix}

BlaApp

\x4
\part{4 Index}

BluIndex

% \printindex[monitor]

\end{document}
'''


indexContent = textwrap.dedent('''\
    Bareos Main Reference and Documentation
    =======================================

    for Bareos version |version| release |release|, created |today|

    Welcome to the new Bareos documentation. The documentation is being
    converted to the format that you are currently reading.
    As the conversion is automated, and there are still some things that are not
    perfectly converted, we still have the `old documentation <http://doc.bareos.org>`_ available.


    The Information regarding the newest release in the :ref:`bareos-current-releasenotes`.
    
    ''')


def convertToFilename(string):
    
    if string is None:
        return ''

    # index file should stay lowercase.
    if string == 'index':
        return string

    result = ''
    
    # replace / by " "
    string1 = re.sub('/', ' ', string)
    # remove all non-standard characters 
    string2 = re.sub('[^a-zA-Z0-9 ]', '', string1)
    for i in string2.split(' '):
        result += i.capitalize()
    return result


def write(path, content, write = True):

    if not content:
        print('FILE {}: [no content]'.format(path))
        return

    dirname = os.path.dirname(path)
    
    #print('FILE {}: [\n{} ...\n]'.format(path, content[0:120]))

    if write:
        if dirname:
            if not os.path.exists(dirname):
                try:
                    os.makedirs(dirname)
                except OSError as exc: # Guard against race condition
                    if exc.errno != errno.EEXIST:
                        raise
        
        with open(path, 'w') as textfile:
            textfile.write(content)
        
    return True




class LatexNode(object):
    def __init__(self, level, title, content):
        self.level = level
        self.title = title
        self.content = content
        
    def __repr__(self):
        return 'LatexNode({}, {}, {})'.format(self.level, self.title, self.content[0:10])
    
    def write(self, targetpath):
        path = '{}/{}.tex'.format(targetpath, convertToFilename(self.title))
        write(path, self.content)


class LatexSplit(object):

    def __init__(self, text, keyword, level, targetpath, relpath, write = True):
        self.text = text
        self.keyword = keyword
        self.level = level
        self.targetdir = targetpath
        self.relpath = relpath        
        self.writeFiles = write
        self.prefix = None
        self.nodes = []

        self.split()


    def split(self):

        partPattern = r'({keyword}{{([^}}]+)}})'.format(keyword=self.keyword)
        findAll = re.split(r'{partPattern}'.format(partPattern=partPattern), self.text, flags = re.MULTILINE | re.DOTALL)
        #print(len(findAll))
        #print(findAll)

        self.prefix = findAll[0]
        i = 1
        while i + 2 <= len(findAll):
            header = findAll[i+0]
            title = findAll[i+1]
            content = header + findAll[i+2]

            # special handling for the Configuration Part,
            # to make URLs shorter.
            if title == 'Director Configuration':
                title = 'Director'
            elif title == 'Storage Daemon Configuration':
                title = 'Storage Daemon'
            elif title == 'Client/File Daemon Configuration':
                title = 'File Daemon'
            elif title == 'Messages Configuration':
                title = 'Messages'
            elif title == 'Console Configuration':
                title = 'Console'
            elif title == 'Monitor Configuration':
                title = 'Monitor'

            self.nodes.append(LatexNode(self.level, title, content))
            i += 3

    def getPrefix(self):
        return self.prefix
        
        
    def getNodes(self):
        return self.nodes

 
    def write(self):
        content = '{}\n{}'.format(self.getPrefix(), self.getTocAsTexVerbatim())
        path = os.path.normpath('{}/{}'.format(self.targetdir, self.relpath))
        write('{}.tex'.format(path), content, self.write)
        
        for node in self.nodes:
            node.write(path)

    
    def getToc(self):
        content  = '\n'
        content += '.. toctree::\n\n'

        for section in self.nodes:
            if self.relpath:
                path = os.path.normpath('/{}/{}.rst'.format(self.relpath, convertToFilename(section.title)))
            else:
                path = os.path.normpath('/{}.rst'.format(convertToFilename(section.title)))
            content += '   {}\n'.format(path)
        
        return content


    def getTocAsTexVerbatim(self):
        content  = '\n'
        content += '\\begin{verbatim}RST:'
        content += self.getToc()
        content += '\\end{verbatim}\n'
        return content
        
        

class LatexParts(LatexSplit):
    
    def __init__(self, text, targetdir, relpath, write = True):
        
        super(LatexParts, self).__init__(text, r'\\part', 1, targetdir, relpath, write)
        self.toc_extra_nodes = [ '/releasenotes-18.2.rst', '/bareos-18.2.rst', '/webui-tls.rst', '/DeveloperGuide.rst', '/DocumentationStyleGuide.rst', '/genindex' ]

    def getToc(self):
        '''
        Generate TOC, but add some static entries.
        '''
        content = super(LatexParts, self).getToc()
        for section in self.toc_extra_nodes:
            path = os.path.normpath('{}'.format(section))
            content += '   {}\n'.format(path)
        
        return content


class LatexChapters(LatexSplit):
    
    def __init__(self, text, targetdir, relpath, write = True):
        
        super(LatexChapters, self).__init__(text, r'\\chapter', 2, targetdir, relpath, write)


if __name__ == "__main__":

    logging.basicConfig(level=logging.DEBUG)
    logger = logging.getLogger()

    parser = argparse.ArgumentParser(description='Split RST file by sections.')
    parser.add_argument('FILE', help='Source file to be splitted.')
    parser.add_argument('DESTDIR', help='Target directory to store new files.')
    args = parser.parse_args()

    with open(args.FILE, 'r') as textfile:
        filetext = textfile.read()

    parts = LatexParts(filetext, args.DESTDIR, '')
    #parts.split()
    print(parts.getToc())

    #print(len(parts.getNodes()))

    #print('PP: {}'.format(parts.getPrefix()))

    write('{}/index.rst'.format(args.DESTDIR), indexContent + parts.getToc())
    
    for part in parts.getNodes():
        print('* {}'.format(part.title))
        #targetdir = '{}/{}'.format(args.DESTDIR, convertToFilename(part.title))
        chapters = LatexChapters(part.content, args.DESTDIR, convertToFilename(part.title))
        #chapters.split()
        #print('CP: {}'.format(chapters.getPrefix()))
        print(chapters.getToc())
        chapters.write()
