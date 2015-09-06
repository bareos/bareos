"""
"""

#import bareos.bsock
from   bareos.bsock import BSockJson
import errno
import fuse
import logging
import os
from   pprint import pformat
import stat

fuse.fuse_python_api = (0, 2)

TYPE_NONE = None
TYPE_FILE = 1
TYPE_DIR  = 2


class Base():
    def __init__(self, bsock):
        self.logger = logging.getLogger()
        self.bsock = bsock
        self.defaultdirs = [ ".", ".." ]
    

class Jobs(Base):

    def getattr(self, path):
        self.logger.debug("Jobs \"%s\"" % (path))
        result = -errno.ENOENT
        st = fuse.Stat()
        part = path.split("/")
        if len(part) == 1:
            if part[0] == "":
                st.st_mode = stat.S_IFDIR | 0755
                #st.st_size = 4096
                number_entires = len(self.bsock.call("list jobs")['jobs'])
                st.st_nlink = len(self.defaultdirs) + 1
                result = st
            else:
                st.st_mode = stat.S_IFDIR | 0755
                #st.st_size = 4096
                st.st_nlink = len(self.defaultdirs) + 1
                result = st
        elif len(part) == 2:
            if part[1] == "":
                st.st_mode = stat.S_IFDIR | 0755
                #st.st_size = 4096
                st.st_nlink = len(self.defaultdirs) + 1
                result = st
            elif part[1] == ".bareos-jobinfo":
                st.st_mode = stat.S_IFREG | 0444
                st.st_nlink = 1
                st.st_size  = 0
                result = st                
        self.logger.debug("Jobs \"%s\": nlink: %s" % (path, str(st.st_nlink)))
        return result

    def readdir(self, path, offset):
        splitted = os.path.split(path)
        # copy default dirs
        dirs = list(self.defaultdirs)
        if path == "":
            # TODO: only successful
            data = self.bsock.call("llist jobs")
            #self.logger.debug(data)
            dirs.extend([ str(d['jobid']) for d in data['jobs'] ])
            self.logger.debug("Jobs: \"%s\": %s" % (path, dirs))
        elif splitted[0] == "" or splitted[1] == 0:
            dirs.append( ".bareos-jobinfo" )
        return dirs
    

class FuseCache():
    """
        jobs/
            <jobid>
    """

    def __init__(self, bsock):
        self.logger = logging.getLogger()
        self.bsock = bsock
        self.jobs = Jobs(bsock)
        self.path = { 
            '': { 'type': TYPE_DIR, 'dirs': [ ".", "..", "jobs" ], 'files': [] },
        }

    def get_type(self, path):
        if self.path.has_key(path):
            return self.path[path]['type']
        
    def is_file(self, path):
        return self.get_type(path) == TYPE_FILE

    def is_directory(self, path):
        return self.get_type(path) == TYPE_DIR
    
    def exists(self, path):
        return self.path.has_key(path)
    
    def get_nlink(self, path):
        if self.exists(path):
            return len(self.path[path]['dirs'])
        else:
            # TODO: correct?
            return 2
        
    def readdir(self, path, offset):
        entries = None
        if self.path.has_key(path) and self.path[path]['type'] == TYPE_DIR:
            entries = self.path[path]['dirs'] + self.path[path]['files']
        self.logger.debug( "Cache: \"%s\": %s" % (path, entries) )
        return entries


class BareosWrapper():
    def __init__(self, bsock):
        self.logger = logging.getLogger()
        self.jobs = Jobs(bsock)
        self.cache = FuseCache(bsock)


    def getattr(self, path):

        # TODO: may cause problems with filenames that ends with "/" 
        path = path.rstrip( '/' )
        self.logger.debug( "wrapper: \"%s\"" % (path) )

        if path == "/jobs" or path.startswith("/jobs/"):
            st = self.jobs.getattr(path.lstrip("/jobs").lstrip("/"))
            return st
        else:
            st = fuse.Stat()

            if self.cache.is_file(path):
                st.st_mode = stat.S_IFREG | 0444
                st.st_nlink = 1
                st.st_size  = 0
                return st
            elif self.cache.is_directory(path):
                st.st_mode = stat.S_IFDIR | 0755
                #if 'dirs' in file:
                #    st.st_nlink = len(file['dirs'])
                #else:
                st.st_nlink = self.cache.get_nlink(path)
                return st

        # TODO: check for existens
        return -errno.ENOENT


    def readdir(self, path, offset):
        #self.logger.debug( '"' + path + '", offset=' + str(offset) + ')' )

        # TODO: may cause problems with filenames that ends with "/" 
        path = path.rstrip( '/' )

        if path == "/jobs" or path.startswith("/jobs/"):
            entries = self.jobs.readdir(path.lstrip("/jobs").lstrip("/"), offset)
        else:
            entries = self.cache.readdir(path, offset)

        self.logger.debug( "wrapper: \"%s\": %s" % (path, pformat(entries)) )

        if not entries:
            return -errno.ENOENT
        else:
            return [fuse.Direntry(f) for f in entries]


class BareosFuse(fuse.Fuse):

    def __init__(self, *args, **kw):
        self.logger = logging.getLogger()
        self.logger.debug('init')

        self.bsock = kw['bsockjson']
        if not isinstance(self.bsock,BSockJson):
            raise RuntimeError("parameter bsock")
        del(kw['bsockjson'])
        self.bareos = BareosWrapper(self.bsock)

        super(BareosFuse, self).__init__(*args, **kw)


    def getattr(self, path):
        stat = self.bareos.getattr(path)
        if isinstance(stat, fuse.Stat):
            self.logger.debug("fuse %s: nlink=%i" % (path, stat.st_nlink))
        else:
            self.logger.debug("fuse %s: (int) %i" % (path, stat))
        return stat


    def readdir(self, path, offset):
        entries = self.bareos.readdir(path, offset)
        self.logger.debug("fuse %s: %s" % (path, entries))
        return entries

    
    #def open( self, path, flags ):
        #logging.debug( "open " + path )
        #return -errno.ENOENT

    #def read( self, path, length, offset):
        #logging.debug( "read " + path )
        #return -errno.ENOENT
