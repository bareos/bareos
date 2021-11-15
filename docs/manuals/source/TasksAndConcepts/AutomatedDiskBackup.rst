.. _PoolsChapter:

Automated Disk Backup
=====================

.. index::
   single: Volumes; Using Pools
   single: Disk; Automated Backup
   single: Automated Disk Backup
   single: Pool

If you manage five or ten machines and have a nice tape backup, you don’t need Pools, and you may wonder what they are good for. In this chapter, you will see that Pools can help you optimize disk storage space. The same techniques can be applied to a shop that has multiple tape drives, or that wants to mount various different Volumes to meet their needs.

The rest of this chapter will give an example involving backup to disk Volumes, but most of the information applies equally well to tape Volumes.

Given is a scenario, where the size of a full backup is about 15GB.

It is required, that backup data is available for six months. Old files should be available on a daily basis for a week, a weekly basis for a month, then monthly for six months. In addition, offsite capability is not needed. The daily changes amount to about 300MB on the average, or about 2GB per week.

As a consequence, the total volume of data they need to keep to meet their needs is about 100GB (15GB x 6 + 2GB x 5 + 0.3 x 7) = 102.1GB.

The chosen solution was to use a 120GB hard disk – far less than 1/10th the price of a tape drive and the cassettes to handle the same amount of data, and to have the backup software write to disk files.

The rest of this chapter will explain how to setup Bareos so that it would automatically manage a set of disk files with the minimum sysadmin intervention.

.. _OverallDesign:

Overall Design
--------------

Getting Bareos to write to disk rather than tape in the simplest case is rather easy.

One needs to consider about what happens if we have only a single large Bareos Volume defined on our hard disk. Everything works fine until the Volume fills, then Bareos will ask you to mount a new Volume. This same problem applies to the use of tape Volumes if your tape fills. Being a hard disk and the only one you have, this will be a bit of a problem. It should be obvious that it is better to use a number of smaller Volumes and arrange for Bareos to automatically recycle them so that the disk
storage space can be reused.

As mentioned, the solution is to have multiple Volumes, or files on the disk. To do so, we need to limit the use and thus the size of a single Volume, by time, by number of jobs, or by size. Any of these would work, but we chose to limit the use of a single Volume by putting a single job in each Volume with the exception of Volumes containing Incremental backup where there will be 6 jobs (a week’s worth of data) per volume. The details of this will be discussed shortly. This is a single client
backup, so if you have multiple clients you will need to multiply those numbers by the number of clients, or use a different system for switching volumes, such as limiting the volume size.

.. TODO: This chapter will get rewritten. Instead of limiting a Volume to one job, we will utilize ``Max Use Duration = 24 hours``\ . This prevents problems when adding more clients, because otherwise each job has to run seperat.

The next problem to resolve is recycling of Volumes. As you noted from above, the requirements are to be able to restore monthly for 6 months, weekly for a month, and daily for a week. So to simplify things, why not do a Full save once a month, a Differential save once a week, and Incremental saves daily. Now since each of these different kinds of saves needs to remain valid for differing periods, the simplest way to do this (and possibly the only) is to have a separate Pool for each backup
type.

The decision was to use three Pools: one for Full saves, one for Differential saves, and one for Incremental saves, and each would have a different number of volumes and a different Retention period to accomplish the requirements.



.. _FullPool:



Full Pool
~~~~~~~~~

:index:`\ <single: Pool; Full>`\  :index:`\ <single: Full Pool>`\

Putting a single Full backup on each Volume, will require six Full save Volumes, and a retention period of six months. The Pool needed to do that is:

.. code-block:: bareosconfig
   :caption: Full-Pool

   Pool {
     Name = Full-Pool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 6 months
     Maximum Volume Jobs = 1
     Label Format = Full-
     Maximum Volumes = 9
   }

Since these are disk Volumes, no space is lost by having separate Volumes for each backup (done once a month in this case). The items to note are the retention period of six months (i.e. they are recycled after six months), that there is one job per volume (Maximum Volume Jobs = 1), the volumes will be labeled Full-0001, ... Full-0006 automatically. One could have labeled these manually from the start, but why not use the features of Bareos.

Six months after the first volume is used, it will be subject to pruning and thus recycling, so with a maximum of 9 volumes, there should always be 3 volumes available (note, they may all be marked used, but they will be marked purged and recycled as needed).

If you have two clients, you would want to set Maximum Volume Jobs to 2 instead of one, or set a limit on the size of the Volumes, and possibly increase the maximum number of Volumes.



.. _DiffPool:



Differential Pool
~~~~~~~~~~~~~~~~~

:index:`\ <single: Pool; Differential>`\  :index:`\ <single: Differential Pool>`\

For the Differential backup Pool, we choose a retention period of a bit longer than a month and ensure that there is at least one Volume for each of the maximum of five weeks in a month. So the following works:

.. code-block:: bareosconfig
   :caption: Differential Pool

   Pool {
     Name = Diff-Pool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 40 days
     Maximum Volume Jobs = 1
     Label Format = Diff-
     Maximum Volumes = 10
   }

As you can see, the Differential Pool can grow to a maximum of 9 volumes, and the Volumes are retained 40 days and thereafter they can be recycled. Finally there is one job per volume. This, of course, could be tightened up a lot, but the expense here is a few GB which is not too serious.

If a new volume is used every week, after 40 days, one will have used 7 volumes, and there should then always be 3 volumes that can be purged and recycled.

See the discussion above concering the Full pool for how to handle multiple clients.



.. _IncPool:



Incremental Pool
~~~~~~~~~~~~~~~~

:index:`\ <single: Incremental Pool>`\  :index:`\ <single: Pool; Incremental>`\

Finally, here is the resource for the Incremental Pool:

.. code-block:: bareosconfig
   :caption: Incremental Pool

   Pool {
     Name = Inc-Pool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 20 days
     Maximum Volume Jobs = 6
     Label Format = Inc-
     Maximum Volumes = 7
   }

We keep the data for 20 days rather than just a week as the needs require. To reduce the proliferation of volume names, we keep a week’s worth of data (6 incremental backups) in each Volume. In practice, the retention period should be set to just a bit more than a week and keep only two or three volumes instead of five. Again, the lost is very little and as the system reaches the full steady state, we can adjust these values so that the total disk usage doesn’t exceed the disk capacity.

If you have two clients, the simplest thing to do is to increase the maximum volume jobs from 6 to 12. As mentioned above, it is also possible limit the size of the volumes. However, in that case, you will need to have a better idea of the volume or add sufficient volumes to the pool so that you will be assured that in the next cycle (after 20 days) there is at least one volume that is pruned and can be recycled.

Configuration Files
-------------------

The following example shows you the actual files used, with only a few minor modifications to simplify things.

The Director’s configuration file is as follows:

.. code-block:: bareosconfig
   :caption: bareos-dir.conf

   Director {          # define myself
     Name = bareos-dir
     QueryFile = "/usr/lib/bareos/scripts/query.sql"
     Maximum Concurrent Jobs = 1
     Password = "*** CHANGE ME ***"
     Messages = Standard
   }

   JobDefs {
     Name = "DefaultJob"
     Type = Backup
     Level = Incremental
     Client = bareos-fd
     FileSet = "Full Set"
     Schedule = "WeeklyCycle"
     Storage = File
     Messages = Standard
     Pool = Inc-Pool
     Full Backup Pool = Full-Pool
     Incremental Backup Pool = Inc-Pool
     Differential Backup Pool = Diff-Pool
     Priority = 10
     Write Bootstrap = "/var/lib/bareos/%c.bsr"
   }

   Job {
     Name = client
     Client = client-fd
     JobDefs = "DefaultJob"
     FileSet = "Full Set"
   }

   # Backup the catalog database (after the nightly save)
   Job {
     Name = "BackupCatalog"
     Client = client-fd
     JobDefs = "DefaultJob"
     Level = Full
     FileSet="Catalog"
     Schedule = "WeeklyCycleAfterBackup"
     # This creates an ASCII copy of the catalog
     # Arguments to make_catalog_backup.pl are:
     #  make_catalog_backup.pl <catalog-name>
     RunBeforeJob = "/usr/lib/bareos/scripts/make_catalog_backup.pl MyCatalog"
     # This deletes the copy of the catalog
     RunAfterJob  = "/usr/lib/bareos/scripts/delete_catalog_backup"
     # This sends the bootstrap via mail for disaster recovery.
     # Should be sent to another system, please change recipient accordingly
     Write Bootstrap = "|/usr/sbin/bsmtp -h localhost -f \"\(Bareos\) \" -s \"Bootstrap for Job %j\" root@localhost"
     Priority = 11                   # run after main backup
   }

   # Standard Restore template, to be changed by Console program
   Job {
     Name = "RestoreFiles"
     Type = Restore
     Client = client-fd
     FileSet="Full Set"
     Storage = File
     Messages = Standard
     Pool = Default
     Where = /tmp/bareos-restores
   }

   # List of files to be backed up
   FileSet {
     Name = "Full Set"
     Include = {
       Options {
         signature=SHA1;
         compression=GZIP9
       }
       File = /
       File = /usr
       File = /home
       File = /boot
       File = /var
       File = /opt
     }
     Exclude = {
       File = /proc
       File = /tmp
       File = /.journal
       File = /.fsck
       ...
     }
   }

   Schedule {
     Name = "WeeklyCycle"
     Run = Level=Full 1st sun at 2:05
     Run = Level=Differential 2nd-5th sun at 2:05
     Run = Level=Incremental mon-sat at 2:05
   }

   # This schedule does the catalog. It starts after the WeeklyCycle
   Schedule {
     Name = "WeeklyCycleAfterBackup"
     Run = Level=Full sun-sat at 2:10
   }

   # This is the backup of the catalog
   FileSet {
     Name = "Catalog"
     Include {
       Options {
         signature = MD5
       }
       File = "/var/lib/bareos/bareos.sql" # database dump
       File = "/etc/bareos"                # configuration
     }
   }

   Client {
     Name = client-fd
     Address = client
     FDPort = 9102
     Password = " *** CHANGE ME ***"
     AutoPrune = yes      # Prune expired Jobs/Files
     Job Retention = 6 months
     File Retention = 60 days
   }

   Storage {
     Name = File
     Address = localhost
     Password = " *** CHANGE ME ***"
     Device = FileStorage
     Media Type = File
   }

   Catalog {
     Name = MyCatalog
     dbname = bareos; user = bareos; password = ""
   }

   Pool {
     Name = Full-Pool
     Pool Type = Backup
     Recycle = yes           # automatically recycle Volumes
     AutoPrune = yes         # Prune expired volumes
     Volume Retention = 6 months
     Maximum Volume Jobs = 1
     Label Format = Full-
     Maximum Volumes = 9
   }

   Pool {
     Name = Inc-Pool
     Pool Type = Backup
     Recycle = yes           # automatically recycle Volumes
     AutoPrune = yes         # Prune expired volumes
     Volume Retention = 20 days
     Maximum Volume Jobs = 6
     Label Format = Inc-
     Maximum Volumes = 7
   }

   Pool {
     Name = Diff-Pool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 40 days
     Maximum Volume Jobs = 1
     Label Format = Diff-
     Maximum Volumes = 10
   }

   Messages {
     Name = Standard
     mailcommand = "bsmtp -h mail.domain.com -f \"\(Bareos\) %r\"
         -s \"Bareos: %t %e of %c %l\" %r"
     operatorcommand = "bsmtp -h mail.domain.com -f \"\(Bareos\) %r\"
         -s \"Bareos: Intervention needed for %j\" %r"
     mail = root@domain.com = all, !skipped
     operator = root@domain.com = mount
     console = all, !skipped, !saved
     append = "/home/bareos/bin/log" = all, !skipped
   }

and the Storage daemon’s configuration file is:

.. code-block:: bareosconfig

   Storage {               # definition of myself
     Name = bareos-sd
   }

   Director {
     Name = bareos-dir
     Password = "*** CHANGE ME ***"
   }

   Device {
     Name = FileStorage
     Media Type = File
     Archive Device = /var/lib/bareos/storage
     LabelMedia = yes;    # lets Bareos label unlabeled media
     Random Access = yes;
     AutomaticMount = yes;   # when device opened, read it
     RemovableMedia = no;
     AlwaysOpen = no;
   }

   Messages {
     Name = Standard
     director = bareos-dir = all
   }
