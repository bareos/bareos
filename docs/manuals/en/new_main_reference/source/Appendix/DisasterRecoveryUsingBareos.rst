.. _RescueChapter:

Disaster Recovery Using Bareos
==============================

:index:`[TAG=Disaster->Recovery] <pair: Disaster; Recovery>` :index:`[TAG=Recovery->Disaster Recovery] <pair: Recovery; Disaster Recovery>`

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

:index:`[TAG=Disaster->Before] <pair: Disaster; Before>`

-  Create a rescue or CDROM for your systems. Generally, they are offered by each distribution, and there are many good rescue disks on the Web

-  Ensure that you always have a valid bootstrap file for your backup and that it is saved to an alternate machine. This will permit you to easily do a full restore of your system.

-  If possible copy your catalog nightly to an alternate machine. If you have a valid bootstrap file, this is not necessary, but can be very useful if you do not want to reload everything.

-  Ensure that you always have a valid :ref:`bootstrap <BootstrapChapter>` file for your catalog backup that is saved to an alternate machine. This will permit you to restore your catalog more easily if needed.

-  Try out how to use the Rescue CDROM before you are forced to use it in an emergency situation.

-  Make a copy of your Bareos .conf files, particularly your bareos-dir.conf, and your bareos-sd.conf files, because if your server goes down, these files will be needed to get it back up and running, and they can be difficult to rebuild from memory.

.. _section-BareMetalRestoreClient:

Bare Metal Recovery of Bareos Clients
-------------------------------------

A so called "Bare Metal" recovery is one where you start with an empty hard disk and you restore your machine.

Generally, following components are required for a Bare Metal Recovery:

-  A rescue CDROM containing a copy of your OS and including the Bareos File daemon, including all libraries

-  The Bareos client configuration files

-  Useful: a copy of your hard disk information

-  A full Bareos backup of your system

.. _section-rear:

Linux
~~~~~

:index:`[TAG=Disaster->Recovery->Linux] <triple: Disaster; Recovery; Linux>`

From the Relax-and-Recover web site (`http://relax-and-recover.org <http://relax-and-recover.org>`_):

   Relax-and-Recover is a setup-and-forget Linux bare metal disaster recovery solution. It is easy to set up and requires no maintenance so there is no excuse for not using it.

Relax-and-Recover (ReaR) is quite easy to use with Bareos.

Installation
^^^^^^^^^^^^

Bareos is a supported backend for ReaR >= 1.15. To use the ``BAREOS_CLIENT`` option, ReaR >= 1.17 is required. If ReaR >= 1.17 is not part of your distribution, check the `download section on the
ReaR website <http://relax-and-recover.org/download/>`_.

Configuration
^^^^^^^^^^^^^

Assuming you have a working Bareos configuration on the system you want to protect with ReaR and Bareos references this system by the name ``bareosclient-fd``, the only configuration for ReaR is:

.. code-block:: cfg

   BACKUP=BAREOS
   BAREOS_CLIENT=bareosclient-fd

You also need to specify in your ReaR configuration file (:file:`/etc/rear/local.conf`) where you want to store your recovery images. Please refer to the `ReaR documentation <http://relax-and-recover.org/documentation/>`_ for details.

For example, if you want to create an ISO image and store it to an NFS server with the IP Address 192.168.10.1, you can use the following configuration:

.. code-block:: cfg
   :caption: Full Rear configuration in /etc/rear/local.conf

   # This is default:
   #OUTPUT=ISO
   # Where to write the iso image
   # You can use NFS, if you want to write your iso image to a nfs server
   # If you leave this blank, it will
   # be written to: /var/lib/rear/output/
   OUTPUT_URL=nfs://192.168.10.1/rear
   BACKUP=BAREOS
   BAREOS_CLIENT=bareosclient-fd

Backup
^^^^^^

If you have installed and configured ReaR on your system, type

.. code-block:: sh
   :caption: Create Rescue Image

   <command>rear</command><parameter> -v mkrescue</parameter>

to create the rescue image. If you used the configuration example above, you will get a bootable ISO image which can be burned onto a CD.



.. warning::
   This will not create a Bareos backup on your system! You will have to do that by
   other means, e.g. by a regular Bareos backup schedule.
   Also :command:`rear mkbackup` will not create a backup. 
   In this configuration it will only create the rescue ISO 
   (same as the :command:`rear mkrescue` command).

Recovery
^^^^^^^^

In case, you want to recover your system, boot it using the generated ReaR recovery ISO. After booting log in as user **root** and type

.. code-block:: sh
   :caption: Restore your system using Rear and Bareos

   <command>rear</command><parameter> recover</parameter>

ReaR will now use the most recent backup from Bareos to restore your system. When the restore job has finished, ReaR will start a new shell which you can use to verify if the system has been restored correctly. The restored system can be found under the :file:`/mnt/local` directory. When you are done< with the verification, type ’exit’ to leave the shell, getting back to the recovery process. Finally, you will be asked to confirm that everything is correct. Type ’yes’ to continue. After that,
ReaR will restore your bootloader. Recovery is complete.







Restoring a Bareos Server
-------------------------

:index:`[TAG=Restore->Bareos Server] <pair: Restore; Bareos Server>` 

.. _section-RestoreServer:



Above, we considered how to recover a client machine where a valid Bareos server was running on another machine. However, what happens if your server goes down and you no longer have a running Director, Catalog, or Storage daemon? There are several solutions:

#. Bring up static versions of your Director, Catalog, and Storage daemon on the damaged machine.

#. Move your server to another machine.

#. Use a Hot Spare Server on another Machine.

The first option, is very difficult because it requires you to have created a static version of the Director and the Storage daemon as well as the Catalog. If the Catalog uses MySQL or PostgreSQL, this may or may not be possible. In addition, to loading all these programs on a bare system (quite possible), you will need to make sure you have a valid driver for your tape drive.

The second suggestion is probably a much simpler solution, and one I have done myself. To do so, you might want to consider the following steps:

-  Install the same database server as on the original system.

-  Install Bareos and initialize the Bareos database.

-  Ideally, you will have a copy of all the Bareos conf files that were being used on your server. If not, you will at a minimum need create a bareos-dir.conf that has the same Client resource that was used to backup your system.

-  If you have a valid saved Bootstrap file as created for your damaged machine with WriteBootstrap, use it to restore the files to the damaged machine, where you have loaded a static Bareos File daemon using the Rescue disk). This is done by using the restore command and at the yes/mod/no prompt, selecting mod then specifying the path to the bootstrap file.

-  If you have the Bootstrap file, you should now be back up and running, if you do not have a Bootstrap file, continue with the suggestions below.

-  Using bscan scan the last set of backup tapes into your MySQL, PostgreSQL or SQLite database.

-  Start Bareos, and using the Console restore command, restore the last valid copy of the Bareos database and the Bareos configuration files.

-  Move the database to the correct location.

-  Start the database, and restart Bareos. Then use the Console restore command, restore all the files on the damaged machine, where you have loaded a Bareos File daemon using the Rescue disk.

For additional details of restoring your database, please see the :ref:`section-RestoreCatalog` chapter.




