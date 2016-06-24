"""
FUSE filesystem on bareos data.
"""

from   bareos.bsock import DirectorConsoleJson
import bareos.bsock
from   bareos.util  import Path
from   bareos.fuse.root  import Root
import errno
import fuse
import logging
import socket
import stat
import os.path
from   pprint import pformat

fuse.fuse_python_api = (0, 2)


class BareosFuse(fuse.Fuse):

    def __init__(self, *args, **kw):
        self.bsock = None
        self.bareos = None
        self.restoreclient = None
        self.restorejob = None
        self.restorepath = '/var/cache/bareosfs/'
        super(BareosFuse, self).__init__(*args, **kw)
        self.multithreaded = False

    def initLogging(self):
        self.logger = logging.getLogger()
        self.logger.setLevel(logging.DEBUG)
        if hasattr(self, "logfile"):
            hdlr = logging.FileHandler(self.logfile)
            # limit message size
            formatter = logging.Formatter('%(asctime)s  %(levelname)-7s %(module)s %(funcName)s( %(message).800s )')
            hdlr.setFormatter(formatter)
            self.logger.addHandler(hdlr)

    def parse(self, *args, **kw):
        super(BareosFuse, self).parse(*args, **kw)
        if self.fuse_args.mount_expected():
            options = [ 'address', 'port', 'dirname', 'name', 'password' ]
            self.bsockParameter = {}
            for i in options:
                if hasattr(self, i):
                    self.bsockParameter[i] = getattr(self,i)
                else:
                    #self.logger.debug( "%s: missing, default: %s" %(i, str(getattr(self,i,None))))
                    pass
            if not hasattr(self, 'password'):
                raise bareos.fuse.ParameterMissing("missing parameter password")
            else:
                password = bareos.bsock.Password(self.password)
                self.bsockParameter['password']=password
            self.restorepath = os.path.normpath(self.restorepath)
            if not os.path.isabs(self.restorepath):
                raise bareos.fuse.RestorePathInvalid("restorepath must be an absolute path")


    def main(self, *args, **kw):
        # use main() instead of fsinit,
        # as this prevents FUSE from being started in case of errors.

        self.initLogging()
        self.logger.debug('start')
        if self.fuse_args.mount_expected():
            try:
                self.bsock = bareos.bsock.BSockJson(**self.bsockParameter)
            except socket.error as e:
                self.logger.exception(e)
                raise bareos.fuse.SocketConnectionRefused(e)
            self.bareos = Root(self.bsock, self.restoreclient, self.restorejob, self.restorepath)
        super(BareosFuse, self).main(*args, **kw)
        self.logger.debug('done')

    def getattr(self, path):
        if self.bareos:
            stat = self.bareos.getattr(Path(path))
            if isinstance(stat, fuse.Stat):
                self.logger.debug("{path}: dev={stat.st_dev}, ino={stat.st_ino}, mode={stat.st_mode}, nlink={stat.st_nlink}, uid={stat.st_uid}, gid={stat.st_gid}, size={stat.st_size}, atime={stat.st_atime}, ctime={stat.st_ctime}, mtime={stat.st_mtime}".format(path=path, stat=stat))
            else:
                self.logger.debug("%s: (int) %i" % (path, stat))
            return stat

    def read(self, path, size, offset):
        result = self.bareos.read(Path(path), size, offset)
        self.logger.debug("%s: %s" % (path, result))
        return result

    def readdir(self, path, offset):
        entries = self.bareos.readdir(Path(path), offset)
        self.logger.debug("%s: %s" % (path, entries))
        if not entries:
            return -errno.ENOENT
        else:
            return [fuse.Direntry(f) for f in entries]

    def readlink(self, path):
        result = self.bareos.readlink(Path(path))
        self.logger.debug("%s: %s" % (path, result))
        return result

    def listxattr(self, path, size):
        '''
        list extended attributes
        '''
        keys = self.bareos.listxattr(Path(path))
        if size == 0:
            # We are asked for size of the attr list, ie. joint size of attrs
            # plus null separators.
            result = len("".join(keys)) + len(keys)
            self.logger.debug("%s: len=%s" % (path, result))
        else:
            result = keys
            self.logger.debug("%s: keys=%s" % (path, ", ".join(keys)))
        return result

    def getxattr(self, path, key, size):
        '''
        get value of extended attribute
        burpfs exposes some filesystem attributes for the root directory
        (e.g. backup number, cache prefix - see FileSystem.xattr_fields_root)
        and may also expose several other attributes for each file/directory
        in the future (see FileSystem.xattr_fields)
        '''
        value = self.bareos.getxattr(Path(path), key)
        self.logger.debug("%s: %s=%s" % (path, key, str(value)))
        if value == None:
            result = -errno.ENODATA
        elif size == 0:
            # we are asked for size of the value
            result = len(value)
            self.logger.debug("%s: len=%s" % (path, result))
        else:
            result = value
            self.logger.debug("%s: %s=%s" % (path, key, result))
        return result

    def setxattr(self, path, key, value, flags):
        '''
        set value of extended attribute
        '''
        result = self.bareos.setxattr(Path(path), key, value, flags)
        self.logger.debug("%s: %s=%s: %s" % (path, key, value, str(result)))
        return result

    def rename(self, oldpath, newpath):
        result = self.bareos.rename(Path(oldpath), Path(newpath))
        self.logger.debug("%s => %s: %s" % (oldpath, newpath, result))
        return result
