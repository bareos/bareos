"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.jobslist import JobsList
from   bareos.fuse.node.jobsname import JobsName

class Jobs(Directory):
    def __init__(self, bsock, name):
        super(Jobs, self).__init__(bsock, name)
        self.static = True

    def do_update(self):
        data = self.bsock.call(".jobs")
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(JobsName(self.bsock, i['name']))
        self.add_subnode(JobsList(self.bsock, "all"))
