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

-  Create a rescue or CDROM for your systems. Generally, they are offered by each distribution, and there are many good rescue disks on the Web

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

For Bareos >= 22, ReaR > 2.7 is required.
If ReaR > 2.7 is not part of your distribution, check the `download section on the
ReaR website <https://relax-and-recover.org/download/>`_.
The current (2024) stable release of ReaR is 2.7,
therefore until a newer version is released,
a ReaR snapshot package must be used,
see :ref:`section-rear-snapshot-package-installation`.

Configuration
^^^^^^^^^^^^^

For ReaR to work with Bareos, the following packages must be installed:

* **rear** > 2.7 (currently only via a ReaR snapshot package)
* **bareos-fd**
* **bareos-bconsole**

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

.. _section-rear-snapshot-package-installation:

Installing a ReaR Snapshot Package
''''''''''''''''''''''''''''''''''

The current (2024) stable release of ReaR is 2.7,
which does not support Bareos > 22.
Therefore until a newer version is released,
a ReaR snapshot package must be used:

* Download the ZIP file that best matches your distribution from https://github.com/rear/rear/releases/tag/snapshot
* Unpack the ZIP file and install the package on the client.

For details, refer to https://relax-and-recover.org/download/


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
      Password = "a very secure password"
   }

   Console {
      Name = "bareosclient-console"
      Password = "secret"
   }

* :config:option:`console/director/Address`: must be set to the DNS name or IP address of the |dir|.
* :config:option:`console/director/Password`: will be ignored and can be empty.
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

Restoring a Bareos Server
-------------------------

.. index::
   single: Restore; Bareos Server

Above, we considered how to recover a client machine where a valid Bareos server was running on another machine. However, what happens if your server goes down and you no longer have a running Director, Catalog, or Storage daemon? There are several solutions:

#. Move your server to another machine.

#. Use a Hot Spare Server on another Machine.

Consider the following steps:

-  Install the same database server as on the original system.

-  Install Bareos and initialize the Bareos database.

-  Ideally, you will have a copy of all the Bareos conf files that were being used on your server. If not, you will at a minimum need create a bareos-dir.conf that has the same Client resource that was used to backup your system.

-  If you have a valid saved Bootstrap file as created for your damaged machine with WriteBootstrap, use it to restore the files to the damaged machine, where you have loaded a static Bareos File daemon using the Rescue disk). This is done by using the restore command and at the yes/mod/no prompt, selecting mod then specifying the path to the bootstrap file.

-  After the Catalog is restored, it should be located in (:file:`./var/lib/bareos/bareos.sql`) relative to the restore location. After stopping the bareos-dir service, this file then needs to be loaded into the PostgreSQL database. (For example by running ``sudo -u bareos psql bareos bareos -f ./var/lib/bareos/bareos.sql`` or ```su postgres -c "psql bareos -f ./var/lib/bareos/bareos.sql``)

-  If you have successfully used a Bootstrap file, you should now be back up and running, if you do not have a Bootstrap file, continue with the suggestions below.

-  Using bscan scan the last set of backup tapes into your catalog database.

-  Start Bareos, and using the Console restore command, restore the last valid copy of the Bareos database and the Bareos configuration files.

-  Move the database to the correct location.

-  Start the database, and restart Bareos. Then use the Console restore command, restore all the files on the damaged machine, where you have loaded a Bareos File daemon using the Rescue disk.

For additional details of restoring your database, please see the :ref:`section-RestoreCatalog` chapter.
