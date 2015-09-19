"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
import stat

class BvfsFile(File):
    def __init__(self, root, file):
        super(BvfsFile, self).__init__(root, file['name'], content = None)
        self.file = file
        self.static = True
        self.set_stat(self.file['stat'])

    @classmethod
    def get_id(cls, file):
        return str(file['fileid'])
