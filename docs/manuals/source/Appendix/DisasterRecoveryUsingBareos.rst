.. _RescueChapter:

Disaster Recovery Using Bareos
==============================

.. index::
   single: Disaster; Recovery
   single: Recovery; Disaster Recovery

General
-------

When disaster strikes, you must have a plan, and you must have prepared in advance, otherwise the work of recovering your system and your files will be considerably greater. For example, if you have not previously saved the partitioning information for your hard disk, how can you properly rebuild it if the disk must be replaced?

Unfortunately, many of the steps one must take before and immediately after a disaster are very much dependent on the operating system in use. As a consequence, this chapter will discuss in detail disaster recovery only for selected operating systems.

Important Considerations
~~~~~~~~~~~~~~~~~~~~~~~~

Here are a few important considerations concerning disaster recovery that you should take into account before a disaster strikes.

-  If the building which houses your computers burns down or is otherwise destroyed, do you have off-site backup data?

-  Disaster recovery is much easier if you have several machines. If you have a single machine, how will you handle unforeseen events if your only machine is down?

-  Do you want to protect your whole system and use Bareos to recover everything? Or do you want to try to restore your system from the original installation disks, apply any other updates and only restore the user files?

.. _section-before-disaster:

Steps to Take Before Disaster Strikes
-------------------------------------

-  Ensure that you always have a valid bootstrap file for your backup and that it is saved to an alternate machine. This will permit you to easily do a full restore of your system.

-  If possible copy your catalog nightly to an alternate machine. If you have a valid bootstrap file, this is not necessary, but can be very useful if you do not want to reload everything.

-  Ensure that you always have a valid :ref:`bootstrap <BootstrapChapter>` file for your catalog backup that is saved to an alternate machine. This will permit you to restore your catalog more easily if needed.

-  Try out how to use the Rescue CDROM before you are forced to use it in an emergency situation.

-  Make a copy of your Bareos .conf files, particularly your bareos-dir  and your bareos-sd configuration files, because if your server goes down, these files will be needed to get it back up and running, and they can be difficult to rebuild from memory.


.. _section-BareMetalRestoreClient:

Bare Metal Recovery of Bareos Clients
-------------------------------------

A so called "Bare Metal" recovery is one where you start with an empty hard disk and you restore your machine.

Generally, following components are required for a Bare Metal Recovery:

*  A rescue boot image (e.g. CDROM or network boot) containing a copy of your OS and including the Bareos File daemon, including all libraries
*  The Bareos client configuration files
*  Useful: a copy of your hard disk information
*  A full Bareos backup of your system


.. _section-rear:

Linux
~~~~~

.. index::
   single: Disaster; Recovery; Linux

From the Relax-and-Recover web site (`https://relax-and-recover.org <https://relax-and-recover.org>`_):

   Relax-and-Recover is a setup-and-forget Linux bare metal disaster recovery solution. It is easy to set up and requires no maintenance so there is no excuse for not using it.

Relax-and-Recover (ReaR) is quite easy to use with Bareos.

Installation
^^^^^^^^^^^^

For Bareos >= 22, ReaR >= 2.8 is required.
If ReaR >= 2.8 is not part of your distribution, check the `download section on the
ReaR website <https://relax-and-recover.org/download/>`_.

Configuration
^^^^^^^^^^^^^

For ReaR to work with Bareos, the following packages must be installed:

* **rear** >= 2.8
* **bareos-fd**
* **bareos-bconsole**
* **jq**

Make sure you have a working |fd| configuration on the system and already did a backup of all disks you want to restore (the fileset has to include all data required to fully restore the system. A good starting point is :config:option:`dir/fileset = LinuxAll`).
You also need the |bconsole| installed and configured,
so that it can connect to the |dir|.
Ideally the |bconsole| is configured in a way that it can only access this client,
see :ref:`section-named-console-for-rear`.

To configure the Bareos backend, ReaR uses the variables
``BAREOS_CLIENT``, ``BAREOS_FILESET`` and ``BAREOS_RESTORE_JOB``.
It tries to detect them automatically,
however it is recommend to set them explicitly to avoid surprises in case the |dir| configuration changes.
All required settings can be specified in a ReaR configuration file,
normally :file:`/etc/rear/local.conf`.
Please refer to the `ReaR documentation <https://relax-and-recover.org/documentation/>`_ for details.

Assuming the system you want to protect with ReaR is referenced by Bareos as :strong:`bareosclient-fd`,
you use :strong:`LinuxAll` as backup fileset
and the default restore job (:strong:`RestoreFiles`) is sufficient for you,
your ReaR configuration will look like this:

.. code-block:: cfg
   :caption: :file:`/etc/rear/local.conf` (minimal)

   BACKUP=BAREOS
   BAREOS_CLIENT=bareosclient-fd
   BAREOS_FILESET=LinuxAll
   BAREOS_RESTORE_JOB=RestoreFiles


If you want to create an ISO image and store it on an NFS server with the IP address 192.168.10.1, you can use the following configuration:

.. code-block:: cfg
   :caption: :file:`/etc/rear/local.conf` (with OUTPUT_URL)

   # This is default:
   #OUTPUT=ISO
   # Where to write the iso image
   # You can use NFS, if you want to write your iso image to a nfs server
   # If you leave this blank, it will
   # be written to: /var/lib/rear/output/
   OUTPUT_URL=nfs://192.168.10.1/rear
   BACKUP=BAREOS
   BAREOS_CLIENT=bareosclient-fd
   BAREOS_FILESET=LinuxAll
   BAREOS_RESTORE_JOB=RestoreFiles


Create Rescue Image
^^^^^^^^^^^^^^^^^^^

If you have installed and configured ReaR on your system, type

.. code-block:: shell-session
   :caption: Create Rescue Image

   root@host:~# rear -v mkrescue

to create the rescue image. If you used the configuration example above, you will get a bootable ISO image file, containing all the relevant programs and tools to do a Bareos restore over the network.

.. warning::

   This will not create a Bareos backup on your system! You will have to do that by
   other means, e.g. by a regular Bareos backup schedule.
   Also :command:`rear mkbackup` will not create a backup.
   In this configuration it will only create the rescue ISO
   (same as the :command:`rear mkrescue` command).

.. note::

   The rescue image is created from the running system.
   It contains the required programs
   (including :command:`bareos-fd` and :command:`bconsole`),
   the relevant configuration,
   information about attached storage devices
   and also the kernel of the running system.
   You should therefore regularly update your rescue images,
   see :ref:`section-rear-retrieve-rescue-image`,
   followed by :ref:`section-rear-create-rescue-image-regularly`.


Recovery
^^^^^^^^

In case, you want to recover your system, boot it using the generated ReaR recovery ISO. After booting log in as user **root** and type

.. code-block:: shell-session
   :caption: Restore your system using ReaR and Bareos

   RESCUE bareosclient:~# rear recover

ReaR detects the attached harddisks and, after confirmation,
recreated the filesystem on them.
For the actual data restore, it again asks for confirmation:

.. code-block:: shell-session

   RESCUE bareosclient:~# rear recover
   [...]
   The system is now ready for a restore via Bareos.

   When choosing 'automatic' a Bareos restore without user interaction
   will be started with following options:
   restore client=bareosclient-fd restorejob="RestoreFiles" fileset="LinuxAll" where=/mnt/local select all done yes

   When choosing 'manual', bconsole will be started
   and let you choose the restore options yourself.
   Keep in mind, that the new root is mounted under '/mnt/local',
   so use where=/mnt/local on restore.
   The bconsole history contains the preconfigured restore command.

   Choose restore mode:
   1) automatic
   2) manual
   (default '1' timeout 300 seconds)


Once selected, the **automatic restore mode** will run the displayed command via :command:`bconsole`.
This will restore the most recent Bareos backup
(from this client with the given fileset)
to your system.

The **manual restore mode** just starts the :command:`bconsole` program
and lets you choose the restore command.
For comfort, the :command:`bconsole` command line history
is already filled with some useful commands,
especially with the :bcommand:`restore` command
that the automatic restore mode would have selected.

When exiting :command:`bconsole`, ReaR tries to determine the restore job
and waits until this job is finished,
while displaying some progress information, like:

.. code-block:: shell-session

   Waiting for restore job 113 to finish.
   Start: [2024-06-28 14:25:24], Duration: [00:00:22], Status: [R], Restored: [2.1G]

and finally:

.. code-block:: shell-session

   Waiting for restore job 113 to finish.
   Start: [2024-06-28 14:25:24], Duration: [00:00:27], Status: [T], Restored: [2.7G] OK
   Information about finished job:
   [...]

   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"
             jobid: 113
               job: RestoreFiles.2024-06-28_14.25.22_15
              name: RestoreFiles
       purgedfiles: 0
              type: R
             level: F
          clientid: 4
            client: bareosclient-fd
         jobstatus: T
         schedtime: 2024-06-28 14:25:22
         starttime: 2024-06-28 14:25:24
           endtime: 2024-06-28 14:25:51
       realendtime: 2024-06-28 14:25:51
          duration: 00:00:27
          jobtdate: 1,719,584,751
      volsessionid: 1
    volsessiontime: 1,719,583,621
          jobfiles: 63,889
          jobbytes: 2,716,580,999
         joberrors: 0
   jobmissingfiles: 0
            poolid: 0
          poolname:
        priorjobid: 0
         filesetid: 0
           fileset:
   Restored 2.7G
   Restore job finished successfully.
   Bareos restore finished.
   Created SELinux /mnt/local/.autorelabel file : after reboot SELinux will relabel all files
   Recreating directories (with permissions) from /var/lib/rear/recovery/directories_permissions_owner_group
   [...]
   Updated initramfs with new drivers for this system.
   Installing GRUB2 boot loader...
   Determining where to install GRUB2 (no GRUB2_INSTALL_DEVICES specified)
   Found possible boot disk /dev/vda - installing GRUB2 there
   Finished 'recover'. The target system is mounted at '/mnt/local'.
   Exiting rear recover (PID 499) and its descendant processes ...
   Running exit tasks
   RESCUE bareosclient:~ #

The restored system can be found under the :file:`/mnt/local` directory.
After restoring the files, ReaR restores the bootloader.
The recovery is now completed and the system can be rebooted.

The ReaR Bareos backend also works in ReaRs **Automatic Recover** mode,
selectable at boot,
which recreates the system without manual interaction.


Advanced ReaR Bareos Backend Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _section-named-console-for-rear:

Configure a Named Console for ReaR
''''''''''''''''''''''''''''''''''

The default :command:`bconsole` configuration on the |dir| offers full access to the |dir|.
This is normally not wanted from a client systems.

Instead it is recommended to configure a :config:option:`dir/console` for every Bareos client using ReaR,
limiting the access to only one system and the required console commands.

As all these consoles share much of their configuration,
best start with a common :config:option:`dir/profile` for them.
:config:option:`dir/profile = rear-client-profile`
is a good starting point for such a profile:

.. literalinclude:: /include/config/rear-client-profile.conf
   :language: bareosconfig
   :caption: :file:`bareos-dir.d/profile/rear-client-profile.conf`


This :config:option:`dir/profile`
limits the access to the required commands.
Access to all clients is denied.
This permission must be given individually in the consoles.

Next, create a :config:option:`dir/console`
for every Bareos client using ReaR:

.. code-block:: bareosconfig
   :caption: :file:`bareos-dir.d/console/bareosclient-console.conf`

   Console {
      Name               = "bareosclient-console"
      Password           = "secret"
      Client ACL         = "bareosclient-fd"
      Profile            = "rear-client-profile"
   }

Create the file on the |dir| and :bcommand:`reload` the configuration.
Of course an individual password should be used.

The corresponding :file:`/etc/bareos/bconsole.conf` on the ReaR client will look like this.

.. code-block:: bareosconfig
   :caption: :file:`/etc/bareos/bconsole.conf`

   Director {
      Name = "bareos-dir"
      Address = "bareos.example.com"
   }

   Console {
      Name = "bareosclient-console"
      Password = "secret"
   }

* :config:option:`console/director/Name`: name of the |dir|.
* :config:option:`console/director/Address`: must be set to the DNS name or IP address of the |dir|.
* :config:option:`console/console/Name`: must correspond to :config:option:`dir/console/Name`.
* :config:option:`console/console/Password`: must correspond to :config:option:`dir/console/Password`.

For more details, see :ref:`section-named-console`.

Make sure (via filesystem permissions) that the file is only readable for required users.
By default that is the user **root** and the members of the group **bareos**.
Access to the rescue image should also be limited to the relevant persons.

.. _section-rear-retrieve-rescue-image:

Storing and Retrieving the ReaR Rescue Image
''''''''''''''''''''''''''''''''''''''''''''

A rescue image is only useful,
if it is available in case of emergency.
ReaR itself offers flexible ways to distribute the rescue image to other systems
when creating a new image.
The default configuration will store the image locally
in the directory :file:`/var/lib/rear/output/`.

As a full system backup will also backup this directory,
it might be sufficient to just store it there and let Bareos back it up.
In case the rescue image is needed,
you have to restore it to another system.

Assuming the following settings,

* system protected by ReaR is :config:option:`dir/client = bareosclient-fd` with fileset :config:option:`dir/fileset = LinuxAll`
* system to restore the rescue image: :config:option:`dir/client = client_with_cd_burner-fd`

the :command:`bconsole` command would be this:

.. code-block:: bconsole
   :caption: restore the a ReaR Rescue Image to another system

   *<input>restore client=bareosclient-fd fileset=LinuxAll current fileregex=/var/lib/rear/output/rear-.* restoreclient=client_with_cd_burner-fd</input>

or if your prefer:

.. code-block:: bconsole
   :caption: restore the a ReaR Rescue Image to another system and adapt the path

   *<input>restore client=bareosclient-fd fileset=LinuxAll current fileregex=/var/lib/rear/output/rear-.* restoreclient=client_with_cd_burner-fd regexwhere=!/var/lib/rear/output/!/tmp/rear-rescue-images/!</input>

Make sure to test this thoroughly, especially if you are using :ref:`DataEncryption`,
as restoring to another system needs special preparations.


.. _section-rear-create-rescue-image-regularly:

Update ReaR Rescue Image Regularly
''''''''''''''''''''''''''''''''''

While a ReaR Rescue Image is quite flexible,
it works best if it matches the current configuration of your system.
Any configuration change (hardware, software or environment/network)
can require an updated rescue image.

This can be automated using various methods, among others:

* periodically (cron)
* on system restart (as a configuration change, like a new kernel version, often requires a reboot)
* Bareos backup job

Doing this via Bareos backup job has two advantages:

#. the rescue image corresponds perfectly to your backup
#. your backup report immediately tells if somethings went wrong with your backup or your restore image.

However, the size of a rescue image is around 200 MB.
Adding this to a daily incremental backup is often unnecessary.

So it often makes sense to wrap the creation of a new rescue image by a script to limit the number of times it gets regenerated.
Limitations could be made based on the backup level or the age of the existing rescue image,
like implemented in the following script with the corresponding :config:option:`dir/job` configuration:

.. code-block:: sh
   :caption: :file:`/usr/local/sbin/update-rear-rescue.sh`

   #!/bin/bash

   set -e
   set -u

   backuplevel="${1:-}"

   REAR_OUTPUT_PATH=/var/lib/rear/output/

   if [ "$backuplevel" = "Full" ]; then
      echo "backup level = $backuplevel: recreating rescue image"
   else
      days=7
      current_rescue_images=( $( find "$REAR_OUTPUT_PATH" -name "rear-*" -mtime -$days ) )
      if  [ ${#current_rescue_images[@]} -gt 0 ]; then
         echo "SKIPPING 'rear mkrescue', as images newer than $days days exists: ${current_rescue_images[@]}"
         exit 0
      else
         echo "no current rescue image found. recreating it"
      fi
   fi

   rear mkrescue
   exit $?


.. code-block:: bareosconfig

   Job {
      ...
      Run Script {
         Runs When = Before
         Runs On Client = yes
         Command = "/usr/local/sbin/update-rear-rescue.sh %l"
      }
   }

Copy the script to :file:`/usr/local/sbin/update-rear-rescue.sh` on the |dir|,
make it executable
and add the Run Script definition to the job intended to backup the data for a ReaR restore.



.. _section-RestoreServer:

Disaster Restoring a Bareos Server
----------------------------------

.. index::
   single: Disaster Recovery; Bareos Server
   single: Restore; Bareos Server

If correctly configured, Bareos protects you from data loss caused by
data deletion or a complete system breakdown. At most, you could lose
the data generated since the last backup.

As long as you still have the data carriers containing your backups, you will
be able to restore your data. Bareos uses the same API for all data
carriers, so the tools will be able to access the data, whether they are stored
on hard drives, tapes or cloud volumes.

Precautionary measures that protect the Bareos system
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first measure you should take in any Bareos installation to prevent
loss of data is to back up the database (the ``Job`` named
``BackupCatalog``). This backup 'must' be configured in every Bareos
installation, and it always runs as the last job of the day so it
includes all jobs of the day.
This job also saves the configuration files of the bareos system.
In the default setup, this job also sends the bootstrap file via email.
Additionally, we strongly suggest to have the current configuration files
also available on a second system.

This bootstrap file allows a very quick recovery of the catalog backup in an
emergency, so you should make sure that the job emails from the catalog backup
are sent to an external server.

The job reports on backups that are sent out via email also contain
important information you may need in an emergency:

-  What is the most recent backup of the ``BackupCatalog`` job?

-  What medium was this backup written to?

Ways to approach a restore
~~~~~~~~~~~~~~~~~~~~~~~~~~

The restore procedure depends on the following factors:

-  How badly damaged is the Bareos system?

-  What data are available for the restore?

In an emergency restore, the first objective is to restore the catalog
database. As soon as the catalog database is restored, all other data
and their storage locations are also available.

Of course an emergency restore of the Bareos system is only possible if
you have storage media containing the necessary data. The degree of
destruction and available restore data determine the restore method. The
aim is always to get the backup system working again in a way as
efficient as possible. Less damage and more available restore data make this
stage quicker to achieve.

Tools
~~~~~

Next to programs for regular operation (director, file daemon, storage daemon,
Bareos console) a Bareos installation contains additional service programs.
Programs that work with media require a valid storage daemon configuration
because they use the same routines to access storage media as the storage
daemon does in normal operation. Other programs require the configuration file
of the director, as they assess the configuration settings stored there.

It is therefore a great advantage if the configuration files of your
installation are available in an emergency. Otherwise, an emergency
restore must be preceded by the restoring of the configuration.
In most cases you will lack the time to do this properly.

.. warning::

   Conclusion: Keep a copy at least of the storage daemon able to read
   the disaster recovery volume in an accessible format and location.


``bextract`` allows you to extract data directly from Bareos media. It
need a functioning storage daemon configuration file.

``bls`` displays the content of Bareos media. It can access both tapes
and files in the file system. It need a functioning storage daemon
configuration file.


Performing an emergency restore
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the following section we will assume that our backup system itself
has failed. We need the following to restore it:

Preparation steps :

-  The media containing the backup. If the backup itself has been
   destroyed, it is of course no longer possible to restore it. You
   should therefore always swap out backups in some form or at least
   separate them from the actual backup system.

- Reinstall Bareos software components to the system. A default configuration will be deployed and can be used as a start.

-  The configuration of the storage daemon. If the properties of the
   replacement system have changed (paths or SCSI devices, for example),
   you have to adjust the storage daemon configuration to suit the new
   environment.

-  The ``Bootstrap`` file of the most recent backup of the database,
   i.e. of the ``BackupCatalog`` job.

In our example, bootstrap file :file:`BackupCatalog.bsr` looks like
this:

::

   # 15-Mar-2025 11:02:42 - BackupCatalog.2025-03-15_11.02.39_10 - Full
   Volume="E01001L4"
   MediaType="LTO-4"
   Slot=1
   VolSessionId=2
   VolSessionTime=1331805611
   VolAddr=4294967296-4294967297
   FileIndex=1-1

We use ``bextract`` to extract the most recent database backup from the
Bareos media. The call-up syntax is:

.. code-block:: shell-session

   bextract -c <SD configuration directory> <drive name in sd.conf> <output directory> -b <bootstrap file>

``bextract`` now extracts the data specified in the ``<Bootstrap file>``
to the ``<output directory>`` using drive ``<drive name>``. ``bextract``
consults the storage daemon configuration file to determine how to
address the drive.

For our example, that means the following call:

.. code-block:: shell-session

   root@bareos:~ #  bextract -c /etc/bareos -V E01001L4  Drive-1 /tmp -b BackupCatalog.bsr


.. code-block:: shell-session

   bextract: butil.c:287 Using device: "Drive-1" for reading.
   15-Mär 12:07 bextract JobId 0: 3301 Issuing autochanger "loaded? drive 0" command.
   15-Mär 12:07 bextract JobId 0: 3302 Autochanger "loaded? drive 0", result is Slot 1.
   15-Mär 12:07 bextract JobId 0: Ready to read from volume "E01001L4" on device "Drive-1" (/dev/nst0).
   15-Mär 12:07 bextract JobId 0: Forward spacing Volume "E01001L4" to file:block 1:0.
   bextract JobId 0: -rw-------   1 bareos   bareos         87862 2025-03-15 11:02:42  /tmp/var/lib/bareos/bareos.sql
   15-Mär 12:07 bextract JobId 0: End of Volume at file 2 on device "Drive-1" (/dev/nst0), Volume "E01001L4"
   15-Mär 12:07 bextract JobId 0: End of all volumes.
   1 files restored.


We have successfully extracted the database dump to file
:file:`/tmp/var/lib/bareos/bareos.sql`. We now want to import it to
the Bareos database, so we use Bareos scripts to create the database,
tables and access permissions:

.. code-block:: shell-session

   root@bareos:~ # su postgres -c /usr/lib/bareos/scripts/create_bareos_database


.. code-block:: none

   Creating postgresql database
   CREATE DATABASE
   ALTER DATABASE
   Creation of bareos database succeeded.
   Database encoding OK

.. code-block:: shell-session

   root@bareos:~ # su postgres -c /usr/lib/bareos/scripts/make_bareos_tables


.. code-block:: none

   Making postgresql tables
   [...]
   INSERT 0 1
   Creation of Bareos PostgreSQL tables succeeded.


.. code-block:: shell-session

   root@bareos:~ # su postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges


.. code-block:: none

   Info: Granting bareos tables
   CREATE ROLE
   GRANT
   Info: Privileges for user bareos granted ON database bareos.


We now import the database dump to the database:

.. code-block:: shell-session

  root@bareos:~ # su postgres -c "psql -d bareos -f /tmp/var/lib/bareos/bareos.sql"


We backup the newly installed configuration to another location

.. code-block:: shell-session

  root@bareos:~ # mv -v /etc/bareos /etc/bareos.new


We bring back the restored configuration to its initial location

.. code-block:: shell-session

  root@bareos:~ # cp -a /tmp/etc/bareos /etc/


Finally you restart the services (|dir|,|sd|,|fd|).

.. code-block:: shell-session

  root@bareos:~ # systemctl restart bareos-director bareos-storage bareos-filedaemon


The system is now restored with all information on all backups and is available for restores.


For additional details of restoring your database, please see the :ref:`section-RestoreCatalog` chapter.
