"""
Bareos specific Fuse node.
"""

from   bareos.exceptions import *
from   bareos.fuse.node.bvfsfile import BvfsFile
from   bareos.fuse.node.directory import Directory
from   dateutil import parser as DateParser
import errno
import logging

class BvfsDir(Directory):
    def __init__(self, bsock, directory, jobid, pathid):
        super(BvfsDir, self).__init__(bsock, directory)
        self.jobid = jobid
        self.pathid = pathid
        self.static = True

    def do_update(self):
        directories = self.get_directories(self.pathid)
        files = self.get_files(self.pathid)
        for i in directories:
            if i['name'] != "." and i['name'] != "..":
                if i['name'] != "/":
                    directory = i['name'].rstrip('/')
                else:
                    directory = "data"
                pathid = i['pathid']
                self.add_subnode(BvfsDir(self.bsock, directory, self.jobid, pathid))
        for i in files:
            self.add_subnode(BvfsFile(self.bsock, i))

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
                        self.pathid = i['pathid']
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
