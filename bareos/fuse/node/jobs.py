"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.jobslist import JobsList
from   bareos.fuse.node.jobsname import JobsName

class Jobs(Directory):
    def __init__(self, root, name):
        super(Jobs, self).__init__(root, name)
        self.static = True

    @classmethod
    def get_id(cls, name):
        return "unique"

    def do_update(self):
        data = self.bsock.call(".jobs")
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(JobsName, i['name'])
        self.add_subnode(JobsList, "all")
        self.add_subnode(JobsList, "running", "jobstatus=running")
        self.add_subnode(JobsList, "each_jobname_last_run", "last")
