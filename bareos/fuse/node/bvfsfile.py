"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
import stat

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
