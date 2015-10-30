"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.job import Job

class JobsName(Directory):
    def __init__(self, root, name):
        super(JobsName, self).__init__(root, "job="+str(name))
        self.jobname=name

    @classmethod
    def get_id(cls, name):
        return name

    def do_update_stat(self):
        data = self.bsock.call("llist job=%s count" % (self.jobname))
        self.subnode_count = int(data['jobs'][0]['count'])

    def do_update(self):
        data = self.bsock.call("llist job=%s" % (self.jobname))
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(Job, i)
