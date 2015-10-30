"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File

class JobLog(File):
    def __init__(self, root, name, job):
        super(JobLog, self).__init__(root, name)
        self.job = job
        if job['jobstatus'] == 'T' or job['jobstatus'] == 'E' or job['jobstatus'] == 'W':
            self.set_static()

    @classmethod
    def get_id(cls, name, job):
        return job['jobid']

    def do_update(self):
        jobid = self.job['jobid']
        data = self.bsock.call( "llist joblog jobid=%s" % (jobid) )
        self.content = ""
        for i in data['joblog']:
            self.content += str(i['time']) + " "
            try:
                self.content += str(i['logtext'])
            except KeyError:
                # some entries don't have logtext
                pass
            except UnicodeEncodeError as e:
                self.logger.error("failed to convert logtext to string: %s" % (str(e)))
                self.content += "BAREOFSFS SKIPPED: converting error\n"
