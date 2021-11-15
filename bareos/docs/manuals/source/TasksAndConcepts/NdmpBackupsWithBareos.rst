NDMP Backups with Bareos
========================

:index:`\ <single: NDMP; Overview>`\

NDMP Basics
-----------

NDMP

-  is the abbreviation for Network Data Management Protocol.

-  is a protocol that transports data between Network Attached Storages (NAS) and backup devices.

-  is widely used by storage product vendors and OS vendors like NetApp, Isilon, EMC, Oracle.

-  information is available at http://www.ndmp.org/.

-  version is currently (2016) NDMP Version 4.

-  uses TCP/IP and XDR (External Data Representation) for the communication.

The Bareos NDMP implementation is based on the NDMJOB NDMP reference implementation of Traakan, Inc., Los Altos, CA which has a BSD style license (2 clause one) with some enhancements.

In NDMP, there are different components (called "agents") involved in doing backups. The most important agents are:

Data Management Agent (DMA)
   is the part that controls the NDMP backup or recover operation.

Data Agent
   (or Primary Storage System) is the part that reads the data from the Filesystem during Backup and writes data to the Filesystem during recover.

Tape Agent
   (or Secondary Storage System) is the part that writes NDMP blocks to the Tape during backup and reads them during recover.

Robot Agent
   is the part that controls the media changer. It loads/unloads tapes and gets the inventory of the Changer. The use of a robot agent is optional. It is possible to run backups on a single tape drive.

All elements involved talk to each other via the NDMP protocol which is usually transported via TCP/IP port 10000.

The Data Management Agent is part of the Backup Application.

The Data Agent is part of the (NAS)-System being backed up and recovered.

The Tape Agent and Robot Agent can

-  run on the system being backed up

-  run as part of the backup application

-  or even run independently on a third system

This flexibility leads to different topologies how NDMP backups can be done.

NDMP Topologies
~~~~~~~~~~~~~~~

When looking at the different topologies, the location of the Robot Agent is not specially considered, as the data needed to control the robot is minimal compared to the backup data.

So the parts considered are

-  the Data Management Agent controlling the operation

-  the Data Agent as source of the backup data and

-  the Tape Agent as destination of the backup data

The Data Management Agent always controls both Data Agent and Tape Agent over the Network via NDMP.

The Tape Agent can either

-  run on a separate system

-  run on the same system

as the Data Agent.

NDMP 3-way Backup: Data Agent and Tape Agent running on different systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

      --+--------------- NETWORK ----+-------------------+----
        |    -->---->---->-->-->-->\ | //==>==>==>==>\\  |
        |   /                      | | ||      (2)   ||  |
        |   |                      | | ||            ||  |
   /----------\                 /----------\     /----------\
   |          |                 |          |     |          |
   |   DMA    |        DISK====>|   DATA   |     |   Tape   |====>TAPE DRIVE
   |          |             (1) |   Agent  |     |   Agent  | (3)
   \----------/                 \----------/     \----------/

The data path consists of three ways

-  From Disk to Data Agent (1)

-  From Data Agent over the Network to the Tape Agent (2)

-  From Tape Agent to the Tape (3)

and is called NDMP 3-way Backup.

NDMP 2-way Backup: Data Agent and Tape Agent running on the same system
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

      --+--------------- NETWORK ----+---------
        |    -->---->---->-->-->-->\ |
        |   /                      | |
        |   |                      | |
   /----------\                 /----------\
   |          |                 | Data     |
   |   DMA    |        DISK====>| Agent    |
   |          |             (1) |          |
   \----------/                 |    Tape  | (2)
                                |    Agent |====>TAPE DRIVE
                                \----------/

Data Agent and Tape Agent are both part of the same process on the system, so the data path consists of two ways:

-  From Disk to Data Agent (1)

-  From Tape Agent to the Tape (2)

and is called NDMP 2-way Backup, also sometimes referred as NDMP local backup.

Properties of the different NDMP Backup topologies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

NDMP 3-way backup:

-  The data can be send to a different location over the network

-  No need to attach a tape drive to the NAS system.

-  The backup speed is usually slower than with 2-way backup as the data is being sent over the network and processed multiple times.

NDMP 2-way backup:

-  The data is directly copied from the NAS system to the Tape

-  Usually the fastest way to do a NDMP backup

-  tape drives need to be attached to the NAS System


NDMP Backup in Bareos
---------------------

Bareos offers two types of NDMP integration:

NDMP_NATIVE
NDMP_BAREOS

In both cases,

-  |dir| acts as Data Management Agent.

-  The Data Agent is part of the storage system and must be provided by the storage vendor.

The main difference is which Tape Agent is used.

When using NDMP_BAREOS, the |sd| acts as Tape Agent.

When using NDMP_NATIVE, the Tape Agent must be provided by some other systems. Some storage vendors provide it with there storages, or offer it as an option, e.g. Isilon with their "Isilon Backup Accelerator".

.. csv-table::
   :header: "", |ndmpbareos|, |ndmpnative|

   Data Management Agent,                                   |dir|,       |dir|
   Tape Agent,                                              |sd|,        external
   Requires external Tape Agent,                                       , |checkmark|
   Backup to tape (and VTL),                                |checkmark|, |checkmark|
   Backup to other :config:option:`sd/device/DeviceType`\ , |checkmark|,
   2-way backup,                                                       , |checkmark|
   3-way backup,                                            |checkmark|, untested
   Full Backups,                                            |checkmark|, |checkmark|
   Differential Backups,                                    |checkmark|, |checkmark|
   Incremental Backups,                                     |checkmark| :ref:`(8) <section-NdmpBackupLevel>`, |checkmark| :ref:`(8) <section-NdmpBackupLevel>`
   Single File Restore,                                     |checkmark|, |checkmark|
   DAR,                                                                , |checkmark|
   DDAR,                                                               , |checkmark|
   :ref:`Copy and Migration jobs <MigrationChapter>`,       |checkmark|,


.. _section-NdmpBareos:

NDMP_BAREOS
-----------

Bareos implements the Data Management Agent inside of the |dir| and a Tape Agent in the |sd|.

The Tape Agent in the |sd| emulates a NDMP tape drive that has an infinite tape. Because of the infinite tape, no Robot Agent is required and therefore not implemented. The blocks being written to the NDMP tape are wrapped into a normal Bareos backup stream and then stored into the volumes managed by Bareos.

There is always a pair of storage resource definitions:

-  a conventional Bareos storage resource and

-  a NDMP storage resource

These two are linked together. Data that is received by the Tape Agent inside of the |sd| is then stored as Bareos backup stream inside of the paired conventional Bareos storage resource.

On restore, the data is read by the conventional resource, and then recovered as NDMP stream from the NDMP resource.

.. note:: Copying and migrating a NDMP_BAREOS job is not an NDMP operation. NDMP jobs are copied and migrated just like every other Bareos job.


.. figure:: /include/images/ndmp-backup.*
   :alt: Relationship between Bareos and NDMP components

   Relationship between Bareos and NDMP components

Example Setup for NDMP_BAREOS backup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: NDMP; Example; NDMP\_BAREOS>`\

This example starts from a clean default Bareos installation.

Enable NDMP on your storage appliance
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The storage appliance needs to be configured to allow NDMP connections. Therefore usually the NDMP service needs to be enabled and configured with a username and password.

Bareos Director: Configure NDMP Client Resource
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Add a Client resource to the |dir| configuration and configure it to access your NDMP storage system (Primary Storage System/Data Agent).

-  :config:option:`dir/client/Protocol`\  must be either NDMPv2, NDMPv3 or NDMPv4.

-  :config:option:`dir/client/Port`\  is set to the NDMP Port (usually 10000).

-  :config:option:`dir/client/Username`\  and :config:option:`dir/client/Password`\  are used for the authentication against the NDMP Storage System.

-  :config:option:`dir/client/AuthType`\  is either Cleartext or MD5. NDMP supports both.

In our example we connect to a Isilon storage appliance emulator:

.. code-block:: bareosconfig

   Client {
     Name = ndmp-client
     Address = isilon.example.com
     Port = 10000            # Default port for NDMP
     Protocol = NDMPv4       # Need to specify protocol before password as protocol determines password encoding used
     Auth Type = Clear       # Cleartext Authentication
     Username = "ndmpadmin"  # username of the NDMP user on the DATA AGENT e.g. storage box being backed up.
     Password = "secret"     # password of the NDMP user on the DATA AGENT e.g. storage box being backed up.
   }

Verify, that you can access your Primary Storage System via Bareos:

.. code-block:: bconsole
   :caption: verify connection to NDMP Primary Storage System

   *<input>status client=ndmp-client</input>

   Data Agent isilon.example.com NDMPv4
     Host info
       hostname   isilonsim-1
       os_type    Isilon OneFS
       os_vers    v7.1.1.5
       hostid     005056ad8483ba43cc55a711cd384506e3c1

     Server info
       vendor     Isilon
       product    Isilon NDMP
       revision   2.2
       auths      (2) NDMP4_AUTH_TEXT NDMP4_AUTH_MD5

     Connection types
       addr_types (2) NDMP4_ADDR_TCP NDMP4_ADDR_LOCAL

     Backup type info of tar format
       attrs      0x7fe
       set        FILESYSTEM=/ifs
       set        FILES=
       set        EXCLUDE=
       set        PER_DIRECTORY_MATCHING=N
       set        HIST=f
       set        DIRECT=N
       set        LEVEL=0
       set        UPDATE=Y
       set        RECURSIVE=Y
       set        ENCODING=UTF-8
       set        ENFORCE_UNIQUE_NODE=N
       set        PATHNAME_SEPARATOR=/
       set        DMP_NAME=
       set        BASE_DATE=0
       set        NDMP_UNICODE_FH=N

     Backup type info of dump format
       attrs      0x7fe
       set        FILESYSTEM=/ifs
       set        FILES=
       set        EXCLUDE=
       set        PER_DIRECTORY_MATCHING=N
       set        HIST=f
       set        DIRECT=N
       set        LEVEL=0
       set        UPDATE=Y
       set        RECURSIVE=Y
       set        ENCODING=UTF-8
       set        ENFORCE_UNIQUE_NODE=N
       set        PATHNAME_SEPARATOR=/
       set        DMP_NAME=
       set        BASE_DATE=0
       set        NDMP_UNICODE_FH=N

     File system /ifs
       physdev    OneFS
       unsupported 0x0
       type       NFS
       status
       space      12182519808 total, 686768128 used, 11495751680 avail
       inodes     17664000 total, 16997501 used
       set        MNTOPTS=
       set        MNTTIME=00:00:00 00:00:00

This output shows that the access to the storage appliance was successful.

.. _section-ndmp-sd-configure:

Bareos Storage Daemon: Configure NDMP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enabling NDMP
'''''''''''''

To enable the NDMP Tape Agent inside of the |sd|, set :config:option:`sd/storage/NdmpEnable`\ =yes:

.. code-block:: bareosconfig
   :caption: enable NDMP in |sd|

   #
   # Default SD config block: enable the NDMP protocol,
   # otherwise it won't listen on port 10000.
   #
   Storage {
      Name = ....
      ...
      NDMP Enable = yes
   }

Add a NDMP resource
'''''''''''''''''''

Additionally, we need to define the access credentials for our NDMP TAPE AGENT (Secondary Storage) inside of this Storage Daemon.

These are configured by adding a NDMP resource to |sd| configuration:

.. code-block:: bareosconfig

   #
   # This resource gives the DMA in the Director access to the Bareos SD via the NDMP protocol.
   # This option is used via the NDMP protocol to open the right TAPE AGENT connection to your
   # Bareos SD via the NDMP protocol. The initialization of the SD is done via the native protocol
   # and is handled via the PairedStorage keyword.
   #
   Ndmp {
     Name = bareos-dir-isilon
     Username = ndmpadmin
     Password = test
     AuthType = Clear
   }

Username and Password can be anything, but they will have to match the settings in the |dir| NDMP Storage resource we configure next.

Now restart the |sd|. If everything is correct, the |sd| starts and listens now on the usual port (9103) and additionally on port 10000 (ndmp).

.. code-block:: shell-session

   root@host:~# netstat -lntp | grep bareos-sd
   tcp        0      0 0.0.0.0:9103            0.0.0.0:*               LISTEN      10661/bareos-sd
   tcp        0      0 0.0.0.0:10000           0.0.0.0:*               LISTEN      10661/bareos-sd


Bareos Director: Configure a Paired Storage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For NDMP Backups, we always need two storages that are paired together. The default configuration already has a Storage :config:option:`Dir/Storage = File`\  defined:

.. code-block:: bareosconfig

   Storage {
     Name = File
     Address = bareos
     Password = "pNZ3TvFAL/t+MyOIQo58p5B/oB79SFncdAmLXKHa9U59"
     Device = FileStorage
     Media Type = File
   }

We now add a paired storage to the already existing :config:option:`Dir/ = File`\  storage:

.. code-block:: bareosconfig

   #
   # Same storage daemon but via NDMP protocol.
   # We link via the PairedStorage config option the Bareos SD
   # instance definition to a NDMP TAPE AGENT.
   #
   Storage {
     Name = NDMPFile
     Address = bareos
     Port = 10000
     Protocol = NDMPv4
     Auth Type = Clear
     Username = ndmpadmin
     Password = "test"
     Device = FileStorage
     Media Type = File
     PairedStorage = File
   }

The settings of Username and Password need to match the settings of the |sd|’s NDMP resource we added before. The address will be used by the storage appliance’s NDMP Daemon to connect to the |sd| via NDMP. Make sure that the Storage appliance can resolve the name or use an IP address.

Now save the director resource and restart the |dir|. Verify that the configuration is correct:

.. code-block:: bconsole
   :caption: verify connection to the |sd|

   *<input>status storage=NDMPFile</input>
   Connecting to Storage daemon File at bareos:9103

   bareos-sd Version: 15.2.2 (16 November 2015) x86_64-redhat-linux-gnu redhat Red Hat Enterprise Linux Server release 7.0 (Maipo)
   Daemon started 14-Jan-16 10:10. Jobs: run=0, running=0.
    Heap: heap=135,168 smbytes=34,085 max_bytes=91,589 bufs=75 max_bufs=77
    Sizes: boffset_t=8 size_t=8 int32_t=4 int64_t=8 mode=0 bwlimit=0kB/s

   Running Jobs:
   No Jobs running.
   ====

   Jobs waiting to reserve a drive:
   ====

   Terminated Jobs:
   ====

   Device status:

   Device "FileStorage" (/var/lib/bareos/storage) is not open.
   ==
   ====

   Used Volume status:
   ====

   ====

   *

The output looks the same, as if a :bcommand:`status storage=File` would have been called.

.. _section-NdmpFileset:

Bareos Director: Configure NDMP Fileset
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To specify what files and directories from the storage appliance should be backed up, a Fileset needs to be specified. In our example, we decided to backup :file:`/ifs/home` directory.

The specified directory needs to be a filesystem or a subdirectory of a filesystem which can be accessed by NDMP. Which filesystems are available is showed in the :bcommand:`status client` output of the NDMP client.

:index:`\ <single: NDMP; Environment variables>`\  Additionally, NDMP can be configured via NDMP environment variables. These can be specified in the Options Block of the Fileset with the :strong:`Meta`\  keyword. Which variables are available is partly depending on the NDMP implementation of the Storage Appliance.

.. code-block:: bareosconfig
   :caption: NDMP Fileset

   Fileset {
     Name = "NDMP Fileset"
     Include {
       Options {
           meta = "BUTYPE=DUMP"
           meta = "USE_TBB_IF_AVAILABLE=y"
           meta = "FH_REPORT_FULL_DIRENTS=y"
           meta = "RESTORE_HARDLINK_BY_TABLE=y"
       }
       File = /ifs/home
     }
   }

.. warning::

   Normally (:config:option:`dir/client/Protocol`\ =Native) Filesets get handled by the \bareosFd. When connecting directly to a NDMP Clients (:config:option:`dir/client/Protocol`\ =NDMP*), no |fd| is involved and therefore most Fileset options can't be used. Instead, parameters are handled via :strong:`Options - Meta`\  from :config:option:`dir/fileset/Include`\ .


.. warning::

   Avoid using multiple :config:option:`dir/fileset/Include`\  :strong:`File`\  directives.
   The |dir| would try to handle them by running multiple NDMP jobs in a single Bareos job.
   Even if this is working fine during backup, restore jobs will cause trouble.

Some NDMP environment variables are set automatically by the DMA in the |dir|. The following environment variables are currently set automatically:

FILESYSTEM
   is set to the :config:option:`dir/fileset/Include`\  :strong:`File`\  directive.

HIST
   = Y
   Specifies the file history format:

   Y
      Specifies the default file history format determined by your NDMP backup settings.

   N
      Disables file history. Without file hostory, single file restore is not possible with Bareos.

   Some NDMP environments (eg. Isilon OneFS) allow additional parameter:

   F
      Specifies path-based file history. This is the most efficient with Bareos.

   D
      Specifies directory or node file history.

LEVEL
   is set accordingly to :ref:`section-NdmpBackupLevel`.

PREFIX

TYPE
   is set accordingly to BUTYPE. Default "DUMP".

UPDATE
   = Y

Example NDMP Fileset to backup a subset of a NDMP filesystem
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

The following fileset is intended to backup all files and directories matching :file:`/ifs/home/users/a*`. It has been tested against Isilon OneFS 7.2.0.1. See `Isilon OneFS 7.2.0 CLI Administration Guide <https://www.emc.com/collateral/TechnicalDocument/docu56048.pdf>`_, section quote{NDMP environment variables} for details about the supported NDMP environment variables. Excludes are not used in this example.

.. code-block:: bareosconfig
   :caption: NDMP Fileset Isilon Include/Exclude

   Fileset {
     Name = "isilon_fileset_home_a"
     Include {
       Options {
           meta = "BUTYPE=DUMP"
           meta = "USE_TBB_IF_AVAILABLE=y"

           #
           # EXCLUDE
           #
           #meta = "EXCLUDE=[b-z]*"

           #
           # INCLUDE
           #
           meta = "FILES=a*"
       }
       File = /ifs/home/users
     }
   }

Bareos Director: Configure NDMP Jobs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To do NDMP backups and restores, some special settings need to be configured. We define special Backup and Restore jobs for NDMP.

.. code-block:: bareosconfig
   :caption: NDMP backup job

   Job {
     Name          = "ndmp-backup-job"
     Type          = Backup
     Protocol      = NDMP_BAREOS
     Level         = Incremental
     Client        = ndmp-client
     Backup Format = dump
     FileSet       = "NDMP Fileset"
     Storage       = NDMPFile
     Pool          = Full
     Messages      = Standard
   }

.. code-block:: bareosconfig
   :caption: NDMP restore job

   Job {
     Name          = "ndmp-restore-job"
     Type          = Restore
     Protocol      = NDMP_BAREOS
     Client        = ndmp-client
     Backup Format = dump
     FileSet       = "NDMP Fileset"
     Storage       = NDMPFile
     Pool          = Full
     Messages      = Standard
     Where         = /
   }

-  :config:option:`dir/job/BackupFormat`\ =dump is used in our example. Other Backup Formats have other advantages/disadvantages.



.. figure:: /include/images/ndmp-cfg.*
   :alt: NDMP configuration overview
   :name: fig:ndmp-overview

   NDMP configuration overview

Run NDMP Backup
~~~~~~~~~~~~~~~

Now we are ready to do our first NDMP backup:

.. code-block:: bconsole
   :caption: run NDMP backup

   *<input>run job=ndmp-backup-job</input>
   Using Catalog "MyCatalog"
   Run Backup job
   JobName:  ndmp-backup-job
   Level:    Incremental
   Client:   ndmp-client
   Format:   dump
   FileSet:  NDMP Fileset
   Pool:     Full (From Job resource)
   Storage:  NDMPFile (From Job resource)
   When:     2016-01-14 10:48:04
   Priority: 10
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=1
   *<input>wait jobid=1</input>
   JobId=1
   JobStatus=OK (T)
   *<input>list joblog jobid=1</input>
    2016-01-14 10:57:53 bareos-dir JobId 1: Start NDMP Backup JobId 1, Job=NDMPJob.2016-01-14_10.57.51_04
    2016-01-14 10:57:53 bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
    2016-01-14 10:57:53 bareos-dir JobId 1: Using Device "FileStorage" to write.
    2016-01-14 10:57:53 bareos-dir JobId 1: Opening tape drive LPDA-DEJC-ENJL-AHAI-JCBD-LICP-LKHL-IEDK@/ifs/home%0 read/write
    2016-01-14 10:57:53 bareos-sd JobId 1: Labeled new Volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
    2016-01-14 10:57:53 bareos-sd JobId 1: Wrote label to prelabeled Volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage)
    2016-01-14 10:57:53 bareos-dir JobId 1: Commanding tape drive to rewind
    2016-01-14 10:57:53 bareos-dir JobId 1: Waiting for operation to start
    2016-01-14 10:57:53 bareos-dir JobId 1: Async request NDMP4_LOG_MESSAGE
    2016-01-14 10:57:53 bareos-dir JobId 1: Operation started
    2016-01-14 10:57:53 bareos-dir JobId 1: Monitoring backup
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: 'Filetransfer: Transferred 5632 bytes in 0.087 seconds throughput of 63.133 KB/s'
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: 'Filetransfer: Transferred 5632 total bytes '
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: 'CPU  user=0.016416  sys=0.029437  ft=0.077296  cdb=0.000000'
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: 'maxrss=14576  in=13  out=22  vol=155  inv=72'
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: '
           Objects (scanned/included):
           ----------------------------
           Regular Files:          (1/1)
           Sparse Files:           (0/0)
           Stub Files:             (0/0)
           Directories:            (2/2)
           ADS Entries:            (0/0)
           ADS Containers:         (0/0)
           Soft Links:             (0/0)
           Hard Links:             (0/0)
           Block Device:           (0/0)
           Char Device:            (0/0)
           FIFO:                   (0/0)
           Socket:                 (0/0)
           Whiteout:               (0/0)
           Unknown:                (0/0)'
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: '
           Dir Depth (count)
           ----------------------------
           Total Dirs:             2
           Max Depth:              1

           File Size (count)
           ----------------------------
           == 0                    0
           <= 8k                   1
           <= 64k                  0
           <= 1M                   0
           <= 20M                  0
           <= 100M                 0
           <= 1G                   0
            > 1G                   0
           -------------------------
           Total Files:            1
           Total Bytes:            643
           Max Size:               643
           Mean Size:              643'
    2016-01-14 10:57:53 bareos-dir JobId 1: LOG_MESSAGE: '
           File History
           ----------------------------
           Num FH_HIST_FILE messages:              0
           Num FH_HIST_DIR  messages:              6
           Num FH_HIST_NODE messages:              3'
    2016-01-14 10:57:54 bareos-dir JobId 1: Async request NDMP4_NOTIFY_MOVER_HALTED
    2016-01-14 10:57:54 bareos-dir JobId 1: DATA: bytes 2053KB  MOVER: written 2079KB record 33
    2016-01-14 10:57:54 bareos-dir JobId 1: Operation done, cleaning up
    2016-01-14 10:57:54 bareos-dir JobId 1: Waiting for operation to halt
    2016-01-14 10:57:54 bareos-dir JobId 1: Commanding tape drive to NDMP9_MTIO_EOF 2 times
    2016-01-14 10:57:54 bareos-dir JobId 1: Commanding tape drive to rewind
    2016-01-14 10:57:54 bareos-dir JobId 1: Closing tape drive LPDA-DEJC-ENJL-AHAI-JCBD-LICP-LKHL-IEDK@/ifs/home%0
    2016-01-14 10:57:54 bareos-dir JobId 1: Operation halted, stopping
    2016-01-14 10:57:54 bareos-dir JobId 1: Operation ended OKAY
    2016-01-14 10:57:54 bareos-sd JobId 1: Elapsed time=00:00:01, Transfer rate=2.128 M Bytes/second
    2016-01-14 10:57:54 bareos-dir JobId 1: Bareos bareos-dir 15.2.2 (16Nov15):
     Build OS:               x86_64-redhat-linux-gnu redhat Red Hat Enterprise Linux Server release 7.0 (Maipo)
     JobId:                  1
     Job:                    ndmp-backup-job.2016-01-14_10.57.51_04
     Backup Level:           Full
     Client:                 "ndmp-client"
     FileSet:                "NDMP Fileset" 2016-01-14 10:57:51
     Pool:                   "Full" (From Job resource)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "NDMPFile" (From Job resource)
     Scheduled time:         14-Jan-2016 10:57:51
     Start time:             14-Jan-2016 10:57:53
     End time:               14-Jan-2016 10:57:54
     Elapsed time:           1 sec
     Priority:               10
     NDMP Files Written:     3
     SD Files Written:       1
     NDMP Bytes Written:     2,102,784 (2.102 MB)
     SD Bytes Written:       2,128,987 (2.128 MB)
     Rate:                   2102.8 KB/s
     Volume name(s):         Full-0001
     Volume Session Id:      4
     Volume Session Time:    1452764858
     Last Volume Bytes:      2,131,177 (2.131 MB)
     Termination:            Backup OK

We have successfully created our first NDMP backup.

Let us have a look what files are in our backup:

.. code-block:: bconsole
   :caption: list the files of the backup job

   *<input>list files jobid=1</input>
    /@NDMP/ifs/home%0
    /ifs/home/
    /ifs/home/admin/
    /ifs/home/admin/.zshrc

The real backup data is stored in the file :file:`/@NDMP/ifs/home%0` (we will refer to it as "NDMP main backup file" or "main backup file" later on). One NDMP main backup file is created for every directory specified in the used Fileset. The other files show the file history and are hardlinks to the backup file.

Run NDMP Restore
~~~~~~~~~~~~~~~~

Now that we have a NDMP backup, we of course also want to restore some data from the backup. If the backup we just did saved the Filehistory, we are able to select single files for restore. Otherwise, we will only be able to restore the whole backup.

Full Restore
^^^^^^^^^^^^

Either select all files or the main backup file (:file:`/@NDMP/ifs/home%0`). If file history is not included in the backup job, than only the main backup file is available.

Restore files to original path
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bconsole

   *<input>restore jobid=1</input>
   You have selected the following JobId: 1

   Building directory tree for JobId(s) 1 ...
   2 files inserted into the tree.

   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.

   cwd is: /
   $ <input>mark /ifs/home/admin/.zshrc</input>
   $ <input>done</input>
   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.1.bsr

   The job will require the following
      Volume(s)                 Storage(s)                SD Device(s)
   ===========================================================================

       Full-0001                 File                      FileStorage

   Volumes marked with "*" are online.


   1 file selected to be restored.

   The defined Restore Job resources are:
        1: RestoreFiles
        2: ndmp-restore-job
   Select Restore Job (1-2): <input>2</input>
   Defined Clients:
        1: bareos-fd
        2: ndmp-client
   Select the Client (1-2): <input>2</input>
   Run Restore job
   JobName:         ndmp-backup-job
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.1.bsr
   Where:           /
   Replace:         Always
   FileSet:         NDMP Fileset
   Backup Client:   ndmp-client
   Restore Client:  ndmp-client
   Format:          dump
   Storage:         File
   When:            2016-01-14 11:04:46
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=2
   *<input>wait jobid=2</input>
   JobId=2
   JobStatus=OK (T)
   *<input>list joblog jobid=2</input>
   14-Jan 11:04 bareos-dir JobId 2: Start Restore Job ndmp-backup-job.2016-01-14_11.04.53_05
   14-Jan 11:04 bareos-dir JobId 2: Using Device "FileStorage" to read.
   14-Jan 11:04 bareos-dir JobId 2: Opening tape drive KKAE-IMLO-NHJD-GOCO-GJCO-GEHB-BODL-ADNG@/ifs/home read-only
   14-Jan 11:04 bareos-dir JobId 2: Commanding tape drive to rewind
   14-Jan 11:04 bareos-dir JobId 2: Waiting for operation to start
   14-Jan 11:04 bareos-sd JobId 2: Ready to read from volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
   14-Jan 11:04 bareos-sd JobId 2: Forward spacing Volume "Full-0001" to file:block 0:194.
   14-Jan 11:04 bareos-dir JobId 2: Async request NDMP4_LOG_MESSAGE
   14-Jan 11:04 bareos-dir JobId 2: Operation started
   14-Jan 11:04 bareos-dir JobId 2: Monitoring recover
   14-Jan 11:04 bareos-dir JobId 2: DATA: bytes 0KB  MOVER: read 0KB record 0
   14-Jan 11:04 bareos-dir JobId 2: LOG_MESSAGE: 'Filetransfer: Transferred 1048576 bytes in 0.135 seconds throughput of 7557.139 KB/s'
   14-Jan 11:04 bareos-dir JobId 2: OK: /admin/.zshrc
   14-Jan 11:04 bareos-dir JobId 2: LOG_MESSAGE: '
           Objects:
           ----------------------------
           Regular Files:          (1)
           Stub Files:             (0)
           Directories:            (0)
           ADS Entries:            (0)
           Soft Links:             (0)
           Hard Links:             (0)
           Block Device:           (0)
           Char Device:            (0)
           FIFO:                   (0)
           Socket:                 (0)
           Unknown:                (0)'
   14-Jan 11:04 bareos-dir JobId 2: LOG_MESSAGE: '
           File Size (count)
           ----------------------------
           == 0                    0
           <= 8k                   1
           <= 64k                  0
           <= 1M                   0
           <= 20M                  0
           <= 100M                 0
           <= 1G                   0
            > 1G                   0
           -------------------------
           Total Files:            1
           Total Bytes:            643
           Max Size:               643
           Mean Size:              643'
   14-Jan 11:04 bareos-dir JobId 2: Async request NDMP4_NOTIFY_MOVER_PAUSED
   14-Jan 11:04 bareos-dir JobId 2: DATA: bytes 1024KB  MOVER: read 2079KB record 32
   14-Jan 11:04 bareos-dir JobId 2: Mover paused, reason=NDMP9_MOVER_PAUSE_EOF
   14-Jan 11:04 bareos-dir JobId 2: End of tapes
   14-Jan 11:04 bareos-dir JobId 2: DATA: bytes 1024KB  MOVER: read 2079KB record 32
   14-Jan 11:04 bareos-dir JobId 2: Operation done, cleaning up
   14-Jan 11:04 bareos-dir JobId 2: Waiting for operation to halt
   14-Jan 11:04 bareos-dir JobId 2: Commanding tape drive to rewind
   14-Jan 11:04 bareos-dir JobId 2: Closing tape drive KKAE-IMLO-NHJD-GOCO-GJCO-GEHB-BODL-ADNG@/ifs/home
   14-Jan 11:04 bareos-dir JobId 2: Operation halted, stopping
   14-Jan 11:04 bareos-dir JobId 2: Operation ended OKAY
   14-Jan 11:04 bareos-dir JobId 2: LOG_FILE messages: 1 OK, 0 ERROR, total 1 of 1
   14-Jan 11:04 bareos-dir JobId 2: Bareos bareos-dir 15.2.2 (16Nov15):
     Build OS:               x86_64-redhat-linux-gnu redhat Red Hat Enterprise Linux Server release 7.0 (Maipo)
     JobId:                  2
     Job:                    ndmp-backup-job.2016-01-14_11.04.53_05
     Restore Client:         ndmp-client
     Start time:             14-Jan-2016 11:04:55
     End time:               14-Jan-2016 11:04:57
     Elapsed time:           2 secs
     Files Expected:         1
     Files Restored:         1
     Bytes Restored:         1,048,576
     Rate:                   524.3 KB/s
     SD termination status:  OK
     Termination:            Restore OK

.. _section-ndmp-where:

Restore files to different path
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The restore location is determined by the :config:option:`dir/job/Where`\  setting of the restore job. In NDMP, this parameter works in a special manner, the prefix can be either "relative" to the filesystem or "absolute". If a prefix is set in form of a directory (like :file:`/bareos-restores`), it will be a relative prefix and will be added between the filesystem and the filename. This is needed to make sure that the
data is restored in a different directory, but into the same filesystem. If the prefix is set with a leading caret (^), it will be an absolute prefix and will be put at the front of the restore path. This is needed if the restored data should be stored into a different filesystem.

Example:

=============================== ===================================== ===============================================
original file name              where                                 restored file
=============================== ===================================== ===============================================
:file:`/ifs/home/admin/.zshrc`  :file:`/bareos-restores`               :file:`/ifs/home/bareos-restores/admin/.zshrc`
:file:`/ifs/home/admin/.zshrc`  :file:`^/ifs/data/bareos-restores`     :file:`/ifs/data/bareos-restores/admin/.zshrc`
=============================== ===================================== ===============================================

NDMP Copy Jobs
~~~~~~~~~~~~~~

:index:`\ <single: Copy; NDMP>`\  :index:`\ <single: NDMP; Copy jobs>`\

To be able to do copy jobs, we need to have a second storage resource where we can copy the data to. Depending on your requirements, this resource can be added to the existing |sd| (e.g. :config:option:`Sd/Storage = autochanger-0`\  for tape based backups) or to an additional |sd|.

We set up an additional |sd| on a host named :strong:`bareos-sd2.example.com` with the default :config:option:`Sd/Storage = FileStorage`\  device.

When this is done, add a second storage resource :config:option:`Dir/Storage = File2`\  to the :file:`bareos-dir.conf`:

.. code-block:: bareosconfig
   :caption: Storage resource File2

   Storage {
     Name = File2
     Address = bareos-sd2.example.com
     Password = <secretpassword>
     Device = FileStorage
     Media Type = File
   }

Copy Jobs copy data from one pool to another (see :ref:`MigrationChapter`). So we need to define a pool where the copies will be written to:

Add a Pool that the copies will run to:

.. code-block:: bareosconfig
   :caption: Pool resource Copy

   #
   # Copy Destination Pool
   #
   Pool {
     Name = Copy
     Pool Type = Backup
     Recycle = yes                       # Bareos can automatically recycle Volumes
     AutoPrune = yes                     # Prune expired volumes
     Volume Retention = 365 days         # How long should the Full Backups be kept? (#06)
     Maximum Volume Bytes = 50G          # Limit Volume size to something reasonable
     Maximum Volumes = 100               # Limit number of Volumes in Pool
     Label Format = "Copy-"              # Volumes will be labeled "Full-<volume-id>"
     Storage = File2                     # Pool belongs to Storage File2
   }

Then we need to define the just defined pool as the :config:option:`dir/pool/NextPool`\  of the pool that actually holds the data to be copied.

In our case this is the :config:option:`Dir/Pool = Full`\  Pool:

.. code-block:: bareosconfig
   :caption: add Next Pool setting

   #
   # Full Pool definition
   #
   Pool {
     Name = Full
     [...]
     Next Pool = Copy   # <- this line needs to be added!
   }

Finally, we need to define a Copy Job that will select the jobs that are in the :config:option:`Dir/Pool = Full`\  pool and copy them over to the :config:option:`Dir/Pool = Copy`\  pool reading the data via the :config:option:`Dir/Storage = File`\  Storage and writing the data via the :config:option:`Dir/Storage = File2`\  Storage:

.. code-block:: bareosconfig
   :caption: NDMP copy job

   Job {
      Name = NDMPCopy
      Type = Copy
      Messages = Standard
      Selection Type = PoolUncopiedJobs
      Pool = Full
      Storage = NDMPFile
   }

After restarting the director and storage daemon, we can run the Copy job:

.. code-block:: bconsole
   :caption: run copy job

   *<input>run job=NDMPCopy</input>
   Run Copy job
   JobName:       NDMPCopy
   Bootstrap:     *None*
   Pool:          Full (From Job resource)
   NextPool:      Copy (From unknown source)
   Write Storage: File2 (From Storage from Run NextPool override)
   JobId:         *None*
   When:          2016-01-21 09:19:49
   Catalog:       MyCatalog
   Priority:      10
   OK to run? (yes/mod/no): <input>yes</input>
   Job queued. JobId=74
   *<input>wait jobid=74</input>
   JobId=74
   JobStatus=OK (T)
   *<input>list joblog jobid=74</input>
   21-Jan 09:19 bareos-dir JobId 74: The following 1 JobId was chosen to be copied: 73
   21-Jan 09:19 bareos-dir JobId 74: Automatically selected Catalog: MyCatalog
   21-Jan 09:19 bareos-dir JobId 74: Using Catalog "MyCatalog"
   21-Jan 09:19 bareos-dir JobId 75: Copying using JobId=73 Job=NDMPJob.2016-01-21_09.18.50_49
   21-Jan 09:19 bareos-dir JobId 75: Bootstrap records written to /var/lib/bareos/bareos-dir.restore.20.bsr
   21-Jan 09:19 bareos-dir JobId 74: Job queued. JobId=75
   21-Jan 09:19 bareos-dir JobId 74: Copying JobId 75 started.
   21-Jan 09:19 bareos-dir JobId 74: Bareos bareos-dir 15.2.2 (16Nov15):
     Build OS:               x86_64-redhat-linux-gnu redhat Red Hat Enterprise Linux Server release 7.0 (Maipo)
     Current JobId:          74
     Current Job:            NDMPCopy.2016-01-21_09.19.50_50
     Catalog:                "MyCatalog" (From Default catalog)
     Start time:             21-Jan-2016 09:19:52
     End time:               21-Jan-2016 09:19:52
     Elapsed time:           0 secs
     Priority:               10
     Termination:            Copying -- OK

   21-Jan 09:19 bareos-dir JobId 75: Start Copying JobId 75, Job=NDMPCopy.2016-01-21_09.19.52_51
   21-Jan 09:19 bareos-dir JobId 75: Using Device "FileStorage" to read.
   21-Jan 09:19 bareos-dir JobId 76: Using Device "FileStorage2" to write.
   21-Jan 09:19 bareos-sd JobId 75: Ready to read from volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
   21-Jan 09:19 bareos-sd JobId 76: Volume "Copy-0004" previously written, moving to end of data.
   21-Jan 09:19 bareos-sd JobId 76: Ready to append to end of Volume "Copy-0004" size=78177310
   21-Jan 09:19 bareos-sd JobId 75: Forward spacing Volume "Full-0001" to file:block 0:78177310.
   21-Jan 09:19 bareos-sd JobId 75: End of Volume at file 0 on device "FileStorage" (/var/lib/bareos/storage), Volume "Full-0001"
   21-Jan 09:19 bareos-sd JobId 75: End of all volumes.
   21-Jan 09:19 bareos-sd JobId 76: Elapsed time=00:00:01, Transfer rate=64.61 K Bytes/second
   21-Jan 09:19 bareos-dir JobId 75: Bareos bareos-dir 15.2.2 (16Nov15):
     Build OS:               x86_64-redhat-linux-gnu redhat Red Hat Enterprise Linux Server release 7.0 (Maipo)
     Prev Backup JobId:      73
     Prev Backup Job:        NDMPJob.2016-01-21_09.18.50_49
     New Backup JobId:       76
     Current JobId:          75
     Current Job:            NDMPCopy.2016-01-21_09.19.52_51
     Backup Level:           Incremental
     Client:                 ndmp-client
     FileSet:                "NDMP Fileset"
     Read Pool:              "Full" (From Job resource)
     Read Storage:           "NDMPFile" (From Job resource)
     Write Pool:             "Copy" (From Job Pool's NextPool resource)
     Write Storage:          "File2" (From Storage from Pool's NextPool resource)
     Next Pool:              "Copy" (From Job Pool's NextPool resource)
     Catalog:                "MyCatalog" (From Default catalog)
     Start time:             21-Jan-2016 09:19:54
     End time:               21-Jan-2016 09:19:54
     Elapsed time:           0 secs
     Priority:               10
     SD Files Written:       1
     SD Bytes Written:       64,614 (64.61 KB)
     Rate:                   0.0 KB/s
     Volume name(s):         Copy-0004
     Volume Session Id:      43
     Volume Session Time:    1453307753
     Last Volume Bytes:      78,242,384 (78.24 MB)
     SD Errors:              0
     SD termination status:  OK
     Termination:            Copying OK

Now we successfully copied over the NDMP job.



   .. warning::

      :bcommand:`list jobs` will only show the number of main backup files as JobFiles. However, with :bcommand:`list files jobid=...` all files are visible.

Restore to NDMP Primary Storage System
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Unfortunately, we are not able to restore the copied data to our NDMP storage. If we try we get this message:

.. code-block:: bareosmessage

   21-Jan 09:21 bareos-dir JobId 77: Fatal error: Read storage File2 doesn't point to storage definition with paired storage option.

To be able to do NDMP operations from the storage that was used to store the copies, we need to define a NDMP storage that is paired with it. The definition is very similar to our :config:option:`Dir/Storage = NDMPFile`\  Storage, as we want to restore the data to the same NDMP Storage system:

.. code-block:: bareosconfig
   :caption: add paired Storage resource for File2

   Storage {
     Name = NDMPFile2
     Address = bareos-sd2.example.com
     Port = 10000
     Protocol = NDMPv4
     Auth Type = Clear
     Username = ndmpadmin
     Password = "test"
     Device = FileStorage2
     Media Type = File
     PairedStorage = File2
   }

Also we have to configure NDMP on the |sd| :strong:`bareos-sd2.example.com`. For this follow the instruction from :ref:`section-ndmp-sd-configure`.

After this, a restore from :strong:`bareos-sd2.example.com` directly to the NDMP Primary Storage System is possible.

Limitations
~~~~~~~~~~~

   This list the specific limitiations of the NDMP_BAREOS protocol. For limitation for all Bareos NDMP implementation, see :ref:`section-NdmpCommonLimitations`.

.. _section-ndmp-filehistory:

NDMP Job limitations when scanning in volumes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: NDMP; File History>`

For NDMP jobs, all data is stored into a single big file. The file and directory information (File History in NDMP Terms) is stored as hardlinks to this big file.

.. limitation:: NDMP: File information are not available in the Bareos backup stream.

   As hardlink information is only stored in the Bareos database, but not int the backup stream itself, it is not possible to recover the file history information from the NDMP stream with :command:`bscan`.

   As storing the database dump for disaster recovery and storing the bootstrap file offsite is recommended  anyway (see :ref:`section-before-disaster`), this should be not a big problem in correctly setup environments.

   For the same reason, the information about the number of files of a job (e.g. JobFiles with :bcommand:`list jobs` command) is limited to the number of NDMP backup files in copied jobs.




Restore always transfers the full main backup file to the Primary Storage System
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Contrary to |ndmpnative|, the |ndmpbareos| implementation do not support NDMP "Direct Access Restore" (DAR).

On restore, the full main backup file (:file:`@NDMP/...%.`) is always transfered back to the Primary Storage System, together with a description, what files to restore.

The reason for this is that the Primary Storage System handles the backup data by itself. Bareos will not modify the backup data it receives from the Primary Storage System.

.. _section-NdmpNative:

NDMP_NATIVE
-----------

The NDMP_NATIVE protocol is implemented since Bareos :sinceVersion:`17.2.3: NDMP NATIVE`.

Bareos implements the Data Management Agent inside of the |dir| and is the only Bareos Daemon involved in the backups.

When using NDMP_NATIVE, the Tape Agent must be provided by some other systems. Some storage vendors provide it with there storages, or offer it as an option, e.g. Isilon with there "Isilon Backup Accelerator".

Example Setup for NDMP_NATIVE backup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: NDMP; Example; NDMP\_NATIVE>`\

Configure a NDMP Client
^^^^^^^^^^^^^^^^^^^^^^^

This defines the connection to the NDMP Data Agent.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Client/isilon.conf

   Client {
     Name = isilon
     Address = isilon.example.com
     Port = 10000
     Protocol = NDMPv4
     Auth Type = MD5
     Username = "ndmpadmin"
     Password = "secret"
     Maximum Concurrent Jobs = 1
   }

Verify, that you can access your Primary Storage System (Tape Agent) via Bareos:

.. code-block:: bconsole
   :caption: status ndmp client

   *<input>status client=isilon</input>

   Data Agent isilon.example.com NDMPv4
     Host info
       hostname   isilon
       os_type    Isilon OneFS
       os_vers    v7.2.1.4
       hostid     xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

     Server info
       vendor     Isilon
       product    Isilon NDMP
       revision   2.2.1
       auths      (2) NDMP4_AUTH_TEXT NDMP4_AUTH_MD5

     Connection types
       addr_types (2) NDMP4_ADDR_TCP NDMP4_ADDR_LOCAL

     Backup type info of tar format
       attrs      0x7fe
       set        FILESYSTEM=/ifs
       set        FILES=
       set        EXCLUDE=
       set        PER_DIRECTORY_MATCHING=N
       set        HIST=f
       set        DIRECT=N
       set        LEVEL=0
       set        UPDATE=Y
       set        RECURSIVE=Y
       set        ENCODING=UTF-8
       set        ENFORCE_UNIQUE_NODE=N
       set        PATHNAME_SEPARATOR=/
       set        DMP_NAME=
       set        BASE_DATE=0
       set        NDMP_UNICODE_FH=N

     Backup type info of dump format
       attrs      0x7fe
       set        FILESYSTEM=/ifs
       set        FILES=
       set        EXCLUDE=
       set        PER_DIRECTORY_MATCHING=N
       set        HIST=f
       set        DIRECT=N
       set        LEVEL=0
       set        UPDATE=Y
       set        RECURSIVE=Y
       set        ENCODING=UTF-8
       set        ENFORCE_UNIQUE_NODE=N
       set        PATHNAME_SEPARATOR=/
       set        DMP_NAME=
       set        BASE_DATE=0
       set        NDMP_UNICODE_FH=N

     File system /ifs
       physdev    OneFS
       unsupported 0x0
       type       NFS
       status
       space      224681156345856 total, 126267678720 used, 224554888667136 avail
       inodes     324102912000 total, 323964781836 used
       set        MNTOPTS=
       set        MNTTIME=00:00:00 00:00:00

Configure a NDMP Fileset
^^^^^^^^^^^^^^^^^^^^^^^^

This determines what filesystem to backup and configures the NDMP environment to use in the meta options for it.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Fileset/isilon.conf

   Fileset {
       Name = "isilon"
       Include {
           Options {
               meta = "HIST=F"
               meta = "DIRECT=Y"
               meta = "RECURSIVE=Y"
               meta = "BUTYPE=DUMP"
           }
       File = /ifs/home
       }
   }

The setting of ``"DIRECT = Y"`` is required for Direct Access Recovery.

For more information, see :ref:`section-NdmpFileset`.

Configure a NDMP Storage
^^^^^^^^^^^^^^^^^^^^^^^^

This defines now to connect to the Tape and Robot Agents and what devices to use.

As we do not yet now the device names, we can put a placeholder string in :config:option:`dir/storage/Device`\  and :config:option:`dir/storage/NdmpChangerDevice`\ :

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Storage/isilon.conf

   Storage {
     Name = isilon
     Address = isilon.example.com
     Port = 10000
     Protocol = NDMPv4
     Auth Type = MD5
     Username = "ndmpadmin"
     Password = "secret"
     Maximum Concurrent Jobs = 1
     Autochanger = yes
     MediaType = NDMP-Tape

     Device = unknown               # use "status storage" to determine the tape device
     NDMP Changer Device = unknown  # use "status storage" to determine the changer device
   }

Verify that the connection to the NDMP Tape Agent and Robot Agent work, by running the :bcommand:`status storage` command.

The Tape Agent will return information about the available tape drives. The Robot Agent will return information about the available tape changer device.

.. code-block:: bconsole
   :caption: status ndmp storage (Tape Agent and Robot Agent)

   *<input>status storage=isilon</input>
   Tape Agent isilon.bareos.com NDMPv4
     Host info
       hostname   isilon
       os_type    Isilon OneFS
       os_vers    v7.2.1.4
       hostid     abcdefg

     Server info
       vendor     Isilon
       product    Isilon NDMP
       revision   2.2.1
       auths      (2) NDMP4_AUTH_TEXT NDMP4_AUTH_MD5

     Connection types
       addr_types (2) NDMP4_ADDR_TCP NDMP4_ADDR_LOCAL

     tape HP Ultrium 5-SCSI I30Z
       device     HP-TLD-004-01
         attr       0x4
         set        EXECUTE_CDB=t
         set        SERIAL_NUMBER=123456

     tape HP Ultrium 5-SCSI I30Z
       device     HP-TLD-004-02
         attr       0x4
         set        EXECUTE_CDB=t
         set        SERIAL_NUMBER=1234567

   Robot Agent isilon.bareos.com NDMPv4
     Host info
       hostname   isilon
       os_type    Isilon OneFS
       os_vers    v7.2.1.4
       hostid     001517db7e38f40dbb4dfc0b823f29a31e09

     Server info
       vendor     Isilon
       product    Isilon NDMP
       revision   2.2.1
       auths      (2) NDMP4_AUTH_TEXT NDMP4_AUTH_MD5

     scsi QUANTUM Scalar i6000 605A
       device     mc001
         set        SERIAL_NUMBER=VL002CX1252BVE01177

The interesting parts of the output is the device information both of the Tape Agent and Robot Agent.

As each NDMP backup or recovery operation always involves exactly one tape and at one robot agent.

We now know the device names and can configure what robot and what tape to use when this storage is used by bareos by updating the :config:option:`Sd/Storage = isilon`\  resource:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Storage/isilon.conf

   Storage {
     Name = isilon
     Address = isilon.example.com
     Port = 10000
     Protocol = NDMPv4
     Auth Type = MD5
     Username = "ndmpadmin"
     Password = "secret"
     Maximum Concurrent Jobs = 1
     Autochanger = yes
     MediaType = NDMP-Tape

     Device = HP-TLD-004-01
     NDMP Changer Device = mc001
   }

Configure a Pool for the NDMP Tapes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Pool/NDMP-Tape.conf

   Pool {
     Name = NDMP-Tape
     Pool Type = Backup
     Recycle = yes                       # Bareos can automatically recycle Volumes
     Auto Prune = yes                    # Prune expired volumes
     Volume Retention = 365 days         # How long should the Full Backups be kept?
   }

Configure NDMP Jobs
^^^^^^^^^^^^^^^^^^^

To be able to do scheduled backups, we need to configure a backup job that will use the NDMP client and NDMP storage resources:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Job/ndmp-native-backup-job.conf

   Job {
      Name = ndmp-native-backup-job
      type = backup
      protocol = NDMP_NATIVE
      level = incremental
      client = isilon
      storage = isilon
      backup format = dump
      messages = Standard
      Pool = NDMP-Tape
      save file history = yes
      FileSet = isilon
   }

As we also need to be able to do a restore of the backed up data, we also need to define an adequate restore job:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/Job/ndmp-native-restore-job.conf

   Job{
      Name = ndmp-restore
      type = restore
      protocol = NDMP_NATIVE
      client = isilon
      backup format = dump
      fileset = isilon
      storage  = isilon
      pool = NDMP-Tape
      Messages = Standard
      where = /
   }

Label Tapes
~~~~~~~~~~~

Before we can really start do do backups, first we need to label the tapes that should be used.

First we check if our robot has tapes with barcodes by running status slots:

.. code-block:: bconsole
   :caption: status storage=isilon slots

   *<input>status slots</input>
    Slot |   Volume Name    |   Status  |  Media Type    |         Pool             |
   ------+------------------+-----------+----------------+--------------------------|
       1@|                ? |         ? |              ? |                        ? |
       2@|                ? |         ? |              ? |                        ? |
       3@|                ? |         ? |              ? |                        ? |
       4@|                ? |         ? |              ? |                        ? |
   [...]
     251*|           BT0001 |         ? |              ? |                        ? |
     252*|           BT0002 |         ? |              ? |                        ? |
     253*|           BT0003 |         ? |              ? |                        ? |
     254*|           BT0004 |         ? |              ? |                        ? |
     255*|           BT0005 |         ? |              ? |                        ? |
     256*|           BT0006 |         ? |              ? |                        ? |
     257*|           BT0007 |         ? |              ? |                        ? |
   [...]

Now we can label these tapes and add them to the pool that we have created for NDMP Tapes:

.. code-block:: bconsole
   :caption: label barcodes

   *<input>label storage=isilon barcodes slots=251-257</input>
   Automatically selected Storage: isilon
   Select Drive:
        1: Drive 0
        2: Drive 1
   Select drive (1-12): 1
   get ndmp_vol_list...
   The following Volumes will be labeled:
   Slot  Volume
   ==============
    251  BT0001
    252  BT0002
    253  BT0003
    254  BT0004
    255  BT0005
    256  BT0006
    257  BT0007
   Do you want to label these Volumes? (yes|no): yes
   Defined Pools:
        1: Scratch
        2: NDMP-Tape
        3: Incremental
        4: Full
        5: Differential
   Select the Pool (1-5): 2
   ndmp_send_label_request: VolumeName=BT0001 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0001", Slot 251 successfully created.
   ndmp_send_label_request: VolumeName=BT0002 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0002", Slot 252 successfully created.
   ndmp_send_label_request: VolumeName=BT0003 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0003", Slot 253 successfully created.
   ndmp_send_label_request: VolumeName=BT0004 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0004", Slot 254 successfully created.
   ndmp_send_label_request: VolumeName=BT0005 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0005", Slot 255 successfully created.
   ndmp_send_label_request: VolumeName=BT0006 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0006", Slot 256 successfully created.
   ndmp_send_label_request: VolumeName=BT0007 MediaType=NDMP-Tape PoolName=NDMP-Tape drive=0
   Catalog record for Volume "BT0007", Slot 257 successfully created.

We have now 7 volumes in our NDMP-Tape Pool that were labeled and can be used for NDMP Backups.

Run NDMP_NATIVE Backup
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bconsole
   :caption: run backup job

   *<input>run job=ndmp-native-backup-job yes</input>
   JobId 1: Start NDMP Backup JobId 1, Job=ndmp.2017-04-07_01.40.31_10
   JobId 1: Using Data  host isilon.bareos.com
   JobId 1: Using Tape  host:device  isilon.bareos.com:HP-TLD-004-01
   JobId 1: Using Robot host:device  isilon.bareos.com:mc001
   JobId 1: Using Tape record size 64512
   JobId 1: Found volume for append: BT0001
   JobId 1: Commanding robot to load slot @4146 into drive @256
   JobId 1: robot moving @4146 to @256
   JobId 1: robot move OK @4146 to @256
   JobId 1: Opening tape drive HP-TLD-004-01 read/write
   JobId 1: Commanding tape drive to rewind
   JobId 1: Checking tape label, expect 'BT0001'
   JobId 1: Reading label
   JobId 1: Commanding tape drive to rewind
   JobId 1: Commanding tape drive to NDMP9_MTIO_FSF 1 times
   JobId 1: Waiting for operation to start
   JobId 1: Operation started
   JobId 1: Monitoring backup
   JobId 1: DATA: bytes 3703831KB  MOVER: written 3703644KB record 58788
   JobId 1: LOG_MESSAGE: 'End of medium reached.'
   JobId 1: DATA: bytes 4834614KB  MOVER: written 4834053KB record 76731
   JobId 1: Mover paused, reason=NDMP9_MOVER_PAUSE_EOM
   JobId 1: Operation requires next tape
   JobId 1: At EOM, not writing filemarks
   JobId 1: Commanding tape drive to rewind
   JobId 1: Closing tape drive HP-TLD-004-01
   JobId 1: Commanding robot to unload drive @256 to slot @4146
   JobId 1: robot moving @256 to @4146
   JobId 1: robot move OK @256 to @4146
   JobId 1: Found volume for append: BT0002
   JobId 1: Commanding robot to load slot @4147 into drive @256
   JobId 1: robot moving @4147 to @256
   JobId 1: robot move OK @4147 to @256
   JobId 1: Opening tape drive HP-TLD-004-01 read/write
   JobId 1: Commanding tape drive to rewind
   JobId 1: Checking tape label, expect 'BT0002'
   JobId 1: Reading label
   JobId 1: Commanding tape drive to rewind
   JobId 1: Commanding tape drive to NDMP9_MTIO_FSF 1 times
   JobId 1: Operation resuming
   JobId 1: DATA: bytes 6047457KB  MOVER: written 6047244KB record 95988
   JobId 1: LOG_MESSAGE: 'End of medium reached.'
   JobId 1: DATA: bytes 9668679KB  MOVER: written 9668106KB record 153462
   JobId 1: Mover paused, reason=NDMP9_MOVER_PAUSE_EOM
   JobId 1: Operation requires next tape
   JobId 1: At EOM, not writing filemarks
   JobId 1: Commanding tape drive to rewind
   JobId 1: Closing tape drive HP-TLD-004-01
   JobId 1: Commanding robot to unload drive @256 to slot @4147
   JobId 1: robot moving @256 to @4147
   JobId 1: robot move OK @256 to @4147
   JobId 1: Found volume for append: BT0003
   JobId 1: Commanding robot to load slot @4148 into drive @256
   JobId 1: robot moving @4148 to @256
   JobId 1: robot move OK @4148 to @256
   JobId 1: Opening tape drive HP-TLD-004-01 read/write
   JobId 1: Commanding tape drive to rewind
   JobId 1: Checking tape label, expect 'BT0003'
   JobId 1: Reading label
   JobId 1: Commanding tape drive to rewind
   JobId 1: Commanding tape drive to NDMP9_MTIO_FSF 1 times
   JobId 1: Operation resuming
   JobId 1: LOG_MESSAGE: 'Filetransfer: Transferred 10833593344 bytes in 87.187 seconds throughput of 121345.079 KB/s'
   JobId 1: LOG_MESSAGE: 'Filetransfer: Transferred 10833593344 total bytes '
   JobId 1: LOG_MESSAGE: 'CPU  user=0.528118  sys=54.575536  ft=87.182576  cdb=0.000000'
   JobId 1: LOG_MESSAGE: 'maxrss=171972  in=1323908  out=17  vol=199273  inv=5883'
   JobId 1: LOG_MESSAGE: '
           Objects (scanned/included):
           ----------------------------
           Regular Files:          (2765/2765)
           Sparse Files:           (0/0)
           Stub Files:             (0/0)
           Directories:            (447/447)
           ADS Entries:            (0/0)
           ADS Containers:         (0/0)
           Soft Links:             (0/0)
           Hard Links:             (0/0)
           Block Device:           (0/0)
           Char Device:            (0/0)
           FIFO:                   (0/0)
           Socket:                 (0/0)
           Whiteout:               (0/0)
           Unknown:                (0/0)'
   JobId 1: LOG_MESSAGE: '
           Dir Depth (count)
           ----------------------------
           Total Dirs:             447
           Max Depth:              10

           File Size (count)
           ----------------------------
           == 0                    14
           <= 8k                   1814
           <= 64k                  658
           <= 1M                   267
           <= 20M                  10
           <= 100M                 0
           <= 1G                   0
            > 1G                   2
           -------------------------
           Total Files:            2765
           Total Bytes:            10827843824
           Max Size:               5368709120
           Mean Size:              3916037'
   JobId 1: LOG_MESSAGE: '
           File History
           ----------------------------
           Num FH_HIST_FILE messages:              3212
           Num FH_HIST_DIR  messages:              0
           Num FH_HIST_NODE messages:              0'
   JobId 1: Async request NDMP4_NOTIFY_MOVER_HALTED
   JobId 1: DATA: bytes 10581729KB  MOVER: written 10581732KB record 167964
   JobId 1: Operation done, cleaning up
   JobId 1: Waiting for operation to halt
   JobId 1: Commanding tape drive to NDMP9_MTIO_EOF 2 times
   JobId 1: Commanding tape drive to rewind
   JobId 1: Closing tape drive HP-TLD-004-01
   JobId 1: Commanding robot to unload drive @256 to slot @4148
   JobId 1: robot moving @256 to @4148
   JobId 1: robot move OK @256 to @4148
   JobId 1: Operation halted, stopping
   JobId 1: Operation ended OKAY
   JobId 1:  ERR-CONN NDMP4_CONNECT_CLOSE  exchange-failed
   JobId 1: media #1 BT0001+1/4834053K@4146
   JobId 1:          valid label=Y filemark=Y n_bytes=Y slot=Y
   JobId 1:          media used=Y written=Y eof=N eom=Y io_error=N
   JobId 1:          label read=Y written=N io_error=N mismatch=N
   JobId 1:          fm_error=N nb_determined=Y nb_aligned=N
   JobId 1:          slot empty=N bad=N missing=N
   JobId 1: media #2 BT0002+1/4834053K@4147
   JobId 1:          valid label=Y filemark=Y n_bytes=Y slot=Y
   JobId 1:          media used=Y written=Y eof=N eom=Y io_error=N
   JobId 1:          label read=Y written=N io_error=N mismatch=N
   JobId 1:          fm_error=N nb_determined=Y nb_aligned=N
   JobId 1:          slot empty=N bad=N missing=N
   JobId 1: media #3 BT0003+1/913626K@4148
   JobId 1:          valid label=Y filemark=Y n_bytes=Y slot=Y
   JobId 1:          media used=Y written=Y eof=N eom=N io_error=N
   JobId 1:          label read=Y written=N io_error=N mismatch=N
   JobId 1:          fm_error=N nb_determined=Y nb_aligned=N
   JobId 1:          slot empty=N bad=N missing=N
   JobId 1: Media: BT0001+1/4834053K@251
   JobId 1: Media: BT0002+1/4834053K@252
   JobId 1: Media: BT0003+1/913626K@253
   JobId 1: ndmp_fhdb_lmdb.c:675 Now processing lmdb database
   JobId 1: ndmp_fhdb_lmdb.c:679 Processing lmdb database done
   JobId 1: Bareos bareos-dir 17.2.3:
     Build OS:               x86_64-unknown-linux-gnu redhat Red Hat Enterprise Linux Server release 6.8 (Santiago)
     JobId:                  1
     Job:                    ndmp.2017-04-07_01.40.31_10
     Backup Level:           Full
     Client:                 "isilon"
     FileSet:                "isilon" 2017-04-07 01:40:31
     Pool:                   "NDMP-Tape" (From Job resource)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "isilon" (From Job resource)
     Scheduled time:         07-Apr-2017 01:40:31
     Start time:             07-Apr-2017 01:40:33
     End time:               07-Apr-2017 01:42:03
     Elapsed time:           1 min 30 secs
     Priority:               10
     NDMP Files Written:     3,212
     NDMP Bytes Written:     10,835,690,496 (10.83 GB)
     Rate:                   120396.6 KB/s
     Volume name(s):         BT0001|BT0002|BT0003
     Volume Session Id:      0
     Volume Session Time:    0
     Last Volume Bytes:      935,553,024 (935.5 MB)
     Termination:            Backup OK

Run NDMP_NATIVE Restore
~~~~~~~~~~~~~~~~~~~~~~~

Now we want to restore some files from the backup we just did:

.. code-block:: bconsole
   :caption: run ndmp restore job

   *<input>restore</input>
   [...]

   cwd is: /
   : mark /ifs/home/testdata/git/bareos/src/console/bconsole
   1 file marked.
   : mark /ifs/home/testdatrandom5G-2
   1 file marked.
   $ done
   Connecting to Director bareos:9101
   1000 OK: bareos-dir Version: 17.2.3
   Enter a period to cancel a command.
   list joblog jobid=2
   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"
   JobId 2: Start Restore Job ndmp-restore.2017-04-07_01.48.23_13
   JobId 2: Namelist add: node:6033532893, info:5464882688, name:"/ifs/home/testdata/random5G-2"
   JobId 2: Namelist add: node:6033077461, info:40076288, name:"/ifs/home/testdata/git/bareos/src/console/bconsole"
   JobId 2: Record size is 64512
   JobId 2: Media: BT0001+1/4834053K@251
   JobId 2: Media: BT0002+1/4834053K@252
   JobId 2: Media: BT0003+1/913626K@253
   JobId 2: Logical slot for volume BT0001 is 251
   JobId 2: Physical(NDMP) slot for volume BT0001 is 4146
   JobId 2: Media Index of volume BT0001 is 1
   JobId 2: Logical slot for volume BT0002 is 252
   JobId 2: Physical(NDMP) slot for volume BT0002 is 4147
   JobId 2: Media Index of volume BT0002 is 2
   JobId 2: Logical slot for volume BT0003 is 253
   JobId 2: Physical(NDMP) slot for volume BT0003 is 4148
   JobId 2: Media Index of volume BT0003 is 3
   JobId 2: Commanding robot to load slot @4146 into drive @256
   JobId 2: robot moving @4146 to @256
   JobId 2: robot move OK @4146 to @256
   JobId 2: Opening tape drive HP-TLD-004-01 read-only
   JobId 2: Commanding tape drive to rewind
   JobId 2: Checking tape label, expect 'BT0001'
   JobId 2: Reading label
   JobId 2: Commanding tape drive to rewind
   JobId 2: Commanding tape drive to NDMP9_MTIO_FSF 1 times
   JobId 2: Waiting for operation to start
   JobId 2: Operation started
   JobId 2: Monitoring recover
   JobId 2: DATA: bytes 0KB  MOVER: read 0KB record 0
   JobId 2: DATA: bytes 11KB  MOVER: read 11KB record 622
   JobId 2: Mover paused, reason=NDMP9_MOVER_PAUSE_SEEK
   JobId 2: Operation requires a different tape
   JobId 2: Commanding tape drive to rewind
   JobId 2: Closing tape drive HP-TLD-004-01
   JobId 2: Commanding robot to unload drive @256 to slot @4146
   JobId 2: robot moving @256 to @4146
   JobId 2: robot move OK @256 to @4146
   JobId 2: Commanding robot to load slot @4147 into drive @256
   JobId 2: robot moving @4147 to @256
   JobId 2: robot move OK @4147 to @256
   JobId 2: Opening tape drive HP-TLD-004-01 read-only
   JobId 2: Commanding tape drive to rewind
   JobId 2: Checking tape label, expect 'BT0002'
   JobId 2: Reading label
   JobId 2: Commanding tape drive to rewind
   JobId 2: Commanding tape drive to NDMP9_MTIO_FSF 1 times
   JobId 2: Operation resuming
   JobId 2: DATA: bytes 79884KB  MOVER: read 79884KB record 85979
   JobId 2: DATA: bytes 201740KB  MOVER: read 201740KB record 87914
   JobId 2: DATA: bytes 321548KB  MOVER: read 321548KB record 89815
   JobId 2: DATA: bytes 440332KB  MOVER: read 440332KB record 91701
   JobId 2: DATA: bytes 556044KB  MOVER: read 556044KB record 93538
   JobId 2: DATA: bytes 674828KB  MOVER: read 674828KB record 95423
   JobId 2: DATA: bytes 796684KB  MOVER: read 796684KB record 97357
   JobId 2: DATA: bytes 915468KB  MOVER: read 915468KB record 99243
   JobId 2: DATA: bytes 1036300KB  MOVER: read 1036300KB record 101161
   JobId 2: DATA: bytes 1157132KB  MOVER: read 1157132KB record 103079
   JobId 2: DATA: bytes 1277964KB  MOVER: read 1277964KB record 104997
   JobId 2: DATA: bytes 1398796KB  MOVER: read 1398796KB record 106915
   JobId 2: DATA: bytes 1518604KB  MOVER: read 1518604KB record 108816
   JobId 2: DATA: bytes 1622028KB  MOVER: read 1622028KB record 110458
   JobId 2: DATA: bytes 1741836KB  MOVER: read 1741836KB record 112360
   JobId 2: DATA: bytes 1859596KB  MOVER: read 1859596KB record 114229
   JobId 2: DATA: bytes 1981452KB  MOVER: read 1981452KB record 116163
   JobId 2: DATA: bytes 2094092KB  MOVER: read 2094092KB record 117951
   JobId 2: DATA: bytes 2207756KB  MOVER: read 2207756KB record 119755
   JobId 2: DATA: bytes 2328588KB  MOVER: read 2328588KB record 121673
   JobId 2: DATA: bytes 2448396KB  MOVER: read 2448396KB record 123575
   JobId 2: DATA: bytes 2569228KB  MOVER: read 2569228KB record 125493
   JobId 2: DATA: bytes 2689036KB  MOVER: read 2689036KB record 127395
   JobId 2: DATA: bytes 2810892KB  MOVER: read 2810892KB record 129329
   JobId 2: DATA: bytes 2926604KB  MOVER: read 2926604KB record 131165
   JobId 2: DATA: bytes 3043340KB  MOVER: read 3043340KB record 133018
   JobId 2: DATA: bytes 3163148KB  MOVER: read 3163148KB record 134920
   JobId 2: DATA: bytes 3279884KB  MOVER: read 3279884KB record 136773
   JobId 2: DATA: bytes 3400716KB  MOVER: read 3400716KB record 138691
   JobId 2: DATA: bytes 3518476KB  MOVER: read 3518476KB record 140560
   JobId 2: DATA: bytes 3636236KB  MOVER: read 3636236KB record 142429
   JobId 2: DATA: bytes 3757068KB  MOVER: read 3757068KB record 144347
   JobId 2: DATA: bytes 3877900KB  MOVER: read 3877900KB record 146265
   JobId 2: DATA: bytes 3994636KB  MOVER: read 3994636KB record 148118
   JobId 2: DATA: bytes 4116492KB  MOVER: read 4116492KB record 150053
   JobId 2: DATA: bytes 4237324KB  MOVER: read 4237324KB record 151971
   JobId 2: DATA: bytes 4331317KB  MOVER: read 4331317KB record 153462
   JobId 2: Mover paused, reason=NDMP9_MOVER_PAUSE_SEEK
   JobId 2: Operation requires a different tape
   JobId 2: Commanding tape drive to rewind
   JobId 2: Closing tape drive HP-TLD-004-01
   JobId 2: Commanding robot to unload drive @256 to slot @4147
   JobId 2: robot moving @256 to @4147
   JobId 2: robot move OK @256 to @4147
   JobId 2: Commanding robot to load slot @4148 into drive @256
   JobId 2: robot moving @4148 to @256
   JobId 2: robot move OK @4148 to @256
   JobId 2: Opening tape drive HP-TLD-004-01 read-only
   JobId 2: Commanding tape drive to rewind
   JobId 2: Checking tape label, expect 'BT0003'
   JobId 2: Reading label
   JobId 2: Commanding tape drive to rewind
   JobId 2: Commanding tape drive to NDMP9_MTIO_FSF 1 times
   JobId 2: Operation resuming
   JobId 2: DATA: bytes 4424716KB  MOVER: read 4424716KB record 154945
   JobId 2: DATA: bytes 4544524KB  MOVER: read 4544524KB record 156847
   JobId 2: DATA: bytes 4663308KB  MOVER: read 4663308KB record 158732
   JobId 2: DATA: bytes 4781068KB  MOVER: read 4781068KB record 160601
   JobId 2: DATA: bytes 4902924KB  MOVER: read 4902924KB record 162536
   JobId 2: DATA: bytes 5022732KB  MOVER: read 5022732KB record 164437
   JobId 2: DATA: bytes 5138444KB  MOVER: read 5138444KB record 166274
   JobId 2: OK: /testdata/git/bareos/src/console/bconsole
   JobId 2: OK: /testdata/random5G-2
   JobId 2: LOG_MESSAGE: 'Filetransfer: Transferred 5368721181 bytes in 223.436 seconds throughput of 23464.803 KB/s'
   JobId 2: LOG_MESSAGE: '
           Objects:
           ----------------------------
           Regular Files:          (2)
           Stub Files:             (0)
           Directories:            (0)
           ADS Entries:            (0)
           Soft Links:             (0)
           Hard Links:             (0)
           Block Device:           (0)
           Char Device:            (0)
           FIFO:                   (0)
           Socket:                 (0)
           Unknown:                (0)'
   JobId 2: LOG_MESSAGE: '
           File Size (count)
           ----------------------------
           == 0                    0
           <= 8k                   1
           <= 64k                  0
           <= 1M                   0
           <= 20M                  0
           <= 100M                 0
           <= 1G                   0
            > 1G                   1
           -------------------------
           Total Files:            2
           Total Bytes:            5368716925
           Max Size:               5368709120
           Mean Size:              2684358462'
   JobId 2: Async request NDMP4_NOTIFY_MOVER_HALTED
   JobId 2: DATA: bytes 5242893KB  MOVER: read 5242893KB record 167932
   JobId 2: Operation done, cleaning up
   JobId 2: Waiting for operation to halt
   JobId 2: Commanding tape drive to rewind
   JobId 2: Closing tape drive HP-TLD-004-01
   JobId 2: Commanding robot to unload drive @256 to slot @4148
   JobId 2: robot moving @256 to @4148
   JobId 2: robot move OK @256 to @4148
   JobId 2: Operation halted, stopping
   JobId 2: Operation ended OKAY
   JobId 2:  ERR-CONN NDMP4_CONNECT_CLOSE  exchange-failed
   JobId 2: LOG_FILE messages: 2 OK, 0 ERROR, total 2 of 2
   JobId 2: media #1 BT0001+1/4834053K@4146
   JobId 2:          valid label=Y filemark=Y n_bytes=Y slot=Y
   JobId 2:          media used=Y written=N eof=N eom=N io_error=N
   JobId 2:          label read=Y written=N io_error=N mismatch=N
   JobId 2:          fm_error=N nb_determined=N nb_aligned=N
   JobId 2:          slot empty=N bad=N missing=N
   JobId 2: media #2 BT0002+1/4834053K@4147
   JobId 2:          valid label=Y filemark=Y n_bytes=Y slot=Y
   JobId 2:          media used=Y written=N eof=N eom=N io_error=N
   JobId 2:          label read=Y written=N io_error=N mismatch=N
   JobId 2:          fm_error=N nb_determined=N nb_aligned=N
   JobId 2:          slot empty=N bad=N missing=N
   JobId 2: media #3 BT0003+1/911610K@4148
   JobId 2:          valid label=Y filemark=Y n_bytes=Y slot=Y
   JobId 2:          media used=Y written=N eof=N eom=N io_error=N
   JobId 2:          label read=Y written=N io_error=N mismatch=N
   JobId 2:          fm_error=N nb_determined=Y nb_aligned=N
   JobId 2:          slot empty=N bad=N missing=N
   JobId 2: Bareos bareos-dir 17.2.3:
     Build OS:               x86_64-unknown-linux-gnu redhat Red Hat Enterprise Linux Server release 6.8 (Santiago)
     JobId:                  2
     Job:                    ndmp-restore.2017-04-07_01.48.23_13
     Restore Client:         isilon
     Start time:             07-Apr-2017 01:48:25
     End time:               07-Apr-2017 01:52:11
     Elapsed time:           3 mins 46 secs
     Files Expected:         2
     Files Restored:         1
     Bytes Restored:         5,368,722,944
     Rate:                   23755.4 KB/s



NDMP Common
-----------

This section contains additional information about the Bareos NDMP implementation that are valid for all Bareos NDMP protocols.

.. _section-NdmpBackupLevel:

NDMP Backup Level
~~~~~~~~~~~~~~~~~

:index:`\ <single: NDMP; Level>`\

The trailing number in the main backup file (after the :file:`%` character) indicates the NDMP backup level:

===== =========================================
Level Description
===== =========================================
0     Full NDMP backup.
1     Differential or first Incremental backup.
2-9   second to ninth Incremental backup.
===== =========================================

Differential Backups
^^^^^^^^^^^^^^^^^^^^

are supported. The NDMP backup level will be 1, visible as trailing number in the backup file (:file:`/@NDMP/ifs/home%1`).

Incremental Backups
^^^^^^^^^^^^^^^^^^^

are supported. The NDMP backup level will increment with each run, until a Full (0) or Differential (1) will be made. The maximum backup level will be 9. Additional Incremental backups will result in a failed job and the message:

.. code-block:: bareosmessage

    2016-01-21 13:35:51 bareos-dir JobId 12: Fatal error: NDMP dump format doesn't support more than 8 incrementals, please run a Differential or a Full Backup

NDMP Debugging
~~~~~~~~~~~~~~

To debug the NDMP backups, these settings can be adapted:

-

   :config:option:`dir/director/NdmpSnooping`\

-

   :config:option:`dir/director/NdmpLogLevel`\

-

   :config:option:`dir/client/NdmpLogLevel`\

-

   :config:option:`sd/storage/NdmpSnooping`\

-

   :config:option:`sd/storage/NdmpLogLevel`\

This will create a lot of debugging output that will help to find the problem during NDMP backups.

.. _section-NdmpCommonLimitations:

Bareos NDMP Common Limitations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

NDMP Fileset limitations
^^^^^^^^^^^^^^^^^^^^^^^^

.. limitation:: NDMP: A NDMP fileset should only contain a single File directive and Meta options.

   Using multiple :config:option:`dir/fileset/Include`\  :strong:`File`\  directives should be avoided.
   The |dir| would try to handle them by running multiple NDMP jobs in a single Bareos job.
   Even if this is working fine during backup, restore jobs will cause trouble.

   Normally (:config:option:`dir/client/Protocol`\ =Native) Filesets get handled by the \bareosFd. When connecting directly to a NDMP Clients (:config:option:`dir/client/Protocol`\ =NDMP*), no |fd| is involved and therefore most Fileset options can't be used. Instead, parameters are handled via :strong:`Options - Meta`\  from :config:option:`dir/fileset/Include`\ .




Single file restore on incremental backups
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. limitation:: NDMP: No single file restore on merged backups.

   Unfortunately, it is currently (bareos-15.2.2) not possible to restore a chain of Full and Incremental backups at once.
   The workaround for that problem is to restore the full backup and each incremental each in a single restore operation.




Temporary memory mapped database
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. limitation:: NDMP: 64-bit system recommended.

   The |dir| uses a memory mapped database (LMBD) to temporarily store NDMP file information.
   On some 32-bit systems the default :config:option:`dir/job/FileHistorySize`\  requires a larger memory area than available.
   In this case, you either have to lower the :config:option:`dir/job/FileHistorySize`\
   or preferably run the |dir| on a 64-bit system.




Tested Environments
~~~~~~~~~~~~~~~~~~~

Bareos NDMP support have been tested against:

.. csv-table::
   :header: Vendor, Product, "NDMP Subsystem", "Bareos version", "Tape Agent", Features, Remarks

   Isilon,     Isilon OneFS v7.2.1.4, Isilon NDMP 2.2.1   , bareos-17.2.3, Isilon Backup Accelerator, ,             Protocol: |ndmpnative|
   Isilon,     Isilon OneFS v7.2.0.1, Isilon NDMP 2.2     , bareos-16.2.6, |sd|
   Isilon,     Isilon OneFS v7.1.1.5, Isilon NDMP 2.2     , bareos-15.2.2, |sd|
   NetApp,                          , Release 8.2.3 7-Mode, bareos-15.2.2, |sd|
   Oracle/Sun, ZFS Storage Appliance, OS 8.3              , bareos-15.2.2, |sd|
