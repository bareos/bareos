"""
"""

from   bareos.fuse.node.base import Base
import errno
import logging
from   pprint import pformat
import stat

class Directory(Base):
    """
    Directory node.
    """
    def __init__(self, bsock, name):
        super(Directory, self).__init__(bsock, name)
        self.defaultdirs = [ ".", ".." ]
        self.stat.st_mode = stat.S_IFDIR | 0755
        self.stat.st_nlink = len(self.defaultdirs)
        # arbitrary default value
        self.stat.st_size = 4096

    def readdir(self, path, offset):
        self.logger.debug("%s(\"%s\")" % (str(self), str(path)))
        # copy default dirs
        if path.len() == 0:
            self.update()
            # TODO: use encode instead str
            result = list(self.defaultdirs) + [ str(i) for i in self.subnodes.keys() ]
        else:
            if path.get(0) in self.subnodes:
                topdir = path.shift()
                result = self.subnodes[topdir].readdir(path, offset)
        return result

    def get_stat(self):
        self.stat.st_nlink = len(self.defaultdirs) + len(self.subnodes)
        return super(Directory, self).get_stat()
