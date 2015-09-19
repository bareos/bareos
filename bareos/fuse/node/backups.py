"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.job import Job

class Backups(Directory):
    def __init__(self, root, name, client):
        super(Backups, self).__init__(root, name)
        self.client = client

    @classmethod
    def get_id(cls, name, client):
        return client

    def do_update(self):
        data = self.bsock.call("llist backups client=%s" % (self.client))
        backups = data['backups']
        for i in backups:
            self.add_subnode(Job, i)
