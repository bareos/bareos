.. _TutorialChapter:

Tutorial
========

.. index::
   single: Tutorial

This chapter will guide you through running Bareos. To do so, we assume you have installed
Bareos. However, we assume that you have not modified the configuration. The examples in
this chapter use the default configuration files and will write the volumes to disk in
your :file:`/var/lib/bareos/storage/` directory.

The general flow of running Bareos is:

#. Start the Database

#.

   :ref:`InstallChapter`

#. Start the Bareos Daemons

#. Start the Console program to interact with the |dir|

#. Run a job

#. Test recovering some files from the Volume just written to ensure the backup is good
   and that you know how to recover. Better test before disaster strikes

#. Add a second client.

Each of these steps is described in more detail below.

Installing Bareos
-----------------

For installing Bareos, follow the instructions from the :ref:`InstallChapter` chapter.

.. _StartDaemon:

Starting the Daemons
--------------------

.. index::
   single: Starting the Daemons
   single: Daemon; Start

Assuming you have installed the packages, to start the three daemons, from your
installation directory, simply enter:

.. code-block:: bconsole
   :caption: start services

   service bareos-dir start
   service bareos-sd start
   service bareos-fd start

With systemd enabled  it is even easier:

.. code-block:: bconsole
   :caption: systemctl start services

   systemctl start bareos-dir bareos-sd bareos-fd

Or enable and start services in one go

.. code-block:: bconsole
   :caption: systemctl enable --now services

   systemctl enable --now bareos-dir bareos-sd bareos-fd


.. _section-TutorialBconsole:

Using the Director to Query and Start Jobs
------------------------------------------

To communicate with the |dir| and to query the state of Bareos or run jobs, the :command:`bconsole`
program can be used as a textual interface. Alternatively, for most purposes, also the
:ref:`|webui| <section-webui>` can be used, but for simplicity, here we will describe
only the :command:`bconsole` program.

The :command:`bconsole` runs the Bareos Console program, which connects to the |dir|.
Since Bareos is a network program, you can run the Console program anywhere on your
network. Most frequently, however, one runs it on the same machine as the |dir|.
Normally, the Console program will print something similar to the following:

.. code-block:: shell-session
   :caption: bconsole

   root@host:~# bconsole
      Connecting to Director localhost:9101
       Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
      1000 OK: bareos-dir Version: 23.0.0 (23 September 2023)
      Bareos subscription release.
      Support available on https://www.bareos.com/support/
      You are connected using the default console
      Enter a period (.) to cancel a command.
      *

The asterisk is the console command prompt.

Type :bcommand:`help` to see a list of available commands:

.. code-block:: bconsole
   :caption: help

   *<input>help</input>
     Command            Description
     =======            ===========
     add                Add media to a pool
     autodisplay        Autodisplay console messages
     automount          Automount after label
     cancel             Cancel a job
     configure          Configure director resources
     create             Create DB Pool from resource
     delete             Delete volume, pool or job
     disable            Disable a job/client/schedule
     enable             Enable a job/client/schedule
     estimate           Performs FileSet estimate, listing gives full listing
     exit               Terminate Bconsole session
     export             Export volumes from normal slots to import/export slots
     gui                Switch between interactive (gui off) and non-interactive (gui on) mode
     help               Print help on specific command
     import             Import volumes from import/export slots to normal slots
     label              Label a tape
     list               List objects from catalog
     llist              Full or long list like list command
     messages           Display pending messages
     memory             Print current memory usage
     mount              Mount storage
     move               Move slots in an autochanger
     prune              Prune records from catalog
     purge              Purge records from catalog
     quit               Terminate Bconsole session
     query              Query catalog
     restore            Restore files
     relabel            Relabel a tape
     release            Release storage
     reload             Reload conf file
     rerun              Rerun a job
     resolve            Resolve a hostname
     run                Run a job
     status             Report status
     setbandwidth       Sets bandwidth
     setdebug           Sets debug level
     setdevice          Sets device parameter
     setip              Sets new client address -- if authorized
     show               Show resource records
     sqlquery           Use SQL to query catalog
     time               Print current time
     trace              Turn on/off trace to file
     truncate           Truncate purged volumes
     unmount            Unmount storage
     umount             Umount - for old-time Unix guys, see unmount
     update             Update volume, pool, slots, job or statistics
     use                Use specific catalog
     var                Does variable expansion
     version            Print Director version
     wait               Wait until no jobs are running
     whoami             Print the user name associated with this console

     When at a prompt, entering a period (.) cancels the command.

Details of the console program’s commands are explained in the :ref:`section-bconsole` chapter.

.. _Running:

Running a Job
-------------

.. index::
   single: Job; Running a
   single: Running a Job

At this point, we assume you have done the following:

-  Started the Database

-  Installed Bareos

-  Prepared the database for Bareos

-  Started Bareos Director, Storage Daemon and File Daemon

-  Invoked the Console program with :command:`bconsole`

Furthermore, we assume for the moment you are using the default configuration files.

At this point, enter the :bcommand:`show filesets` and you should get something similar this:

.. code-block:: bconsole
   :caption: show filesets

   *<input>show filesets</input>
   ...

   FileSet {
     Name = "SelfTest"
     Description = "fileset just to backup some files for selftest"
     Include {
       Options {
         Signature = "XXH128"
         HardLinks = No
         AclSupport = Yes
         XattrSupport = Yes
       }
       File = "/usr/sbin"
     }
   }


   FileSet {
     Name = "Catalog"
     Description = "Backup the catalog dump and Bareos configuration files."
     Include {
       Options {
         Signature = "XXH128"
         HardLinks = No
         AclSupport = Yes
         XattrSupport = Yes
       }
       File = "/var/lib/bareos/bareos.sql"
       File = "/etc/bareos"
     }
   }
   ...

One of the FileSets is the pre-defined :config:option:`dir/fileset = SelfTest`\  FileSet
that will backup the :file:`/usr/sbin` directory. For testing purposes, we have chosen a
directory of moderate size (about 30 Megabytes) and complexity without being too big.
The FileSet :config:option:`dir/fileset = Catalog`\  is used for backing up Bareos’s
catalog and is not of interest to us for the moment. You can change what is backed up
by editing the configuration and changing the :file:`File =` line in the :config:option:`Dir/FileSet`
resource.

Now is the time to run your first backup job. We are going to backup your Bareos source
directory to a File Volume in your :file:`/var/lib/bareos/storage/` directory just to
show you how easy it is.

Now enter:

.. code-block:: bconsole
   :caption: status dir

   *<input>status dir</input>
   bareos-dir Version: 23.0.0 (23 September 2023) GNU/Linux
   Daemon started 2023-09-23T08:59:05+0000. Jobs: run=0, running=0 db:postgresql,

   Scheduled Jobs:
   Level          Type     Pri  Scheduled          Name               Volume
   ===================================================================================
   Incremental    Backup    10  2023-09-23T21:00:00+0000 backup-bareos-fd   *unknown*
   Full           Backup    11  2023-09-23T21:10:00+0000 BackupCatalog      *unknown*
   ====

   Running Jobs:
   Console connected at 2023-09-23T08:59:09+0000
   No Jobs running.
   ====
   No Terminated Jobs.

   Client Initiated Connections (waiting for jobs):
   Connect time        Protocol            Authenticated       Name
   ===================================================================================
   ====

where the times and the Director’s name will be different according to your setup. This
shows that an Incremental job is scheduled to run for the Job
:config:option:`Dir/Job = BackupClient1`\ at 21:00 and that at 21:10,
a :config:option:`Dir/Job = BackupCatalog`\ is scheduled to run.

Now enter:

.. code-block:: bconsole
   :caption: status client

   *<input>status client</input>
   Automatically selected Client: bareos-fd
   Connecting to Client bareos-fd at localhost:9102
   Probing client protocol... (result will be saved until config reload)
    Handshake: Immediate TLS, Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3

   bareos-fd Version: 23.0.0 (23 September 2023) GNU/Linux
   Daemon started 2023-09-23T08:59:01+0000. Jobs: run=0 running=0, Bareos subscription binary
    Sizeof: boffset_t=8 size_t=8 debug=0 trace=0 bwlimit=0kB/s

   Running Jobs:
   bareos-dir (director) connected at: 2023-09-23T09:47:03+0000
   No Jobs running.
   ====

   Terminated Jobs:
   ====

In this case, the client is named :config:option:`Dir/Client = bareos-fd`\  your name
might be different, but the line beginning with :file:`bareos-fd Version` is printed by
your |fd|, so we are now sure it is up and running.

Finally do the same for your |sd| with:

.. code-block:: bconsole
   :caption: status storage

   *<input>status storage</input>
   Automatically selected Storage: File
   Connecting to Storage daemon File at bareos-sd:9103
    Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3

   bareos-sd Version: 23.0.0 (23 September 2023) GNU/Linux
   Daemon started 2023-09-23T08:59:03+0000. Jobs: run=0, running=0, Bareos subscription binary
    Sizes: boffset_t=8 size_t=8 int32_t=4 int64_t=8 bwlimit=0kB/s

   Job inventory:

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


You will notice that the default |sd| device is named :config:option:`Dir/Storage = File`\
and that it will use device :file:`/var/lib/bareos/storage`, which is not currently open.

Now, let’s actually run a job with:



.. code-block:: bconsole
   :caption: run

   <input>run</input>



you should get the following output:



.. code-block:: bconsole
   :caption: select job

   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"
   A job name must be specified.
   The defined Job resources are:
   1: backup-bareos-fd
   2: BackupCatalog
   3: RestoreFiles
   Select Job resource (1-3):


Here, Bareos has listed the three different Jobs that you can run, and you should choose
number :strong:`1` and type enter, at which point you will get:



.. code-block:: bconsole
   :caption: run job

   Run Backup job
   JobName:  backup-bareos-fd
   Level:    Incremental
   Client:   bareos-fd
   Format:   Native
   FileSet:  SelfTest
   Pool:     Incremental (From Job IncPool override)
   Storage:  File (From Job resource)
   When:     2023-09-23T09:48:54+0000
   Priority: 10
   OK to run? (yes/mod/no):



At this point, take some time to look carefully at what is printed and understand it.
It is asking you if it is OK to run a job named :config:option:`dir/job = BackupClient1`\
with FileSet :config:option:`dir/fileset = SelfTest`\ as an Incremental job on your
Client, and to use Storage :config:option:`Dir/Storage = File`\ and Pool
:config:option:`dir/pool = Incremental`\ ,and finally, it wants to run it now (the
current time should be displayed by your console).

Here we have the choice to run (:strong:`yes`), to modify one or more of the above
parameters (:strong:`mod`), or to not run the job (:strong:`no`). Please enter :strong:`yes`,
at which point you should immediately get a job queued confirmation and the associated
jobid and the command prompt (an asterisk).

.. code-block:: bconsole
   :caption: job queued

   Job queued. JobId=1
   You have messages.
   *

If you wait a few seconds, then enter the command :bcommand:`messages` you will get back
something like:


.. code-block:: bconsole
   :caption: joblog output

   *<input>messages</input>
   2023-09-23T09:50:04+0000 bareos-dir JobId 1: No prior Full backup Job record found.
   2023-09-23T09:50:04+0000 bareos-dir JobId 1: No prior or suitable Full backup found in catalog. Doing FULL backup.
   2023-09-23T09:50:06+0000 bareos-dir JobId 1: Start Backup JobId 1, Job=backup-bareos-fd.2023-09-23T09.50.04_03
   2023-09-23T09:50:06+0000 bareos-dir JobId 1: Connected Storage daemon at localhost:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:50:06+0000 bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:50:06+0000 bareos-dir JobId 1: Connected Client: bareos-fd at localhost:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:50:06+0000 bareos-dir JobId 1:  Handshake: Immediate TLS
   2023-09-23T09:50:06+0000 bareos-dir JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:50:06+0000 bareos-dir JobId 1: Created new Volume "Full-0001" in catalog.
   2023-09-23T09:50:06+0000 bareos-dir JobId 1: Using Device "FileStorage" to write.
   2023-09-23T09:50:06+0000 bareos-fd  JobId 1: Connected Storage daemon at localhost:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:50:06+0000 bareos-fd  JobId 1:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:50:06+0000 bareos-fd  JobId 1: Extended attribute support is enabled
   2023-09-23T09:50:06+0000 bareos-fd  JobId 1: ACL support is enabled
   2023-09-23T09:50:06+0000 bareos-sd  JobId 1: Labeled new Volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
   2023-09-23T09:50:06+0000 bareos-sd  JobId 1: Wrote label to prelabeled Volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage)
   2023-09-23T09:50:07+0000 bareos-sd  JobId 1: Releasing device "FileStorage" (/var/lib/bareos/storage).
   2023-09-23T09:50:07+0000 bareos-sd  JobId 1: Elapsed time=00:00:01, Transfer rate=62.68 M Bytes/second
   2023-09-23T09:50:07+0000 bareos-dir JobId 1: Insert of attributes batch table with 173 entries start
   2023-09-23T09:50:07+0000 bareos-dir JobId 1: Insert of attributes batch table done
   2023-09-23T09:50:07+0000 bareos-dir JobId 1: Bareos bareos-dir 23.0.0 (23Sep23):
     Build OS:               GNU/Linux
     JobId:                  1
     Job:                    backup-bareos-fd.2023-09-23T09.50.04_03
     Backup Level:           Full (upgraded from Incremental)
     Client:                 "bareos-fd" 23.0.0 (23Sep23) GNU/Linux
     FileSet:                "SelfTest" 2023-09-23T09:50:04+0000
     Pool:                   "Full" (From Job FullPool override)
     Catalog:                "MyCatalog" (From Client resource)
     Storage:                "File" (From Job resource)
     Scheduled time:         2023-09-23T09:48:54+0000
     Start time:             2023-09-23T09:50:06+0000
     End time:               2023-09-23T09:50:07+0000
     Elapsed time:           1 sec
     Priority:               10
     FD Files Written:       173
     SD Files Written:       173
     FD Bytes Written:       62,668,227 (62.66 MB)
     SD Bytes Written:       62,685,875 (62.68 MB)
     Rate:                   62668.2 KB/s
     Software Compression:   None
     VSS:                    no
     Encryption:             no
     Accurate:               no
     Volume name(s):         Full-0001
     Volume Session Id:      1
     Volume Session Time:    1695718743
     Last Volume Bytes:      62,706,904 (62.70 MB)
     Non-fatal FD errors:    0
     SD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos subscription release
     Job triggered by:       User
     Termination:            Backup OK

If you don’t see the output immediately, you can keep entering :bcommand:`messages` until
the job terminates.

Instead of typing :bcommand:`messages` multiple times, you can also ask bconsole to wait,
until a specific job is finished:

.. code-block:: bconsole
   :caption: wait

   *<input>wait jobid=1</input>

or just :bcommand:`wait`, which waits for all running jobs to finish.

Another useful command is :bcommand:`autodisplay on`. With autodisplay activated,
messages will automatically be displayed as soon as they are ready.

If you do an :command:`ls -l` of your :file:`/var/lib/bareos/storage` directory, you will
see that you have the following item


.. code-block:: bconsole
   :caption: ls -l /var/lib/bareos/storage

   total 61240
   -rw-r-----. 1 bareos bareos 62706904 Sep 23 09:50 Full-0001


This is the file Volume that you just wrote and it contains all the data of the job just
run. If you run additional jobs, they will be appended to this Volume unless you specify
otherwise.

If you would like to stop here, you can simply enter :bcommand:`quit` in the Console program.

If you would like to try restoring the files that you just backed up, read the following section.

.. _restoring:



Restoring Your Files
--------------------

.. index::
   single: Files; Restoring Your
   single: Restoring Your Files

If you have run the default configuration and run the job as demonstrated above, you can
restore the backed up files in the Console program by entering:

.. code-block:: bconsole
   :caption: restore

   *<input>restore all</input>
   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"

   First you select one or more JobIds that contain files
   to be restored. You will be presented several methods
   of specifying the JobIds. Then you will be allowed to
   select which files from those JobIds are to be restored.

   To select the JobIds, you have the following choices:
   1: List last 20 Jobs run
   2: List Jobs where a given File is saved
   3: Enter list of comma separated JobIds to select
   4: Enter SQL list command
   5: Select the most recent backup for a client
   6: Select backup for a client before a specified time
   7: Enter a list of files to restore
   8: Enter a list of files to restore before a specified time
   9: Find the JobIds of the most recent backup for a client
   10: Find the JobIds for a backup for a client before a specified time
   11: Enter a list of directories to restore for found JobIds
   12: Select full restore to a specified Job date
   13: Cancel
   Select item:  (1-13):

As you can see, there are a number of options, but for the current demonstration, please
enter :strong:`5` to do a restore of the last backup you did, and you will get the following output:



.. code-block:: bconsole
   :caption: select resource

   Select item:  (1-13): 5
   Automatically selected Client: bareos-fd
   The defined FileSet resources are:
   1: Catalog
   2: SelfTest
   Select FileSet resource (1-2):


As you can see, Bareos knows what client you have, and since there was only one, it
selected it automatically. Select :strong:`2`, because you want to restore files from
that fileset.



.. code-block:: bconsole
   :caption: restore filesystem

   Select FileSet resource (1-2): 2
   +-------+-------+----------+------------+------------------------+------------+
   | jobid | level | jobfiles | jobbytes   | starttime              | volumename |
   +-------+-------+----------+------------+------------------------+------------+
   |     1 | F     |      173 | 62,668,227 | 2023-09-23 09:50:06+00 | Full-0001  |
   +-------+-------+----------+------------+------------------------+------------+
   You have selected the following JobId: 1

   Building directory tree for JobId(s) 1 ...  +++++++++++++++++++++++++++++++++++++++
   89 files inserted into the tree.

   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.

   cwd is: /
   $


where I have truncated the listing on the right side to make it more readable.

Then Bareos produced a listing containing all the jobs that form the current backup,
in this case, there is only one, and the Storage daemon was also automatically chosen.
Bareos then took all the files that were in Job number 1 and entered them into a directory
tree (a sort of in memory representation of your filesystem). At this point, you can use
the :bcommand:`cd` and :bcommand:`ls` or :bcommand:`dir` commands to walk up and down
the directory tree and view what files will be restored.
For example, if you enter :bcommand:`cd /usr/sbin` and then enter :bcommand:`dir` you
will get a listing of all the files in the :file:`/usr/sbin/` directory. On your system,
the path might be somewhat different.

For more information on this, please refer to the :ref:`Restore Command Chapter <RestoreChapter>`
of this manual for more details.

To exit this mode and cancel the restore, simply enter: :strong:`.`

To select all files to be restored you enter:

.. code-block:: bconsole
   :caption: mark all

   cwd is: /
   $ mark *
   173 files marked.
   $


To exit the selection mode and continue restore operation enter:



.. code-block:: bconsole
   :caption: done

   done



and you will get the following output:



.. code-block:: bconsole
   :caption: restore preview

   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.2.bsr

   The job will require the following
      Volume(s)                 Storage(s)                SD Device(s)
   ===========================================================================

      Full-0001                 File                      FileStorage

   Volumes marked with "*" are online.


   173 files selected to be restored.

   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.1.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         LinuxAll
   Backup Client:   bareos-fd
   Restore Client:  bareos-fd
   Format:          Native
   Storage:         File
   When:            2023-09-23T09:55:09+0000
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no): yes



If you answer :strong:`yes` your files will be restored to :file:`/tmp/bareos-restores`.
If you want to restore the files to their original locations, you must use the mod option
and explicitly set `Where:` to nothing (or to `/`). We recommend you go ahead and answer
yes and after a brief moment, enter :bcommand:`messages`, at which point you should get
a listing of all the files that were restored as well as a summary of the job that looks
similar to this:



.. code-block:: bconsole
   :caption: restore job report


   2023-09-23T09:57:34+0000 bareos-dir JobId 3: Start Restore Job RestoreFiles.2023-09-23T09.57.32_06
   2023-09-23T09:57:34+0000 bareos-dir JobId 3: Connected Storage daemon at localhost:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:57:34+0000 bareos-dir JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:57:35+0000 bareos-dir JobId 3: Using Device "FileStorage" to read.
   2023-09-23T09:57:35+0000 bareos-dir JobId 3: Connected Client: bareos-fd at localhost:9102, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:57:35+0000 bareos-dir JobId 3:  Handshake: Immediate TLS
   2023-09-23T09:57:35+0000 bareos-dir JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:57:37+0000 bareos-fd  JobId 3: Connected Storage daemon at localhost:9103, encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:57:37+0000 bareos-fd  JobId 3:  Encryption: TLS_CHACHA20_POLY1305_SHA256 TLSv1.3
   2023-09-23T09:57:37+0000 bareos-sd  JobId 3: Ready to read from volume "Full-0001" on device "FileStorage" (/var/lib/bareos/storage).
   2023-09-23T09:57:37+0000 bareos-sd  JobId 3: Forward spacing Volume "Full-0001" to file:block 0:217.
   2023-09-23T09:57:37+0000 bareos-sd  JobId 3: Releasing device "FileStorage" (/var/lib/bareos/storage).
   2023-09-23T09:57:37+0000 bareos-dir JobId 4: Bareos bareos-dir 23.0.0 (23Sep23):
     Build OS:               GNU/Linux
     JobId:                  4
     Job:                    RestoreFiles.2023-09-23T09.57.32_06
     Restore Client:         "bareos-fd" 23.0.0 (23Sep23) GNU/Linux
     Start time:             2023-09-23T09:57:34+0000
     End time:               2023-09-23T09:57:37+0000
     Elapsed time:           3 secs
     Files Expected:         173
     Files Restored:         173
     Bytes Restored:         62,654,733
     Rate:                   20884.9 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Bareos binary info:     Bareos subscription release
     Job triggered by:       User
     Termination:            Restore OK


After exiting the Console program, you can examine the files in :file:`/tmp/bareos-restores`,
which will contain a small directory tree with all the files.

Be sure to clean up at the end with:

.. code-block:: shell-session
   :caption: remove restore directory

   root@host:~# rm -rf /tmp/bareos-restore

Quitting the Console Program
----------------------------

.. index::
   single: Program; Quitting the Console
   single: Quitting the Console Program

Simply enter the command :bcommand:`quit`.

.. _SecondClient:

.. _section-AddAClient:

Adding a Client
---------------

.. index::
   single: Client; Adding a Second
   single: Adding a Client

If you have gotten the example shown above to work on your system, you may be ready to
add a second Client (|fd|). That is you have a second machine that you would like backed up.
Lets assume, following settings about the machine you want to add to your backup environment:

Hostname (FQDN)
   :strong:`client2.example.com`

IP Address
   192.168.0.2

OS
   Linux (otherwise the paths may differ)

For this you have to make changes on the server side (|dir|) and the client side.

Client: install package
~~~~~~~~~~~~~~~~~~~~~~~

See :ref:`InstallChapter` about how to add the Bareos repository. The only part you need
installed on the other machine is the :strong:`bareos-filedaemon`.

Director: configure client
~~~~~~~~~~~~~~~~~~~~~~~~~~

Bareos :sinceVersion:`16.2.4: configure add` offers the :ref:`configure add command <section-bcommandConfigure>` to add resources to the |dir|.

Start the :command:`bconsole` and use the :bcommand:`configure add client` command.
Address must be a DNS resolvable name or an IP address.

.. code-block:: bconsole
   :caption: add a client

   *<input>configure add client name=client2-fd address=192.168.0.2 password=secret</input>
   Created resource config file "/etc/bareos/bareos-dir.d/client/client2-fd.conf":
   Client {
     Name = client2-fd
     Address = 192.168.0.2
     Password = secret
   }

This creates two resource configuration files:

-

   :file:`/etc/bareos/bareos-dir.d/client/client2-fd.conf`

-  :file:`/etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf` (assuming your director resource is named :strong:`bareos-dir`)

The :file:`/etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf` is the required resource needed on the |fd|. You can copy it to the destination:

.. code-block:: shell-session
   :caption: Copy the bareos-fd director resource to the new client

   scp /etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf root@client2.example.com:/etc/bareos/bareos-fd.d/director/

Manual configuration of client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Alternatively you can configure your resources manually. On the |dir| create the file

.. code-block:: bareosconfig
   :caption: bareos-dir.d/client/client2-fd.conf

   Client {
     Name = client2-fd
     Address = 192.168.0.2
     Password = secret
   }

Reload or restart your |dir|:

.. code-block:: bconsole
   :caption: reload the Director configuration

   *<input>reload</input>
   reloaded

The corresponding |fd| director resource can be created directly on the client, see below.

.. note::

   You should also consider using Passive mode for the the client,
   see :ref:`section-PassiveClient`, especially if you are backing up clients that are
   not part of your local network.


Client: configure
~~~~~~~~~~~~~~~~~

The package :strong:`bareos-filedaemon` :sinceVersion:`16.2.4: Client resource files`
brings several configuration files:

-

   :file:`/etc/bareos/bareos-fd.d/client/myself.conf`

-

   :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`

-

   :file:`/etc/bareos/bareos-fd.d/director/bareos-mon.conf`

-

   :file:`/etc/bareos/bareos-fd.d/messages/Standard.conf`

In detail:

:file:`client/myself.conf`
   defines the name of the client. The default is :file:`<hostname>-fd`. Changes are only
   required, if you want to use another name or en- or disable special |fd| features.
   See :ref:`ClientResourceClient`.

:file:`director/bareos-dir.conf`
   gives the |dir| :strong:`bareos-dir` full access to this |fd|. During installation, the
   :config:option:`fd/director/Password`\ is set to a random default. Adapt the name and/or
   the password to your |dir|. (The name :strong:`bareos-dir` is the default |dir| name since
   Bareos :sinceVersion:`16.2.4: bareos-dir is the default |dir| name`.)

:file:`director/bareos-mon.conf`
   gives the |dir| :strong:`bareos-mon` restricted access to this |fd|. During installation,
   the :config:option:`fd/director/Password`\ is set to a random value. This resource is
   intended to be used by the local :strong:`bareos-tray-monitor`.

:file:`messages/Standard.conf`
   defines, how messages should be handled. The default sends all relevant messages to the |dir|.

If your |dir| is named :strong:`bareos-dir`, the :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`
may already be overwritten by the file you copied from the |dir|. If your Director has
another name, an addition resource file will exists. You can define an arbitrary number
of |dir|’s in your |fd| configuration. However, normally you will only have one :config:option:`Fd/Director`\
with full control of your |fd| and optional one :config:option:`Fd/Director`\ for monitoring
(used by the |traymonitor|).

Anyhow, the resource will look similar to this:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
     Name = bareos-dir
     Password = "[md5]5ebe2294ecd0e0f08eab7690d2a6ee69"
   }

After a restart of the |fd| to reload the configuration this resource allows the access
for a |dir| with name :strong:`bareos-dir` and password :strong:`secret` (stored in MD5 format).

.. code-block:: shell-session
   :caption: restart bareos-fd

   service bareos-fd restart


Manual configuration
^^^^^^^^^^^^^^^^^^^^

If you have not created the :config:option:`Fd/Director`\ by :bcommand:`configure`, you
can create it also manually. If your |dir| is also named :strong:`bareos-dir`, modify or
create the file :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
     Name = "bareos-dir"   # Name of your Bareos Director
     Password = "secret"   # Password (cleartext or MD5) must be identical
                           # to the password of your client reosurce in the Direcotr
                           # (bareos-dir.d/client/client2-fd.conf)
   }

See the relation between resource names and password of the different Bareos components
in :ref:`section-resource-relation`.

If your are not using the :ref:`section-SubdirectoryConfigurationScheme`, make sure that
this resource file gets included in your |fd| configuration. You can verify this by

.. code-block:: shell-session
   :caption: show how bareos-fd would read the current configuration files

   bareos-fd --export-config

After modifying the file, you have to restart the |fd|:

.. code-block:: shell-session
   :caption: restart bareos-fd

   service bareos-fd restart

.. note::

   You should also consider using Passive mode for the the client, see :ref:`section-PassiveClient`,
   especially if you are backing up clients that are not part of your local network.


Director: test client, add a job
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following example show how to

-  Verify the network connection from |dir| to the |fd|.

-  Add a job resource.

-  Dry-run the job (:bcommand:`estimate listing`).

-  Run the job.

-  Wait for the job to finish.

-  Verify the job.

.. code-block:: bconsole
   :caption: test the client and add a job resource

   *<input>status client=client2-fd</input>
   ...
   *<input>configure add job name=client2-job client=client2-fd jobdefs=DefaultJob</input>
   Created resource config file "/etc/bareos/bareos-dir.d/job/client2-job.conf":
   Job {
     Name = client2-job
     Client = client2-fd
     JobDefs = DefaultJob
   }
   *<input>estimate listing job=client2-job</input>
   ...
   *<input>run job=client2-job</input>
   ...
   *<input>wait jobid=...</input>
   ...
   *<input>list joblog jobid=...</input>
   ...
   *<input>list files jobid=...</input>
   ...
   *<input>list volumes</input>
   ...



Patience When Starting Daemons or Mounting Blank Tapes
------------------------------------------------------

When you start the Bareos daemons, the Storage daemon attempts to open all defined storage
devices and verify the currently mounted Volume (if configured). Until all the storage
devices are verified, the Storage daemon will not accept connections from the Console
program. If a tape was previously used, it will be rewound, and on some devices this can
take several minutes. As a consequence, you may need to have a bit of patience when first
contacting the Storage daemon after starting the daemons.
If you can see your tape drive, once the lights stop flashing, the drive will be ready to
be used.

The same considerations apply if you have just mounted a blank tape in a drive. It can
take a minute or two before the drive properly recognizes that the tape is blank. If you
attempt to :bcommand:`mount` the tape with the Console program during this recognition
period, it is quite possible that you will hang your SCSI driver. As a consequence, you
are again urged to have patience when inserting blank tapes. Let the device settle down
before attempting to access it.

.. _Pool:

Pools
-----

.. index::
   single: Pool; Overview

Creating the Pool is automatically done when the |dir| starts, so if you understand Pools,
you can skip to the next section.

When you run a backup job, one of the things that Bareos must know is what Volumes to use.
Instead of specifying a Volume (tape) directly, you specify which Pool of Volumes you want
Bareos to consult when it wants a Volume for writing backups. Bareos will select the first
available Volume from the Pool that is appropriate for the :config:option:`dir/job/Storage`\
you have specified for the Job being run. When a volume has filled up with data, Bareos
will change its :strong:`VolStatus` from :strong:`Append` to :strong:`Full`, and then
Bareos will use the next volume and so on. If no appendable Volume exists in the Pool,
the Director will attempt to recycle an old Volume. For details, please read the
:ref:`RecyclingChapter` chapter.

If there are still no appendable Volumes available, Bareos will send a message requesting
the operator to create an appropriate Volume.

Bareos keeps track of the Pool name, the volumes contained in the Pool, and a number of
attributes of each of those Volumes.

When Bareos starts, it ensures that all Pool resource definitions have been recorded in
the catalog. You can verify this by entering:

.. code-block:: bconsole
   :caption: list pools

   *<input>list pools</input>
   +--------+--------------+---------+---------+----------+---------------+
   | PoolId | Name         | NumVols | MaxVols | PoolType | LabelFormat   |
   +--------+--------------+---------+---------+----------+---------------+
   | 1      | Full         | 1       | 100     | Backup   | Full-         |
   | 2      | Differential | 0       | 100     | Backup   | Differential- |
   | 3      | Incremental  | 1       | 100     | Backup   | Incremental-  |
   | 4      | Scratch      | 0       | 0       | Backup   | *             |
   +--------+--------------+---------+---------+----------+---------------+

Other Useful Console Commands
-----------------------------

.. index::
   single: Console; Commands; Useful

help
   Show the list all all available commands.

help list
   Show detail information about a specific command, in this case the command :bcommand:`list`.

.. index::
   single: Console; Command; status dir

status dir
   Print a status of all running jobs and jobs scheduled in the next 24 hours.

.. index::
   single: Console; Command; status

status
   The console program will prompt you to select a daemon type, then will request the daemon’s status.

.. index::
   single: Console; Command; status jobid

status jobid=nn
   Print a status of JobId nn if it is running. The Storage daemon is contacted and
   requested to print a current status of the job as well.

.. index::
   single: Console; Command; list pools

list pools
   List the pools defined in the Catalog (normally only Default is used).

.. index::
   single: Console; Command; list volumes

list volumes
   Lists all the media defined in the Catalog.

.. index::
   single: Console; Command; list jobs

list jobs
   Lists all jobs in the Catalog that have run.

.. index::
   single: Console; Command; list jobid

list jobid=nn
   Lists JobId nn from the Catalog.

.. index::
   single: Console; Command; list jobtotals

list jobtotals
   Lists totals for all jobs in the Catalog.

.. index::
   single: Console; Command; list files jobid

list files jobid=nn
   List the files that were saved for JobId nn.

.. index::
   single: Console; Command; list jobmedia

list jobmedia
   List the media information for each Job run.

.. index::
   single: Console; Command; messages

messages
   Prints any messages that have been directed to the console.

.. index::
   single: Console; Command; quit

quit
   Exit or quit the console program.

Most of the commands given above, with the exception of list, will prompt you for the
necessary arguments if you simply enter the command name.

The full list of commands is shown in the chapter :ref:`section-ConsoleCommands`.
