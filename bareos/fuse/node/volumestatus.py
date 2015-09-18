"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File

class VolumeStatus(File):
    def __init__(self, bsock, name, volume):
        super(VolumeStatus, self).__init__(bsock, name, None)
        self.volume = volume
        self.update_stat()

    def do_update(self):
        volumename = self.volume['volumename']
        data = self.bsock.call( "llist volume=%s" % (volumename) )
        self.volume = data['volume']
        self.update_stat()

    def update_stat(self):
        try:
            self.stat.st_size = int(self.volume['volbytes'])            
            self.stat.st_atime = self._convert_date_bareos_unix(self.volume['labeldate'])
            #self.stat.st_ctime = stat['ctime']
            self.stat.st_mtime = self._convert_date_bareos_unix(self.volume['lastwritten'])
            # TODO: set mode dependend on if volume is in apppend mode or full
            #self.stat.st_mode = stat['mode']
        except KeyError as e:
            self.logger.warning(str(e))
            pass
