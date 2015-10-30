"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.pool import Pool

class Pools(Directory):
    def __init__(self, root, name):
        super(Pools, self).__init__(root, name)
        self.static = True

    @classmethod
    def get_id(cls, name):
        return "unique"

    def do_update(self):
        data = self.bsock.call("llist pools")
        pools = data['pools']
        for i in pools:
            self.add_subnode(Pool, i['name'], i)
