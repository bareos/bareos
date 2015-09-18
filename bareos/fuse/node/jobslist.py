"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.job import Job

class JobsList(Directory):
    def __init__(self, bsock, name, selector = ''):
        super(JobsList, self).__init__(bsock, name)
        self.selector = selector

    def do_update(self):
        data = self.bsock.call("llist jobs %s" % (self.selector))
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(Job(self.bsock, i))
