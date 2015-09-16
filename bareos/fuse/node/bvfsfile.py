"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
import stat

class BvfsFile(File):
    def __init__(self, bsock, file):
        super(BvfsFile, self).__init__(bsock, file['name'], content = None)
        self.file = file
        self.static = True
        self.set_stat(self.file['stat'])
