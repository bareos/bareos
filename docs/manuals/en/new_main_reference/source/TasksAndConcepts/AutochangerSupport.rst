.. _AutochangersChapter:

Autochanger Support
===================

:index:`\ <single: Support; Autochanger>`\  :index:`\ <single: Autochanger; Support>`\ 

Bareos provides autochanger support for reading and writing tapes. In order to work with an autochanger, Bareos requires a number of things, each of which is explained in more detail after this list:

-  The package **bareos-storage-tape** must be installed.

-  A script that actually controls the autochanger according to commands sent by Bareos. Bareos contains the script :command:`mtx-changer`, that utilize the command :command:`mtx`. It’s config file is normally located at :file:`/etc/bareos/mtx-changer.conf`

-  That each Volume (tape) to be used must be defined in the Catalog and have a Slot number assigned to it so that Bareos knows where the Volume is in the autochanger. This is generally done with the :bcommand:`label` command, but can also done after the tape is labeled using the :bcommand:`update slots` command. See below for more details. You must pre-label the tapes manually before using them.

-  Modifications to your Storage daemon’s Device configuration resource to identify that the device is a changer, as well as a few other parameters.

-  You need to ensure that your Storage daemon has access permissions to both the tape drive and the control device. On Linux, the system user **bareos** is added to the groups :strong:`disk` and :strong:`tape`, so that it should have the permission to access the library.

-  Set :config:option:`dir/storage/AutoChanger = yes`\ .

Bareos uses its own :command:`mtx-changer` script to interface with a program that actually does the tape changing. Thus in principle, :command:`mtx-changer` can be adapted to function with any autochanger program, or you can call any other script or program. The current version of :command:`mtx-changer` works with the :command:`mtx` program. FreeBSD users might need to adapt this script to use :command:`chio`. For more details, refer
to the :ref:`Testing Autochanger <AutochangerTesting>` chapter.

Bareos also supports autochangers with barcode readers. This support includes two Console commands: :bcommand:`label barcodes` and :bcommand:`update slots`. For more details on these commands, see the chapter about :ref:`Barcodes`.

Current Bareos autochanger support does not include cleaning, stackers, or silos. Stackers and silos are not supported because Bareos expects to be able to access the Slots randomly. However, if you are very careful to setup Bareos to access the Volumes in the autochanger sequentially, you may be able to make Bareos work with stackers (gravity feed and such).

In principle, if :command:`mtx` will operate your changer correctly, then it is just a question of adapting the :command:`mtx-changer` script (or selecting one already adapted) for proper interfacing.

If you are having troubles, please use the auto command in the :command:`btape` program to test the functioning of your autochanger with Bareos. Please remember, that on most distributions, the |sd| runs as user **bareos** and not as **root**. You will need to ensure that the Storage daemon has sufficient permissions to access the autochanger.

Some users have reported that the the Storage daemon blocks under certain circumstances in trying to mount a volume on a drive that has a different volume loaded. As best we can determine, this is simply a matter of waiting a bit. The drive was previously in use writing a Volume, and sometimes the drive will remain BLOCKED for a good deal of time (up to 7 minutes on a slow drive) waiting for the cassette to rewind and to unload before the drive can be used with a different Volume.

.. _SCSI devices:

Knowing What SCSI Devices You Have
----------------------------------

:index:`\ <single: SCSI devices>`\  :index:`\ <single: Devices; SCSI>`\  :index:`\ <single: Devices; Detecting>`\ 

Linux
~~~~~

Under Linux, you can



::

   cat /proc/scsi/scsi



to see what SCSI devices you have available. You can also:



::

   cat /proc/scsi/sg/device_hdr /proc/scsi/sg/devices



to find out how to specify their control address (/dev/sg0 for the first, /dev/sg1 for the second, ...) on the :config:option:`sd/autochanger/ChangerDevice`\  Bareos directive.

You can also use the excellent lsscsi tool. 

::

   $ lsscsi -g
    [1:0:2:0]    tape    SEAGATE  ULTRIUM06242-XXX 1619  /dev/st0  /dev/sg9
    [1:0:14:0]   mediumx STK      L180             0315  /dev/sch0 /dev/sg10
    [2:0:3:0]    tape    HP       Ultrium 3-SCSI   G24S  /dev/st1  /dev/sg11
    [3:0:0:0]    enclosu HP       A6255A           HP04  -         /dev/sg3
    [3:0:1:0]    disk    HP 36.4G ST336753FC       HP00  /dev/sdd  /dev/sg4



FreeBSD
~~~~~~~

Under FreeBSD, use the following command to list the SCSI devices as well as the :file:`/dev/passN` that you will use on the Bareos :config:option:`sd/autochanger/ChangerDevice`\  directive:



::

   camcontrol devlist



Please check that your Storage daemon has permission to access this device.

The following tip for FreeBSD users comes from Danny Butroyd: on reboot Bareos will NOT have permission to control the device :file:`/dev/pass0` (assuming this is your changer device). To get around this just edit the :file:`/etc/devfs.conf` file and add the following to the bottom: 

::

   own     pass0   root:bareos
   perm    pass0   0666
   own     nsa0.0  root:bareos
   perm    nsa0.0    0666



This gives the bareos group permission to write to the nsa0.0 device too just to be on the safe side. To bring these changes into effect just run:-

.. code-block:: shell-session

   /etc/rc.d/devfs restart

Basically this will stop you having to manually change permissions on these devices to make Bareos work when operating the AutoChanger after a reboot.

Solaris
~~~~~~~

On Solaris, the changer device will typically be some file under :file:`/dev/rdsk`.

Slots
-----

:index:`\ <single: Slots>`\  

.. _Slots:



To properly address autochangers, Bareos must know which Volume is in each slot of the autochanger. Slots are where the changer cartridges reside when not loaded into the drive. Bareos numbers these slots from one to the number of cartridges contained in the autochanger.

Bareos will not automatically use a Volume in your autochanger unless it is labeled and the slot number is stored in the catalog and the Volume is marked as InChanger. This is because it must know where each volume is to be able to load the volume. For each Volume in your changer, you will, using the Console program, assign a slot. This information is kept in Bareos’s catalog database along with the other data for the volume. If no slot is given, or the slot is set to zero, Bareos will not
attempt to use the autochanger even if all the necessary configuration records are present. When doing a :bcommand:`mount` command on an autochanger, you must specify which slot you want mounted. If the drive is loaded with a tape from another slot, it will unload it and load the correct tape, but normally, no tape will be loaded because an :bcommand:`unmount` command causes Bareos to unload the tape in the drive.

You can check if the Slot number and InChanger flag by:

.. code-block:: bconsole
   :caption: list volumes

   *list volumes

.. _section-MultipleDevices:

Multiple Devices
----------------

:index:`\ <single: Devices; Multiple>`\  :index:`\ <single: Multiple Devices>`\ 

Some autochangers have more than one read/write device (drive). The :ref:`Autochanger resource <AutochangerRes>` permits you to group Device resources, where each device represents a drive. The Director may still reference the Devices (drives) directly, but doing so, bypasses the proper functioning of the drives together. Instead, the Director (in the Storage resource) should reference the Autochanger resource name. Doing so permits the Storage daemon to ensure that only one drive
uses the mtx-changer script at a time, and also that two drives don’t reference the same Volume.

Multi-drive requires the use of the :config:option:`sd/device/DriveIndex`\  directive. Drive numbers or the Device Index are numbered beginning at zero, which is the default. To use the second Drive in an autochanger, you need to define a second Device resource, set the :config:option:`sd/device/DriveIndex`\  and set the :config:option:`sd/device/ArchiveDevice`\ .

As a default, Bareos jobs will prefer to write to a Volume that is already mounted. If you have a multiple drive autochanger and you want Bareos to write to more than one Volume in the same Pool at the same time, you will need to set :config:option:`dir/job/PreferMountedVolumes = no`\ . This will cause the Storage daemon to maximize the use of drives.

Device Configuration Records
----------------------------

:index:`\ <single: Device Configuration Records>`\ 

Configuration of autochangers within Bareos is done in the Device resource of the Storage daemon.

Following records control how Bareos uses the autochanger:

:config:option:`sd/device/Autochanger`\ 
   Specifies if the current device belongs to an autochanger resource.

:config:option:`sd/autochanger/ChangerCommand`\  (:config:option:`sd/device/ChangerCommand`\ )
:config:option:`sd/autochanger/ChangerDevice`\  (:config:option:`sd/device/ChangerDevice`\ )
:config:option:`sd/device/DriveIndex`\ 
   Individual driver number, starting at 0.

:config:option:`sd/device/MaximumChangerWait`\ 

Specifying Slots When Labeling
------------------------------

:index:`\ <single: Specifying Slots When Labeling>`\  :index:`\ <single: Label; Specifying Slots When Labeling>`\  

.. _SpecifyingSlots:



If you add an Autochanger = yes record to the Storage resource in your Director’s configuration file, the Bareos Console will automatically prompt you for the slot number when the Volume is in the changer when you add or label tapes for that Storage device. If your mtx-changer script is properly installed, Bareos will automatically load the correct tape during the label command.

You must also set Autochanger = yes in the Storage daemon’s Device resource as we have described above in order for the autochanger to be used. Please see :config:option:`dir/storage/AutoChanger`\  and :config:option:`sd/device/Autochanger`\  for more details on these records.

Thus all stages of dealing with tapes can be totally automated. It is also possible to set or change the Slot using the update command in the Console and selecting Volume Parameters to update.

Even though all the above configuration statements are specified and correct, Bareos will attempt to access the autochanger only if a slot is non-zero in the catalog Volume record (with the Volume name).

If your autochanger has barcode labels, you can label all the Volumes in your autochanger one after another by using the :bcommand:`label barcodes` command. For each tape in the changer containing a barcode, Bareos will mount the tape and then label it with the same name as the barcode. An appropriate Media record will also be created in the catalog. Any barcode that begins with the same characters as specified on the "CleaningPrefix=xxx" command, will be treated as a cleaning tape,
and will not be labeled. For example with:



::

   Pool {
     Name ...
     Cleaning Prefix = "CLN"
   }



Any slot containing a barcode of CLNxxxx will be treated as a cleaning tape and will not be mounted.

Changing Cartridges
-------------------

:index:`\ <single: Cartridges; Changing>`\  If you wish to insert or remove cartridges in your autochanger or you manually run the mtx program, you must first tell Bareos to release the autochanger by doing:



::

   unmount
   (change cartridges and/or run mtx)
   mount



If you do not do the unmount before making such a change, Bareos will become completely confused about what is in the autochanger and may stop function because it expects to have exclusive use of the autochanger while it has the drive mounted.

Dealing with Multiple Magazines
-------------------------------

:index:`\ <single: Magazines; Dealing with Multiple>`\ 

If you have several magazines or if you insert or remove cartridges from a magazine, you should notify Bareos of this. By doing so, Bareos will as a preference, use Volumes that it knows to be in the autochanger before accessing Volumes that are not in the autochanger. This prevents unneeded operator intervention.

If your autochanger has barcodes (machine readable tape labels), the task of informing Bareos is simple. Every time, you change a magazine, or add or remove a cartridge from the magazine, simply use following commands in the Console program:



::

   unmount
   (remove magazine)
   (insert new magazine)
   update slots
   mount



This will cause Bareos to request the autochanger to return the current Volume names in the magazine. This will be done without actually accessing or reading the Volumes because the barcode reader does this during inventory when the autochanger is first turned on. Bareos will ensure that any Volumes that are currently marked as being in the magazine are marked as no longer in the magazine, and the new list of Volumes will be marked as being in the magazine. In addition, the Slot numbers of the
Volumes will be corrected in Bareos’s catalog if they are incorrect (added or moved).

If you do not have a barcode reader on your autochanger, you have several alternatives.

#. You can manually set the Slot and InChanger flag using the update volume command in the Console (quite painful).

#. You can issue a

   

   ::

      update slots scan

   

   command that will cause Bareos to read the label on each of the cartridges in the magazine in turn and update the information (Slot, InChanger flag) in the catalog. This is quite effective but does take time to load each cartridge into the drive in turn and read the Volume label.



Update Slots Command
--------------------

:index:`\ <single: Console; Command; update slots>`\  

.. _updateslots:



If you change only one cartridge in the magazine, you may not want to scan all Volumes, so the update slots command (as well as the update slots scan command) has the additional form:



::

   update slots=n1,n2,n3-n4, ...



where the keyword scan can be appended or not. The n1,n2, ... represent Slot numbers to be updated and the form n3-n4 represents a range of Slot numbers to be updated (e.g. 4-7 will update Slots 4,5,6, and 7).

This form is particularly useful if you want to do a scan (time expensive) and restrict the update to one or two slots.

For example, the command:



::

   update slots=1,6 scan



will cause Bareos to load the Volume in Slot 1, read its Volume label and update the Catalog. It will do the same for the Volume in Slot 6. The command:



::

   update slots=1-3,6



will read the barcoded Volume names for slots 1,2,3 and 6 and make the appropriate updates in the Catalog. If you don’t have a barcode reader the above command will not find any Volume names so will do nothing.

Using the Autochanger
---------------------

:index:`\ <single: Autochanger; Using the>`\  

.. _using:



Let’s assume that you have properly defined the necessary Storage daemon Device records, and you have added the Autochanger = yes record to the Storage resource in your Director’s configuration file.

Now you fill your autochanger with say six blank tapes.

What do you do to make Bareos access those tapes?

One strategy is to prelabel each of the tapes. Do so by starting Bareos, then with the Console program, enter the label command:



::

   ./bconsole
   Connecting to Director rufus:8101
   1000 OK: rufus-dir Version: 1.26 (4 October 2002)
   *label



it will then print something like:



::

   Using default Catalog name=BackupDB DB=bareos
   The defined Storage resources are:
        1: Autochanger
        2: File
   Select Storage resource (1-2): 1



I select the autochanger (1), and it prints:



::

   Enter new Volume name: TestVolume1
   Enter slot (0 for none): 1



where I entered TestVolume1 for the tape name, and slot 1 for the slot. It then asks:



::

   Defined Pools:
        1: Default
        2: File
   Select the Pool (1-2): 1



I select the Default pool. This will be automatically done if you only have a single pool, then Bareos will proceed to unload any loaded volume, load the volume in slot 1 and label it. In this example, nothing was in the drive, so it printed:



::

   Connecting to Storage daemon Autochanger at localhost:9103 ...
   Sending label command ...
   3903 Issuing autochanger "load slot 1" command.
   3000 OK label. Volume=TestVolume1 Device=/dev/nst0
   Media record for Volume=TestVolume1 successfully created.
   Requesting mount Autochanger ...
   3001 Device /dev/nst0 is mounted with Volume TestVolume1
   You have messages.
   *



You may then proceed to label the other volumes. The messages will change slightly because Bareos will unload the volume (just labeled TestVolume1) before loading the next volume to be labeled.

Once all your Volumes are labeled, Bareos will automatically load them as they are needed.

To "see" how you have labeled your Volumes, simply enter the list volumes command from the Console program, which should print something like the following:



::

   *:strong:`list volumes`
   Using default Catalog name=BackupDB DB=bareos
   Defined Pools:
        1: Default
        2: File
   Select the Pool (1-2): 1
   +-------+----------+--------+---------+-------+--------+----------+-------+------+
   | MedId | VolName  | MedTyp | VolStat | Bites | LstWrt | VolReten | Recyc | Slot |
   +-------+----------+--------+---------+-------+--------+----------+-------+------+
   | 1     | TestVol1 | DDS-4  | Append  | 0     | 0      | 30672000 | 0     | 1    |
   | 2     | TestVol2 | DDS-4  | Append  | 0     | 0      | 30672000 | 0     | 2    |
   | 3     | TestVol3 | DDS-4  | Append  | 0     | 0      | 30672000 | 0     | 3    |
   | ...                                                                            |
   +-------+----------+--------+---------+-------+--------+----------+-------+------+



Barcode Support
---------------

:index:`\ <single: Support; Barcode>`\  :index:`\ <single: Barcode Support>`\  

.. _Barcodes:



Bareos provides barcode support with two Console commands, label barcodes and update slots.

The label barcodes will cause Bareos to read the barcodes of all the cassettes that are currently installed in the magazine (cassette holder) using the mtx-changer list command. Each cassette is mounted in turn and labeled with the same Volume name as the barcode.

The update slots command will first obtain the list of cassettes and their barcodes from mtx-changer. Then it will find each volume in turn in the catalog database corresponding to the barcodes and set its Slot to correspond to the value just read. If the Volume is not in the catalog, then nothing will be done. This command is useful for synchronizing Bareos with the current magazine in case you have changed magazines or in case you have moved cassettes from one slot to another. If the
autochanger is empty, nothing will be done.

The Cleaning Prefix statement can be used in the Pool resource to define a Volume name prefix, which if it matches that of the Volume (barcode) will cause that Volume to be marked with a VolStatus of Cleaning. This will prevent Bareos from attempting to write on the Volume.

Use bconsole to display Autochanger content
-------------------------------------------

The status slots storage=xxx command displays autochanger content.



::

    Slot |  Volume Name    |  Status  |      Type         |    Pool        |  Loaded |
   ------+-----------------+----------+-------------------+----------------+---------|
       1 |           00001 |   Append |  DiskChangerMedia |        Default |    0    |
       2 |           00002 |   Append |  DiskChangerMedia |        Default |    0    |
       3*|           00003 |   Append |  DiskChangerMedia |        Scratch |    0    |
       4 |                 |          |                   |                |    0    |



If you see a near the slot number, you have to run update slots command to synchronize autochanger content with your catalog.

Bareos Autochanger Interface
----------------------------

:index:`\ <single: Autochanger; Interface>`\  

.. _autochanger-interface:



Bareos calls the autochanger script that you specify on the Changer Command statement. Normally this script will be the mtx-changer script that we provide, but it can in fact be any program. The only requirement for the script is that it must understand the commands that Bareos uses, which are loaded, load, unload, list, and slots. In addition, each of those commands must return the information in the precise format as specified below:



::

   - Currently the changer commands used are:
       loaded -- returns number of the slot that is loaded, base 1,
                 in the drive or 0 if the drive is empty.
       load   -- loads a specified slot (note, some autochangers
                 require a 30 second pause after this command) into
                 the drive.
       unload -- unloads the device (returns cassette to its slot).
       list   -- returns one line for each cassette in the autochanger
                 in the format <slot>:<barcode>. Where
                 the :strong:`slot` is the non-zero integer representing
                 the slot number, and :strong:`barcode` is the barcode
                 associated with the cassette if it exists and if you
                 autoloader supports barcodes. Otherwise the barcode
                 field is blank.
       slots  -- returns total number of slots in the autochanger.



Bareos checks the exit status of the program called, and if it is zero, the data is accepted. If the exit status is non-zero, Bareos will print an error message and request the tape be manually mounted on the drive.

Tapespeed and blocksizes
------------------------

:index:`\ <single: Tuning; Tape>`\  :index:`\ <single: Tuning; blocksize>`\  :index:`\ <single: Tape; speed>`\  :index:`\ <single: Blocksize; optimize>`\  

.. _Tapespeed and blocksizes:

 

.. _setblocksizes:



The `Bareos Whitepaper Tape Speed Tuning <http://www.bareos.org/en/Whitepapers/articles/Speed_Tuning_of_Tape_Drives.html>`_ shows that the two parameters :strong:`Maximum File Size`\  and :strong:`Maximum Block Size`\  of the device have significant influence on the tape speed.

While it is no problem to change the :config:option:`sd/device/MaximumFileSize`\  parameter, unfortunately it is not possible to change the :config:option:`sd/device/MaximumBlockSize`\  parameter, because the previously written tapes would become unreadable in the new setup. It would require that the :config:option:`sd/device/MaximumBlockSize`\  parameter is switched back to the old value to be able to read the old volumes, but of
course then the new volumes would be unreadable.

Why is that the case?

The problem is that Bareos writes the label block (header) in the same block size that is configured in the :config:option:`sd/device/MaximumBlockSize`\  parameter in the device. Per default, this value is 63k, so usually a tape written by Bareos looks like this:

::

   |-------------------
   |label block  (63k)|
   |-------------------
   |data block  1(63k)|
   |data block  2(63k)|
   |...               |
   |data block  n(63k)|
   --------------------

Setting the maximum block size to e.g. 512k, would lead to the following:

::

   |-------------------
   |label block (512k)|
   |-------------------
   |data block 1(512k)|
   |data block 2(512k)|
   |...               |
   |data block n(512k)|
   --------------------

As you can see, every block is written with the maximum block size, also the label block.

The problem that arises here is that reading a block header with a wrong block size causes a read error which is interpreted as an non-existent label by Bareos.

This is a potential source of data loss, because in normal operation, Bareos refuses to relabel an already labeled volume to be sure to not overwrite data that is still needed. If Bareos cannot read the volume label, this security mechanism does not work and you might label tapes already labeled accidentally.

To solve this problem, the block size handling was changed in Bareos :sinceVersion:`14.2.0: Maximum Block Size` in the following way:

-  The tape label block is always written in the standard 63k (64512) block size.

-  The following blocks are then written in the block size configured in the :strong:`Maximum Block Size`\  directive.

-  To be able to change the block size in an existing environment, it is now possible to set the :config:option:`dir/pool/MaximumBlockSize`\  and :config:option:`dir/pool/MinimumBlockSize`\  in the pool resource. This setting is automatically promoted to each medium in that pool as usual (i.e. when a medium is labeled for that pool or if a volume is transferred to that pool from the scratch pool). When a volume is mounted, the volume’s block size is
   used to write and read the data blocks that follow the header block.

The following picture shows the result:

::

   |--------------------------------|
   |label block (label block size)  |
   |--------------------------------|
   |data block 1(maximum block size)|
   |data block 2(maximum block size)|
   |...                             |
   |data block n(maximum block size)|
   ---------------------------------|

We have a label block with a certain size (63k per default to be compatible to old installations), and the following data blocks are written with another blocksize.

This approach has the following advantages:

-  If nothing is configured, existing installations keep on working without problems.

-  If you want to switch an existing installation that uses the default block size and move to a new (usually bigger) block size, you can do that easily by creating a new pool, where :config:option:`dir/pool/MaximumBlockSize`\  is set to the new value that you wish to use in the future:

.. code-block:: bareosconfig
   :caption: Pool Ressource: setting Maximum Block Size

   Pool {
      Name = LTO-4-1M
         Pool Type = Backup
         Recycle = yes                       # Bareos can automatically recycle Volumes
         AutoPrune = yes                     # Prune expired volumes
         Volume Retention = 1 Month          # How long should the Full Backups be kept? (#06)
         Maximum Block Size = 1048576
         Recycle Pool = Scratch
   }

Now configure your backups that they will write into the newly defined pool in the future, and your backups will be written with the new block size.

Your existing tapes can be automatically transferred to the new pool when they expire via the :ref:`Scratch Pool <TheScratchPool>` mechanism. When a tape in your old pool expires, it is transferred to the scratch pool if you set Recycle Pool = Scratch. When your new pool needs a new volume, it will get it from the scratch pool and apply the new pool’s properties to that tape which also include :config:option:`dir/pool/MaximumBlockSize`\  and
:config:option:`dir/pool/MinimumBlockSize`\ .

This way you can smoothly switch your tapes to a new block size while you can still restore the data on your old tapes at any time.

Possible Problems
~~~~~~~~~~~~~~~~~

There is only one case where the new block handling will cause problems, and this is if you have used bigger block sizes already in your setup. As we now defined the label block to always be 63k, all labels will not be readable.

To also solve this problem, the directive :config:option:`sd/device/LabelBlockSize`\  can be used to define a different label block size. That way, everything should work smoothly as all label blocks will be readable again.

How can I find out which block size was used when the tape was written?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

At least on Linux, you can see if Bareos tries to read the blocks with the wrong block size. In that case, you get a kernel message like the following in your system’s messages:

::

   [542132.410170] st1: Failed to read 1048576 byte block with 64512 byte transfer.

Here, the block was written with 1M block size but we only read 64k.

.. _direct-access-to-volumes-with-non-default-blocksizes:

Direct access to Volumes with with non-default block sizes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: bls; block size>`\  :index:`\ <single: bextract; block size>`\  :index:`\ <single: Command; bls; block size>`\  :index:`\ <single: Command; bextract; block size>`\ 

:command:`bls` and :command:`bextract` can directly access Bareos volumes without catalog database. This means that these programs don’t have information about the used block size.

To be able to read a volume written with an arbitrary block size, you need to set the :config:option:`sd/device/LabelBlockSize`\  (to be able to to read the label block) and the :config:option:`sd/device/MaximumBlockSize`\  (to be able to read the data blocks) setting in the device definition used by those tools to be able to open the medium.

Example using :command:`bls` with a tape that was written with another blocksize than the ``DEFAULT_BLOCK_SIZE``\  (63k), but with the default label block size of 63k:

.. code-block:: shell-session
   :caption: bls with non-default block size

   <command>bls</command> <parameter>FC-Drive-1 -V A00007L4</parameter>
   bls: butil.c:289-0 Using device: "FC-Drive-1" for reading.
   25-Feb 12:47 bls JobId 0: No slot defined in catalog (slot=0) for Volume "A00007L4" on "FC-Drive-1" (/dev/tape/by-id/scsi-350011d00018a5f03-nst).
   25-Feb 12:47 bls JobId 0: Cartridge change or "update slots" may be required.
   25-Feb 12:47 bls JobId 0: Ready to read from volume "A00007L4" on device "FC-Drive-1" (/dev/tape/by-id/scsi-350011d00018a5f03-nst).
   25-Feb 12:47 bls JobId 0: Error: block.c:1004 Read error on fd=3 at file:blk 0:1 on device "FC-Drive-1" (/dev/tape/by-id/scsi-350011d00018a5f03-nst). ERR=Cannot allocate memory.
    Bareos status: file=0 block=1
    Device status: ONLINE IM_REP_EN file=0 block=2
   0 files found.

As can be seen, :command:`bls` manages to read the label block as it knows what volume is mounted (Ready to read from volume :strong:`A00007L4`), but fails to read the data blocks.

.. code-block:: shell-session
   :caption: dmesg

   <command>dmesg</command>
   [...]
   st2: Failed to read 131072 byte block with 64512 byte transfer.
   [...]

This shows that the block size for the data blocks that we need is 131072.

Now we have to set this block size in the :file:`bareos-sd.conf`, device resource as :config:option:`sd/device/MaximumBlockSize`\ :

.. code-block:: bareosconfig
   :caption: Storage Device Resource: setting Maximum Block Size

   Device {
     Name = FC-Drive-1
     Drive Index = 0
     Media Type = LTO-4
     Archive Device = /dev/tape/by-id/scsi-350011d00018a5f03-nst
     AutomaticMount = yes
     AlwaysOpen = yes
     RemovableMedia = yes
     RandomAccess = no
     AutoChanger = yes
     Maximum Block Size = 131072
   }

Now we can call bls again, and everything works as expected:

.. code-block:: shell-session
   :caption: bls with non-default block size

   <command>bls</command> <parameter>FC-Drive-1 -V A00007L4</parameter>
   bls: butil.c:289-0 Using device: "FC-Drive-1" for reading.
   25-Feb 12:49 bls JobId 0: No slot defined in catalog (slot=0) for Volume "A00007L4" on "FC-Drive-1" (/dev/tape/by-id/scsi-350011d00018a5f03-nst).
   25-Feb 12:49 bls JobId 0: Cartridge change or "update slots" may be required.
   25-Feb 12:49 bls JobId 0: Ready to read from volume "A00007L4" on device "FC-Drive-1" (/dev/tape/by-id/scsi-350011d00018a5f03-nst).
   bls JobId 203: [...]

How to configure the block sizes in your environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following chart shows how to set the directives for maximum block size and label block size depending on how your current setup is:

.. image:: /include/images/blocksize-decisionchart.*




Tape Drive Cleaning
-------------------

Bareos has no build-in functionality for tape drive cleaning. Fortunately this is not required as most modern tape libraries have build in auto-cleaning functionality. This functionality might require an empty tape drive, so the tape library gets aware, that it is currently not used. However, by default Bareos keeps tapes in the drives, in case the same tape is required again.

The directive :config:option:`dir/pool/CleaningPrefix`\  is only used for making sure that Bareos does not try to write backups on a cleaning tape.

If your tape libraries auto-cleaning won’t work when there are tapes in the drives, it’s probably best to set up an admin job that removes the tapes from the drives. This job has to run, when no other backups do run. A job definition for an admin job to do that may look like this:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/job/ReleaseAllTapeDrives.conf

   Job {
       Name = ReleaseAllTapeDrives
       JobDefs = DefaultJob
       Schedule = "WeeklyCycleAfterBackup"
       Type = Admin
       Priority = 200

       RunScript {
           Runs When = Before
           Runs On Client = no
           Console = "release storage=Tape alldrives"
       }
   }

Replace :config:option:`Dir/Storage = Tape`\  by the storage name of your tape library. Use the highest :config:option:`dir/job/Priority`\  value to make sure no other jobs are running. In the default configuration for example, the :config:option:`dir/job = CatalogBackup`\  job has Priority = 100. The higher the number, the lower the job priority.  


