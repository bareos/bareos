"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.jobslist import JobsList
from   bareos.fuse.node.volumestatus import VolumeStatus
from   pprint import pformat

class Volume(Directory):
    def __init__(self, root, volume):
        super(Volume, self).__init__(root, volume['volumename'])
        self.volume = volume

    @classmethod
    def get_id(cls, volume):
        return volume['volumename']

    def do_update(self):
        self.add_subnode(File, name="info.txt", content=pformat(self.volume) + "\n")
        self.add_subnode(JobsList, name="jobs", selector="volume=%s" % (self.name))
        self.add_subnode(VolumeStatus, name="volume", volume=self.volume)
