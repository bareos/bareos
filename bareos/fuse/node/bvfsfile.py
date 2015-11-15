"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.bvfscommon import BvfsCommon
from   bareos.fuse.node.file import File
import errno
import os

class BvfsFile(File, BvfsCommon):
    def __init__(self, root, file, job, bvfspath):
        super(BvfsFile, self).__init__(root, file['name'], content = None)
        self.file = file
        self.job = job
        self.bvfspath = bvfspath
        self.id = self.get_id(file, job, bvfspath)
        BvfsCommon.init(self, self.root, self.file['jobid'], self.bvfspath,  self.file['name'], self.file['stat'])

    @classmethod
    def get_id(cls, file, job, bvfspath):
        return "jobid=%s_fileid=%s" % (str(job.job['jobid']),str(file['fileid']))


    # Filesystem methods
    # ==================

    def readlink(self, path):
        self.logger.debug( "readlink %s" % (path) )
        try:
            pathname = os.readlink(self.restorepathfull)
            if pathname.startswith("/"):
                # Path name is absolute, sanitize it.
                return str(os.path.relpath(pathname, self.restorepath))
            else:
                return str(pathname)
        except OSError:
            return -errno.ENOENT

    def setxattr(self, path, key, value, flags):
        if path.len() == 0:
            if key == self.XattrKeyRestoreTrigger and value == "restore":
                if not self.root.restoreclient:
                    self.logger.error("no restoreclient given, files can not be restored")
                    return -errno.EPERM
                if not self.root.restorepath:
                    self.logger.error("no restorepath given, files can not be restored")
                    return -errno.EPERM
                else:
                    self.restore(path, None, [ self.file['fileid'] ])
        return super(BvfsFile, self).setxattr(path, key, value, flags)


    # File methods
    # ============

    def open(self, path, flags):
        return os.open(self.restorepathfull, flags)

    #def create(self, path, mode, fi=None):
        #full_path = self._full_path(path)
        #return os.open(full_path, os.O_WRONLY | os.O_CREAT, mode)

    def read(self, path, length, offset):
        try:
            fh=self.open(path, os.O_RDONLY)
            os.lseek(fh, offset, os.SEEK_SET)
            return os.read(fh, length)
        except OSError:
            return -errno.ENOENT

    #def write(self, path, buf, offset, fh):
        #os.lseek(fh, offset, os.SEEK_SET)
        #return os.write(fh, buf)

    #def truncate(self, path, length, fh=None):
        #full_path = self._full_path(path)
        #with open(full_path, 'r+') as f:
            #f.truncate(length)

    #def flush(self, path, fh):
        #return os.fsync(fh)

    #def release(self, path, fh):
        #return os.close(fh)

    #def fsync(self, path, fdatasync, fh):
        #return self.flush(path, fh)
