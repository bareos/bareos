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
    def __init__(self, root, name, jobid, pathid, directory = None):
        super(BvfsDir, self).__init__(root, name)
        self.jobid = jobid
        self.pathid = pathid
        self.static = True
        if directory:
            self.set_stat(directory['stat'])

    @classmethod
    def get_id(cls, name, jobid, pathid, directory = None):
        return str(pathid)

    def do_update(self):
        directories = self.get_directories(self.pathid)
        files = self.get_files(self.pathid)
        for i in directories:
            if i['name'] != "." and i['name'] != "..":
                name = i['name'].rstrip('/')
                pathid = i['pathid']
                self.add_subnode(BvfsDir, name, self.jobid, pathid, i)
        for i in files:
            self.add_subnode(BvfsFile, i)

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
