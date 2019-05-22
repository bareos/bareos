If No, the archive device is not required to support keeping track of the file number (MTIOCGET ioctl) during forward space file. If Yes, the archive device must support the \tt ioctl \tt MTFSF call, which virtually all drivers support, but in addition, your SCSI driver must keep track of the file number on the tape and report it back correctly by the MTIOCGET ioctl. Note, some SCSI drivers will correctly forward space, but they do not keep track of the file number or
more seriously, they do not report end of medium.

