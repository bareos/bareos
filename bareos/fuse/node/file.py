"""
"""

from   bareos.fuse.node.base import Base
import errno
import fuse
import logging
from   pprint import pformat
import stat

class File(Base):
    """
    File node.
    """
    def __init__(self, root, name, content = ""):
        super(File, self).__init__(root, name)
        self.content = content
        self.stat.st_mode = stat.S_IFREG | 0o444
        self.stat.st_nlink = 1

    def get_stat(self):
        if self.content != None:
            self.stat.st_size = len(self.content)
        return super(File, self).get_stat()
