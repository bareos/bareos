"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.file import File
from   bareos.fuse.node.volumelist import VolumeList
from   pprint import pformat

class Pool(Directory):
    def __init__(self, root, name, pool):
        super(Pool, self).__init__(root, name)
        self.pool = pool

    @classmethod
    def get_id(cls, name, pool):
        return name

    def do_update_stat(self):
        self.subnode_count = 2

    def do_update(self):
        self.add_subnode(File, name="info.txt", content=pformat(self.pool) + "\n")
        self.add_subnode(VolumeList, name="volumes", selector="pool=%s" % (self.name))
