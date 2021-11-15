.. _StrategiesChapter:

Using Tape Drives without Autochanger
=====================================

:index:`\ <single: Strategy; Backup>`\  :index:`\ <single: Backup Strategies>`\

Although Recycling and Backing Up to Disk Volume have been discussed in previous chapters, this chapter is meant to give you an overall view of possible backup strategies and to explain their advantages and disadvantages.

.. _Simple:



Simple One Tape Backup
----------------------

:index:`\ <single: Backup; One Tape>`\  :index:`\ <single: One Tape Backup>`\

Probably the simplest strategy is to back everything up to a single tape and insert a new (or recycled) tape when it fills and Bareos requests a new one.

Advantages
~~~~~~~~~~

-  The operator intervenes only when a tape change is needed (e.g. once a month).

-  There is little chance of operator error because the tape is not changed daily.

-  A minimum number of tapes will be needed for a full restore. Typically the best case will be one tape and worst two.

-  You can easily arrange for the Full backup to occur a different night of the month for each system, thus load balancing and shortening the backup time.

Disadvantages
~~~~~~~~~~~~~

-  If your site burns down, you will lose your current backups

-  After a tape fills and you have put in a blank tape, the backup will continue, and this will generally happen during working hours.

Practical Details
~~~~~~~~~~~~~~~~~

This system is very simple. When the tape fills and Bareos requests a new tape, you unmount the tape from the Console program, insert a new tape and label it. In most cases after the label, Bareos will automatically mount the tape and resume the backup. Otherwise, you simply mount the tape.

Using this strategy, one typically does a Full backup once a week followed by daily Incremental backups. To minimize the amount of data written to the tape, one can do a Full backup once a month on the first Sunday of the month, a Differential backup on the 2nd-5th Sunday of the month, and incremental backups the rest of the week.

.. _Manual:



Manually Changing Tapes
-----------------------

:index:`\ <single: Tape; Manually Changing>`\

If you use the strategy presented above, Bareos will ask you to change the tape, and you will unmount it and then remount it when you have inserted the new tape.

If you do not wish to interact with Bareos to change each tape, there are several ways to get Bareos to release the tape:

-  In your Storage daemon’s Device resource, set :strong:`AlwaysOpen = no`\ . In this case, Bareos will release the tape after every job. If you run several jobs, the tape will be rewound and repositioned to the end at the beginning of every job. This is not very efficient, but does let you change the tape whenever you want.

-  Use a RunAfterJob statement to run a script after your last job. This could also be an Admin job that runs after all your backup jobs. The script could be something like:



   ::

            #!/bin/sh
            bconsole <<END_OF_DATA
            release storage=your-storage-name
            END_OF_DATA



   In this example, you would have AlwaysOpen=yes, but the release command would tell Bareos to rewind the tape and on the next job assume the tape has changed. This strategy may not work on some systems, or on autochangers because Bareos will still keep the drive open.

-  The final strategy is similar to the previous case except that you would use the unmount command to force Bareos to release the drive. Then you would eject the tape, and remount it as follows:



   ::

            #!/bin/sh
            bconsole <<END_OF_DATA
            unmount storage=your-storage-name
            END_OF_DATA

            # the following is a shell command
            mt eject

            bconsole <<END_OF_DATA
            mount storage=your-storage-name
            END_OF_DATA





.. _Daily:



Daily Tape Rotation
-------------------

:index:`\ <single: Rotation; Daily Tape>`\  :index:`\ <single: Daily Tape Rotation>`\

This scheme is quite different from the one mentioned above in that a Full backup is done to a different tape every day of the week. Generally, the backup will cycle continuously through five or six tapes each week. Variations are to use a different tape each Friday, and possibly at the beginning of the month. Thus if backups are done Monday through Friday only, you need only five tapes, and by having two Friday tapes, you need a total of six tapes. Many sites run this way, or using
modifications of it based on two week cycles or longer.

.. _advantages-1:

Advantages of Daily Tape Rotation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  All the data is stored on a single tape, so recoveries are simple and faster.

-  Assuming the previous day’s tape is taken offsite each day, a maximum of one days data will be lost if the site burns down.

.. _disadvantages-1:

Disadvantages of Daily Tape Rotation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  The tape must be changed every day requiring a lot of operator intervention.

-  More errors will occur because of human mistakes.

-  If the wrong tape is inadvertently mounted, the Backup for that day will not occur exposing the system to data loss.

-  There is much more movement of the tape each day (rewinds) leading to shorter tape drive life time.

-  Initial setup of Bareos to run in this mode is more complicated than the Single tape system described above.

-  Depending on the number of systems you have and their data capacity, it may not be possible to do a Full backup every night for time reasons or reasons of tape capacity.

.. _practical-details-1:

Practical Details of Daily Tape Rotation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest way to "force" Bareos to use a different tape each day is to define a different Pool for each day of the the week a backup is done. In addition, you will need to specify appropriate Job and File retention periods so that Bareos will relabel and overwrite the tape each week rather than appending to it. Nic Bellamy has supplied an actual working model of this which we include here.

What is important is to create a different Pool for each day of the week, and on the run statement in the Schedule, to specify which Pool is to be used. He has one Schedule that accomplishes this, and a second Schedule that does the same thing for the Catalog backup run each day after the main backup (Priorities were not available when this script was written). In addition, he uses a Max Start Delay of 22 hours so that if the wrong tape is premounted by the operator, the job will be
automatically canceled, and the backup cycle will re-synchronize the next day. He has named his Friday Pool WeeklyPool because in that Pool, he wishes to have several tapes to be able to restore to a time older than one week.

And finally, in his Storage daemon’s Device resource, he has Automatic Mount = yes and Always Open = No. This is necessary for the tape ejection to work in his end_of_backup.sh script below.

For example, his bareos-dir.conf file looks like the following:



::


   # /etc/bareos/bareos-dir.conf
   #
   # Bareos Director Configuration file
   #
   Director {
     Name = ServerName
     DIRport = 9101
     QueryFile = "/etc/bareos/query.sql"
     Maximum Concurrent Jobs = 1
     Password = "console-pass"
     Messages = Standard
   }
   #
   # Define the main nightly save backup job
   #
   Job {
     Name = "NightlySave"
     Type = Backup
     Client = ServerName
     FileSet = "Full Set"
     Schedule = "WeeklyCycle"
     Storage = Tape
     Messages = Standard
     Pool = Default
     Write Bootstrap = "/var/lib/bareos/NightlySave.bsr"
     Max Start Delay = 22h
   }
   # Backup the catalog database (after the nightly save)
   Job {
     Name = "BackupCatalog"
     Type = Backup
     Client = ServerName
     FileSet = "Catalog"
     Schedule = "WeeklyCycleAfterBackup"
     Storage = Tape
     Messages = Standard
     Pool = Default
     # This creates an ASCII copy of the catalog
     # WARNING!!! Passing the password via the command line is insecure.
     # see comments in make_catalog_backup for details.
     RunBeforeJob = "/usr/lib/bareos/make_catalog_backup -u bareos"
     # This deletes the copy of the catalog, and ejects the tape
     RunAfterJob  = "/etc/bareos/end_of_backup.sh"
     Write Bootstrap = "/var/lib/bareos/BackupCatalog.bsr"
     Max Start Delay = 22h
   }
   # Standard Restore template, changed by Console program
   Job {
     Name = "RestoreFiles"
     Type = Restore
     Client = ServerName
     FileSet = "Full Set"
     Storage = Tape
     Messages = Standard
     Pool = Default
     Where = /tmp/bareos-restores
   }
   # List of files to be backed up
   FileSet {
     Name = "Full Set"
     Include = signature=MD5 {
       /
       /data
     }
     Exclude = { /proc /tmp /.journal }
   }
   #
   # When to do the backups
   #
   Schedule {
     Name = "WeeklyCycle"
     Run = Level=Full Pool=MondayPool Monday at 8:00pm
     Run = Level=Full Pool=TuesdayPool Tuesday at 8:00pm
     Run = Level=Full Pool=WednesdayPool Wednesday at 8:00pm
     Run = Level=Full Pool=ThursdayPool Thursday at 8:00pm
     Run = Level=Full Pool=WeeklyPool Friday at 8:00pm
   }
   # This does the catalog. It starts after the WeeklyCycle
   Schedule {
     Name = "WeeklyCycleAfterBackup"
     Run = Level=Full Pool=MondayPool Monday at 8:15pm
     Run = Level=Full Pool=TuesdayPool Tuesday at 8:15pm
     Run = Level=Full Pool=WednesdayPool Wednesday at 8:15pm
     Run = Level=Full Pool=ThursdayPool Thursday at 8:15pm
     Run = Level=Full Pool=WeeklyPool Friday at 8:15pm
   }
   # This is the backup of the catalog
   FileSet {
     Name = "Catalog"
     Include = signature=MD5 {
        /var/lib/bareos/bareos.sql
     }
   }
   # Client (File Services) to backup
   Client {
     Name = ServerName
     Address = dionysus
     FDPort = 9102
     Password = "client-pass"
     File Retention = 30d
     Job Retention = 30d
     AutoPrune = yes
   }
   # Definition of file storage device
   Storage {
     Name = Tape
     Address = dionysus
     SDPort = 9103
     Password = "storage-pass"
     Device = Tandberg
     Media Type = MLR1
   }
   # Generic catalog service
   Catalog {
     Name = MyCatalog
     dbname = bareos; user = bareos; password = ""
   }
   # Reasonable message delivery -- send almost all to email address
   #  and to the console
   Messages {
     Name = Standard
     mailcommand = "/usr/sbin/bsmtp -h localhost -f \"\(Bareos\) %r\" -s \"Bareos: %t %e of %c %l\" %r"
     operatorcommand = "/usr/sbin/bsmtp -h localhost -f \"\(Bareos\) %r\" -s \"Bareos: Intervention needed for %j\" %r"
     mail = root@localhost = all, !skipped
     operator = root@localhost = mount
     console = all, !skipped, !saved
     append = "/var/lib/bareos/log" = all, !skipped
   }

   # Pool definitions
   #
   # Default Pool for jobs, but will hold no actual volumes
   Pool {
     Name = Default
     Pool Type = Backup
   }
   Pool {
     Name = MondayPool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 6d
     Maximum Volume Jobs = 2
   }
   Pool {
     Name = TuesdayPool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 6d
     Maximum Volume Jobs = 2
   }
   Pool {
     Name = WednesdayPool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 6d
     Maximum Volume Jobs = 2
   }
   Pool {
     Name = ThursdayPool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 6d
     Maximum Volume Jobs = 2
   }
   Pool {
     Name = WeeklyPool
     Pool Type = Backup
     Recycle = yes
     AutoPrune = yes
     Volume Retention = 12d
     Maximum Volume Jobs = 2
   }
   # EOF



In order to get Bareos to release the tape after the nightly backup, this setup uses a RunAfterJob script that deletes the database dump and then rewinds and ejects the tape. The following is a copy of end_of_backup.sh



::

   #! /bin/sh
   /usr/lib/bareos/delete_catalog_backup
   mt rewind
   mt eject
   exit 0



Finally, if you list his Volumes, you get something like the following:



::

   *list media
   Using default Catalog name=MyCatalog DB=bareos
   Pool: WeeklyPool
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | MeId| VolumeName| MedTyp| VolStat| VolBytes  | LastWritten     | VolRet| Recyc|
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | 5   | Friday_1  | MLR1  | Used   | 2157171998| 2003-07-11 20:20| 103680| 1    |
   | 6   | Friday_2  | MLR1  | Append | 0         | 0               | 103680| 1    |
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   Pool: MondayPool
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | MeId| VolumeName| MedTyp| VolStat| VolBytes  | LastWritten     | VolRet| Recyc|
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | 2   | Monday    | MLR1  | Used   | 2260942092| 2003-07-14 20:20| 518400| 1    |
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   Pool: TuesdayPool
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | MeId| VolumeName| MedTyp| VolStat| VolBytes  | LastWritten     | VolRet| Recyc|
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | 3   | Tuesday   | MLR1  | Used   | 2268180300| 2003-07-15 20:20| 518400| 1    |
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   Pool: WednesdayPool
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | MeId| VolumeName| MedTyp| VolStat| VolBytes  | LastWritten     | VolRet| Recyc|
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | 4   | Wednesday | MLR1  | Used   | 2138871127| 2003-07-09 20:2 | 518400| 1    |
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   Pool: ThursdayPool
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | MeId| VolumeName| MedTyp| VolStat| VolBytes  | LastWritten     | VolRet| Recyc|
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   | 1   | Thursday  | MLR1  | Used   | 2146276461| 2003-07-10 20:50| 518400| 1    |
   +-----+-----------+-------+--------+-----------+-----------------+-------+------+
   Pool: Default
   No results to list.
