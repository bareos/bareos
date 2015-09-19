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
        self.set_name(name)
        self.stat = fuse.Stat()
        self.content = None
        self.subnodes = {}
        self.subnodes_old = self.subnodes.copy()
        self.static = False
        self.lastupdate = None
        # timeout for caching
        self.cache_timeout = timedelta(seconds=60)

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

    def get_name(self):
        return self.name

    def set_name(self, name):
        self.name = name

    def set_static(self, value=True):
        self.static = value

    def getattr(self, path):
        self.logger.debug("%s(\"%s\")" % (str(self), str(path)))
        result = -errno.ENOENT
        if path.len() == 0:
            self.update()
            result = self.get_stat()
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].getattr(path)
        return result

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

    def read(self, path, size, offset):
        self.logger.debug("%s(\"%s\", %d, %d)" % (str(self), str(path), size, offset))
        result = -errno.ENOENT
        if path.len() == 0:
            self.logger.debug( "content: " + str(self.content) )
            self.update()
            if self.content != None:
                result = self.content[offset:offset+size]
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].read(path, size, offset)
        return result

    def add_subnode(self, classtype, *args, **kwargs):
        instance = self.root.factory.get_instance(classtype, *args, **kwargs)
        name = instance.get_name()
        if not self.subnodes.has_key(name):
            self.subnodes[name] = instance
        else:
            if self.subnodes_old.has_key(name):
                del(self.subnodes_old[name])

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
            for i in self.subnodes_old.keys():
                del(self.subnodes[name])
            self.lastupdate = datetime.now()
        else:
            self.logger.debug("skipped")

    def do_update(self):
        # dummy, to be filled by inherented classes
        pass

    def _convert_date_bareos_unix(self, bareosdate):
        unixtimestamp = int(DateParser.parse(bareosdate).strftime("%s"))
        self.logger.debug( "unix timestamp: %d" % (unixtimestamp))
        return unixtimestamp
