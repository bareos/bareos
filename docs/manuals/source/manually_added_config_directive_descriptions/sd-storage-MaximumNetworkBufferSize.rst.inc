where bytes specifies the initial network buffer size to use with the File daemon. This size will be adjusted down if it is too large until it is accepted by the OS. Please use care in setting this value since if it is too large, it will be trimmed by 512 bytes until the OS is happy, which may require a large number of system calls. The default value is 256KiB.

It is recommended to not set this value unless necessary.  If necessary you should set this to the bandwidth delay product of your network.
For example if your fd is connected to the sd through a Gigabit connection (e.g. 128MiB/s bandwidth) and the connection has a latency of 4ms,
then the BDP of that connection is 128MiB/s * 4ms = 512KiB, if the connection has a latency of 32ms, then the BDP is
128MiB/s * 32ms = 4MiB.

You will probably also want to make the corresponding change in each of your File daemons conf files.

.. warning:: Setting this variable may disable your operating systems dynamic network buffering schema.  This could cause performance degradation and
   excessive memory use. Use with care.
