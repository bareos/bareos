"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.bvfsdir import BvfsDir
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node.file import File
from   bareos.fuse.node.joblog import JobLog
from   dateutil import parser as DateParser
import errno
import logging
from   pprint import pformat
import stat

class Job(Directory):
    def __init__(self, bsock, job):
        self.job = job
        super(Job, self).__init__(bsock, self.get_name())
        try:
            self.stat.st_ctime = self.convert_date_bareos_unix(self.job['starttime'])
        except KeyError:
            pass
        try:
            self.stat.st_mtime = self.convert_date_bareos_unix(self.job['realendtime'])
        except KeyError:
            pass

    def get_name(self):
        # TODO: adapt list backups to include name
        try:
            name = "jobid={jobid}_client={client}_name={name}_level={level}_status={jobstatus}".format(**self.job)
        except KeyError:
            name = "jobid={jobid}_level={level}_status={jobstatus}".format(**self.job)
        return name

    def convert_date_bareos_unix(self, bareosdate):
        unixtimestamp = int(DateParser.parse(bareosdate).strftime("%s"))
        self.logger.debug( "unix timestamp: %d" % (unixtimestamp))
        return unixtimestamp

    def do_update(self):
        self.add_subnode(File(self.bsock, name="info.txt", content = pformat(self.job) + "\n"))
        self.add_subnode(JobLog(self.bsock, self.job))
        self.add_subnode(BvfsDir(self.bsock, "data", self.job['jobid'], None))
