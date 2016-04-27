"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.file import File
import stat

class VolumeStatus(File):

    __volstatus2filemode = {
        "Archive": 0o440,
        # rw
        "Append": 0o660,
        # ro
        "Full": 0o440,
        "Used": 0o440,
        # to be used
        "Recycle": 0o100,
        "Purged": 0o100,
        # not to use
        "Cleaning": 0000,
        "Error": 0000,
    }

    def __init__(self, root, basename, volume):
        super(VolumeStatus, self).__init__(root, None, None)
        self.basename = basename
        self.volume = volume
        self.do_update_stat()
        self.set_name( "%s%s" % (self.basename, self.volume['volstatus']) )

    @classmethod
    def get_id(cls, basename, volume):
        return "%s%s" % (basename, volume['mediaid'])

    def do_get_name(self, basename, volume):
        return "%s%s" % (basename, volume['volstatus'])

    def do_update(self):
        volumename = self.volume['volumename']
        data = self.bsock.call( "llist volume=%s" % (volumename) )
        self.volume = data['volume']
        self.do_update_stat()

    def do_update_stat(self):
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
        if volstatus in self.__volstatus2filemode:
            self.stat.st_mode = stat.S_IFREG | self.__volstatus2filemode[volstatus]
        else:
            self.logger.warning( "volume status %s unknown" % (self.volume['volstatus']) )
            self.stat.st_mode = stat.S_IFREG | 0000
