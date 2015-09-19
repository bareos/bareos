"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.volume import Volume

class Volumes(Directory):
    def __init__(self, root, name):
        super(Volumes, self).__init__(root, name)

    @classmethod
    def get_id(cls, name):
        return "unique"

    def do_update(self):
        data = self.bsock.call("llist volumes all")
        volumes = data['volumes']
        for i in volumes:
            volume = i['volumename']
            self.add_subnode(Volume, i)
