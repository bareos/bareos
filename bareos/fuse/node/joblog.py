"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File

class JobLog(File):
    def __init__(self, bsock, job):
        super(JobLog, self).__init__(bsock, "joblog.txt")
        self.job = job
        # TODO: static when job is finished

    def do_update(self):
        jobid = self.job['jobid']
        data = self.bsock.call( "llist joblog jobid=%s" % (jobid) )
        self.content = ""
        for i in data['joblog']:
            self.content += str(i['time']) + " "
            self.content += str(i['logtext']) + "\n"
