"""
FUSE filesystem on bareos data.
"""

from   bareos.bsock import BSockJson
import bareos.bsock
from   bareos.util  import Path
from   bareos.fuse.root  import Root
import errno
import fuse
import logging
import stat
from   pprint import pformat

fuse.fuse_python_api = (0, 2)


class BareosFuse(fuse.Fuse):

    def __init__(self, *args, **kw):
        self.bsock = None
        self.bareos = None
        super(BareosFuse, self).__init__(*args, **kw)
        self.multithreaded = False

    def initLogging(self):
        self.logger = logging.getLogger()
        self.logger.setLevel(logging.DEBUG)
        if hasattr(self, "logfile"):
            hdlr = logging.FileHandler(self.logfile)
            formatter = logging.Formatter('%(asctime)s  %(levelname)-7s %(module)s %(funcName)s( %(message)s )')
            hdlr.setFormatter(formatter)
            self.logger.addHandler(hdlr)

    def main(self, *args, **kw):
        # use main() instead of fsinit,
        # as this prevents FUSE from being started in case of errors.

        self.initLogging()
        self.logger.debug('start')
        if self.fuse_args.mount_expected():
            options = [ 'address', 'port', 'dirname', 'name', 'password' ]
            parameter = {}
            for i in options:
                if hasattr(self, i):
                    self.logger.debug( "%s: %s" %(i, getattr(self,i)))
                    parameter[i] = getattr(self,i)
                else:
                    self.logger.debug( "%s: missing" %(i))
            self.logger.debug('options: %s' % (parameter))
            password = bareos.bsock.Password(self.password)
            parameter['password']=password
            self.bsock = bareos.bsock.BSockJson(**parameter)
            self.bareos = Root(self.bsock)
        super(BareosFuse, self).main(*args, **kw)
        self.logger.debug('done')


    def getattr(self, path):
        if self.bareos:
            stat = self.bareos.getattr(Path(path))
            if isinstance(stat, fuse.Stat):
                self.logger.debug("fuse %s: nlink=%i" % (path, stat.st_nlink))
            else:
                self.logger.debug("fuse %s: (int) %i" % (path, stat))
            return stat


    def readdir(self, path, offset):
        entries = self.bareos.readdir(Path(path), offset)
        self.logger.debug("fuse %s: %s" % (path, entries))
        if not entries:
            return -errno.ENOENT
        else:
            return [fuse.Direntry(f) for f in entries]


    def read(self, path, size, offset):
        result = self.bareos.read(Path(path), size, offset)
        self.logger.debug("fuse %s: %s" % (path, result))
        return result
