"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.bvfsdir import BvfsDir
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.file import File
from   bareos.fuse.node.joblog import JobLog
import errno
import logging
from   pprint import pformat
import stat

class Job(Directory):
    def __init__(self, root, job):
        self.job = job
        super(Job, self).__init__(root, self.get_name())
        try:
            if not job.has_key('client'):
                job['client'] = job['clientname']
        except KeyError:
            pass
        try:
            self.stat.st_ctime = self._convert_date_bareos_unix(self.job['starttime'])
        except KeyError:
            pass
        try:
            self.stat.st_mtime = self._convert_date_bareos_unix(self.job['realendtime'])
        except KeyError:
            pass
        if job['jobstatus'] == 'T' or job['jobstatus'] == 'E' or job['jobstatus'] == 'W':           
            self.set_static()

    @classmethod
    def get_id(cls, job):
        return job['jobid']

    def get_name(self):
        try:
            name = "jobid={jobid}_name={name}_client={client}_level={level}_status={jobstatus}".format(**self.job)
        except KeyError:
            try:
                name = "jobid={jobid}_name={name}_client={clientname}_level={level}_status={jobstatus}".format(**self.job)
            except KeyError:
                name = "jobid={jobid}_level={level}_status={jobstatus}".format(**self.job)
        return name

    def do_update(self):
        self.add_subnode(File, name="info.txt", content = pformat(self.job) + "\n")
        self.add_subnode(JobLog, name="job.log", job=self.job)
        self.add_subnode(BvfsDir, "data", self, None)
