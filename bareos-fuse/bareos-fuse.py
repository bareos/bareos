#!/usr/bin/env python
# -*- coding: utf-8 -*-

# $URL: https://svn.dass-it.de/svn/pub/baculafs/trunk/bareos-fuse.py $
# $Id: bareos-fuse.py 1150 2013-05-22 10:54:32Z joergs $

import fuse
import logging
import stat
import errno
import os
import pexpect
import sys
import re

fuse.fuse_python_api = (0, 2)

###### bconsole
################

BAREOS_FUSE_VERSION = "$Rev: 1150 $"

LOG_FILENAME        = '/tmp/bareos-fuse.log'
LOG_BCONSOLE_DUMP   = '/tmp/bareos-fuse-bconsole.log'


#BAREOS_CMD = 'strace -f -o /tmp/bconsole.strace /usr/sbin/bconsole -n'
BAREOS_CMD = '/usr/sbin/bconsole -n'

BCONSOLE_CMD_PROMPT='\*'
BCONSOLE_SELECT_PROMPT='Select .*:.*'
BCONSOLE_RESTORE_PROMPT='\$ '

# define the states bconsole can be in
class BconsoleState:
    UNKNOWN         = ''
    CMD_PROMPT      = BCONSOLE_CMD_PROMPT
    SELECT_PROMPT   = BCONSOLE_SELECT_PROMPT
    RESTORE_PROMPT  = BCONSOLE_RESTORE_PROMPT
    ERROR           = "error"

# direct access to the program bconsole
class Bconsole:

    def __init__(self, timeout = 300):
        #logging.debug('BC init')

        self.timeout = timeout
        self.cwd = "/"
        self.state = BconsoleState.UNKNOWN
        self.last_items_dict = {}

        self.bconsole = pexpect.spawn( BAREOS_CMD, logfile=file(LOG_BCONSOLE_DUMP, 'w'), timeout=timeout )
        self.bconsole.setecho( False )
        self.bconsole.expect( BCONSOLE_CMD_PROMPT )
        self.bconsole.sendline( 'restore' )
        self.bconsole.expect( BCONSOLE_SELECT_PROMPT )
        self.bconsole.sendline( "5" )
        self.wait_for_prompt()


    def _normalize_dir( self, path ):
        # guarantee that directory path ends with (exactly one) "/"
        return path.rstrip('/') + "/"

    def _get_select_items(self,lines):
        re_select_items = re.compile('\s*[0-9]+:\s*.+')
        return filter( re_select_items.match, lines )

    def _get_item_dict( self,lines ):
        dictionary = {}
        for line in self._get_select_items(lines):
            number,item = line.split( ":", 1 )
            item = self._normalize_dir( item.strip() )
            number = number.strip()
            dictionary[item]=number
        return dictionary



    def wait_for_prompt(self):

        # set to error state. 
        # Only set back to valid state,
        # if a valid prompt is received
        self.state=BconsoleState.ERROR

        try:
            index = self.bconsole.expect( [ BCONSOLE_SELECT_PROMPT, BCONSOLE_RESTORE_PROMPT ] )
            if index == 0:
                # SELECT_PROMPT
                self.state=BconsoleState.SELECT_PROMPT
                lines = self.bconsole.before.splitlines()
                self.last_items_dict = self._get_item_dict(lines)
                logging.debug( str( self.last_items_dict ) )
            elif index == 1:
                # RESTORE_PROMPT
                self.state=BconsoleState.RESTORE_PROMPT
            else:
                logging.error( "unexpected result" )
        except pexpect.EOF:
            logging.error( "EOF bconsole" )
        except pexpect.TIMEOUT:
            logging.error( "TIMEOUT bconsole" )
            raise

        return self.state



    def cd(self,path):
        path = self._normalize_dir( path )
        logging.debug( "(" + path + ")" )

        #  parse for BCONSOLE_SELECT_PROMPT or BCONSOLE_RESTORE_PROMPT
        #    BCONSOLE_SELECT_PROMPT: take first part of path and try to match. send number. iterate
        #    BCONSOLE_RESTORE_PROMPT: cd to directory (as before)

        if not path:
            return True

        if path == "/":
            return True

        if self.state == BconsoleState.SELECT_PROMPT:
            return self.cd_select( path )
        elif self.state == BconsoleState.RESTORE_PROMPT:
            return self.cd_restore( path )
        # else error
        return False


    def cd_select(self, path):
        logging.debug( "(" + path + ")" )

        # get top level directory
        directory,sep,path=path.lstrip( "/" ).partition( "/" )
        directory=self._normalize_dir(directory)
        logging.debug( "directory: " + directory )

        if self.last_items_dict[directory]:
            logging.debug( "directory: " + directory  + " (" + self.last_items_dict[directory] + ")" )
            self.bconsole.sendline( self.last_items_dict[directory] )
            self.wait_for_prompt()
            self.cd( path )
            return True

        return False



    def cd_restore(self, path):
        
        result = False

        logging.debug( "(" + path + ")" )

        self.bconsole.sendline( 'cd ' + path )
        #self.bconsole.expect( BCONSOLE_RESTORE_PROMPT, timeout=10 )
        #self.bconsole.sendline( 'pwd' )

        try:
            index = self.bconsole.expect( ["cwd is: " + path + "[/]?", BCONSOLE_RESTORE_PROMPT ] )
            logging.debug( "cd result: " + str(index) )

            if index == 0:
                # path ok, now wait for prompt
                self.bconsole.expect( BCONSOLE_RESTORE_PROMPT )
                result = True
            elif index == 1:
                # TODO: this is even returned, if path is correct. fix this.
                logging.warning( "wrong path" )
            else:
                logging.error( "unexpected result" )

        except pexpect.EOF:
            logging.error( "EOF bconsole" )
        except pexpect.TIMEOUT:
            logging.error( "TIMEOUT bconsole" )
            #raise

        return result


    def ls(self, path):
        logging.debug( "(" + path + ")" )

        if self.cd( path ):
            if self.state == BconsoleState.SELECT_PROMPT:
                return self.last_items_dict.keys()
            elif self.state == BconsoleState.RESTORE_PROMPT:
                return self.ls_restore( path )
        else:
            return


    def ls_restore( self, path ):
        self.bconsole.sendline( 'ls' )
        self.bconsole.expect( BCONSOLE_RESTORE_PROMPT )
        lines = self.bconsole.before.splitlines()
        #logging.debug( str(lines) )
        return lines


###############

class BareosFuse(fuse.Fuse):

    TYPE_NONE = 0
    TYPE_FILE = 1
    TYPE_DIR  = 2

    files = { '': {'type': TYPE_DIR} }

    def __init__(self, *args, **kw):
        logging.debug('init')
        #self.console = Bconsole()
        fuse.Fuse.__init__(self, *args, **kw)
        #logging.debug('init finished')


    def _getattr(self,path):
        # TODO: may cause problems with filenames that ends with "/"
        path = path.rstrip( '/' )
        logging.debug( '"' + path + '"' )

        if (path in self.files):
            #logging.debug( "(" + path + ")=" + str(self.files[path]) )
            return self.files[path]

        if Bconsole().cd(path):
            # don't define files, because these have not been checked
            self.files[path] = { 'type': self.TYPE_DIR, 'dirs': [ ".", ".." ] }

        return self.files[path]

    # TODO: only works after readdir for the directory (eg. ls)
    def getattr(self, path):

        # TODO: may cause problems with filenames that ends with "/" 
        path = path.rstrip( '/' )
        logging.debug( '"' + path + '"' )

        st = fuse.Stat()

        if not (path in self.files):
            self._getattr(path)

        if not (path in self.files):
            return -errno.ENOENT

        file = self.files[path]

        if file['type'] == self.TYPE_FILE:
            st.st_mode = stat.S_IFREG | 0444
            st.st_nlink = 1
            st.st_size  = 0
            return st
        elif file['type'] == self.TYPE_DIR:
            st.st_mode = stat.S_IFDIR | 0755
            if 'dirs' in file:
                st.st_nlink = len(file['dirs'])
            else:
                st.st_nlink = 2
            return st

        # TODO: check for existens
        return -errno.ENOENT



    def _getdir(self, path):

        # TODO: may cause problems with filenames that ends with "/" 
        path = path.rstrip( '/' )
        logging.debug( '"' + path + '"' )

        if (path in self.files):
            #logging.debug( "(" + path + ")=" + str(self.files[path]) )
            if self.files[path]['type'] == self.TYPE_NONE:
                logging.info( '"' + path + '" does not exist (cached)' )
                return self.files[path]
            elif self.files[path]['type'] == self.TYPE_FILE:
                logging.info( '"' + path + '"=file (cached)' )
                return self.files[path]
            elif ((self.files[path]['type'] == self.TYPE_DIR) and ('files' in self.files[path])):
                logging.info( '"' + path + '"=dir (cached)' )
                return self.files[path]

        try:
            files = Bconsole().ls(path)
            logging.debug( "  files: " + str( files ) )

            # setting initial empty directory. Add entires later in this function
            self.files[path] = { 'type': self.TYPE_DIR, 'dirs': [ ".", ".." ], 'files': [] }
            for i in files:
                if i.endswith('/'):
                    # we expect a directory
                    # TODO: error with filesnames, that ends with '/'
                    i = i.rstrip( '/' )
                    self.files[path]['dirs'].append(i)
                    if not (i in self.files):
                        self.files[path + "/" + i] = { 'type': self.TYPE_DIR }
                else:
                    self.files[path]['files'].append(i)
                    self.files[path + "/" + i] = { 'type': self.TYPE_FILE }

        except pexpect.TIMEOUT:
            logging.error( "failed to access path " + path + " (TIMEOUT)" )
        except Exception as e:
            logging.exception(e)
            logging.error( "no access to path " + path )
            self.files[path] = { 'type': self.TYPE_NONE }

        logging.debug( '"' + path + '"=' + str( self.files[path] )  )
        return self.files[path]



    def readdir(self, path, offset):
        logging.debug( '"' + path + '", offset=' + str(offset) + ')' )

        dir = self._getdir( path )

        #logging.debug( "  readdir: type: " + str( dir['type'] ) )
        #logging.debug( "  readdir: dirs: " + str( dir['dirs'] ) )
        #logging.debug( "  readdir: file: " + str( dir['files'] ) )

        if dir['type'] != self.TYPE_DIR:
            return -errno.ENOENT
        else:
            return [fuse.Direntry(f) for f in dir['files'] + dir['dirs']]

    #def open( self, path, flags ):
        #logging.debug( "open " + path )
        #return -errno.ENOENT

    #def read( self, path, length, offset):
        #logging.debug( "read " + path )
        #return -errno.ENOENT

if __name__ == "__main__":
    # initialize logging
    logging.basicConfig(filename=LOG_FILENAME,level=logging.DEBUG,format="%(asctime)s %(process)5d(%(threadName)s) %(levelname)-7s %(funcName)s( %(message)s )")


    usage = """
    Bareos Fuse filesystem: displays files from Bareos backups as a (userspace) filesystem.
                       Internaly, it uses the Bareos bconsole.

    """ + fuse.Fuse.fusage


    fs = BareosFuse(
        version="%prog: " + BAREOS_FUSE_VERSION,
        usage=usage,
        # required to let "-s" set single-threaded execution
        dash_s_do='setsingle'
    )

    #server.parser.add_option(mountopt="root", metavar="PATH", default='/',help="mirror filesystem from under PATH [default: %default]")
    #server.parse(values=server, errex=1)
    fs.parse()

    fs.main()
