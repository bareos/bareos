.. _DiskChapter:

Volume Management
=================

:index:`\ <single: Volume; Management>` :index:`\ <single: Disk Volumes>`

This chapter presents most all the features needed to do Volume management. Most of the concepts apply equally well to both tape and disk Volumes. However, the chapter was originally written to explain backing up to disk, so you will see it is slanted in that direction, but all the directives presented here apply equally well whether your volume is disk or tape.

If you have a lot of hard disk storage or you absolutely must have your backups run within a small time window, you may want to direct Bareos to backup to disk Volumes rather than tape Volumes. This chapter is intended to give you some of the options that are available to you so that you can manage either disk or tape volumes.

Key Concepts and Resource Records
---------------------------------

:index:`\ <single: Volume; Management; Key Concepts and Resource Records>`

Getting Bareos to write to disk rather than tape in the simplest case is rather easy. In the Storage daemon’s configuration file, you simply define an :config:option:`sd/device/ArchiveDevice`\  to be a directory. The default directory to store backups on disk is :file:`/var/lib/bareos/storage`:



::

   Device {
     Name = FileBackup
     Media Type = File
     Archive Device = /var/lib/bareos/storage
     Random Access = Yes;
     AutomaticMount = yes;
     RemovableMedia = no;
     AlwaysOpen = no;
   }



Assuming you have the appropriate :config:option:`Dir/Storage`\  resource in your Director’s configuration file that references the above Device resource,



::

   Storage {
     Name = FileStorage
     Address = ...
     Password = ...
     Device = FileBackup
     Media Type = File
   }



Bareos will then write the archive to the file /var/lib/bareos/storage/<volume-name> where <volume-name> is the volume name of a Volume defined in the Pool. For example, if you have labeled a Volume named Vol001, Bareos will write to the file /var/lib/bareos/storage/Vol001. Although you can later move the archive file to another directory, you should not rename it or it will become unreadable by Bareos. This is because each archive has the filename as part of the internal label, and the internal
label must agree with the system filename before Bareos will use it.

Although this is quite simple, there are a number of problems. The first is that unless you specify otherwise, Bareos will always write to the same volume until you run out of disk space. This problem is addressed below.

In addition, if you want to use concurrent jobs that write to several different volumes at the same time, you will need to understand a number of other details. An example of such a configuration is given at the end of this chapter under :ref:`ConcurrentDiskJobs`.

Pool Options to Limit the Volume Usage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Pool; Options to Limit the Volume Usage>`

Some of the options you have, all of which are specified in the Pool record, are:

-  :config:option:`dir/pool/MaximumVolumeJobs`\ : write only the specified number of jobs on each Volume.

-  :config:option:`dir/pool/MaximumVolumeBytes`\ : limit the maximum size of each Volume.

   Note, if you use disk volumes you should probably limit the Volume size to some reasonable value. If you ever have a partial hard disk failure, you are more likely to be able to recover more data if they are in smaller Volumes.

-  :config:option:`dir/pool/VolumeUseDuration`\ : restrict the time between first and last data written to Volume.

Note that although you probably would not want to limit the number of bytes on a tape as you would on a disk Volume, the other options can be very useful in limiting the time Bareos will use a particular Volume (be it tape or disk). For example, the above directives can allow you to ensure that you rotate through a set of daily Volumes if you wish.

As mentioned above, each of those directives is specified in the Pool or Pools that you use for your Volumes. In the case of :config:option:`dir/pool/MaximumVolumeJobs`\ , :config:option:`dir/pool/MaximumVolumeBytes`\  and :config:option:`dir/pool/VolumeUseDuration`\ , you can actually specify the desired value on a Volume by Volume basis. The value specified in the Pool record becomes the default when labeling new Volumes. Once a
Volume has been created, it gets its own copy of the Pool defaults, and subsequently changing the Pool will have no effect on existing Volumes. You can either manually change the Volume values, or refresh them from the Pool defaults using the :bcommand:`update volume` command in the Console. As an example of the use of one of the above, suppose your Pool resource contains:

.. code-block:: bareosconfig
   :caption: Volume Use Duration

   Pool {
     Name = File
     Pool Type = Backup
     Volume Use Duration = 23h
   }

then if you run a backup once a day (every 24 hours), Bareos will use a new Volume for each backup, because each Volume it writes can only be used for 23 hours after the first write. Note, setting the use duration to 23 hours is not a very good solution for tapes unless you have someone on-site during the weekends, because Bareos will want a new Volume and no one will be present to mount it, so no weekend backups will be done until Monday morning.

.. _AutomaticLabeling:

Automatic Volume Labeling
~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Label; Automatic Volume Labeling>` :index:`\ <single: Volume; Labeling; Automatic>`

Use of the above records brings up another problem – that of labeling your Volumes. For automated disk backup, you can either manually label each of your Volumes, or you can have Bareos automatically label new Volumes when they are needed.

Please note that automatic Volume labeling can also be used with tapes, but it is not nearly so practical since the tapes must be pre-mounted. This requires some user interaction. Automatic labeling from templates does NOT work with autochangers since Bareos will not access unknown slots. There are several methods of labeling all volumes in an autochanger magazine. For more information on this, please see the :ref:`AutochangersChapter` chapter.

Automatic Volume labeling is enabled by making a change to both the :config:option:`Dir/Pool`\  resource and to the :config:option:`Sd/Device`\  resource shown above. In the case of the Pool resource, you must provide Bareos with a label format that it will use to create new names. In the simplest form, the label format is simply the Volume name, to which Bareos will append a four digit number. This number starts at 0001 and is incremented for each Volume the catalog
contains. Thus if you modify your Pool resource to be:

.. code-block:: bareosconfig
   :caption: Label Format

   Pool {
     Name = File
     Pool Type = Backup
     Volume Use Duration = 23h
     Label Format = "Vol"
   }

Bareos will create Volume names Vol0001, Vol0002, and so on when new Volumes are needed. Much more complex and elaborate labels can be created using variable expansion defined in the :ref:`Variable Expansion <VarsChapter>` chapter of this manual.

The second change that is necessary to make automatic labeling work is to give the Storage daemon permission to automatically label Volumes. Do so by adding :config:option:`sd/device/LabelMedia`\  = yes to the :config:option:`Sd/Device`\  resource as follows:

.. code-block:: bareosconfig
   :caption: Label Media = yes

   Device {
     Name = File
     Media Type = File
     Archive Device = /var/lib/bareos/storage/
     Random Access = yes
     Automatic Mount = yes
     Removable Media = no
     Always Open = no
     Label Media = yes
   }

See :config:option:`dir/pool/LabelFormat`\  for details about the labeling format.

Restricting the Number of Volumes and Recycling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Recycling; Restricting the Number of Volumes and Recycling>` :index:`\ <single: Restricting the Number of Volumes and Recycling>`

Automatic labeling discussed above brings up the problem of Volume management. With the above scheme, a new Volume will be created every day. If you have not specified Retention periods, your Catalog will continue to fill keeping track of all the files Bareos has backed up, and this procedure will create one new archive file (Volume) every day.

The tools Bareos gives you to help automatically manage these problems are the following:

-  :config:option:`dir/client/FileRetention`\ : catalog file record retention period.

-  :config:option:`dir/client/JobRetention`\ : catalog job record retention period.

-  :config:option:`dir/client/AutoPrune`\  = yes: permit the application of the above two retention periods.

-  

   :config:option:`dir/pool/VolumeRetention`\ 

-  :config:option:`dir/pool/AutoPrune`\  = yes: permit the application of the :config:option:`dir/pool/VolumeRetention`\  period.

-  :config:option:`dir/pool/Recycle`\  = yes: permit automatic recycling of Volumes whose Volume retention period has expired.

-  :config:option:`dir/pool/RecycleOldestVolume`\  = yes: prune the oldest volume in the Pool, and if all files were pruned, recycle this volume and use it.

-  :config:option:`dir/pool/RecycleCurrentVolume`\  = yes: prune the currently mounted volume in the Pool, and if all files were pruned, recycle this volume and use it.

-  | :config:option:`dir/pool/PurgeOldestVolume`\  = yes: permits a forced recycling of the oldest Volume when a new one is needed.
   | 

.. warning::

   This record ignores retention periods! We highly
        recommend  not to use this record, but instead use :config:option:`dir/pool/RecycleOldestVolume`\ .

-  :config:option:`dir/pool/MaximumVolumes`\ : limit the number of Volumes that can be created.

The first three records (:config:option:`dir/client/FileRetention`\ , :config:option:`dir/client/JobRetention`\  and :config:option:`dir/client/AutoPrune`\ ) determine the amount of time that Job and File records will remain in your Catalog and they are discussed in detail in the :ref:`Automatic Volume Recycling <RecyclingChapter>` chapter.

:config:option:`dir/pool/VolumeRetention`\ , :config:option:`dir/pool/AutoPrune`\  and :config:option:`dir/pool/Recycle`\  determine how long Bareos will keep your Volumes before reusing them and they are also discussed in detail in the :ref:`Automatic Volume Recycling <RecyclingChapter>` chapter.

The :config:option:`dir/pool/MaximumVolumes`\  record can also be used in conjunction with the :config:option:`dir/pool/VolumeRetention`\  period to limit the total number of archive Volumes that Bareos will create. By setting an appropriate :config:option:`dir/pool/VolumeRetention`\  period, a Volume will be purged just before it is needed and thus Bareos can cycle through a fixed set of Volumes. Cycling through a fixed set of
Volumes can also be done by setting :config:option:`dir/pool/PurgeOldestVolume`\  = yes or :config:option:`dir/pool/RecycleCurrentVolume`\  = yes. In this case, when Bareos needs a new Volume, it will prune the specified volume.

Concurrent Disk Jobs
--------------------

:index:`\ <single: Concurrent Disk Jobs>` 

.. _ConcurrentDiskJobs:

 Above, we discussed how you could have a single device named :config:option:`Sd/Device = FileBackup`\  that writes to volumes in :file:`/var/lib/bareos/storage/`. You can, in fact, run multiple concurrent jobs using the Storage definition given with this example, and all the jobs will simultaneously write into the Volume that is being written.

Now suppose you want to use multiple Pools, which means multiple Volumes, or suppose you want each client to have its own Volume and perhaps its own directory such as /home/bareos/client1 and /home/bareos/client2 ... . With the single Storage and Device definition above, neither of these two is possible. Why? Because Bareos disk storage follows the same rules as tape devices. Only one Volume can be mounted on any Device at any time. If you want to simultaneously write multiple Volumes, you will
need multiple Device resources in your |sd| configuration and thus multiple Storage resources in your |dir| configuration.

Okay, so now you should understand that you need multiple Device definitions in the case of different directories or different Pools, but you also need to know that the catalog data that Bareos keeps contains only the Media Type and not the specific storage device. This permits a tape for example to be re-read on any compatible tape drive. The compatibility being determined by the Media Type (:config:option:`dir/storage/MediaType`\  and
:config:option:`sd/device/MediaType`\ ). The same applies to disk storage. Since a volume that is written by a Device in say directory :file:`/home/bareos/backups` cannot be read by a Device with an :config:option:`sd/device/ArchiveDevice`\  = :file:`/home/bareos/client1`, you will not be able to restore all your files if you give both those devices :config:option:`sd/device/MediaType`\  = File. During the restore, Bareos will
simply choose the first available device, which may not be the correct one. If this is confusing, just remember that the Directory has only the Media Type and the Volume name. It does not know the :config:option:`sd/device/ArchiveDevice`\  (or the full path) that is specified in the |sd|. Thus you must explicitly tie your Volumes to the correct Device by using the Media Type.

Example for two clients, separate devices and recycling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following example is not very practical, but can be used to demonstrate the proof of concept in a relatively short period of time.

The example consists of a two clients that are backed up to a set of 12 Volumes for each client into different directories on the Storage machine. Each Volume is used (written) only once, and there are four Full saves done every hour (so the whole thing cycles around after three hours).

What is key here is that each physical device on the |sd| has a different Media Type. This allows the Director to choose the correct device for restores.

The |dir| configuration is as follows:

.. code-block:: bareosconfig

   Director {
     Name = bareos-dir
     QueryFile = "/usr/lib/bareos/scripts/query.sql"
     Password = "<secret>"
   }

   Schedule {
     Name = "FourPerHour"
     Run = Level=Full hourly at 0:05
     Run = Level=Full hourly at 0:20
     Run = Level=Full hourly at 0:35
     Run = Level=Full hourly at 0:50
   }

   FileSet {
     Name = "Example FileSet"
     Include {
       Options {
         compression=GZIP
         signature=SHA1
       }
       File = /etc
     }
   }

   Job {
     Name = "RecycleExample"
     Type = Backup
     Level = Full
     Client = client1-fd
     FileSet= "Example FileSet"
     Messages = Standard
     Storage = FileStorage
     Pool = Recycle
     Schedule = FourPerHour
   }

   Job {
     Name = "RecycleExample2"
     Type = Backup
     Level = Full
     Client = client2-fd
     FileSet= "Example FileSet"
     Messages = Standard
     Storage = FileStorage2
     Pool = Recycle2
     Schedule = FourPerHour
   }

   Client {
     Name = client1-fd
     Address = client1.example.com
     Password = client1_password
   }

   Client {
     Name = client2-fd
     Address = client2.example.com
     Password = client2_password
   }

   Storage {
     Name = FileStorage
     Address = bareos-sd.example.com
     Password = local_storage_password
     Device = RecycleDir
     Media Type = File
   }

   Storage {
     Name = FileStorage2
     Address = bareos-sd.example.com
     Password = local_storage_password
     Device = RecycleDir2
     Media Type = File1
   }

   Catalog {
     Name = MyCatalog
     ...
   }

   Messages {
     Name = Standard
     ...
   }

   Pool {
     Name = Recycle
     Pool Type = Backup
     Label Format = "Recycle-"
     Auto Prune = yes
     Use Volume Once = yes
     Volume Retention = 2h
     Maximum Volumes = 12
     Recycle = yes
   }

   Pool {
     Name = Recycle2
     Pool Type = Backup
     Label Format = "Recycle2-"
     Auto Prune = yes
     Use Volume Once = yes
     Volume Retention = 2h
     Maximum Volumes = 12
     Recycle = yes
   }

and the |sd| configuration is:

.. code-block:: bareosconfig

   Storage {
     Name = bareos-sd
     Maximum Concurrent Jobs = 10
   }

   Director {
     Name = bareos-dir
     Password = local_storage_password
   }

   Device {
     Name = RecycleDir
     Media Type = File
     Archive Device = /home/bareos/backups
     LabelMedia = yes;
     Random Access = Yes;
     AutomaticMount = yes;
     RemovableMedia = no;
     AlwaysOpen = no;
   }

   Device {
     Name = RecycleDir2
     Media Type = File2
     Archive Device = /home/bareos/backups2
     LabelMedia = yes;
     Random Access = Yes;
     AutomaticMount = yes;
     RemovableMedia = no;
     AlwaysOpen = no;
   }

   Messages {
     Name = Standard
     director = bareos-dir = all
   }

With a little bit of work, you can change the above example into a weekly or monthly cycle (take care about the amount of archive disk space used).

.. _section-MultipleStorageDevices:

Using Multiple Storage Devices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Multiple Storage Devices>` :index:`\ <single: Storage Device; Multiple>`

Bareos treats disk volumes similar to tape volumes as much as it can. This means that you can only have a single Volume mounted at one time on a disk as defined in your :config:option:`Sd/Device`\  resource.

If you use Bareos without :ref:`section-DataSpooling`, multiple concurrent backup jobs can be written to a Volume using interleaving. However, interleaving has disadvantages, see :ref:`section-Interleaving`.

Also the :config:option:`Sd/Device`\  will be in use. If there are other jobs, requesting other Volumes, these jobs have to wait.

On a tape (or autochanger), this is a physical limitation of the hardware. However, when using disk storage, this is only a limitation of the software.

To enable Bareos to run concurrent jobs (on disk storage), define as many :config:option:`Sd/Device`\  as concurrent jobs should run. All these :config:option:`Sd/Device`\ s can use the same :config:option:`sd/device/ArchiveDevice`\  directory. Set :config:option:`sd/device/MaximumConcurrentJobs`\  = 1 for all these devices.

Example: use four storage devices pointing to the same directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bareosconfig
   :caption: |dir| configuration: using 4 storage devices

   Director {
     Name = bareos-dir.example.com
     QueryFile = "/usr/lib/bareos/scripts/query.sql"
     Maximum Concurrent Jobs = 10
     Password = "<secret>"
   }

   Storage {
     Name = File
     Address = bareos-sd.bareos.com
     Password = "<sd-secret>"
     Device = FileStorage1
     Device = FileStorage2
     Device = FileStorage3
     Device = FileStorage4
     # number of devices = Maximum Concurrent Jobs
     Maximum Concurrent Jobs = 4
     Media Type = File
   }

   [...]

.. code-block:: bareosconfig
   :caption: |sd| configuraton: using 4 storage devices

   Storage {
     Name = bareos-sd.example.com
     # any number >= 4
     Maximum Concurrent Jobs = 20
   }

   Director {
     Name = bareos-dir.example.com
     Password = "<sd-secret>"
   }

   Device {
     Name = FileStorage1
     Media Type = File
     Archive Device = /var/lib/bareos/storage
     LabelMedia = yes
     Random Access = yes
     AutomaticMount = yes
     RemovableMedia = no
     AlwaysOpen = no
     Maximum Concurrent Jobs = 1
   }

   Device {
     Name = FileStorage2
     Media Type = File
     Archive Device = /var/lib/bareos/storage
     LabelMedia = yes
     Random Access = yes
     AutomaticMount = yes
     RemovableMedia = no
     AlwaysOpen = no
     Maximum Concurrent Jobs = 1
   }

   Device {
     Name = FileStorage3
     Media Type = File
     Archive Device = /var/lib/bareos/storage
     LabelMedia = yes
     Random Access = yes
     AutomaticMount = yes
     RemovableMedia = no
     AlwaysOpen = no
     Maximum Concurrent Jobs = 1
   }

   Device {
     Name = FileStorage4
     Media Type = File
     Archive Device = /var/lib/bareos/storage
     LabelMedia = yes
     Random Access = yes
     AutomaticMount = yes
     RemovableMedia = no
     AlwaysOpen = no
     Maximum Concurrent Jobs = 1
   }



.. _RecyclingChapter:

Automatic Volume Recycling
--------------------------

:index:`\ <single: Recycle; Automatic Volume>` :index:`\ <single: Volume; Recycle; Automatic>`

By default, once Bareos starts writing a Volume, it can append to the volume, but it will not overwrite the existing data thus destroying it. However when Bareos recycles a Volume, the Volume becomes available for being reused and Bareos can at some later time overwrite the previous contents of that Volume. Thus all previous data will be lost. If the Volume is a tape, the tape will be rewritten from the beginning. If the Volume is a disk file, the file will be truncated before being rewritten.

You may not want Bareos to automatically recycle (reuse) tapes. This would require a large number of tapes though, and in such a case, it is possible to manually recycle tapes. For more on manual recycling, see the :ref:`manualrecycling` chapter.

Most people prefer to have a Pool of tapes that are used for daily backups and recycled once a week, another Pool of tapes that are used for Full backups once a week and recycled monthly, and finally a Pool of tapes that are used once a month and recycled after a year or two. With a scheme like this, the number of tapes in your pool or pools remains constant.

By properly defining your Volume Pools with appropriate Retention periods, Bareos can manage the recycling (such as defined above) automatically.

Automatic recycling of Volumes is controlled by four records in the :config:option:`Dir/Pool`\  resource definition. These four records are:

-  :config:option:`dir/pool/AutoPrune`\  = yes

-  

   :config:option:`dir/pool/VolumeRetention`\ 

-  :config:option:`dir/pool/Recycle`\  = yes

-  

   :config:option:`dir/pool/RecyclePool`\ 

The above three directives are all you need assuming that you fill each of your Volumes then wait the Volume Retention period before reusing them. If you want Bareos to stop using a Volume and recycle it before it is full, you can use one or more additional directives such as:

-  

   :config:option:`dir/pool/VolumeUseDuration`\ 

-  

   :config:option:`dir/pool/MaximumVolumeJobs`\ 

-  

   :config:option:`dir/pool/MaximumVolumeBytes`\ 

Please see below and the :ref:`Basic Volume Management <DiskChapter>` chapter of this manual for complete examples.

Automatic recycling of Volumes is performed by Bareos only when it wants a new Volume and no appendable Volumes are available in the Pool. It will then search the Pool for any Volumes with the Recycle flag set and the Volume Status is **Purged**. At that point, it will choose the oldest purged volume and recycle it.

If there are no volumes with status **Purged**, then the recycling occurs in two steps:

#. The Catalog for a Volume must be pruned of all Jobs (i.e. Purged).

#. The actual recycling of the Volume.

Only Volumes marked **Full** or **Used** will be considerd for pruning. The Volume will be purged if the **Volume Retention** period has expired. When a Volume is marked as **Purged**, it means that no Catalog records reference that Volume and the Volume can be recycled.

Until recycling actually occurs, the Volume data remains intact. If no Volumes can be found for recycling for any of the reasons stated above, Bareos will request operator intervention (i.e. it will ask you to label a new volume).

A key point mentioned above, that can be a source of frustration, is that Bareos will only recycle purged Volumes if there is no other appendable Volume available. Otherwise, it will always write to an appendable Volume before recycling even if there are Volume marked as Purged. This preserves your data as long as possible. So, if you wish to "force" Bareos to use a purged Volume, you must first ensure that no other Volume in the Pool is marked Append. If necessary, you can
manually set a volume to Full. The reason for this is that Bareos wants to preserve the data on your old tapes (even though purged from the catalog) as long as absolutely possible before overwriting it. There are also a number of directives such as **Volume Use Duration** that will automatically mark a volume as **Used** and thus no longer appendable.

.. _AutoPruning:

Automatic Pruning
~~~~~~~~~~~~~~~~~

:index:`\ <single: Automatic; Pruning>` :index:`\ <single: Pruning; Automatic>`

As Bareos writes files to tape, it keeps a list of files, jobs, and volumes in a database called the catalog. Among other things, the database helps Bareos to decide which files to back up in an incremental or differential backup, and helps you locate files on past backups when you want to restore something. However, the catalog will grow larger and larger as time goes on, and eventually it can become unacceptably large.

Bareos’s process for removing entries from the catalog is called Pruning. The default is Automatic Pruning, which means that once an entry reaches a certain age (e.g. 30 days old) it is removed from the catalog. Note that Job records that are required for current restore and File records are needed for VirtualFull and Accurate backups won’t be removed automatically.

Once a job has been pruned, you can still restore it from the backup tape, but one additional step is required: scanning the volume with :command:`bscan`.

The alternative to Automatic Pruning is Manual Pruning, in which you explicitly tell Bareos to erase the catalog entries for a volume. You’d usually do this when you want to reuse a Bareos volume, because there’s no point in keeping a list of files that USED TO BE on a tape. Or, if the catalog is starting to get too big, you could prune the oldest jobs to save space. Manual pruning is done with the :ref:`prune command <ManualPruning>` in the console.

Pruning Directives
~~~~~~~~~~~~~~~~~~

:index:`\ <single: Pruning; Directives>`

There are three pruning durations. All apply to catalog database records and not to the actual data in a Volume. The pruning (or retention) durations are for: Volumes (Media records), Jobs (Job records), and Files (File records). The durations inter-depend because if Bareos prunes a Volume, it automatically removes all the Job records, and all the File records. Also when a Job record is pruned, all the File records for that Job are also pruned (deleted) from the catalog.

Having the File records in the database means that you can examine all the files backed up for a particular Job. They take the most space in the catalog (probably 90-95% of the total). When the File records are pruned, the Job records can remain, and you can still examine what Jobs ran, but not the details of the Files backed up. In addition, without the File records, you cannot use the Console restore command to restore the files.

When a Job record is pruned, the Volume (Media record) for that Job can still remain in the database, and if you do a :bcommand:`list volumes`, you will see the volume information, but the Job records (and its File records) will no longer be available.

In each case, pruning removes information about where older files are, but it also prevents the catalog from growing to be too large. You choose the retention periods in function of how many files you are backing up and the time periods you want to keep those records online, and the size of the database. It is possible to re-insert the records (with 98% of the original data) by using :command:`bscan` to scan in a whole Volume or any part of the volume that you want.

By setting :config:option:`dir/pool/AutoPrune`\  = yes you will permit the |dir| to automatically prune all Volumes in the Pool when a Job needs another Volume. Volume pruning means removing records from the catalog. It does not shrink the size of the Volume or affect the Volume data until the Volume gets overwritten. When a Job requests another volume and there are no Volumes with Volume status **Append** available, Bareos will
begin volume pruning. This means that all Jobs that are older than the **Volume Retention** period will be pruned from every Volume that has Volume status **Full** or **Used** and has **Recycle = yes**. Pruning consists of deleting the corresponding Job, File, and JobMedia records from the catalog database. No change to the physical data on the Volume occurs during the pruning process. When all
files are pruned from a Volume (i.e. no records in the catalog), the Volume will be marked as **Purged** implying that no Jobs remain on the volume. The Pool records that control the pruning are described below.

:config:option:`dir/pool/AutoPrune`\  = yes
   when running a Job and it needs a new Volume but no appendable volumes are available, apply the Volume retention period. At that point, Bareos will prune all Volumes that can be pruned in an attempt to find a usable volume. If during the autoprune, all files are pruned from the Volume, it will be marked with Volume status **Purged**.

   Note, that although the File and Job records may be pruned from the catalog, a Volume will only be marked **Purged** (and hence ready for recycling) if the Volume status is **Append**, **Full**, **Used**, or **Error**. If the Volume has another status, such as **Archive**, **Read-Only**, **Disabled**,
   **Busy** or **Cleaning**, the Volume status will not be changed to **Purged**.

:config:option:`dir/pool/VolumeRetention`\ 
   defines the length of time that Bareos will guarantee that the Volume is not reused counting from the time the last job stored on the Volume terminated. A key point is that this time period is not even considered as long at the Volume remains appendable. The Volume Retention period count down begins only when the **Append** status has been changed to some other status (**Full**, **Used**,
   **Purged**, ...).

   When this time period expires and if :config:option:`dir/pool/AutoPrune`\  = yes and a new Volume is needed, but no appendable Volume is available, Bareos will prune (remove) Job records that are older than the specified **Volume Retention** period.

   The **Volume Retention** period takes precedence over any :config:option:`dir/client/JobRetention`\  period you have specified in the Client resource. It should also be noted, that the **Volume Retention** period is obtained by reading the Catalog Database Media record rather than the Pool resource record. This means that if you change the :config:option:`dir/pool/VolumeRetention`\  in the Pool
   resource record, you must ensure that the corresponding change is made in the catalog by using the :bcommand:`update pool` command. Doing so will insure that any new Volumes will be created with the changed **Volume Retention** period. Any existing Volumes will have their own copy of the **Volume Retention** period that can only be changed on a Volume by Volume basis using the :bcommand:`update volume`
   command.

   When all file catalog entries are removed from the volume, its Volume status is set to **Purged**. The files remain physically on the Volume until the volume is overwritten.

:config:option:`dir/pool/Recycle`\ 
   defines whether or not the particular Volume can be recycled (i.e. rewritten). If Recycle is set to :strong:`no`, then even if Bareos prunes all the Jobs on the volume and it is marked **Purged**, it will not consider the tape for recycling. If Recycle is set to :strong:`yes` and all Jobs have been pruned, the volume status will be set to **Purged** and the volume may then be reused when another volume is needed. If
   the volume is reused, it is relabeled with the same Volume Name, however all previous data will be lost.

Recycling Algorithm
~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Algorithm; Recycling>` :index:`\ <single: Recycle; Algorithm>` 

.. _RecyclingAlgorithm:

 

.. _Recycling:



After all Volumes of a Pool have been pruned (as mentioned above, this happens when a Job needs a new Volume and no appendable Volumes are available), Bareos will look for the oldest Volume that is **Purged** (all Jobs and Files expired), and if the **Recycle = yes** for that Volume, Bareos will relabel it and write new data on it.

As mentioned above, there are two key points for getting a Volume to be recycled. First, the Volume must no longer be marked **Append** (there are a number of directives to automatically make this change), and second since the last write on the Volume, one or more of the Retention periods must have expired so that there are no more catalog backup job records that reference that Volume. Once both those conditions are satisfied, the volume can be marked
**Purged** and hence recycled.

The full algorithm that Bareos uses when it needs a new Volume is: :index:`\ <single: New Volume Algorithm>` :index:`\ <single: Algorithm; New Volume>`

The algorithm described below assumes that :strong:`Auto Prune`\  is enabled, that Recycling is turned on, and that you have defined appropriate Retention periods or used the defaults for all these items.

#. If the request is for an Autochanger device, look only for Volumes in the Autochanger (i.e. with InChanger set and that have the correct Storage device).

#. Search the Pool for a Volume with Volume status=**Append** (if there is more than one, the Volume with the oldest date last written is chosen. If two have the same date then the one with the lowest MediaId is chosen).

#. Search the Pool for a Volume with Volume status=**Recycle** and the InChanger flag is set true (if there is more than one, the Volume with the oldest date last written is chosen. If two have the same date then the one with the lowest MediaId is chosen).

#. Try recycling any purged Volumes.

#. Prune volumes applying Volume retention period (Volumes with VolStatus Full, Used, or Append are pruned). Note, even if all the File and Job records are pruned from a Volume, the Volume will not be marked Purged until the Volume retention period expires.

#. Search the Pool for a Volume with VolStatus=Purged

#. If a Pool named :config:option:`dir/pool = Scratch`\  exists, search for a Volume and if found move it to the current Pool for the Job and use it. Note, when the Scratch Volume is moved into the current Pool, the basic Pool defaults are applied as if it is a newly labeled Volume (equivalent to an :bcommand:`update volume from pool` command).

#. If we were looking for Volumes in the Autochanger, go back to step 2 above, but this time, look for any Volume whether or not it is in the Autochanger.

#. Attempt to create a new Volume if automatic labeling enabled. If the maximum number of Volumes specified for the pool is reached, no new Volume will be created.

#. Prune the oldest Volume if :config:option:`dir/pool/RecycleOldestVolume`\ =yes (the Volume with the oldest LastWritten date and VolStatus equal to Full, Recycle, Purged, Used, or Append is chosen). This record ensures that all retention periods are properly respected.

#. Purge the oldest Volume if :config:option:`dir/pool/PurgeOldestVolume`\ =yes (the Volume with the oldest LastWritten date and VolStatus equal to Full, Recycle, Purged, Used, or Append is chosen). 

.. warning::

   We strongly recommend against the use of :strong:`Purge Oldest Volume`\  as it can quite easily lead to loss of current backup
      data.

#. Give up and ask operator.

The above occurs when Bareos has finished writing a Volume or when no Volume is present in the drive.

On the other hand, if you have inserted a different Volume after the last job, and Bareos recognizes the Volume as valid, it will request authorization from the Director to use this Volume. In this case, if you have set :config:option:`dir/pool/RecycleCurrentVolume`\  = yes and the Volume is marked as Used or Full, Bareos will prune the volume and if all jobs were removed during the pruning (respecting the retention periods), the Volume will be recycled and used.

The recycling algorithm in this case is:

-  If the Volume status is **Append** or **Recycle**, the volume will be used.

-  If :config:option:`dir/pool/RecycleCurrentVolume`\  = yes and the volume is marked **Full** or **Used**, Bareos will prune the volume (applying the retention period). If all Jobs are pruned from the volume, it will be recycled.

This permits users to manually change the Volume every day and load tapes in an order different from what is in the catalog, and if the volume does not contain a current copy of your backup data, it will be used.

A few points from Alan Brown to keep in mind:

-  If :config:option:`dir/pool/MaximumVolumes`\  is not set, Bareos will prefer to demand new volumes over forcibly purging older volumes.

-  If volumes become free through pruning and the Volume retention period has expired, then they get marked as **Purged** and are immediately available for recycling - these will be used in preference to creating new volumes.

Recycle Status
~~~~~~~~~~~~~~

:index:`\ <single: Recycle Status>`

Each Volume inherits the Recycle status (yes or no) from the Pool resource record when the Media record is created (normally when the Volume is labeled). This Recycle status is stored in the Media record of the Catalog. Using the Console program, you may subsequently change the Recycle status for each Volume. For example in the following output from list volumes:



::

   +----------+-------+--------+---------+------------+--------+-----+
   | VolumeNa | Media | VolSta | VolByte | LastWritte | VolRet | Rec |
   +----------+-------+--------+---------+------------+--------+-----+
   | File0001 | File  | Full   | 4190055 | 2002-05-25 | 14400  | 1   |
   | File0002 | File  | Full   | 1896460 | 2002-05-26 | 14400  | 1   |
   | File0003 | File  | Full   | 1896460 | 2002-05-26 | 14400  | 1   |
   | File0004 | File  | Full   | 1896460 | 2002-05-26 | 14400  | 1   |
   | File0005 | File  | Full   | 1896460 | 2002-05-26 | 14400  | 1   |
   | File0006 | File  | Full   | 1896460 | 2002-05-26 | 14400  | 1   |
   | File0007 | File  | Purged | 1896466 | 2002-05-26 | 14400  | 1   |
   +----------+-------+--------+---------+------------+--------+-----+



all the volumes are marked as recyclable, and the last Volume, File0007 has been purged, so it may be immediately recycled. The other volumes are all marked recyclable and when their Volume Retention period (14400 seconds or four hours) expires, they will be eligible for pruning, and possibly recycling. Even though Volume File0007 has been purged, all the data on the Volume is still recoverable. A purged Volume simply means that there are no entries in the Catalog. Even if the Volume Status is
changed to Recycle, the data on the Volume will be recoverable. The data is lost only when the Volume is re-labeled and re-written.

To modify Volume File0001 so that it cannot be recycled, you use the update volume pool=File command in the console program, or simply update and Bareos will prompt you for the information.



::

   +----------+------+-------+---------+-------------+-------+-----+
   | VolumeNa | Media| VolSta| VolByte | LastWritten | VolRet| Rec |
   +----------+------+-------+---------+-------------+-------+-----+
   | File0001 | File | Full  | 4190055 | 2002-05-25  | 14400 | 0   |
   | File0002 | File | Full  | 1897236 | 2002-05-26  | 14400 | 1   |
   | File0003 | File | Full  | 1896460 | 2002-05-26  | 14400 | 1   |
   | File0004 | File | Full  | 1896460 | 2002-05-26  | 14400 | 1   |
   | File0005 | File | Full  | 1896460 | 2002-05-26  | 14400 | 1   |
   | File0006 | File | Full  | 1896460 | 2002-05-26  | 14400 | 1   |
   | File0007 | File | Purged| 1896466 | 2002-05-26  | 14400 | 1   |
   +----------+------+-------+---------+-------------+-------+-----+



In this case, File0001 will never be automatically recycled. The same effect can be achieved by setting the Volume Status to Read-Only.

As you have noted, the Volume Status (VolStatus) column in the catalog database contains the current status of the Volume, which is normally maintained automatically by Bareos. To give you an idea of some of the values it can take during the life cycle of a Volume, here is a picture created by Arno Lehmann:



::

   A typical volume life cycle is like this:

                 because job count or size limit exceeded
         Append  -------------------------------------->  Used/Full
           ^                                                  |
           | First Job writes to        Retention time passed |
           | the volume                   and recycling takes |
           |                                            place |
           |                                                  v
         Recycled <-------------------------------------- Purged
                        Volume is selected for reuse



Daily, Weekly, Monthly Tape Usage Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This example is meant to show you how one could define a fixed set of volumes that Bareos will rotate through on a regular schedule. There are an infinite number of such schemes, all of which have various advantages and disadvantages.

We start with the following assumptions:

-  A single tape has more than enough capacity to do a full save.

-  There are ten tapes that are used on a daily basis for incremental backups. They are prelabeled Daily1 ... Daily10.

-  There are four tapes that are used on a weekly basis for full backups. They are labeled Week1 ... Week4.

-  There are 12 tapes that are used on a monthly basis for full backups. They are numbered Month1 ... Month12

-  A full backup is done every Saturday evening (tape inserted Friday evening before leaving work).

-  No backups are done over the weekend (this is easy to change).

-  The first Friday of each month, a Monthly tape is used for the Full backup.

-  Incremental backups are done Monday - Friday (actually Tue-Fri mornings).

We start the system by doing a Full save to one of the weekly volumes or one of the monthly volumes. The next morning, we remove the tape and insert a Daily tape. Friday evening, we remove the Daily tape and insert the next tape in the Weekly series. Monday, we remove the Weekly tape and re-insert the Daily tape. On the first Friday of the next month, we insert the next Monthly tape in the series rather than a Weekly tape, then continue. When a Daily tape finally fills up, Bareos will request
the next one in the series, and the next day when you notice the email message, you will mount it and Bareos will finish the unfinished incremental backup.

What does this give? Well, at any point, you will have the last complete Full save plus several Incremental saves. For any given file you want to recover (or your whole system), you will have a copy of that file every day for at least the last 14 days. For older versions, you will have at least three and probably four Friday full saves of that file, and going back further, you will have a copy of that file made on the beginning of the month for at least a year.

So you have copies of any file (or your whole system) for at least a year, but as you go back in time, the time between copies increases from daily to weekly to monthly.

What would the Bareos configuration look like to implement such a scheme?



::

   Schedule {
     Name = "NightlySave"
     Run = Level=Full Pool=Monthly 1st sat at 03:05
     Run = Level=Full Pool=Weekly 2nd-5th sat at 03:05
     Run = Level=Incremental Pool=Daily tue-fri at 03:05
   }
   Job {
     Name = "NightlySave"
     Type = Backup
     Level = Full
     Client = LocalMachine
     FileSet = "File Set"
     Messages = Standard
     Storage = DDS-4
     Pool = Daily
     Schedule = "NightlySave"
   }
   # Definition of file storage device
   Storage {
     Name = DDS-4
     Address = localhost
     SDPort = 9103
     Password = XXXXXXXXXXXXX
     Device = FileStorage
     Media Type = 8mm
   }
   FileSet {
     Name = "File Set"
     Include {
       Options {
         signature=MD5
       }
       File = fffffffffffffffff
     }
     Exclude  { File=*.o }
   }
   Pool {
     Name = Daily
     Pool Type = Backup
     AutoPrune = yes
     VolumeRetention = 10d   # recycle in 10 days
     Maximum Volumes = 10
     Recycle = yes
   }
   Pool {
     Name = Weekly
     Use Volume Once = yes
     Pool Type = Backup
     AutoPrune = yes
     VolumeRetention = 30d  # recycle in 30 days (default)
     Recycle = yes
   }
   Pool {
     Name = Monthly
     Use Volume Once = yes
     Pool Type = Backup
     AutoPrune = yes
     VolumeRetention = 365d  # recycle in 1 year
     Recycle = yes
   }



.. _PruningExample:

Automatic Pruning and Recycling Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Automatic; Pruning and Recycling Example>` :index:`\ <single: Example; Automatic Pruning and Recycling>` :index:`\ <single: Pruning; Automatic; Example>` :index:`\ <single: Recycle; Automatic; Example>`

Perhaps the best way to understand the various resource records that come into play during automatic pruning and recycling is to run a Job that goes through the whole cycle. If you add the following resources to your Director’s configuration file:



::

   Schedule {
     Name = "30 minute cycle"
     Run = Level=Full Pool=File Messages=Standard Storage=File
            hourly at 0:05
     Run = Level=Full Pool=File Messages=Standard Storage=File
            hourly at 0:35
   }
   Job {
     Name = "Filetest"
     Type = Backup
     Level = Full
     Client=XXXXXXXXXX
     FileSet="Test Files"
     Messages = Standard
     Storage = File
     Pool = File
     Schedule = "30 minute cycle"
   }
   # Definition of file storage device
   Storage {
     Name = File
     Address = XXXXXXXXXXX
     SDPort = 9103
     Password = XXXXXXXXXXXXX
     Device = FileStorage
     Media Type = File
   }
   FileSet {
     Name = "File Set"
     Include {
       Options {
         signature=MD5
       }
       File = fffffffffffffffff
     }
     Exclude  { File=*.o }
   }
   Pool {
     Name = File
     Use Volume Once = yes
     Pool Type = Backup
     LabelFormat = "File"
     AutoPrune = yes
     VolumeRetention = 4h
     Maximum Volumes = 12
     Recycle = yes
   }



Where you will need to replace the ffffffffff’s by the appropriate files to be saved for your configuration. For the FileSet Include, choose a directory that has one or two megabytes maximum since there will probably be approximately eight copies of the directory that Bareos will cycle through.

In addition, you will need to add the following to your Storage daemon’s configuration file:



::

   Device {
     Name = FileStorage
     Media Type = File
     Archive Device = /tmp
     LabelMedia = yes;
     Random Access = Yes;
     AutomaticMount = yes;
     RemovableMedia = no;
     AlwaysOpen = no;
   }



With the above resources, Bareos will start a Job every half hour that saves a copy of the directory you chose to /tmp/File0001 ... /tmp/File0012. After 4 hours, Bareos will start recycling the backup Volumes (/tmp/File0001 ...). You should see this happening in the output produced. Bareos will automatically create the Volumes (Files) the first time it uses them.

To turn it off, either delete all the resources you’ve added, or simply comment out the Schedule record in the Job resource.

.. _manualrecycling:

Manually Recycling Volumes
~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Volume; Recycle; Manual>` :index:`\ <single: Recycle; Manual>`

Although automatic recycling of Volumes is implemented (see the :ref:`RecyclingChapter` chapter of this manual), you may want to manually force reuse (recycling) of a Volume.

Assuming that you want to keep the Volume name, but you simply want to write new data on the tape, the steps to take are:

-  Use the :bcommand:`update volume` command in the Console to ensure that **Recycle = yes**.

-  Use the :bcommand:`purge jobs volume` command in the Console to mark the Volume as **Purged**. Check by using :bcommand:`list volumes`.

Once the Volume is marked Purged, it will be recycled the next time a Volume is needed.

If you wish to reuse the tape by giving it a new name, use the :bcommand:`relabel` instead of the :bcommand:`purge` command.



   .. warning::

      The :bcommand:`delete` command can be dangerous. Once it is
   done, to recover the File records, you must either restore your database as it
   was before the :bcommand:`delete` command or use the :ref:`bscan` utility program to
   scan the tape and recreate the database entries.




