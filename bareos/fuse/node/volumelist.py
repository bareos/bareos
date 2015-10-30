"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.volume import Volume
from   bareos.fuse.node.volumestatus import VolumeStatus

class VolumeList(Directory):
    def __init__(self, root, name, selector = 'all'):
        super(VolumeList, self).__init__(root, name)
        self.selector = selector

    @classmethod
    def get_id(cls, name, selector = 'all'):
        return selector

    def do_update(self):
        data = self.bsock.call("llist volumes %s" % (self.selector))
        volumes = data['volumes']
        for i in volumes:
            volumename = i['volumename']
            self.add_subnode(Volume, name=volumename, volume=i)
            self.add_subnode(VolumeStatus, "%s=" % (volumename), volume=i)
