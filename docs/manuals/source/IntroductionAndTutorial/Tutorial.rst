.. _TutorialChapter:

Tutorial
========

:index:`\ <single: Tutorial>`\ 

This chapter will guide you through running Bareos. To do so, we assume you have installed Bareos. However, we assume that you have not modified the configuration. The examples in this chapter use the default configuration files and will write the volumes to disk in your :file:`/var/lib/bareos/storage/` directory.

The general flow of running Bareos is:

#. Start the Database (if using |postgresql| or |mysql|)

#. 

   :ref:`InstallChapter`

#. Start the Bareos Daemons

#. Start the Console program to interact with the |dir|

#. Run a job

#. Test recovering some files from the Volume just written to ensure the backup is good and that you know how to recover. Better test before disaster strikes

#. Add a second client.

Each of these steps is described in more detail below.

Starting the Database
---------------------

If you are using |postgresql| or |mysql| as the Bareos database, you should start it before you install Bareos. If you are using |sqlite| you need do nothing. |sqlite| is automatically started by the |dir|.

Installing Bareos
-----------------

For installing Bareos, follow the instructions from the :ref:`InstallChapter` chapter.

.. _StartDaemon:

Starting the Daemons
--------------------

:index:`\ <single: Starting the Daemons>`\  :index:`\ <single: Daemon; Start>`\ 

Assuming you have installed the packages, to start the three daemons, from your installation directory, simply enter:

.. code-block:: bconsole
   :caption: start services

   service bareos-dir start
   service bareos-sd start
   service bareos-fd start

.. _section-TuturialBconsole:

Using the Director to Query and Start Jobs
------------------------------------------

To communicate with the |dir| and to query the state of Bareos or run jobs, the :command:`bconsole` program can be used as a textual interface. Alternatively, for most purposes, also the :ref:`|webui| <section-webui>` can be used, but for simplicity, here we will describe only the :command:`bconsole` program.

The :command:`bconsole` runs the Bareos Console program, which connects to the |dir|. Since Bareos is a network program, you can run the Console program anywhere on your network. Most frequently, however, one runs it on the same machine as the |dir|. Normally, the Console program will print something similar to the following:

.. code-block:: shell-session
   :caption: bconsole

   root@host:~# bconsole
   Connecting to Director bareos:9101
   Enter a period to cancel a command.
   *

The asterisk is the console command prompt.

Type :bcommand:`help` to see a list of available commands:

.. code-block:: bconsole
   :caption: help

   *<input>help</input>
     Command       Description
     =======       ===========
     add           Add media to a pool
     autodisplay   Autodisplay console messages
     automount     Automount after label
     cancel        Cancel a job
     create        Create DB Pool from resource
     delete        Delete volume, pool or job
     disable       Disable a job
     enable        Enable a job
     estimate      Performs FileSet estimate, listing gives full listing
     exit          Terminate Bconsole session
     export        Export volumes from normal slots to import/export slots
     gui           Non-interactive gui mode
     help          Print help on specific command
     import        Import volumes from import/export slots to normal slots
     label         Label a tape
     list          List objects from catalog
     llist         Full or long list like list command
     messages      Display pending messages
     memory        Print current memory usage
     mount         Mount storage
     move          Move slots in an autochanger
     prune         Prune expired records from catalog
     purge         Purge records from catalog
     quit          Terminate Bconsole session
     query         Query catalog
     restore       Restore files
     relabel       Relabel a tape
     release       Release storage
     reload        Reload conf file
     rerun         Rerun a job
     run           Run a job
     status        Report status
     setbandwidth  Sets bandwidth
     setdebug      Sets debug level
     setip         Sets new client address -- if authorized
     show          Show resource records
     sqlquery      Use SQL to query catalog
     time          Print current time
     trace         Turn on/off trace to file
     unmount       Unmount storage
     umount        Umount - for old-time Unix guys, see unmount
     update        Update volume, pool or stats
     use           Use specific catalog
     var           Does variable expansion
     version       Print Director version
     wait          Wait until no jobs are running

Details of the console program’s commands are explained in the :ref:`section-bconsole` chapter.

.. _Running:

Running a Job
-------------

:index:`\ <single: Job; Running a>`\  :index:`\ <single: Running a Job>`\ 

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
     Include {
       Options {
         Signature = MD5
       }
       File = "/usr/sbin"
     }
   }

   FileSet {
     Name = "Catalog"
     Include {
       Options {
         Signature = MD5
       }
       File = "/var/lib/bareos/bareos.sql"
       File = "/etc/bareos"
     }
   }
   ...

One of the FileSets is the pre-defined :config:option:`dir/fileset = SelfTest`\  FileSet that will backup the :file:`/usr/sbin` directory. For testing purposes, we have chosen a directory of moderate size (about 30 Megabytes) and complexity without being too big. The FileSet :config:option:`dir/fileset = Catalog`\  is used for backing up Bareos’s catalog and is not of interest to us for the moment. You can change what is backed up by editing the configuration and changing the :file:`File =` line in the
:config:option:`Dir/FileSet`\  resource.

Now is the time to run your first backup job. We are going to backup your Bareos source directory to a File Volume in your :file:`/var/lib/bareos/storage/` directory just to show you how easy it is. Now enter:

.. code-block:: bconsole
   :caption: status dir

   *<input>status dir</input>
   bareos-dir Version: 13.2.0 (09 April 2013) x86_64-pc-linux-gnu debian Debian GNU/Linux 6.0 (squeeze)
   Daemon started 23-May-13 13:17. Jobs: run=0, running=0 mode=0
    Heap: heap=270,336 smbytes=59,285 max_bytes=59,285 bufs=239 max_bufs=239

   Scheduled Jobs:
   Level          Type     Pri  Scheduled          Name               Volume
   ===================================================================================
   Incremental    Backup    10  23-May-13 23:05    BackupClient1      testvol
   Full           Backup    11  23-May-13 23:10    BackupCatalog      testvol
   ====

   Running Jobs:
   Console connected at 23-May-13 13:34
   No Jobs running.
   ====

where the times and the Director’s name will be different according to your setup. This shows that an Incremental job is scheduled to run for the Job :config:option:`Dir/Job = BackupClient1`\  at 1:05am and that at 1:10, a :config:option:`Dir/Job = BackupCatalog`\  is scheduled to run.

Now enter:

.. code-block:: bconsole
   :caption: status client

   *<input>status client</input>
   Automatically selected Client: bareos-fd
   Connecting to Client bareos-fd at bareos:9102

   bareos-fd Version: 13.2.0 (09 April 2013)  x86_64-pc-linux-gnu debian Debian GNU/Linux 6.0 (squeeze)
   Daemon started 23-May-13 13:17. Jobs: run=0 running=0.
    Heap: heap=135,168 smbytes=26,000 max_bytes=26,147 bufs=65 max_bufs=66
    Sizeof: boffset_t=8 size_t=8 debug=0 trace=0 bwlimit=0kB/s

   Running Jobs:
   Director connected at: 23-May-13 13:58
   No Jobs running.
   ====

In this case, the client is named :config:option:`Dir/Client = bareos-fd`\  your name might be different, but the line beginning with :file:`bareos-fd Version` is printed by your |fd|, so we are now sure it is up and running.

Finally do the same for your |sd| with:

.. code-block:: bconsole
   :caption: status storage

   *<input>status storage</input>
   Automatically selected Storage: File
   Connecting to Storage daemon File at bareos:9103

   bareos-sd Version: 13.2.0 (09 April 2013) x86_64-pc-linux-gnu debian Debian GNU/Linux 6.0 (squeeze)
   Daemon started 23-May-13 13:17. Jobs: run=0, running=0.
    Heap: heap=241,664 smbytes=28,574 max_bytes=88,969 bufs=73 max_bufs=74
    Sizes: boffset_t=8 size_t=8 int32_t=4 int64_t=8 mode=0 bwlimit=0kB/s

   Running Jobs:
   No Jobs running.
   ====

   Device status:

   Device "FileStorage" (/var/lib/bareos/storage) is not open.
   ==
   ====

   Used Volume status:
   ====

   ====

You will notice that the default |sd| device is named :config:option:`Dir/Storage = File`\  and that it will use device :file:`/var/lib/bareos/storage`, which is not currently open.

Now, let’s actually run a job with:



.. code-block:: bconsole
   :caption: run

   run



you should get the following output:



.. code-block:: bconsole
   :caption: select job

   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"
   A job name must be specified.
   The defined Job resources are:
        1: BackupClient1
        2: BackupCatalog
        3: RestoreFiles
   Select Job resource (1-3):



Here, Bareos has listed the three different Jobs that you can run, and you should choose number 1 and type enter, at which point you will get:



.. code-block:: bconsole
   :caption: run job

   Run Backup job
   JobName:  BackupClient1
   Level:    Incremental
   Client:   bareos-fd
   Format:   Native
   FileSet:  SelfTest
   Pool:     Full (From Job resource)
   NextPool: *None* (From unknown source)
   Storage:  File (From Job resource)
   When:     2013-05-23 14:50:04
   Priority: 10
   OK to run? (yes/mod/no):



At this point, take some time to look carefully at what is printed and understand it. It is asking you if it is OK to run a job named :config:option:`dir/job = BackupClient1`\  with FileSet :config:option:`dir/fileset = SelfTest`\  as an Incremental job on your Client, and to use Storage :config:option:`Dir/Storage = File`\  and Pool :config:option:`dir/pool = Full`\ , and finally, it wants to run it now (the current time should be displayed by your console).

Here we have the choice to run (yes), to modify one or more of the above parameters (mod), or to not run the job (no). Please enter yes, at which point you should immediately get the command prompt (an asterisk).

If you wait a few seconds, then enter the command :bcommand:`messages` you will get back something like:

.. TODO: Replace bconsole output by current version of Bareos.

.. code-block:: bconsole
   :caption: run

   *<input>messages</input>
   28-Apr-2003 14:30 bareos-sd: Wrote label to prelabeled Volume
      "TestVolume001" on device /var/lib/bareos/storage
   28-Apr-2003 14:30 rufus-dir: Bareos 1.30 (28Apr03): 28-Apr-2003 14:30
   JobId:                  1
   Job:                    BackupClient1.2003-04-28_14.22.33
   FileSet:                Full Set
   Backup Level:           Full
   Client:                 bareos-fd
   Start time:             28-Apr-2003 14:22
   End time:               28-Apr-2003 14:30
   Files Written:          1,444
   Bytes Written:          38,988,877
   Rate:                   81.2 KB/s
   Software Compression:   None
   Volume names(s):        TestVolume001
   Volume Session Id:      1
   Volume Session Time:    1051531381
   Last Volume Bytes:      39,072,359
   FD termination status:  OK
   SD termination status:  OK
   Termination:            Backup OK
   28-Apr-2003 14:30 rufus-dir: Begin pruning Jobs.
   28-Apr-2003 14:30 rufus-dir: No Jobs found to prune.
   28-Apr-2003 14:30 rufus-dir: Begin pruning Files.
   28-Apr-2003 14:30 rufus-dir: No Files found to prune.
   28-Apr-2003 14:30 rufus-dir: End auto prune.

If you don’t see the output immediately, you can keep entering :bcommand:`messages` until the job terminates.

Instead of typing :bcommand:`messages` multiple times, you can also ask bconsole to wait, until a specific job is finished:

.. code-block:: bconsole
   :caption: wait

   *<input>wait jobid=1</input>

or just :bcommand:`wait`, which waits for all running jobs to finish.

Another useful command is :bcommand:`autodisplay on`. With autodisplay activated, messages will automatically be displayed as soon as they are ready.

If you do an :command:`ls -l` of your :file:`/var/lib/bareos/storage` directory, you will see that you have the following item:



.. code-block:: bconsole
   :caption: volume

   -rw-r-----    1 bareos bareos   39072153 Apr 28 14:30 Full-001



This is the file Volume that you just wrote and it contains all the data of the job just run. If you run additional jobs, they will be appended to this Volume unless you specify otherwise.

If you would like to stop here, you can simply enter :bcommand:`quit` in the Console program.

If you would like to try restoring the files that you just backed up, read the following section. 

.. _restoring:



Restoring Your Files
--------------------

:index:`\ <single: Files; Restoring Your>`\  :index:`\ <single: Restoring Your Files>`\ 

If you have run the default configuration and run the job as demonstrated above, you can restore the backed up files in the Console program by entering:

.. code-block:: bconsole
   :caption: restore

   *<input>restore all</input>
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

As you can see, there are a number of options, but for the current demonstration, please enter 5 to do a restore of the last backup you did, and you will get the following output:



.. code-block:: bconsole
   :caption: select resource

   Automatically selected Client: bareos-fd
   The defined FileSet resources are:
        1: Catalog
        2: Full Set
   Select FileSet resource (1-2): 



As you can see, Bareos knows what client you have, and since there was only one, it selected it automatically. Select 2, because you want to restore files from the file set.



.. code-block:: bconsole
   :caption: restore filesystem

   +-------+-------+----------+------------+---------------------+---------------+
   | jobid | level | jobfiles | jobbytes   | starttime           | volumename    |
   +-------+-------+----------+------------+---------------------+---------------+
   |     1 | F     |      166 | 19,069,526 | 2013-05-05 23:05:02 | TestVolume001 |
   +-------+-------+----------+------------+---------------------+---------------+
   You have selected the following JobIds: 1

   Building directory tree for JobId(s) 1 ...  +++++++++++++++++++++++++++++++++++++++++
   165 files inserted into the tree and marked for extraction.

   You are now entering file selection mode where you add (mark) and
   remove (unmark) files to be restored. No files are initially added, unless
   you used the "all" keyword on the command line.
   Enter "done" to leave this mode.

   cwd is: /
   $ 



where I have truncated the listing on the right side to make it more readable.

Then Bareos produced a listing containing all the jobs that form the current backup, in this case, there is only one, and the Storage daemon was also automatically chosen. Bareos then took all the files that were in Job number 1 and entered them into a directory tree (a sort of in memory representation of your filesystem). At this point, you can use the :bcommand:`cd` and :bcommand:`ls` or :bcommand:`dir` commands to walk up and down the directory tree
and view what files will be restored. For example, if you enter :bcommand:`cd /usr/sbin` and then enter :bcommand:`dir` you will get a listing of all the files in the :file:`/usr/sbin/` directory. On your system, the path might be somewhat different. For more information on this, please refer to the :ref:`Restore Command Chapter <RestoreChapter>` of this manual for more details.

To exit this mode, simply enter:



.. code-block:: bconsole
   :caption: done

   done



and you will get the following output:



.. code-block:: bconsole
   :caption: job report

   Bootstrap records written to
      /home/user/bareos/testbin/working/restore.bsr
   The restore job will require the following Volumes:

      TestVolume001
   1444 files selected to restore.
   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:      /home/user/bareos/testbin/working/restore.bsr
   Where:          /tmp/bareos-restores
   Replace:        always
   FileSet:        Full Set
   Backup Client:  rufus-fd
   Restore Client: rufus-fd
   Storage:        File
   JobId:          *None*
   When:           2005-04-28 14:53:54
   OK to run? (yes/mod/no):
   Bootstrap records written to /var/lib/bareos/bareos-dir.restore.1.bsr

   The job will require the following
      Volume(s)                 Storage(s)                SD Device(s)
   ===========================================================================
      
       TestVolume001             File                      FileStorage

   Volumes marked with "*" are online.


   166 files selected to be restored.

   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/bareos-dir.restore.1.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         Full Set
   Backup Client:   bareos-fd
   Restore Client:  bareos-fd
   Format:          Native
   Storage:         File
   When:            2013-05-23 15:56:53
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no): 



If you answer yes your files will be restored to :file:`/tmp/bareos-restores`. If you want to restore the files to their original locations, you must use the mod option and explicitly set Where: to nothing (or to /). We recommend you go ahead and answer yes and after a brief moment, enter :bcommand:`messages`, at which point you should get a listing of all the files that were restored as well as a summary of the job that looks similar to this:



.. code-block:: bconsole
   :caption: job report

   23-May 15:24 bareos-dir JobId 2: Start Restore Job RestoreFiles.2013-05-23_15.24.01_10
   23-May 15:24 bareos-dir JobId 2: Using Device "FileStorage" to read.
   23-May 15:24 bareos-sd JobId 2: Ready to read from volume "TestVolume001" on device "FileStorage" (/var/lib/bareos/storage).
   23-May 15:24 bareos-sd JobId 2: Forward spacing Volume "TestVolume001" to file:block 0:194.
   23-May 15:58 bareos-dir JobId 3: Bareos bareos-dir 13.2.0 (09Apr13):
     Build OS:               x86_64-pc-linux-gnu debian Debian GNU/Linux 6.0 (squeeze)
     JobId:                  2
     Job:                    RestoreFiles.2013-05-23_15.58.48_11
     Restore Client:         bareos-fd
     Start time:             23-May-2013 15:58:50
     End time:               23-May-2013 15:58:52
     Files Expected:         166
     Files Restored:         166
     Bytes Restored:         19,069,526
     Rate:                   9534.8 KB/s
     FD Errors:              0
     FD termination status:  OK
     SD termination status:  OK
     Termination:            Restore OK



After exiting the Console program, you can examine the files in :file:`/tmp/bareos-restores`, which will contain a small directory tree with all the files. Be sure to clean up at the end with:

.. code-block:: shell-session
   :caption: remove restore directory

   root@host:~# rm -rf /tmp/bareos-restore

Quitting the Console Program
----------------------------

:index:`\ <single: Program; Quitting the Console>`\  :index:`\ <single: Quitting the Console Program>`\ 

Simply enter the command :bcommand:`quit`.

.. _SecondClient:

.. _section-AddAClient:

Adding a Client
---------------

:index:`\ <single: Client; Adding a Second>`
:index:`\ <single: Adding a Client>`

If you have gotten the example shown above to work on your system, you may be ready to add a second Client (|fd|). That is you have a second machine that you would like backed up. Lets assume, following settings about the machine you want to add to your backup environment:

Hostname (FQDN)
   :strong:`client2.example.com`

IP Address
   192.168.0.2

OS
   Linux (otherwise the paths may differ)

For this you have to make changes on the server side (|dir|) and the client side.

Client: install package
~~~~~~~~~~~~~~~~~~~~~~~

See :ref:`InstallChapter` about how to add the Bareos repository. The only part you need installed on the other machine is the **bareos-filedaemon**.

Director: configure client
~~~~~~~~~~~~~~~~~~~~~~~~~~

Bareos :sinceVersion:`16.2.4: configure add` offers the :ref:`configure add command <section-bcommandConfigure>` to add resources to the |dir|.

Start the :command:`bconsole` and use the :bcommand:`configure add client` command. Address must be a DNS resolvable name or an IP address.

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

-  :file:`/etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf` (assuming your director resource is named **bareos-dir**)

The :file:`/etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf` is the required resource needed on the |fd|. You can copy it to the destination:

.. code-block:: shell-session
   :caption: Copy the bareos-fd director resource to the new client

   scp /etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf root@client2.example.com:/etc/bareos/bareos-fd.d/director/

Manual configuration
^^^^^^^^^^^^^^^^^^^^

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

Client: configure
~~~~~~~~~~~~~~~~~

The package **bareos-filedaemon** :sinceVersion:`16.2.4: Client resource files` brings several configuration files:

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
   defines the name of the client. The default is :file:`<hostname>-fd`. Changes are only required, if you want to use another name or en- or disable special |fd| features. See :ref:`ClientResourceClient`.

:file:`director/bareos-dir.conf`
   gives the |dir| **bareos-dir** full access to this |fd|. During installation, the :config:option:`fd/director/Password`\  is set to a random default. Adapt the name and/or the password to your |dir|. (The name **bareos-dir** is the default |dir| name since Bareos :sinceVersion:`16.2.4: bareos-dir is the default |dir| name`.)

:file:`director/bareos-mon.conf`
   gives the |dir| **bareos-mon** restricted access to this |fd|. During installation, the :config:option:`fd/director/Password`\  is set to a random value. This resource is intended to be used by the local **bareos-tray-monitor**.

:file:`messages/Standard.conf`
   defines, how messages should be handled. The default sends all relevant messages to the |dir|.

If your |dir| is named **bareos-dir**, the :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf` may already be overwritten by the file you copied from the |dir|. If your Director has another name, an addition resource file will exists. You can define an arbitrary number of |dir|’s in your |fd| configuration. However, normally you will only have one :config:option:`Fd/Director`\  with
full control of your |fd| and optional one :config:option:`Fd/Director`\  for monitoring (used by the |traymonitor|).

Anyhow, the resource will look similar to this:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
     Name = bareos-dir
     Password = "[md5]5ebe2294ecd0e0f08eab7690d2a6ee69"
   }

After a restart of the |fd| to reload the configuration this resource allows the access for a |dir| with name **bareos-dir** and password **secret** (stored in MD5 format).

.. code-block:: shell-session
   :caption: restart bareos-fd

   service bareos-fd restart


Manual configuration
^^^^^^^^^^^^^^^^^^^^

If you have not created the :config:option:`Fd/Director`\  by :bcommand:`configure`, you can create it also manually. If your |dir| is also named **bareos-dir**, modify or create the file :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
     Name = "bareos-dir"   # Name of your Bareos Director
     Password = "secret"   # Password (cleartext or MD5) must be identical
                           # to the password of your client reosurce in the Direcotr
                           # (bareos-dir.d/client/client2-fd.conf)
   }

See the relation between resource names and password of the different Bareos components in :ref:`section-resource-relation`.

If your are not using the :ref:`section-SubdirectoryConfigurationScheme`, make sure that this resource file gets included in your |fd| configuration. You can verify this by

.. code-block:: shell-session
   :caption: show how bareos-fd would read the current configuration files

   bareos-fd -xc

After modifying the file, you have to restart the |fd|:

.. code-block:: shell-session
   :caption: restart bareos-fd

   service bareos-fd restart

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

When you start the Bareos daemons, the Storage daemon attempts to open all defined storage devices and verify the currently mounted Volume (if configured). Until all the storage devices are verified, the Storage daemon will not accept connections from the Console program. If a tape was previously used, it will be rewound, and on some devices this can take several minutes. As a consequence, you may need to have a bit of patience when first contacting the Storage daemon after starting the daemons.
If you can see your tape drive, once the lights stop flashing, the drive will be ready to be used.

The same considerations apply if you have just mounted a blank tape in a drive. It can take a minute or two before the drive properly recognizes that the tape is blank. If you attempt to :bcommand:`mount` the tape with the Console program during this recognition period, it is quite possible that you will hang your SCSI driver. As a consequence, you are again urged to have patience when inserting blank tapes. Let the device settle down before attempting to access it.

.. _Pool:

Pools
-----

:index:`\ <single: Pool; Overview>`\ 

Creating the Pool is automatically done when the |dir| starts, so if you understand Pools, you can skip to the next section.

When you run a backup job, one of the things that Bareos must know is what Volumes to use. Instead of specifying a Volume (tape) directly, you specify which Pool of Volumes you want Bareos to consult when it wants a Volume for writing backups. Bareos will select the first available Volume from the Pool that is appropriate for the :config:option:`dir/job/Storage`\  you have specified for the Job being run. When a volume has filled up with data, Bareos will change its
**VolStatus** from **Append** to **Full**, and then Bareos will use the next volume and so on. If no appendable Volume exists in the Pool, the Director will attempt to recycle an old Volume. For details, please read the :ref:`RecyclingChapter` chapter.

If there are still no appendable Volumes available, Bareos will send a message requesting the operator to create an appropriate Volume.

Bareos keeps track of the Pool name, the volumes contained in the Pool, and a number of attributes of each of those Volumes.

When Bareos starts, it ensures that all Pool resource definitions have been recorded in the catalog. You can verify this by entering:

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

:index:`\ <single: Console; Commands; Useful>`\ 

help
   Show the list all all available commands.

help list
   Show detail information about a specific command, in this case the command :bcommand:`list`.

status dir
   :index:`\ <single: Console; Command; status dir>`\  Print a status of all running jobs and jobs scheduled in the next 24 hours.

status
   :index:`\ <single: Console; Command; status>`\  The console program will prompt you to select a daemon type, then will request the daemon’s status.

status jobid=nn
   :index:`\ <single: Console; Command; status jobid>`\  Print a status of JobId nn if it is running. The Storage daemon is contacted and requested to print a current status of the job as well.

list pools
   :index:`\ <single: Console; Command; list pools>`\  List the pools defined in the Catalog (normally only Default is used).

list volumes
   :index:`\ <single: Console; Command; list volumes>`\  Lists all the media defined in the Catalog.

list jobs
   :index:`\ <single: Console; Command; list jobs>`\  Lists all jobs in the Catalog that have run.

list jobid=nn
   :index:`\ <single: Console; Command; list jobid>`\  Lists JobId nn from the Catalog.

list jobtotals
   :index:`\ <single: Console; Command; list jobtotals>`\  Lists totals for all jobs in the Catalog.

list files jobid=nn
   :index:`\ <single: Console; Command; list files jobid>`\  List the files that were saved for JobId nn.

list jobmedia
   :index:`\ <single: Console; Command; list jobmedia>`\  List the media information for each Job run.

messages
   :index:`\ <single: Console; Command; messages>`\  Prints any messages that have been directed to the console.

quit
   :index:`\ <single: Console; Command; quit>`\  Exit or quit the console program.

Most of the commands given above, with the exception of list, will prompt you for the necessary arguments if you simply enter the command name.

The full list of commands is shown in the chapter :ref:`section-ConsoleCommands`.  

