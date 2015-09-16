"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.job import Job

class Jobs(Directory):
    def __init__(self, bsock, name):
        super(Jobs, self).__init__(bsock, name)

    def do_update(self):
        data = self.bsock.call("llist jobs")
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(Job(self.bsock, i))
