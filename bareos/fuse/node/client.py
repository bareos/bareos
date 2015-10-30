"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.backups import Backups
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.jobslist import JobsList

class Client(Directory):
    def __init__(self, root, name):
        super(Client, self).__init__(root, name)

    @classmethod
    def get_id(cls, name):
        return name

    def do_update(self):
        self.add_subnode(Backups, "backups", client=self.name)
        self.add_subnode(JobsList, "jobs", "client=%s" % (self.name))
