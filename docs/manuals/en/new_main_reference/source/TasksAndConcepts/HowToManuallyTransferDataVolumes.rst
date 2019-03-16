How to manually transfer data/volumes
=====================================

The always incremental backup scheme minimizes the amount of data that needs to be transferred over the wire.

This makes it possible to backup big filesystems over small bandwidths.

The only challenge is to do the first full backup.

The easiest way to transfer the data is to copy it to a portable data medium (or even directly store it on there) and import the data into the local bareos catalog as if it was backed up from the original client.

This can be done in two ways

#. Install a storage daemon in the remote location that needs to be backed up and connect it to the main director. This makes it easy to make a local backup in the remote location and then transfer the volumes to the local storage. For this option the communication between the local director and the remote storage daemon needs to be possible.

.. image:: /include/images/ai-transfer-first-backup2.*



#. Install a director and a storage daemon in the remote location. This option means that the backup is done completely independent from the local director and only the volume is then transferred and needs to be imported afterwards.

.. image:: /include/images/ai-transfer-first-backup3.*




Import Data from a Remote Storage Daemon
----------------------------------------

First setup client, fileset, job and schedule as needed for a always incremental backup of the remote client.

Run the first backup but make sure that you choose the remote storage to be used.

.. code-block:: bconsole
   :caption: run

   *run job=BackupClient-remote level=Full storage=File-remote

Transport the volumes that were used for that backup over to the local storage daemon and make them available to the local storage daemon. This can be either by putting the tapes into the local changer or by storing the file volumes into the local file volume directory.

If copying a volume to the local storage directory make sure that the file rights are correct.

Now tell the director that the volume now belongs to the local storage daemon.

List volumes shows that the volumes used still belong to the remote storage:

.. code-block:: bconsole
   :caption: list volumes

   *<input>list volumes</input>
   .....
   Pool: Full
   +---------+------------+-----------+---------+----------+----------+--------------+---------+------+-----------+-----------+---------------------+-------------+
   | MediaId | VolumeName | VolStatus | Enabled | VolBytes | VolFiles | VolRetention | Recycle | Slot | InChanger | MediaType | LastWritten         | Storage     |
   +---------+------------+-----------+---------+----------+----------+--------------+---------+------+-----------+-----------+---------------------+-------------+
   | 1       | Full-0001  | Append    | 1       | 38600329 | 0        | 31536000     | 1       | 0    | 0         | File      | 2016-07-28 14:00:47 | File-remote |
   +---------+------------+-----------+---------+----------+----------+--------------+---------+------+-----------+-----------+---------------------+-------------+

Use :bcommand:`update volume` to set the right storage and check with list volumes that it worked:

.. code-block:: bconsole
   :caption: update volume

   *<input>update volume=Full-0001 storage=File</input>
   *<input>list volumes</input>
   ...
   Pool: Full
   +---------+------------+-----------+---------+----------+----------+--------------+---------+------+-----------+-----------+---------------------+---------+
   | MediaId | VolumeName | VolStatus | Enabled | VolBytes | VolFiles | VolRetention | Recycle | Slot | InChanger | MediaType | LastWritten         | Storage |
   +---------+------------+-----------+---------+----------+----------+--------------+---------+------+-----------+-----------+---------------------+---------+
   | 1       | Full-0001  | Append    | 1       | 38600329 | 0        | 31536000     | 1       | 0    | 0         | File      | 2016-07-28 14:00:47 | File    |
   +---------+------------+-----------+---------+----------+----------+--------------+---------+------+-----------+-----------+---------------------+---------+

Now the remote storage daemon can be disabled as it is not needed anymore.

The next incremental run will take the previously taken full backup as reference.

Import Data from a Independent Remote Full Bareos Installation
--------------------------------------------------------------

If a network connection between the local director and the remote storage daemon is not possible, it is also an option to setup a fully functional Bareos installation remotely and then to import the created volumes. Of course the network connection between the |bareosDir| and the |bareosFd| is needed in any case to make the incremental backups possible.

-  Configure the connection from local |bareosDir| to remote |bareosFd|, give the remote client the same name as it was when the data was backed up.

-  Add the Fileset created on remote machine to local machine.

-  Configure the Job that should backup the remote client with the fileset.

-  Run :bcommand:`estimate listing` on the remote backup job.

-  Run :bcommand:`list filesets` to make sure the fileset was added to the catalog.

Then we need to create a backup on the remote machine onto a portable disk which we can then import into our local installation.

On remote machine:

-  Install full Bareos server on remote server (sd, fd, dir). Using the Sqlite backend is sufficient.

-  Add the client to the remote backup server.

-  Add fileset which the client will be backed up.

-  Add Pool with name :config:option:`dir/pool = transfer`\  where the data will be written to.

-  create job that will backup the remote client with the remote fileset into the new pool

-  Do the local backup using the just created Pool and Filesets.

Transport the newly created volume over to the director machine (e.g. via external harddrive) and store the file where the device stores its files (e.g. /var/lib/bareos/storage)

Shutdown Director on local director machine.

Import data form volume via :command:`bscan`, you need to set which database backend is used: :command:`bscan -B sqlite3 FileStorage -V Transfer-0001 -s -S`

If the import was successfully completed, test if an incremental job really only backs up the minimum amount of data.







