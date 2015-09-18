"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
import stat

class VolumeStatus(File):

    __volstatus2filemode = {
        # rw
        "Append": 0660,
        # ro
        "Full": 0440,
        "Used": 0440,
        # to be used
        "Recycle": 0100,
        "Purged": 0100,
        # not to use
        "Cleaning": 0000,
        "Error": 0000,
    }

    def __init__(self, bsock, name, volume):
        super(VolumeStatus, self).__init__(bsock, None, None)
        self.volume = volume
        self.update_stat()
        self.set_name( "status=%s" % (self.volume['volstatus']) )

    def do_update(self):
        volumename = self.volume['volumename']
        data = self.bsock.call( "llist volume=%s" % (volumename) )
        self.volume = data['volume']
        self.update_stat()

    def update_stat(self):
        try:
            self.set_name( "status=%s" % (self.volume['volstatus']) )
            self.stat.st_size = int(self.volume['volbytes'])            
            self.stat.st_atime = self._convert_date_bareos_unix(self.volume['labeldate'])
            #self.stat.st_ctime = stat['ctime']
            self.stat.st_mtime = self._convert_date_bareos_unix(self.volume['lastwritten'])
        except KeyError as e:
            self.logger.warning(str(e))
            pass

        # set mode dependend on if volume is in apppend mode or full
        volstatus = self.volume['volstatus']
        if self.__volstatus2filemode.has_key(volstatus):
            self.stat.st_mode = stat.S_IFREG | self.__volstatus2filemode[volstatus]
        else:
            self.logger.warning( "volume status %s unknown" )
            self.stat.st_mode = stat.S_IFREG | 0000
