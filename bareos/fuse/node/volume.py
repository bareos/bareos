"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.jobslist import JobsList
from   bareos.fuse.node.volumestatus import VolumeStatus
from   pprint import pformat

class Volume(Directory):
    def __init__(self, root, name, volume):
        super(Volume, self).__init__(root, name)
        self.volume = volume

    @classmethod
    def get_id(cls, name, volume):
        return volume['volumename']

    def do_update_stat(self):
        self.subnode_count = 3

    def do_update(self):
        self.add_subnode(File, name="info.txt", content=pformat(self.volume) + "\n")
        self.add_subnode(JobsList, name="jobs", selector="volume=%s" % (self.name))
        self.add_subnode(VolumeStatus, "status=", volume=self.volume)
