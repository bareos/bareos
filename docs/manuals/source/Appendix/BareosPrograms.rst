.. _section-Utilities:

Bareos Programs
===============

Bareos Daemons
--------------

.. _section-daemon-command-line-options:

Daemon Command Line Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   pair: Daemon; Command Line Options

Each of the three daemons (Director, File, Storage) accepts a small set of options on the command line. In general, each of the daemons as well as the Console program accepts the following options:

-c <path>
   Define the file or directory to use for the configuration. See :ref:`section-ConfigurationPathLayout`.

-d nnn
   Set the debug level to nnn. Generally anything between 50 and 200 is reasonable. The higher the number, the more output is produced. The output is written to standard output. The debug level can also be set during runtime, see section :ref:`bconsole: setdebug <bcommandSetdebug>`.

-f
   Run the daemon in the foreground. This option is needed to run the daemon under the debugger.

-g <group>
   Run the daemon under this group. This must be a group name, not a GID.

-s
   Do not trap signals. This option is needed to run the daemon under the debugger.

-t
   Read the configuration file and print any error messages, then immediately exit. Useful for syntax testing of new configuration files.

-u <user>
   Run the daemon as this user. This must be a user name, not a UID.

-v
   Be more verbose or more complete in printing error and informational messages.

-xc
   Print the current configuration and exit.

-xs
   Print configuration schema in JSON format and exit.

-?
   Print the version and list of options.

.. _command-bareos-dir:

bareos-dir
~~~~~~~~~~

:index:`\ <single: Command; bareos-dir>`\  :index:`\ <single: Command Line Options>`\

|dir|.

.. _command-bareos-sd:

bareos-sd
~~~~~~~~~

:index:`\ <single: Command; bareos-sd>`\  :index:`\ <single: Command Line Options>`\

|sd|.

.. _command-bareos-fd:

bareos-fd
~~~~~~~~~

:index:`\ <single: Command; bareos-fd>`\  :index:`\ <single: Command Line Options>`\

|fd|.

Interactive Programs
--------------------

bconsole
~~~~~~~~

There is an own chapter on :command:`bconsole`. Please refer to chapter :ref:`section-bconsole`.

bareos-webui
~~~~~~~~~~~~

For further information regarding the Bareos Webui, please refer to :ref:`section-webui`.

bat
~~~

:index:`\ <single: Command; bat>`\

.. _bat:



The Bacula/Bareos Administration Tool (:command:`bat`) has been a native GUI for Bareos. It has been marked deprecated since :sinceVersion:`15.2.0: bat vs. bareos-webui`. Since Bareos :sinceVersion:`17.2.0: bat: removed from core distribution` it is no longer part of Bareos. We encourage the use of |webui| instead.

.. _section-VolumeUtilityCommands:

Volume Utility Commands
-----------------------

:index:`\ <single: Volume Utility Tools>`
:index:`\ <single: Tools; Volume Utility>`

This document describes the utility programs written to aid Bareos users and developers in dealing with Volumes external to Bareos and to perform other useful tasks.

Parameter
~~~~~~~~~

Specifying the Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each of the utilities that deal with Volumes require a valid |sd| configuration (actually, the only part of the configuration file that these programs need is the :config:option:`Sd/Device`\  resource definitions). This permits the programs to find the configuration parameters for your :config:option:`sd/device/ArchiveDevice`\ . Using the :strong:`-c` option a custom |sd| configuration file or directory can be
selected.

Specifying a Device
^^^^^^^^^^^^^^^^^^^

Each of these programs require a :strong:`device-name` where the Volume can be found. The device-name is either the name of the |sd| device (:config:option:`sd/device/Name`\ ) or its :config:option:`sd/device/ArchiveDevice`\ .

Specifying a Device Name For a Tape
'''''''''''''''''''''''''''''''''''

In the case of a tape, this is the physical device name such as /dev/nst0 or /dev/rmt/0ubn depending on your system.



.. warning::

   If you have Bareos running and you want to use
   one of these programs, you will either need to stop the |sd|
   or :bcommand:`unmount` any tape drive you want to use,
   otherwise the drive may get busy because Bareos is using it.
   After this, you can use the command :command:`mtx` or :ref:`mtx-changer script <section-MtxChangerManualUsage>`
   to load the required volume into the tape drive.


Specifying a Device Name For a File
'''''''''''''''''''''''''''''''''''

If you are attempting to read or write an archive file rather than a tape, the :strong:`device-name` can be the full path to the archive location specified at :config:option:`sd/device/ArchiveDevice`\  or this including the filename of the volume. The filename (last part of the specification) will be stripped and used as the Volume name So, the path is equivalent to the :config:option:`sd/device/ArchiveDevice`\  and the filename is
equivalent to the volume name.

Specifying Volumes
^^^^^^^^^^^^^^^^^^

:index:`\ <single: Volumes; Specifying>`\  :index:`\ <single: Bootstrap>`\

Often you must specify the Volume name to the programs below. The best method to do so is to specify a bootstrap file on the command line with the :strong:`-b` option. As part of the bootstrap file, you will then specify the Volume name or Volume names if more than one volume is needed. For example, suppose you want to read tapes **tapevolume1** and **tapevolume2**. First construct a bootstrap file named say, :file:`list.bsr` which
contains:



::

   Volume=tapevolume1|tapevolume2



where each Volume is separated by a vertical bar. Then simply use:

.. code-block:: shell-session

   bls -b list.bsr /dev/nst0

In the case of Bareos Volumes that are on files, you may simply append volumes as follows:

.. code-block:: shell-session

   bls /var/lib/bareos/storage/volume1\|volume2

where the backslash (\) was necessary as a shell escape to permit entering the vertical bar (|).

And finally, if you feel that specifying a Volume name is a bit complicated with a bootstrap file, you can use the :strong:`-V` option (on all programs except :command:`bcopy`) to specify one or more Volume names separated by the vertical bar (|). For example:

.. code-block:: shell-session

   bls /dev/nst0 -V tapevolume1

You may also specify an asterisk (*) to indicate that the program should accept any volume. For example:

.. code-block:: shell-session

   bls /dev/nst0 -V*

If your |sd| has following resource,

.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/FileStorage.conf

   Device {
     Name = FileStorage
     Archive Device = /var/lib/bareos/storage
     ...
   }

following calls of :command:`bls` should behave identical:

.. code-block:: shell-session
   :caption: bls using Storage Device Name

   bls FileStorage -V Full1

or

.. code-block:: shell-session
   :caption: bls using the Archive Device of a Storage Device

   bls /var/lib/bareos/storage -V Full1

or

.. code-block:: shell-session
   :caption: bls using the Archive Device of a Storage Device and volume name

   bls /var/lib/bareos/storage/Full1

Specifying Maximum Block Size
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you use Bareos with non-default block sizes defined in the pools (:config:option:`dir/pool/MaximumBlockSize`\ ), it might be necessary to specify the :config:option:`sd/device/MaximumBlockSize`\  also in the storage device resource, see :ref:`Direct access to Volumes with non-default blocksizes <direct-access-to-volumes-with-non-default-blocksizes>`.

.. _bls:

bls
~~~

:index:`\ <single: bls>`
:index:`\ <single: Command; bls>`

:command:`bls` can be used to do an :command:`ls` type listing of a Bareos tape or file. It is called:

.. code-block:: shell-session

   Usage: bls [options] <device-name>
          -b <file>       specify a bootstrap file
          -c <file>       specify a Storage configuration file
          -D <director>   specify a director name specified in the Storage
                          configuration file for the Key Encryption Key selection
          -d <nn>         set debug level to <nn>
          -dt             print timestamp in debug output
          -e <file>       exclude list
          -i <file>       include list
          -j              list jobs
          -k              list blocks
       (no j or k option) list saved files
          -L              dump label
          -p              proceed inspite of errors
          -v              be verbose
          -V              specify Volume names (separated by |)
          -?              print this message

Normally if no options are specified, :command:`bls` will produce the equivalent output to the :command:`ls -l` command for each volume.

For example, to list the contents of a tape:

.. code-block:: shell-session

   bls -V Volume-name /dev/nst0

Or to list the contents of a volume file:

.. code-block:: shell-session

   bls FileStorage -V Full1

or

.. code-block:: shell-session

   bls /var/lib/bareos/storage -V Full1

or

.. code-block:: shell-session

   bls /var/lib/bareos/storage/Full1

For example:

.. code-block:: shell-session

   root@host:~# bls FileStorage -V Full1
   bls: butil.c:282-0 Using device: "/var/lib/bareos/storage" for reading.
   12-Sep 18:30 bls JobId 0: Ready to read from volume "Full1" on device "FileStorage" (/var/lib/bareos/storage).
   bls JobId 1: -rwxr-xr-x   1 root     root            4614 2013-01-22 22:24:11  /usr/sbin/service
   bls JobId 1: -rwxr-xr-x   1 root     root           13992 2013-01-22 22:24:12  /usr/sbin/rtcwake
   bls JobId 1: -rwxr-xr-x   1 root     root            6243 2013-02-06 11:01:29  /usr/sbin/update-fonts-scale
   bls JobId 1: -rwxr-xr-x   1 root     root           43240 2013-01-22 22:24:10  /usr/sbin/grpck
   bls JobId 1: -rwxr-xr-x   1 root     root           16894 2013-01-22 22:24:11  /usr/sbin/update-rc.d
   bls JobId 1: -rwxr-xr-x   1 root     root            9480 2013-01-22 22:47:43  /usr/sbin/gss_clnt_send_err
   ...
   bls JobId 456: -rw-r-----   1 root     bareos          1008 2013-05-23 13:17:45  /etc/bareos/bareos-fd.conf
   bls JobId 456: drwxr-xr-x   2 root     root            4096 2013-07-04 17:40:21  /etc/bareos/
   12-Sep 18:30 bls JobId 0: End of Volume at file 0 on device "FileStorage" (/var/lib/bareos/storage), Volume "Full1"
   12-Sep 18:30 bls JobId 0: End of all volumes.
   2972 files found.

Show Detailed File Information
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To retrieve information, about how a file is stored on the volume, you can use :command:`bls` in verbose mode:

.. code-block:: shell-session

   root@host:~# bls FileStorage -V TestVolume001 -v
   bls: butil.c:273-0 Using device: "FileStorage" for reading.
   22-Jun 19:34 bls JobId 0: Ready to read from volume "TestVolume001" on device "Storage1" (/var/lib/bareos/storage).
   Volume Label Record: VolSessionId=1 VolSessionTime=1498152622 JobId=0 DataLen=168
   Begin Job Session Record: VolSessionId=1 VolSessionTime=1498152622 JobId=1 DataLen=169
   FileIndex=1 Stream=1  UATTR                     DataLen=129   | -rw-rw-r--   1 root     root               5 2017-06-22 19:30:21
                                                                 | /srv/data/test1.dat
   FileIndex=1 Stream=29 COMPRESSED                DataLen=25    | GZIP, level=9, version=1, length=13
   FileIndex=1 Stream=3  MD5                       DataLen=16    | 2Oj8otwPiW/Xy0ywAxuiSQ (base64)
   FileIndex=2 Stream=1  UATTR                     DataLen=123   | drwxrwxr-x   2 root     root            4096 2017-06-22 19:30:21
                                                                 | /srv/data/
   ...
   End Job Session Record: VolSessionId=1 VolSessionTime=1498152622 JobId=1
   DataLen=205
   22-Jun 19:34 bls JobId 0: End of Volume at file 0 on device "FileStorage" (/var/lib/bareos/storage), Volume "TestVolume001"
   22-Jun 19:34 bls JobId 0: End of all volumes.
   End of Physical Medium Record: VolSessionId=0 VolSessionTime=0 JobId=0 DataLen=0
   9 files and directories found.

For details about the Volume format, see \bareosDeveloperGuideStorageMediaOutputFormat.

Show Label Information
^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: bls; Label>`\

Using the :strong:`-L` the label information of a Volume is shown:

.. code-block:: shell-session
   :caption: bls: show volume label

   root@host:~# bls -L /var/lib/bareos/storage/testvol
   bls: butil.c:282-0 Using device: "/var/lib/bareos/storage" for reading.
   12-Sep 18:41 bls JobId 0: Ready to read from volume "testvol" on device "FileStorage" (/var/lib/bareos/storage).

   Volume Label:
   Id                : Bareos 0.9 mortal
   VerNo             : 10
   VolName           : File002
   PrevVolName       :
   VolFile           : 0
   LabelType         : VOL_LABEL
   LabelSize         : 147
   PoolName          : Default
   MediaType         : File
   PoolType          : Backup
   HostName          : debian6
   Date label written: 06-Mar-2013 17:21

Listing Jobs
^^^^^^^^^^^^

:index:`\ <single: Listing Jobs with bls>`\  :index:`\ <single: bls; Listing Jobs>`\

If you are listing a Volume to determine what Jobs to restore, normally the :strong:`-j` option provides you with most of what you will need as long as you don’t have multiple clients. For example:

.. code-block:: shell-session
   :caption: bls: list jobs

   root@host:~# bls /var/lib/bareos/storage/testvol -j
   bls: butil.c:282-0 Using device: "/var/lib/bareos/storage" for reading.
   12-Sep 18:33 bls JobId 0: Ready to read from volume "testvol" on device "FileStorage" (/var/lib/bareos/storage).
   Volume Record: File:blk=0:193 SessId=1 SessTime=1362582744 JobId=0 DataLen=158
   Begin Job Session Record: File:blk=0:64705 SessId=1 SessTime=1362582744 JobId=1
      Job=BackupClient1.2013-03-06_17.22.48_05 Date=06-Mar-2013 17:22:51 Level=F Type=B
   End Job Session Record: File:blk=0:6499290 SessId=1 SessTime=1362582744 JobId=1
      Date=06-Mar-2013 17:22:52 Level=F Type=B Files=162 Bytes=6,489,071 Errors=0 Status=T
   Begin Job Session Record: File:blk=0:6563802 SessId=2 SessTime=1362582744 JobId=2
      Job=BackupClient1.2013-03-06_23.05.00_02 Date=06-Mar-2013 23:05:02 Level=I Type=B
   End Job Session Record: File:blk=0:18832687 SessId=2 SessTime=1362582744 JobId=2
      Date=06-Mar-2013 23:05:02 Level=I Type=B Files=3 Bytes=12,323,791 Errors=0 Status=T
   ...
   Begin Job Session Record: File:blk=0:319219736 SessId=299 SessTime=1369307832 JobId=454
      Job=BackupClient1.2013-09-11_23.05.00_25 Date=11-Sep-2013 23:05:03 Level=I Type=B
   End Job Session Record: File:blk=0:319219736 SessId=299 SessTime=1369307832 JobId=454
      Date=11-Sep-2013 23:05:03 Level=I Type=B Files=0 Bytes=0 Errors=0 Status=T
   Begin Job Session Record: File:blk=0:319284248 SessId=301 SessTime=1369307832 JobId=456
      Job=BackupCatalog.2013-09-11_23.10.00_28 Date=11-Sep-2013 23:10:03 Level=F Type=B
   End Job Session Record: File:blk=0:320694269 SessId=301 SessTime=1369307832 JobId=456
      Date=11-Sep-2013 23:10:03 Level=F Type=B Files=12 Bytes=1,472,681 Errors=0 Status=T
   12-Sep 18:32 bls JobId 0: End of Volume at file 0 on device "FileStorage" (/var/lib/bareos/storage), Volume "testvol"
   12-Sep 18:32 bls JobId 0: End of all volumes.

Adding the :strong:`-v` option will display virtually all information that is available for each record.

Listing Blocks
^^^^^^^^^^^^^^

:index:`\ <single: Listing Blocks with bls>`\  :index:`\ <single: bls; Listing Blocks>`\

Normally, except for debugging purposes, you will not need to list Bareos blocks (the "primitive" unit of Bareos data on the Volume). However, you can do so with:

.. code-block:: shell-session

   root@host:~# bls -k /tmp/File002
   bls: butil.c:148 Using device: /tmp
   Block: 1 size=64512
   Block: 2 size=64512
   ...
   Block: 65 size=64512
   Block: 66 size=19195
   bls: Got EOF on device /tmp
   End of File on device

By adding the :strong:`-v` option, you can get more information, which can be useful in knowing what sessions were written to the volume:

.. code-block:: shell-session

   root@host:~# bls -k -v /tmp/File002
   Date label written: 2002-10-19 at 21:16
   Block: 1 blen=64512 First rec FI=VOL_LABEL SessId=1 SessTim=1035062102 Strm=0 rlen=147
   Block: 2 blen=64512 First rec FI=6 SessId=1 SessTim=1035062102 Strm=DATA rlen=4087
   Block: 3 blen=64512 First rec FI=12 SessId=1 SessTim=1035062102 Strm=DATA rlen=5902
   Block: 4 blen=64512 First rec FI=19 SessId=1 SessTim=1035062102 Strm=DATA rlen=28382
   ...
   Block: 65 blen=64512 First rec FI=83 SessId=1 SessTim=1035062102 Strm=DATA rlen=1873
   Block: 66 blen=19195 First rec FI=83 SessId=1 SessTim=1035062102 Strm=DATA rlen=2973
   bls: Got EOF on device /tmp
   End of File on device

Armed with the SessionId and the SessionTime, you can extract just about anything.

If you want to know even more, add a second :strong:`-v` to the command line to get a dump of every record in every block.

.. code-block:: shell-session

   root@host:~# bls -k -vv /tmp/File002
   bls: block.c:79 Dump block  80f8ad0: size=64512 BlkNum=1
                  Hdrcksum=b1bdfd6d cksum=b1bdfd6d
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=VOL_LABEL Strm=0 len=147 p=80f8b40
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=SOS_LABEL Strm=-7 len=122 p=80f8be7
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=1 Strm=UATTR len=86 p=80f8c75
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=2 Strm=UATTR len=90 p=80f8cdf
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=3 Strm=UATTR len=92 p=80f8d4d
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=3 Strm=DATA len=54 p=80f8dbd
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=3 Strm=MD5 len=16 p=80f8e07
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=4 Strm=UATTR len=98 p=80f8e2b
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=4 Strm=DATA len=16 p=80f8ea1
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=4 Strm=MD5 len=16 p=80f8ec5
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=5 Strm=UATTR len=96 p=80f8ee9
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=5 Strm=DATA len=1783 p=80f8f5d
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=5 Strm=MD5 len=16 p=80f9668
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=6 Strm=UATTR len=95 p=80f968c
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=6 Strm=DATA len=32768 p=80f96ff
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=6 Strm=DATA len=32768 p=8101713
   bls: block.c:79 Dump block  80f8ad0: size=64512 BlkNum=2
                  Hdrcksum=9acc1e7f cksum=9acc1e7f
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=6 Strm=contDATA len=4087 p=80f8b40
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=6 Strm=DATA len=31970 p=80f9b4b
   bls: block.c:92    Rec: VId=1 VT=1035062102 FI=6 Strm=MD5 len=16 p=8101841
   ...


.. _bextract:

bextract
~~~~~~~~

:index:`\ <single: bextract>`\  :index:`\ <single: Command; bextract>`\  :index:`\ <single: Disaster; Recovery; bextract>`\

If you find yourself using :command:`bextract`, you probably have done something wrong. For example, if you are trying to recover a file but are having problems, please see the :ref:`section-RestoreCatalog` chapter.

Normally, you will restore files by running a Restore Job from the Console program. However, :command:`bextract` can be used to extract a single file or a list of files from a Bareos tape or file. In fact, :command:`bextract` can be a useful tool to restore files to an empty system assuming you are able to boot, you have statically linked :command:`bextract` and you have an appropriate bootstrap file.

Please note that some of the current limitations of :command:`bextract` are:

#. It cannot restore access control lists (ACL) that have been backed up along with the file data.

#. It cannot restore encrypted files.

#. The command line length is relatively limited, which means that you cannot enter a huge number of volumes. If you need to enter more volumes than the command line supports, please use a bootstrap file (see below).

#. Extracting files from a Windows backup on a Linux system will only extract the plain files, not the additional Windows file information. If you have to extract files from a Windows backup, you should use the Windows version of :command:`bextract`.

It is called:

.. code-block:: shell-session

   Usage: bextract <options> <bareos-archive-device-name> <directory-to-store-files>
          -b <file>       specify a bootstrap file
          -c <file>       specify a Storage configuration file
          -D <director>   specify a director name specified in the Storage
                          configuration file for the Key Encryption Key selection
          -d <nn>         set debug level to <nn>
          -dt             print timestamp in debug output
          -e <file>       exclude list
          -i <file>       include list
          -p              proceed inspite of I/O errors
          -v              verbose
          -V <volumes>    specify Volume names (separated by |)
          -?              print this message

where device-name is the Archive Device (raw device name or full filename) of the device to be read, and directory-to-store-files is a path prefix to prepend to all the files restored.



.. warning::

   On Windows systems, if you specify a prefix of say d:/tmp, any file that
   would have been restored to :file:`C:/My Documents` will be restored to :file:`D:/tmp/My Documents`.
   That is, the original drive specification will be
   stripped. If no prefix is specified, the file will be restored to the original
   drive.

Extracting with Include or Exclude Lists
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Using the -e option, you can specify a file containing a list of files to be excluded. Wildcards can be used in the exclusion list. This option will normally be used in conjunction with the -i option (see below). Both the -e and the -i options may be specified at the same time as the -b option. The bootstrap filters will be applied first, then the include list, then the exclude list.

Likewise, and probably more importantly, with the -i option, you can specify a file that contains a list (one file per line) of files and directories to include to be restored. The list must contain the full filename with the path. If you specify a path name only, all files and subdirectories of that path will be restored. If you specify a line containing only the filename (e.g. my-file.txt) it probably will not be extracted because you have not specified the full path.

For example, if the file include-list contains:



::

   /etc/bareos
   /usr/sbin



Then the command:

.. code-block:: shell-session

   bextract -i include-list -V Volume /dev/nst0 /tmp

will restore from the Bareos archive /dev/nst0 all files and directories in the backup from /etc/bareos and from /usr/sbin. The restored files will be placed in a file of the original name under the directory /tmp (i.e. /tmp/etc/bareos/... and /tmp/usr/sbin/...).

Extracting With a Bootstrap File
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The -b option is used to specify a bootstrap file containing the information needed to restore precisely the files you want. Specifying a bootstrap file is optional but recommended because it gives you the most control over which files will be restored. For more details on the bootstrap file, please see :ref:`Restoring Files with the Bootstrap File <BootstrapChapter>` chapter of this document. Note, you may also use a bootstrap file produced by the restore command. For example:

.. code-block:: shell-session

   bextract -b bootstrap-file /dev/nst0 /tmp

The bootstrap file allows detailed specification of what files you want restored (extracted). You may specify a bootstrap file and include and/or exclude files at the same time. The bootstrap conditions will first be applied, and then each file record seen will be compared to the include and exclude lists.

Extracting From Multiple Volumes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you wish to extract files that span several Volumes, you can specify the Volume names in the bootstrap file or you may specify the Volume names on the command line by separating them with a vertical bar. See the section above under the bls program entitled Listing Multiple Volumes for more information. The same techniques apply equally well to the bextract program or read the :ref:`Bootstrap <BootstrapChapter>` chapter of this document.

Extracting Under Windows
^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   pair: Windows; bextract

.. warning::

   If you use :command:`bextract` under Windows, the order of the parameters is essential.


To use :command:`bextract`, the Bareos Storage Daemon must be installed. As bextract works on tapes or disk volumes, these must be configured in the Storage Daemon configuration file, normally found at :file:`C:\\ProgrammData\\Bareos\\`. However, it is not required to start the Bareos Storage Daemon. Normally, if the Storage Daemon would be able to run, :command:`bextract` would not be required.

After installing, :command:`bextract` can be called via command line:

.. code-block:: doscon
   :caption: Call of bextract

   C:\Program Files\Bareos> .\bextract.exe -c "C:\ProgrammData\Bareos\" -V <Volume> <YourStorage> <YourDestination>

If you want to use exclude or include files you need to write them like you do on Linux. That means each path begins with a "/" and not with "yourdrive:/". You need to specify the parameter -e exclude.list as first parameter. For example:

.. code-block:: cfg
   :caption: Example exclude.list

   /Program Files/Bareos/bareos-dir.exe
   /ProgramData/

.. code-block:: doscon
   :caption: Call bextract with exclude list

   C:\Program Files\Bareos> .\bextract.exe -e exclude.list -c "C:\ProgrammData\Bareos\" -V <Volume> <YourStorage> <YourDestination>


.. _bscan:

bscan
~~~~~

:index:`\ <single: bscan>`\  :index:`\ <single: Command; bscan>`\

If you find yourself using this program, you have probably done something wrong. For example, the best way to recover a lost or damaged Bareos database is to reload the database by using the bootstrap file that was written when you saved it (default Bareos-dir.conf file).

The bscan program can be used to re-create a database (catalog) records from the backup information written to one or more Volumes. This is normally needed only if one or more Volumes have been pruned or purged from your catalog so that the records on the Volume are no longer in the catalog, or for Volumes that you have archived. Note, if you scan in Volumes that were previously purged, you will be able to do restores from those Volumes. However, unless you modify the Job and File retention
times for the Jobs that were added by scanning, the next time you run any backup Job with the same name, the records will be pruned again. Since it takes a long time to scan Volumes this can be very frustrating.

With some care, :command:`bscan` can also be used to synchronize your existing catalog with a Volume. Although we have never seen a case of bscan damaging a catalog, since bscan modifies your catalog, we recommend that you do a simple ASCII backup of your database before running :command:`bscan` just to be sure.

:command:`bscan` can also be useful in a disaster recovery situation, after the loss of a hard disk, if you do not have a valid bootstrap file for reloading your system, or if a Volume has been recycled but not overwritten, you can use :command:`bscan` to re-create your database, which can then be used to restore your system or a file to its previous state.

It is called:

.. code-block:: shell-session

   Usage: bscan [options] <Bareos-archive>
          -B <driver name>  specify the database driver name (default NULL) <postgresql|mysql|sqlite>
          -b bootstrap      specify a bootstrap file
          -c <file>         specify configuration file
          -d <nn>           set debug level to nn
          -dt               print timestamp in debug output
          -m                update media info in database
          -D <director>     specify a director name specified in the Storage
                            configuration file for the Key Encryption Key selection
          -n <name>         specify the database name (default Bareos)
          -u <user>         specify database user name (default Bareos)
          -P <password>     specify database password (default none)
          -h <host>         specify database host (default NULL)
          -t <port>         specify database port (default 0)
          -p                proceed inspite of I/O errors
          -r                list records
          -s                synchronize or store in database
          -S                show scan progress periodically
          -v                verbose
          -V <Volumes>      specify Volume names (separated by |)
          -w <dir>          specify working directory (default from conf file)
          -?                print this message

As Bareos supports loading its database backend dynamically you need to specify the right database driver to use using the -B option.

If you are using MySQL or PostgreSQL, there is no need to supply a working directory since in that case, bscan knows where the databases are. However, if you have provided security on your database, you may need to supply either the database name (-b option), the user name (-u option), and/or the password (-p) options.

NOTE: before :command:`bscan` can work, it needs at least a bare bones valid database. If your database exists but some records are missing because they were pruned, then you are all set. If your database was lost or destroyed, then you must first ensure that you have the SQL program running (MySQL or PostgreSQL), then you must create the Bareos database (normally named bareos), and you must create the Bareos tables. This is explained in :ref:`section-CreateDatabase`
chapter of the manual. Finally, before scanning into an empty database, you must start and stop the Director with the appropriate Bareos-dir.conf file so that it can create the Client and Storage records which are not stored on the Volumes. Without these records, scanning is unable to connect the Job records to the proper client.

Forgetting for the moment the extra complications of a full rebuild of your catalog, let’s suppose that you did a backup to Volumes "Vol001" and "Vol002", then sometime later all records of one or both those Volumes were pruned or purged from the database. By using bscan you can recreate the catalog entries for those Volumes and then use the restore command in the Console to restore whatever you want. A command something like:

.. code-block:: shell-session

   bscan -v -V Vol001|Vol002 /dev/nst0

will give you an idea of what is going to happen without changing your catalog. Of course, you may need to change the path to the Storage daemon’s conf file, the Volume name, and your tape (or disk) device name. This command must read the entire tape, so if it has a lot of data, it may take a long time, and thus you might want to immediately use the command listed below. Note, if you are writing to a disk file, replace the device name with the path to the directory that contains the Volumes.
This must correspond to the Archive Device in the conf file.

Then to actually write or store the records in the catalog, add the -s option as follows:

.. code-block:: shell-session

   bscan -s -m -v -V Vol001|Vol002 /dev/nst0

When writing to the database, if :command:`bscan` finds existing records, it will generally either update them if something is wrong or leave them alone. Thus if the Volumes you are scanning are all or partially in the catalog already, no harm will be done to that existing data. Any missing data will simply be added.

If you have multiple tapes, you should scan them with:

.. code-block:: shell-session

   bscan -s -m -v -V Vol001|Vol002|Vol003 /dev/nst0

Since there is a limit on the command line length (511 bytes) accepted by :command:`bscan`, if you have too many Volumes, you will need to manually create a bootstrap file. See the :ref:`Bootstrap <BootstrapChapter>` chapter of this manual for more details, in particular the section entitled :ref:`Bootstrap for bscan <bscanBootstrap>`. Basically, the .bsr file for the above example might look like:



::

   Volume=Vol001
   Volume=Vol002
   Volume=Vol003



Note: :command:`bscan` does not support supplying Volume names on the command line and at the same time in a bootstrap file. Please use only one or the other.

You should, always try to specify the tapes in the order they are written. If you do not, any Jobs that span a volume may not be fully or properly restored. However, bscan can handle scanning tapes that are not sequential. Any incomplete records at the end of the tape will simply be ignored in that case. If you are simply repairing an existing catalog, this may be OK, but if you are creating a new catalog from scratch, it will leave your database in an incorrect state. If you do not specify all
necessary Volumes on a single bscan command, bscan will not be able to correctly restore the records that span two volumes. In other words, it is much better to specify two or three volumes on a single bscan command (or in a .bsr file) rather than run bscan two or three times, each with a single volume.

Note, the restoration process using bscan is not identical to the original creation of the catalog data. This is because certain data such as Client records and other non-essential data such as volume reads, volume mounts, etc is not stored on the Volume, and thus is not restored by bscan. The results of bscanning are, however, perfectly valid, and will permit restoration of any or all the files in the catalog using the normal Bareos console commands. If you are starting with an empty catalog
and expecting bscan to reconstruct it, you may be a bit disappointed, but at a minimum, you must ensure that your Bareos-dir.conf file is the same as what it previously was – that is, it must contain all the appropriate Client resources so that they will be recreated in your new database before running bscan. Normally when the Director starts, it will recreate any missing Client records in the catalog. Another problem you will have is that even if the Volumes (Media records) are recreated in the
database, they will not have their autochanger status and slots properly set. As a result, you will need to repair that by using the :bcommand:`update slots` command. There may be other considerations as well. Rather than bscanning, you should always attempt to recover you previous catalog backup.

Using bscan to Compare a Volume to an existing Catalog
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Catalog; Using bscan to Compare a Volume to an existing>`\

If you wish to compare the contents of a Volume to an existing catalog without changing the catalog, you can safely do so if and only if you do not specify either the -m or the -s options. However, the comparison routines are not as good or as thorough as they should be, so we don’t particularly recommend this mode other than for testing.

Using bscan to Recreate a Catalog from a Volume
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Catalog; Recreate Using bscan>`\  :index:`\ <single: bscan; Recreate Catalog>`\

This is the mode for which bscan is most useful. You can either bscan into a freshly created catalog, or directly into your existing catalog (after having made an ASCII copy as described above). Normally, you should start with a freshly created catalog that contains no data.

Starting with a single Volume named TestVolume1, you run a command such as:

.. code-block:: shell-session

   bscan -V TestVolume1 -v -s -m /dev/nst0

If there is more than one volume, simply append it to the first one separating it with a vertical bar. You may need to precede the vertical bar with a forward slash escape the shell – e.g. TestVolume1|TestVolume2. The -v option was added for verbose output (this can be omitted if desired). The -s option that tells :command:`bscan` to store information in the database. The physical device name /dev/nst0 is specified after all the options.

For example, after having done a full backup of a directory, then two incrementals, I reinitialized the SQLite database as described above, and using the bootstrap.bsr file noted above, I entered the following command:

.. code-block:: shell-session

   bscan -b bootstrap.bsr -v -s /dev/nst0

which produced the following output:

.. code-block:: shell-session

   bscan: bscan.c:182 Using Database: Bareos, User: bacula
   bscan: bscan.c:673 Created Pool record for Pool: Default
   bscan: bscan.c:271 Pool type "Backup" is OK.
   bscan: bscan.c:632 Created Media record for Volume: TestVolume1
   bscan: bscan.c:298 Media type "DDS-4" is OK.
   bscan: bscan.c:307 VOL_LABEL: OK for Volume: TestVolume1
   bscan: bscan.c:693 Created Client record for Client: Rufus
   bscan: bscan.c:769 Created new JobId=1 record for original JobId=2
   bscan: bscan.c:717 Created FileSet record "Users Files"
   bscan: bscan.c:819 Updated Job termination record for new JobId=1
   bscan: bscan.c:905 Created JobMedia record JobId 1, MediaId 1
   bscan: Got EOF on device /dev/nst0
   bscan: bscan.c:693 Created Client record for Client: Rufus
   bscan: bscan.c:769 Created new JobId=2 record for original JobId=3
   bscan: bscan.c:708 Fileset "Users Files" already exists.
   bscan: bscan.c:819 Updated Job termination record for new JobId=2
   bscan: bscan.c:905 Created JobMedia record JobId 2, MediaId 1
   bscan: Got EOF on device /dev/nst0
   bscan: bscan.c:693 Created Client record for Client: Rufus
   bscan: bscan.c:769 Created new JobId=3 record for original JobId=4
   bscan: bscan.c:708 Fileset "Users Files" already exists.
   bscan: bscan.c:819 Updated Job termination record for new JobId=3
   bscan: bscan.c:905 Created JobMedia record JobId 3, MediaId 1
   bscan: Got EOF on device /dev/nst0
   bscan: bscan.c:652 Updated Media record at end of Volume: TestVolume1
   bscan: bscan.c:428 End of Volume. VolFiles=3 VolBlocks=57 VolBytes=10,027,437

The key points to note are that bscan prints a line when each major record is created. Due to the volume of output, it does not print a line for each file record unless you supply the -v option twice or more on the command line.

In the case of a Job record, the new JobId will not normally be the same as the original Jobid. For example, for the first JobId above, the new JobId is 1, but the original JobId is 2. This is nothing to be concerned about as it is the normal nature of databases. bscan will keep everything straight.

Although :command:`bscan` claims that it created a Client record for Client: Rufus three times, it was actually only created the first time. This is normal.

You will also notice that it read an end of file after each Job (Got EOF on device ...). Finally the last line gives the total statistics for the bscan.

If you had added a second -v option to the command line, Bareos would have been even more verbose, dumping virtually all the details of each Job record it encountered.

Now if you start Bareos and enter a :bcommand:`list jobs` command to the console program, you will get:

.. code-block:: bconsole
   :caption: list jobs

   +-------+----------+------------------+------+-----+----------+----------+---------+
   | JobId | Name     | StartTime        | Type | Lvl | JobFiles | JobBytes | JobStat |
   +-------+----------+------------------+------+-----+----------+----------+---------+
   | 1     | usersave | 2002-10-07 14:59 | B    | F   | 84       | 4180207  | T       |
   | 2     | usersave | 2002-10-07 15:00 | B    | I   | 15       | 2170314  | T       |
   | 3     | usersave | 2002-10-07 15:01 | B    | I   | 33       | 3662184  | T       |
   +-------+----------+------------------+------+-----+----------+----------+---------+

which corresponds virtually identically with what the database contained before it was re-initialized and restored with bscan. All the Jobs and Files found on the tape are restored including most of the Media record. The Volume (Media) records restored will be marked as Full so that they cannot be rewritten without operator intervention.

It should be noted that :command:`bscan` cannot restore a database to the exact condition it was in previously because a lot of the less important information contained in the database is not saved to the tape. Nevertheless, the reconstruction is sufficiently complete, that you can run restore against it and get valid results.

An interesting aspect of restoring a catalog backup using :command:`bscan` is that the backup was made while Bareos was running and writing to a tape. At the point the backup of the catalog is made, the tape Bareos is writing to will have say 10 files on it, but after the catalog backup is made, there will be 11 files on the tape Bareos is writing. This there is a difference between what is contained in the backed up catalog and what is actually on the tape. If after restoring a
catalog, you attempt to write on the same tape that was used to backup the catalog, Bareos will detect the difference in the number of files registered in the catalog compared to what is on the tape, and will mark the tape in error.

There are two solutions to this problem. The first is possibly the simplest and is to mark the volume as Used before doing any backups. The second is to manually correct the number of files listed in the Media record of the catalog. This procedure is documented elsewhere in the manual and involves using the :bcommand:`update volume` command in :command:`bconsole`.

Using bscan to Correct the Volume File Count
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: bscan; Correct Volume File Count>`\  :index:`\ <single: Volume; File Count>`\

If the Storage daemon crashes during a backup Job, the catalog will not be properly updated for the Volume being used at the time of the crash. This means that the Storage daemon will have written say 20 files on the tape, but the catalog record for the Volume indicates only 19 files.

Bareos refuses to write on a tape that contains a different number of files from what is in the catalog. To correct this situation, you may run a bscan with the -m option (but without the -s option) to update only the final Media record for the Volumes read.

After bscan
^^^^^^^^^^^

:index:`\ <single: bscan; after>`\

If you use bscan to enter the contents of the Volume into an existing catalog, you should be aware that the records you entered may be immediately pruned during the next job, particularly if the Volume is very old or had been previously purged. To avoid this, after running bscan, you can manually set the volume status (VolStatus) to Read-Only by using the update command in the catalog. This will allow you to restore from the volume without having it immediately purged. When you have restored and
backed up the data, you can reset the VolStatus to Used and the Volume will be purged from the catalog.

.. limitation:: bscan: importing jobs with bscan may break the correct job chain order

   The Start Of Session (SOS) record is a special record placed by the Storage daemon on the storage medium as the first record of an append session job with a File daemon.
   This record is needed for finding the beginning of a particular session (Job), since no records with the same VolSessionId and VolSessionTime will precede this record.
   This record is not normally visible outside of the Storage daemon.
   The Begin Session Label is similar to the Volume Label except that it contains additional information pertaining to the Session.

   Having just a single timestamp generated by the SD in the SOS record is a limitation because there's currently no way for bscan to be aware of a jobs SchedTime, StartTime, EndTime, RealEndTime and JobTDate.
   Those values are required to keep the historical and correct order of a CopyJob, a MigrationJob or a VirtualFull.

   Due to this fact timestamps of a CopyJob, a MigrationJob or a VirtualFull will always differ from the timestamp in the original catalog after using bscan.

   Please be aware that this can lead to serious problems when using the Always Incremental backup scheme for example.


.. _bcopy:

bcopy
~~~~~

:index:`\ <single: bcopy>`\  :index:`\ <single: Command; bcopy>`\

The :command:`bcopy` program can be used to copy one Bareos archive file to another. For example, you may copy a tape to a file, a file to a tape, a file to a file, or a tape to a tape. For tape to tape, you will need two tape drives. In the process of making the copy, no record of the information written to the new Volume is stored in the catalog. This means that the new Volume, though it contains valid backup data, cannot be accessed directly from existing catalog entries. If you
wish to be able to use the Volume with the Console restore command, for example, you must first bscan the new Volume into the catalog.

.. code-block:: shell-session

   Usage: bcopy [-d debug_level] <input-archive> <output-archive>
          -b bootstrap    specify a bootstrap file
          -c <file>       specify configuration file
          -D <director>   specify a director name specified in the Storage
                          configuration file for the Key Encryption Key selection
          -dnn            set debug level to nn
          -dt             print timestamp in debug output
          -i              specify input Volume names (separated by |)
          -o              specify output Volume names (separated by |)
          -p              proceed inspite of I/O errors
          -v              verbose
          -w dir          specify working directory (default /tmp)
          -?              print this message

By using a bootstrap file, you can copy parts of a Bareos archive file to another archive.

One of the objectives of this program is to be able to recover as much data as possible from a damaged tape. However, the current version does not yet have this feature.

As this is a new program, any feedback on its use would be appreciated. In addition, I only have a single tape drive, so I have never been able to test this program with two tape drives.


.. _btape:

btape
~~~~~

:index:`\ <single: btape>`\  :index:`\ <single: Command; btape>`\

This program permits a number of elementary tape operations via a tty command interface. It works only with tapes and not with other kinds of Bareos storage media (DVD, File, ...). The test command, described below, can be very useful for testing older tape drive compatibility problems. Aside from initial testing of tape drive compatibility with Bareos, btape will be mostly used by developers writing new tape drivers.

btape can be dangerous to use with existing Bareos tapes because it will relabel a tape or write on the tape if so requested regardless that the tape may contain valuable data, so please be careful and use it only on blank tapes.

To work properly, :command:`btape` needs to read the Storage daemon’s configuration file.

The physical device name must be specified on the command line, and this same device name must be present in the Storage daemon’s configuration file read by :command:`btape`.

.. code-block:: shell-session

   Usage: btape <options> <device_name>
          -b <file>     specify bootstrap file
          -c <file>     set configuration file to file
          -D <director> specify a director name specified in the Storage
                        configuration file for the Key Encryption Key selection
          -d <nn>       set debug level to nn
          -dt           print timestamp in debug output
          -p            proceed inspite of I/O errors
          -s            turn off signals
          -v            be verbose
          -?            print this message.

Using btape to Verify your Tape Drive
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Drive; Verify using btape>`\

An important reason for this program is to ensure that a Storage daemon configuration file is defined so that Bareos will correctly read and write tapes.

It is highly recommended that you run the test command before running your first Bareos job to ensure that the parameters you have defined for your storage device (tape drive) will permit Bareos to function properly. You only need to mount a blank tape, enter the command, and the output should be reasonably self explanatory. Please see the :ref:`Tape Testing <TapeTestingChapter>` Chapter of this manual for the details.

btape Commands
^^^^^^^^^^^^^^

The full list of commands are:

.. code-block:: bconsole
   :caption: btape commands

     Command    Description
     =======    ===========
     autochanger test autochanger
     bsf        backspace file
     bsr        backspace record
     cap        list device capabilities
     clear      clear tape errors
     eod        go to end of Bareos data for append
     eom        go to the physical end of medium
     fill       fill tape, write onto second volume
     unfill     read filled tape
     fsf        forward space a file
     fsr        forward space a record
     help       print this command
     label      write a Bareos label to the tape
     load       load a tape
     quit       quit btape
     rawfill    use write() to fill tape
     readlabel  read and print the Bareos tape label
     rectest    test record handling functions
     rewind     rewind the tape
     scan       read() tape block by block to EOT and report
     scanblocks Bareos read block by block to EOT and report
     speed      report drive speed
     status     print tape status
     test       General test Bareos tape functions
     weof       write an EOF on the tape
     wr         write a single Bareos block
     rr         read a single record
     qfill      quick fill command

The most useful commands are:

-  test – test writing records and EOF marks and reading them back.

-  fill – completely fill a volume with records, then write a few records on a second volume, and finally, both volumes will be read back. This command writes blocks containing random data, so your drive will not be able to compress the data, and thus it is a good test of the real physical capacity of your tapes.

-  readlabel – read and dump the label on a Bareos tape.

-  cap – list the device capabilities and status.

The readlabel command can be used to display the details of a Bareos tape label. This can be useful if the physical tape label was lost or damaged.

In the event that you want to relabel a Bareos volume, you can simply use the label command which will write over any existing label. However, please note for labeling tapes, we recommend that you use the label command in the Console program since it will never overwrite a valid Bareos tape.

.. _section-btapespeed:

Testing your Tape Drive
'''''''''''''''''''''''

To determine the best configuration of your tape drive, you can run the new ``speed`` command available in the ``btape`` program.

This command can have the following arguments:

-  Specify the :config:option:`sd/device/MaximumFileSize`\  for this test. This counter is in GB.

-  Specify the number of file to be written. The amount of data should be greater than your memory (file_size * nb_file).

-  This flag permits to skip tests with constant data.

-  This flag permits to skip tests with random data.

-  This flag permits to skip tests with raw access.

-  This flag permits to skip tests with Bareos block access.

.. code-block:: bconsole
   :caption: btape speed

   *speed file_size=3 skip_raw
   btape.c:1078 Test with zero data and Bareos block structure.
   btape.c:956 Begin writing 3 files of 3.221 GB with blocks of 129024 bytes.
   ++++++++++++++++++++++++++++++++++++++++++
   btape.c:604 Wrote 1 EOF to "Drive-0" (/dev/nst0)
   btape.c:406 Volume bytes=3.221 GB. Write rate = 44.128 MB/s
   ...
   btape.c:383 Total Volume bytes=9.664 GB. Total Write rate = 43.531 MB/s

   btape.c:1090 Test with random data, should give the minimum throughput.
   btape.c:956 Begin writing 3 files of 3.221 GB with blocks of 129024 bytes.
   +++++++++++++++++++++++++++++++++++++++++++
   btape.c:604 Wrote 1 EOF to "Drive-0" (/dev/nst0)
   btape.c:406 Volume bytes=3.221 GB. Write rate = 7.271 MB/s
   +++++++++++++++++++++++++++++++++++++++++++
   ...
   btape.c:383 Total Volume bytes=9.664 GB. Total Write rate = 7.365 MB/s

When using compression, the random test will give your the minimum throughput of your drive . The test using constant string will give you the maximum speed of your hardware chain. (cpu, memory, scsi card, cable, drive, tape).

You can change the block size in the Storage Daemon configuration file.


.. _bscrypto:

bscrypto
~~~~~~~~

:index:`\ <single: bscrypto>`\  :index:`\ <single: Command; bscrypto>`\

:command:`bscrypto` is used in the process of encrypting tapes (see also :ref:`LTOHardwareEncryptionGeneral`). The |sd| and the btools (:command:`bls`, :command:`bextract`, :command:`bscan`, :command:`btape`, :command:`bextract`) will use a so called |sd| plugin to perform the setting and clearing of the encryption keys. To bootstrap the encryption support and for
populating things like the crypto cache with encryption keys of volumes that you want to scan, you need to use the bscrypto tool. The bscrypto tool has the following capabilities:

-  Generate a new passphrase

   -  | to be used as a so called Key Encryption Key (KEK) for wrapping a passphrase using RFC3394 key wrapping with aes-wrap
      | - or -

   -  for usage as a clear text encryption key loaded into the tape drive.

-  Base64-encode a key if requested

-  Generate a wrapped passphrase which performs the following steps:

   -  generate a semi random clear text passphrase

   -  wrap the passphrase using the Key Encryption Key using RFC3394

   -  base64-encode the wrapped key (as the wrapped key is binary, we always need to base64-encode it in order to be able to pass the data as part of the director to storage daemon protocol

-  | show the content of a wrapped or unwrapped keyfile.
   | This can be used to reveal the content of the passphrase when a passphrase is stored in the database and you have the urge to change the Key Encryption Key. Normally it is unwise to change the Key Encryption Key, as this means that you have to redo all your stored encryption keys, as they are stored in the database wrapped using the Key Encryption Key available in the config during the label phase of the volume.

-  Clear the crypto cache on the machine running the bareos-sd, which keeps a cache of used encryption keys, which can be used when the bareos-sd is restarted without the need to connect to the bareos-dir to retrieve the encryption keys.

-  Set the encryption key of the drive

-  Clear the encryption key of the drive

-  Show the encryption status of the drive

-  Show the encryption status of the next block (e.g. volume)

-  Populate the crypto cache with data

Other Programs
--------------

The following programs are general utility programs and in general do not need a configuration file nor a device name.


.. _bsmtp:

bsmtp
~~~~~

:index:`\ <single: bsmtp>`\  :index:`\ <single: Command; bsmtp>`\

:command:`bsmtp` is a simple mail transport program that permits more flexibility than the standard mail programs typically found on Unix systems. It can even be used on Windows machines.

It is called:

.. code-block:: shell-session
   :caption: bsmtp

   Usage: bsmtp [-f from] [-h mailhost] [-s subject] [-c copy] [recipient ...]
          -4          forces bsmtp to use IPv4 addresses only.
          -6          forces bsmtp to use IPv6 addresses only.
          -8          set charset to UTF-8
          -a          use any ip protocol for address resolution
          -c          set the Cc: field
          -d <nn>     set debug level to <nn>
          -dt         print a timestamp in debug output
          -f          set the From: field
          -h          use mailhost:port as the SMTP server
          -s          set the Subject: field
          -r          set the Reply-To: field
          -l          set the maximum number of lines to send (default: unlimited)
          -?          print this message.

If the -f option is not specified, :command:`bsmtp` will use your userid. If the option -h is not specified :command:`bsmtp` will use the value in the environment variable bsmtpSERVER or if there is none localhost. By default port 25 is used.

If a line count limit is set with the -l option, :command:`bsmtp` will not send an email with a body text exceeding that number of lines. This is especially useful for large restore job reports where the list of files restored might produce very long mails your mail-server would refuse or crash. However, be aware that you will probably suppress the job report and any error messages unless you check the log file written by the Director (see the messages resource in this manual for
details).

recipients is a space separated list of email recipients.

The body of the email message is read from standard input.

An example of the use of :command:`bsmtp` would be to put the following statement in the :ref:`Messages resource <MessagesChapter>` of your |dir| configuration.

.. code-block:: bareosconfig
   :caption: bsmtp in Message resource

   Mail Command     = "bsmtp -h mail.example.com -f \"\(Bareos\) %r\" -s \"Bareos: %t %e of %c %l\" %r"
   Operator Command = "bsmtp -h mail.example.com -f \"\(Bareos\) %r\" -s \"Bareos: Intervention needed for %j\" %r"

You have to replace mail.example.com with the fully qualified name of your SMTP (email) server, which normally listens on port 25. For more details on the substitution characters (e.g. %r) used in the above line, please see the documentation of the :ref:`MailCommand in the Messages Resource <mailcommand>` chapter of this manual.

It is HIGHLY recommended that you test one or two cases by hand to make sure that the mailhost that you specified is correct and that it will accept your email requests. Since bsmtp always uses a TCP connection rather than writing in the spool file, you may find that your from address is being rejected because it does not contain a valid domain, or because your message is caught in your spam filtering rules. Generally, you should specify a fully qualified domain name in the from field, and
depending on whether your bsmtp gateway is Exim or Sendmail, you may need to modify the syntax of the from part of the message. Please test.

When running :command:`bsmtp` by hand, you will need to terminate the message by entering a ctrl-d in column 1 of the last line.

If you are getting incorrect dates (e.g. 1970) and you are running with a non-English language setting, you might try adding a :command:`LANG=C` immediately before the :command:`bsmtp` call.

In general, :command:`bsmtp` attempts to cleanup email addresses that you specify in the from, copy, mailhost, and recipient fields, by adding the necessary < and > characters around the address part. However, if you include a display-name (see RFC 5332), some SMTP servers such as Exchange may not accept the message if the display-name is also included in < and >. As mentioned above, you must test, and if you run into this situation, you may manually add the < and > to the Bareos
:config:option:`dir/messages/MailCommand`\  or :config:option:`dir/messages/OperatorCommand`\  and when :command:`bsmtp` is formatting an address if it already contains a < or > character, it will leave the address unchanged.


.. _dbcheck:

.. _bareos-dbcheck:

bareos-dbcheck
~~~~~~~~~~~~~~

 :index:`\ <single: bareos-dbcheck>`
 :index:`\ <single: Command; bareos-dbcheck>`
 :index:`\ <single: Catalog; database check>`

:command:`bareos-dbcheck` is a simple program that will search for logical inconsistencies in the Bareos tables in your database, and optionally fix them. It is a database maintenance routine, in the sense that it can detect and remove unused rows, but it is not a database repair routine. To repair a database, see the tools furnished by the database vendor. Normally :command:`bareos-dbcheck` should never need to be run, but if Bareos has crashed or you have a lot of
Clients, Pools, or Jobs that you have removed, it could be useful.

:command:`bareos-dbcheck` is best started as the same user, as the |dir| is running, normally **bareos**. If you are **root** on Linux, use the following command to switch to user **bareos**:

.. code-block:: shell-session
   :caption: Substitute user to bareos

   su -s /bin/bash - bareos

If not, problems of reading the Bareos configuration or accessing the database can arise.

:command:`bareos-dbcheck` supports following command line options:

.. code-block:: shell-session

   Usage: bareos-dbcheck [-c config ] [-B] [-C catalog name] [-d debug level] [-D driver name] <working-directory> <bareos-database> <user> <password> [<dbhost>] [<dbport>]
          -b                batch mode
          -C                catalog name in the director conf file
          -c                Director configuration filename or configuration directory (e.g. /etc/bareos)
          -B                print catalog configuration and exit
          -d <nn>           set debug level to <nn>
          -dt               print a timestamp in debug output
          -D <driver name>  specify the database driver name (default NULL) <postgresql|mysql|sqlite>
          -f                fix inconsistencies
          -v                verbose
          -?                print this message

When using the default configuration paths, it is not necessary to specify any options. Optionally, as Bareos supports loading its database backend dynamically you may specify the right database driver to use using the :strong:`-D` option.

If the :strong:`-B` option is specified, :command:`bareos-dbcheck` will print out catalog information in a simple text based format:

.. code-block:: shell-session

   # <input>bareos-dbcheck -B</input>
   catalog=MyCatalog
   db_type=SQLite
   db_name=bareos
   db_driver=
   db_user=bareos
   db_password=
   db_address=
   db_port=0
   db_socket=

If the :strong:`-c` option is given with the |dir| configuration, there is no need to enter any of the command line arguments, in particular the working directory as :command:`bareos-dbcheck` will read them from the file.

If the :strong:`-f` option is specified, :command:`bareos-dbcheck` will repair (fix) the inconsistencies it finds. Otherwise, it will report only.

If the :strong:`-b` option is specified, :command:`bareos-dbcheck` will run in batch mode, and it will proceed to examine and fix (if :strong:`-f` is set) all programmed inconsistency checks. If the :strong:`-b` option is not specified, :command:`bareos-dbcheck` will enter interactive mode and prompt with the following:

.. code-block:: shell-session

    Hello, this is the Bareos database check/correct program.
    Modify database is off. Verbose is off.
    Please select the function you want to perform.
         0) Quit
         1) Toggle modify database flag
         2) Toggle verbose flag
         3) Check for bad Filename records
         4) Check for bad Path records
         5) Check for duplicate Path records
         6) Check for orphaned Jobmedia records
         7) Check for orphaned File records
         8) Check for orphaned Path records
         9) Check for orphaned FileSet records
        10) Check for orphaned Client records
        11) Check for orphaned Job records
        12) Check for orphaned storage records
        13) Check for all Admin records
        14) Check for all Restore records
        15) Run ALL checks
    Select function number:

By entering 1 or 2, you can toggle the modify database flag (:strong:`-f` option) and the verbose flag (:strong:`-v`). It can be helpful and reassuring to turn off the modify database flag, then select one or more of the consistency checks (items 3 through 13) to see what will be done, then toggle the modify flag on and re-run the check.

Since Bareos :sinceVersion:`16.2.5: bareos-dbcheck -b -v`, when running :command:`bareos-dbcheck` with :strong:`-b` and :strong:`-v`, it will not interactively ask if results should be printed or not. Instead, it does not print any detail results.

The inconsistencies examined are the following:

-  Duplicate Filename records. This can happen if you accidentally run two copies of Bareos at the same time, and they are both adding filenames simultaneously. It is a rare occurrence, but will create an inconsistent database. If this is the case, you will receive error messages during Jobs warning of duplicate database records. If you are not getting these error messages, there is no reason to run this check.

-  Repair bad Filename records. This checks and corrects filenames that have a trailing slash. They should not.

-  Repair bad Path records. This checks and corrects path names that do not have a trailing slash. They should.

-  Duplicate Path records. This can happen if you accidentally run two copies of Bareos at the same time, and they are both adding filenames simultaneously. It is a rare occurrence, but will create an inconsistent database. See the item above for why this occurs and how you know it is happening.

-  Orphaned JobMedia records. This happens when a Job record is deleted (perhaps by a user issued SQL statement), but the corresponding JobMedia record (one for each Volume used in the Job) was not deleted. Normally, this should not happen, and even if it does, these records generally do not take much space in your database. However, by running this check, you can eliminate any such orphans.

-  Orphaned File records. This happens when a Job record is deleted (perhaps by a user issued SQL statement), but the corresponding File record (one for each Volume used in the Job) was not deleted. Note, searching for these records can be very time consuming (i.e. it may take hours) for a large database. Normally this should not happen as Bareos takes care to prevent it. Just the same, this check can remove any orphaned File records. It is recommended that you run this once a year since
   orphaned File records can take a large amount of space in your database. You might want to ensure that you have indexes on JobId, FilenameId, and PathId for the File table in your catalog before running this command.

-  Orphaned Path records. This condition happens any time a directory is deleted from your system and all associated Job records have been purged. During standard purging (or pruning) of Job records, Bareos does not check for orphaned Path records. As a consequence, over a period of time, old unused Path records will tend to accumulate and use space in your database. This check will eliminate them. It is recommended that you run this check at least once a year.

-  Orphaned Filename records. This condition happens any time a file is deleted from your system and all associated Job records have been purged. This can happen quite frequently as there are quite a large number of files that are created and then deleted. In addition, if you do a system update or delete an entire directory, there can be a very large number of Filename records that remain in the catalog but are no longer used.

   During standard purging (or pruning) of Job records, Bareos does not check for orphaned Filename records. As a consequence, over a period of time, old unused Filename records will accumulate and use space in your database. This check will eliminate them. It is strongly recommended that you run this check at least once a year, and for large database (more than 200 Megabytes), it is probably better to run this once every 6 months.

-  Orphaned Client records. These records can remain in the database long after you have removed a client.

-  Orphaned Job records. If no client is defined for a job or you do not run a job for a long time, you can accumulate old job records. This option allow you to remove jobs that are not attached to any client (and thus useless).

-  Orphaned storage records. If you delete a storage configuration file from the bareos configurations folder, you end up with unused storages in the database that can cause certain visual inconsistencies. This option allows you to delete these orphaned storages, but you have to make sure first that they are not used by any Media or Device.

-  All Admin records. This command will remove all Admin records, regardless of their age.

-  All Restore records. This command will remove all Restore records, regardless of their age.

If you are using MySQL, :command:`bareos-dbcheck` in interactive mode will ask you if you want to create temporary indexes to speed up orphaned Path and Filename elimination. In batch mode (:strong:`-b`) the temporary indexes will be created without asking.

If you are using bvfs (e.g. used by :ref:`bareos-webui <section-webui>`), don’t eliminate orphaned path, else you will have to rebuild ``brestore_pathvisibility``\  and ``brestore_pathhierarchy``\  indexes.

Normally you should never need to run :command:`bareos-dbcheck` in spite of the recommendations given above, which are given so that users don’t waste their time running :command:`bareos-dbcheck` too often.


.. _bregex:

bregex
~~~~~~

:index:`\ <single: bregex>`\  :index:`\ <single: Command; bregex>`\

:command:`bregex` is a simple program that will allow you to test regular expressions against a file of data. This can be useful because the regex libraries on most systems differ, and in addition, regex expressions can be complicated.

To run it, use:

::

   Usage: bregex [-d debug_level] -f <data-file>
          -f          specify file of data to be matched
          -l          suppress line numbers
          -n          print lines that do not match
          -?          print this message.

The <data-file> is a filename that contains lines of data to be matched (or not) against one or more patterns. When the program is run, it will prompt you for a regular expression pattern, then apply it one line at a time against the data in the file. Each line that matches will be printed preceded by its line number. You will then be prompted again for another pattern.

Enter an empty line for a pattern to terminate the program. You can print only lines that do not match by using the -n option, and you can suppress printing of line numbers with the -l option.

This program can be useful for testing regex expressions to be applied against a list of filenames.

.. _bwild:

bwild
~~~~~

:index:`\ <single: bwild>`\  :index:`\ <single: Command; bwild>`\

:command:`bwild` is a simple program that will allow you to test wild-card expressions against a file of data.

To run it, use:

::

   Usage: bwild [-d debug_level] -f <data-file>
          -f          specify file of data to be matched
          -l          suppress line numbers
          -n          print lines that do not match
          -?          print this message.

The <data-file> is a filename that contains lines of data to be matched (or not) against one or more patterns. When the program is run, it will prompt you for a wild-card pattern, then apply it one line at a time against the data in the file. Each line that matches will be printed preceded by its line number. You will then be prompted again for another pattern.

Enter an empty line for a pattern to terminate the program. You can print only lines that do not match by using the -n option, and you can suppress printing of line numbers with the -l option.

This program can be useful for testing wild expressions to be applied against a list of filenames.


.. _bpluginfo:

bpluginfo
~~~~~~~~~

:index:`\ <single: bpluginfo>`\  :index:`\ <single: Command; bpluginfo>`\

The main purpose of bpluginfo is to display different information about Bareos plugin. You can use it to check a plugin name, author, license and short description. You can use -f option to display API implemented by the plugin. Some plugins may require additional ’-a’ option for val- idating a Bareos Daemons API. In most cases it is not required.

.. _program-bareos-dbcopy:

.. include:: ../man/bareos-dbcopy.rst
