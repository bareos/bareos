This directive specifies the initial network buffer size to use. If not specified the operating system will choose an acceptable size.
Otherwise it will be adjusted down if it is too large until it is accepted by the OS.
Please use care in setting this value since if it is too large, it will be trimmed by 512 bytes until the OS is happy,
which may require a large number of system calls. The default value is 256 KiB.

This size is ultimately also used for the read buffer size on the client.

Note, on certain Windows machines, there are reports that the transfer rates are very slow and this seems to be related to the default size.
On systems where the transfer rates seem abnormally slow compared to other systems,
you might try setting the Maximum Network Buffer Size to 32,768 in both the File daemon and in the Storage daemon.
