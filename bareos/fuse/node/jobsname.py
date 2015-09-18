"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.job import Job

class JobsName(Directory):
    def __init__(self, bsock, name):
        super(JobsName, self).__init__(bsock, "job="+str(name))
        self.jobname=name

    def do_update(self):
        data = self.bsock.call("llist job=%s" % (self.jobname))
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(Job(self.bsock, i))
