If Yes the archive device must support the \tt MTOFFL ioctl to rewind and take the volume offline. In this case, Bareos will issue the offline (eject) request before closing the device during the unmount command. If No Bareos will not attempt to offline the device before unmounting it. After an offline is issued, the cassette will be ejected thus requiring operator intervention to continue, and on some systems require an explicit load command to be issued (mt -f /dev/xxx load)
before the system will recognize the tape. If you are using an autochanger, some devices require an offline to be issued prior to changing the volume. However, most devices do not and may get very confused.

If you are using a Linux 2.6 kernel or other OSes such as FreeBSD or Solaris, the Offline On Unmount will leave the drive with no tape, and Bareos will not be able to properly open the drive and may fail the job.

