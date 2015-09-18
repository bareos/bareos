"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.jobslist import JobsList
from   bareos.fuse.node.volumestatus import VolumeStatus
from   pprint import pformat

class Volume(Directory):
    def __init__(self, bsock, volume):
        super(Volume, self).__init__(bsock, volume['volumename'])
        self.volume = volume

    def do_update(self):
        self.add_subnode(File(self.bsock, name="info.txt", content=pformat(self.volume) + "\n"))
        self.add_subnode(JobsList(self.bsock, name="jobs", selector="volume=%s" % (self.name)))
        self.add_subnode(VolumeStatus(self.bsock, name="volume", volume=self.volume))
