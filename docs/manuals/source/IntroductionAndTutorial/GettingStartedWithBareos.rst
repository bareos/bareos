.. _QuickStartChapter:

Getting Started with Bareos
===========================

Understanding Jobs and Schedules
--------------------------------

:index:`\ <single: Schedule; Understanding Schedules>`\

.. _JobsandSchedules:



In order to make Bareos as flexible as possible, the directions given to Bareos are specified in several pieces. The main instruction is the job resource, which defines a job. A backup job generally consists of a FileSet, a Client, a Schedule for one or several levels or times of backups, a Pool, as well as additional instructions. Another way of looking at it is the FileSet is what to backup; the Client is who to backup; the Schedule defines when, and the Pool defines where (i.e. what Volume).

Typically one FileSet/Client combination will have one corresponding job. Most of the directives, such as FileSets, Pools, Schedules, can be mixed and matched among the jobs. So you might have two different Job definitions (resources) backing up different servers using the same Schedule, the same Fileset (backing up the same directories on two machines) and maybe even the same Pools. The Schedule will define what type of backup will run when (e.g. Full on Monday, incremental the rest of the
week), and when more than one job uses the same schedule, the job priority determines which actually runs first. If you have a lot of jobs, you might want to use JobDefs, where you can set defaults for the jobs, which can then be changed in the job resource, but this saves rewriting the identical parameters for each job. In addition to the FileSets you want to back up, you should also have a job that backs up your catalog.

Finally, be aware that in addition to the backup jobs there are restore, verify, and admin jobs, which have different requirements.

Understanding Pools, Volumes and Labels
---------------------------------------

:index:`\ <single: Pools; Understanding>`\  :index:`\ <single: Volumes; Understanding>`\  :index:`\ <single: Label; Understanding Labels>`\

.. _PoolsVolsLabels:



If you have been using a program such as :command:`tar` to backup your system, Pools, Volumes, and labeling may be a bit confusing at first. A Volume is a single physical tape (or possibly a single file) on which Bareos will write your backup data. Pools group together Volumes so that a backup is not restricted to the length of a single Volume (tape). Consequently, rather than explicitly naming Volumes in your Job, you specify a Pool, and Bareos will select the next appendable Volume
from the Pool and mounts it.

Although the basic Pool options are specified in the Director’s :ref:`Pool <DirectorResourcePool>` resource, the real Pool is maintained in the Bareos Catalog. It contains information taken from the Pool resource (configuration file) as well as information on all the Volumes that have been added to the Pool.

For each Volume, Bareos maintains a fair amount of catalog information such as the first write date/time, the last write date/time, the number of files on the Volume, the number of bytes on the Volume, the number of Mounts, etc.

Before Bareos will read or write a Volume, the physical Volume must have a Bareos software label so that Bareos can be sure the correct Volume is mounted. Depending on your configuration, this is either done automatically by Bareos or manually using the :bcommand:`label` command in the Console program.

The steps for creating a Pool, adding Volumes to it, and writing software labels to the Volumes, may seem tedious at first, but in fact, they are quite simple to do, and they allow you to use multiple Volumes (rather than being limited to the size of a single tape). Pools also give you significant flexibility in your backup process. For example, you can have a "Daily" Pool of Volumes for Incremental backups and a "Weekly" Pool of Volumes for Full backups. By specifying the appropriate Pool in
the daily and weekly backup Jobs, you thereby insure that no daily Job ever writes to a Volume in the Weekly Pool and vice versa, and Bareos will tell you what tape is needed and when.

For more on Pools, see the :ref:`DirectorResourcePool` section of the Director Configuration chapter, or simply read on, and we will come back to this subject later.

.. _config:

Setting Up Bareos Configuration Files
-------------------------------------

:index:`\ <single: Configuration; Files>`\

On Unix, Bareos configuration files are usually located in the :file:`/etc/bareos/` directory and are named accordingly to the programs that use it. Since Bareos :sinceVersion:`16.2.4: Subdirectory Configuration Scheme used as Default` the default configuration is stored as one file per resource in subdirectories under :file:`bareos-dir.d`, :file:`bareos-sd.d` or :file:`bareos-fd.d`. For details, see
:ref:`ConfigureChapter` and :ref:`section-SubdirectoryConfigurationScheme`.

Testing your Configuration Files
--------------------------------

:index:`\ <single: Testing; Configuration Files>`\

You can test if your configuration file is syntactically correct by running the appropriate daemon with the :strong:`-t` option. The daemon will process the configuration file and print any error messages then terminate.

As the |dir| and |sd| runs as user **bareos**, testing the configuration should be done as **bareos**.

This is especially required to test the |dir|, as it also connects to the database and checks if the catalog schema version is correct. Depending on your database, only the **bareos** has permission to access it.

.. code-block:: shell-session
   :caption: Testing Configuration Files

   su bareos -s /bin/sh -c "/usr/sbin/bareos-dir -t"
   su bareos -s /bin/sh -c "/usr/sbin/bareos-sd -t"
   bareos-fd -t
   bconsole -t
   bareos-tray-monitor -t
