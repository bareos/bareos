"""
"""

from    datetime import datetime, timedelta
from    dateutil import parser as DateParser
import  errno
import  fuse
import  grp
import  logging
import  pwd
import  stat

class Base(object):
    """
    Base class for filesystem nodes.
    """
    def __init__(self, root, name):
        self.logger = logging.getLogger()
        self.root = root
        self.bsock = root.bsock
        self.id = None
        self.set_name(name)
        self.stat = fuse.Stat()
        self.xattr = {}
        self.content = None
        self.subnodes = {}
        self.subnodes_old = self.subnodes.copy()
        self.subnode_count = len(self.subnodes)
        self.static = False
        self.lastupdate = None
        self.lastupdate_stat = None
        # timeout for caching
        self.cache_timeout = timedelta(seconds=60)
        self.cache_stat_timeout = timedelta(seconds=60)

    @classmethod
    def get_id(cls, *args, **kwargs):
        """
        To enable reusing instances at different places in the filesystem,
        this method must be overwritten
        and return a unique id (string) for this class.
        The parameter of this must be identical to the parameter
        of the __init__ method must be identical to the parameter of get_id,
        except that the "root" parameter is excluded.
        """
        return None

    def do_get_name(self, *args, **kwargs):
        """
        Get name from init parameter.
        Normally name is statically set,
        but for some objects is it based on status data
        and dependend on where the instance is locationed 
        in the directory tree (e.g. volumestatus).
        """
        return self.name

    # Interface
    # =========

    def get_name(self, *args, **kwargs):
        result = self.do_get_name(*args, **kwargs)
        if isinstance(result, unicode):
            result = result.encode('utf-8', 'replace')
        return result

    def set_name(self, name):
        if isinstance(name, unicode):
            self.name = name.encode('utf-8', 'replace')
        else:
            # should be str or NoneType
            self.name = name

    def set_static(self, value=True):
        self.static = value

    def get_stat(self):
        return self.stat

    def set_stat(self, stat):
        try:
            if stat['mode'] > 0:
                self.stat.st_mode = stat['mode']
            self.stat.st_size = stat['size']
            self.stat.st_atime = stat['atime']
            self.stat.st_ctime = stat['ctime']
            self.stat.st_mtime = stat['mtime']
        except KeyError as e:
            self.logger.warning(str(e))
            pass
        try:
            uid = pwd.getpwnam(stat['user']).pw_uid
            self.stat.st_uid = uid
        except KeyError as e:
            self.logger.info("user %s not known on this system, fall back to uid 0" % (stat['user']))
            pass
        try:
            gid = grp.getgrnam(stat['group']).gr_gid
            self.stat.st_gid = gid
        except KeyError as e:
            self.logger.info("group %s not known on this system, fall back to gid 0" % (stat['group']))
            pass
        #"stat": {
          #"atime": 1441134679,
          #"ino": 3689524,
          #"dev": 2051,
          #"mode": 33256,
          #"nlink": 1,
          #"user": "joergs",
          #"group": "joergs",
          #"ctime": 1441134679,
          #"rdev": 0,
          #"size": 1613,
          #"mtime": 1441134679
        #},

    def add_subnode(self, classtype, *args, **kwargs):
        instance = self.root.factory.get_instance(classtype, *args, **kwargs)
        name = instance.get_name(*args, **kwargs)
        if name not in self.subnodes:
            self.subnodes[name] = instance
        else:
            if name in self.subnodes_old:
                del(self.subnodes_old[name])

    def update_stat(self):
        # update status, content, ...
        now = datetime.now()
        if self.lastupdate_stat == None:
            self.logger.debug("reason: first time")
            self.do_update_stat()
            self.lastupdate_stat = now
        elif not self.static and (self.lastupdate_stat + self.cache_stat_timeout) < now:
            diff = now - self.lastupdate_stat
            self.logger.debug("reason: non-static and timeout (%d seconds)" % (diff.seconds))
            self.do_update_stat()
            self.lastupdate_stat = datetime.now()
        else:
            self.logger.debug("skipped (lastupdate: %s, static: %s)" % ( str(self.lastupdate_stat), str(self.static)))

    def do_update_stat(self):
        """
        if not overwritten, a full update is performed.
        Only updating the stat can be more efficient.
        """
        self.update()

    def update(self):
        # update status, content, ...
        now = datetime.now()
        if self.lastupdate == None:
            self.logger.debug("reason: first time")
            self.do_update()
            self.lastupdate = now
        elif not self.static and (self.lastupdate + self.cache_timeout) < now:
            diff = now - self.lastupdate
            self.logger.debug("reason: non-static and timeout (%d seconds)" % (diff.seconds))
            # store current subnodes
            #  and delete all not updated subnodes after do_update()
            self.subnodes_old = self.subnodes.copy()
            self.do_update()
            for i in list(self.subnodes_old.keys()):
                self.logger.debug("removing outdated node %s" % (i))
                del(self.subnodes[i])
            self.subnode_count = len(self.subnodes)
            self.lastupdate = datetime.now()
        else:
            self.logger.debug("skipped (lastupdate: %s, static: %s)" % ( str(self.lastupdate), str(self.static)))

    def do_update(self):
        """
        dummy, to be overwritten (not inherited!) by inherented classes
        """
        # if no node specific do_update exists,
        # remove marker for nodes to be deleted after update
        self.subnodes_old = {}


    # Filesystem methods
    # ==================

    def getattr(self, path):
        #self.logger.debug("%s(\"%s\")" % (str(self), str(path)))
        result = -errno.ENOENT
        if path.len() == 0:
            self.update_stat()
            result = self.get_stat()
        else:
            if not (path.get(0) in self.subnodes):
                self.update()
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].getattr(path)
        return result


    def read(self, path, size, offset):
        #self.logger.debug("%s(\"%s\", %d, %d)" % (str(self), str(path), size, offset))
        result = -errno.ENOENT
        if path.len() == 0:
            self.logger.debug( "content: " + str(self.content) )
            self.update()
            if self.content != None:
                result = self.content[offset:offset+size]
        else:
            if not (path.get(0) in self.subnodes):
                self.update()
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].read(path, size, offset)
        return result

    def readlink(self, path):
        #self.logger.debug("%s(\"%s\", %d, %d)" % (str(self), str(path), size, offset))
        result = -errno.ENOENT
        if path.len() == 0:
            pass
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].readlink(path)
        return result

    def listxattr(self, path):
        '''
        list extended attributes
        '''
        #self.logger.debug("%s(\"%s\", %s)" % (str(self.get_name()), str(path), str(size)))
        result = []
        if path.len() == 0:
            result = list(self.xattr.keys())
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].listxattr(path)
        return result

    def getxattr(self, path, key):
        '''
        get value of extended attribute
        '''
        #self.logger.debug("%s(\"%s\")" % (str(self), str(path)))
        result = None
        if path.len() == 0:
            try:
                result = self.xattr[key]
                if isinstance(self.xattr[key], unicode):
                    result = self.xattr[key].encode('utf-8','replace')
            except KeyError:
                pass
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].getxattr(path, key)
        return result

    def setxattr(self, path, key, value, flags):
        '''
        set value of extended attribute
        '''
        result = 0
        if path.len() == 0:
            self.xattr[key] = value
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].setxattr(path, key, value, flags)
        return result

    def rename(self, oldpath, newpath):
        self.logger.debug("%s(\"%s %s\")" % (str(self), str(oldpath), str(newpath)))
        result = -errno.ENOENT
        if path.len() == 0:
            #self.update()
            #result = self.get_stat()
            pass
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].rename(oldpath, newpath)
        return result

    # Helpers
    # =======

    def _convert_date_bareos_unix(self, bareosdate):
        unixtimestamp = 0
        try:
            unixtimestamp = int(DateParser.parse(bareosdate).strftime("%s"))
            #self.logger.debug( "unix timestamp: %d" % (unixtimestamp))
        except ValueError:
            pass
        # could happen because of timezones
        if unixtimestamp < 0:
            unixtimestamp = 0
        return unixtimestamp
