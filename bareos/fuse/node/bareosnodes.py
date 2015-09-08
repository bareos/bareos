"""
Bareos specific Fuse nodes.
"""

from   bareos.fuse.node import Base
from   bareos.fuse.node import File
from   bareos.fuse.node import Directory
from   dateutil import parser as DateParser
import errno
import logging
from   pprint import pformat
import stat

class Jobs(Directory):
    def __init__(self, bsock, name):
        super(Jobs, self).__init__(bsock, name)

    def do_update(self):
        data = self.bsock.call("llist jobs")
        jobs = data['jobs']
        for i in jobs:
            self.add_subnode(Job(self.bsock, i))

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
            name = "jobid={jobid}_client={clientname}_name={name}_level={level}_status={jobstatus}".format(**self.job)
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


class Volumes(Directory):
    def __init__(self, bsock, name):
        super(Volumes, self).__init__(bsock, name)

    def do_update(self):
        data = self.bsock.call("llist volumes all")
        volumes = data['volumes']
        for i in volumes:
            volume = i['volumename']
            self.add_subnode(Volume(self.bsock, i))

class Volume(Directory):
    def __init__(self, bsock, volume):
        super(Volume, self).__init__(bsock, volume['volumename'])
        self.volume = volume

    def do_update(self):
        self.add_subnode(File(self.bsock, name="info.txt", content = pformat(self.volume) + "\n"))

class Clients(Directory):
    def __init__(self, bsock, name):
        super(Clients, self).__init__(bsock, name)

    def do_update(self):
        data = self.bsock.call(".clients")
        clients = data['clients']
        for i in clients:
            name = i['name']
            self.add_subnode(Client(self.bsock, name))

class Client(Directory):
    def __init__(self, bsock, name):
        super(Client, self).__init__(bsock, name)

    def do_update(self):
        self.add_subnode(Backups(self.bsock, "backups", client=self.get_name()))

class Backups(Directory):
    def __init__(self, bsock, name, client):
        super(Backups, self).__init__(bsock, name)
        self.client = client

    def do_update(self):
        data = self.bsock.call("llist backups client=%s" % (self.client))
        backups = data['backups']
        for i in backups:
            self.add_subnode(Job(self.bsock, i))

class BvfsDir(Directory):
    def __init__(self, bsock, directory, jobid, pathid):
        super(BvfsDir, self).__init__(bsock, directory)
        self.jobid = jobid
        self.pathid = pathid
        self.static = True

    def do_update(self):
        if self.pathid == None:
            self.bsock.call(".bvfs_update jobid=%s" % (self.jobid))
            path = "path=/"
        else:
            path = "pathid=%s" % self.pathid
        data = self.bsock.call(".bvfs_lsdirs  jobid=%s %s" % (self.jobid, path))
        directories = data['directories']
        data = self.bsock.call(".bvfs_lsfiles jobid=%s %s" % (self.jobid, path))
        files = data['files']
        for i in directories:
            if i['name'] != "." and i['name'] != "..":
                directory = i['name'].rstrip('/')
                pathid = i['pathid']
                self.add_subnode(BvfsDir(self.bsock, directory, self.jobid, pathid))
        for i in files:
            self.add_subnode(BvfsFile(self.bsock, i))

class BvfsFile(File):
    def __init__(self, bsock, file):
        super(BvfsFile, self).__init__(bsock, file['name'], content = None)
        self.file = file
        self.static = True
        self.do_update()

    def do_update(self):
        self.stat.st_mode = self.file['stat']['mode']
        self.stat.st_size = self.file['stat']['size']
        self.stat.st_atime = self.file['stat']['atime']
        self.stat.st_ctime = self.file['stat']['ctime']
        self.stat.st_mtime = self.file['stat']['mtime']

        #"stat": {
          #"atime": 1441134679,
          #"ino": 3689524,
          #"dev": 2051,
          #"mode": 33256,
          #"nlink": 1,
          #"user": "joergs",
          #"group": "joergs",
          #"ctime": 1441134679,
          #"rdev": 0,
          #"size": 1613,
          #"mtime": 1441134679
        #},
