"""
Bareos specific Fuse node.
"""

from   bareos.exceptions import *
from   bareos.fuse.node.bvfscommon import BvfsCommon
from   bareos.fuse.node.bvfsfile import BvfsFile
from   bareos.fuse.node.directory import Directory
from   dateutil import parser as DateParser
import errno
import logging

class BvfsDir(Directory, BvfsCommon):
    def __init__(self, root, name, job, pathid, directory = None):
        super(BvfsDir, self).__init__(root, name)
        self.job = job
        self.jobid = job.job['jobid']
        self.pathid = pathid
        self.static = True
        self.directory = directory
        if directory:
            self.set_stat(directory['stat'])
            BvfsCommon.init(self, self.root, self.jobid, self.directory['fullpath'],  None, self.directory['stat'])

    @classmethod
    def get_id(cls, name, job, pathid, directory = None):
        id = None
        if pathid == None:
            id = "jobid=%s" % (str(job.job['jobid']))
        else:
            id = "jobid=%s_pathid=%s" % (str(job.job['jobid']),str(pathid))
        return id

    def setxattr(self, path, key, value, flags):
        if path.len() == 0:
            self.logger.debug("value: " + value + " ,type: " + str(type(value)))
            if key == self.XattrKeyRestoreTrigger and value == b"restore":
                if not self.root.restoreclient:
                    self.logger.warning("no restoreclient given, files can not be restored")
                    return -errno.ENOENT
                if not self.root.restorepath:
                    self.logger.warning("no restorepath given, files can not be restored")
                    return -errno.ENOENT
                else:
                    self.restore(path, [ self.pathid ], None)
        return super(BvfsDir, self).setxattr(path, key, value, flags)

    def do_update(self):
        directories = self.get_directories(self.pathid)
        files = self.get_files(self.pathid)
        for i in directories:
            if i['name'] != "." and i['name'] != "..":
                name = i['name'].rstrip('/')
                pathid = i['pathid']
                self.add_subnode(BvfsDir, name, self.job, pathid, i)
        self.subnode_count = len(self.subnodes)
        if self.directory:
            for i in files:
                self.add_subnode(BvfsFile, i, self.job, self.directory['fullpath'])

    def get_directories(self, pathid):
        if pathid == None:
            self.bsock.call(".bvfs_update jobid=%s" % (self.jobid))
            path = 'path=""'
        else:
            path = 'pathid=%s' % pathid
        data = self.bsock.call('.bvfs_lsdirs  jobid=%s %s' % (self.jobid, path))
        directories = data['directories']
        if pathid == None:        
            found = False
            for i in directories:
                if i['name'] != "." and i['name'] != "..":
                    if i['name'] == "/":
                        self.directory = i
                        self.pathid = i['pathid']
                        BvfsCommon.init(self, self.root, self.jobid, i['fullpath'],  None, i['stat'])
                    else:
                        found = True
            if not found:
                # skip empty root directory
                if not self.pathid:
                    # no pathid and no directoies found. This should not happen
                    return []
                return self.get_directories(self.pathid)
        return directories

    def get_files(self, pathid):
        if pathid:
            data = self.bsock.call(".bvfs_lsfiles jobid=%s pathid=%s" % (self.jobid, pathid))
            return data['files']
        else:
            return []
