"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File

class JobLog(File):
    def __init__(self, root, job):
        super(JobLog, self).__init__(root, "joblog.txt")
        self.job = job
        if job['jobstatus'] == 'T' or job['jobstatus'] == 'E' or job['jobstatus'] == 'W':
            self.set_static()

    @classmethod
    def get_id(cls, job):
        return job['jobid']

    def do_update(self):
        jobid = self.job['jobid']
        data = self.bsock.call( "llist joblog jobid=%s" % (jobid) )
        self.content = ""
        for i in data['joblog']:
            self.content += str(i['time']) + " "
            self.content += str(i['logtext']) + "\n"
