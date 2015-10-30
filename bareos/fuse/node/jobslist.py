"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.job import Job

class JobsList(Directory):
    def __init__(self, root, name, selector = ''):
        super(JobsList, self).__init__(root, name)
        self.selector = selector

    @classmethod
    def get_id(cls, name, selector = ''):
        return selector

    def do_update_stat(self):
        data = self.bsock.call("llist jobs %s count" % (self.selector))
        self.subnode_count = int(data['jobs'][0]['count'])

    def do_update(self):
        data = self.bsock.call("llist jobs %s" % (self.selector))
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(Job, i)
