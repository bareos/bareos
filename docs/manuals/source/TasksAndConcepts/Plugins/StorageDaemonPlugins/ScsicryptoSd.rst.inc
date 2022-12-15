scsicrypto-sd
~~~~~~~~~~~~~

.. index::
   single: Plugin; scsicrypto-sd

This plugin is part of the **bareos-storage-tape** package.

General
^^^^^^^

.. _LTOHardwareEncryptionGeneral:

LTO Hardware Encryption
'''''''''''''''''''''''

Modern tape-drives, for example LTO (from LTO4 onwards) support hardware encryption. There are several ways of using encryption with these drives. The following three types of key management are available for encrypting drives. The transmission of the keys to the volumes is accomplished by either of the three:

-  A backup application that supports Application Managed Encryption (AME)

-  A tape library that supports Library Managed Encryption (LME)

-  A Key Management Appliance (KMA)

We added support for Application Managed Encryption (AME) scheme, where on labeling a crypto key is generated for a volume and when the volume is mounted, the crypto key is loaded. When finally the volume is unmounted, the key is cleared from the memory of the Tape Drive using the SCSI SPOUT command set.

If you have implemented Library Managed Encryption (LME) or a Key Management Appliance (KMA), there is no need to have support from Bareos on loading and clearing the encryption keys, as either the Library knows the per volume encryption keys itself, or it will ask the KMA for the encryption key when it needs it. For big installations you might consider using a KMA, but the Application Managed Encryption implemented in Bareos should also scale rather well and have a low overhead as the keys are
only loaded and cleared when needed.

The scsicrypto-sd plugin
''''''''''''''''''''''''

The :command:`scsicrypto-sd` hooks into the :strong:`unload`, :strong:`label read`, :strong:`label write` and :strong:`label verified` events for loading and clearing the key. It checks whether it it needs to clear the drive by either using an internal state (if it loaded a key before) or by checking the state of a special option that first issues an encrytion status query. If there is a connection to the director
and the volume information is not available, it will ask the director for the data on the currently loaded volume. If no connection is available, a cache will be used which should contain the most recently mounted volumes. If an encryption key is available, it will be loaded into the drive’s memory.

Changes in the director
'''''''''''''''''''''''

The director has been extended with additional code for handling hardware data encryption. The extra keyword **encrypt** on the label of a volume will force the director to generate a new semi-random passphrase for the volume, which will be stored in the database as part of the media information.

A passphrase is always stored in the database base64-encoded. When a so called **Key Encryption Key** is set in the config of the director, the passphrase is first wrapped using RFC3394 key wrapping and then base64-encoded. By using key wrapping, the keys in the database are safe against people sniffing the info, as the data is still encrypted using the Key Encryption Key (which in essence is just an extra passphrase of the same length as the volume passphrases used).

When the storage daemon needs to mount the volume, it will ask the director for the volume information and that protocol is extended with the exchange of the base64-wrapped encryption key (passphrase). The storage daemon provides an extra config option in which it records the Key Encryption Key of the particular director, and as such can unwrap the key sent into the original passphrase.

As can be seen from the above info we don’t allow the user to enter a passphrase, but generate a semi-random passphrase using the openssl random functions (if available) and convert that into a readable ASCII stream of letters, numbers and most other characters, apart from the quotes and space etc. This will produce much stronger passphrases than when requesting the info from a user. As we store this information in the database, the user never has to enter these passphrases.

The volume label is written in unencrypted form to the volume, so we can always recognize a Bareos volume. When the key is loaded onto the drive, we set the decryption mode to mixed, so we can read both unencrypted and encrypted data from the volume. When no key or the wrong key has been loaded, the drive will give an IO error when trying to read the volume. For disaster recovery you can store the Key Encryption Key and the content of the wrapped encryption keys somewhere safe and the
:ref:`bscrypto <bscrypto>` tool together with the scsicrypto-sd plugin can be used to get access to your volumes, in case you ever lose your complete environment.

If you don’t want to use the scsicrypto-sd plugin when doing DR and you are only reading one volume, you can also set the crypto key using the bscrypto tool. Because we use the mixed decryption mode, in which you can read both encrypted and unencrypted data from a volume, you can set the right encryption key before reading the volume label.

If you need to read more than one volume, you better use the scsicrypto-sd plugin with tools like bscan/bextract, as the plugin will then auto-load the correct encryption key when it loads the volume, similarly to what the storage daemon does when performing backups and restores.

The volume label is unencrypted, so a volume can also be recognized by a non-encrypted installation, but it won’t be able to read the actual data from it. Using an encrypted volume label doesn’t add much security (there is no security-related info in the volume label anyhow) and it makes it harder to recognize either a labeled volume with encrypted data or an unlabeled new volume (both would return an IO-error on read of the label.)

.. _configuration-1:

Configuration of the scsicrypto-sd plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

SCSI crypto setup
'''''''''''''''''

The initial setup of SCSI crypto looks something like this:

-  Generate a Key Encryption Key e.g.

   .. code-block:: shell-session

      bscrypto -g -

For details see :ref:`bscrypto <bscrypto>`.

Security Setup
''''''''''''''

Some security levels need to be increased for the storage daemon to be able to use the low level SCSI interface for setting and getting the encryption status on a tape device.

The following additional security is needed for the following operating systems:

Linux (SG_IO ioctl interface):


.. index::
  single: Platform; Linux; Privileges

To perform the operations required for **scsicrypto**, the programs must either run as user **root** or the additional capability :strong:`CAP_SYS_RAWIO+EP` (see capabilities(7)) must be set.
The |sd| normally runs as user **bareos**. Running it as **root** is not recommended.

If :command:`bareos-sd` does not have the appropriate capabilities, all other tape operations may still work correctly, but you will get "Unable to perform SG\_IO ioctl" errors.

.. note::

    Since :sinceVersion:`21.0.1: scscicrypto capabilities handling` package installation and upgrade will check for the presence of :file:`.enable-cap_sys_rawio` in your bareos config dir and will configure the required capabilities.
    If you want capabilities automatically set up during package install, you can just create :file:`/etc/bareos/.enable-cap_sys_rawio`.

    Before :sinceVersion:`21.0.1: scscicrypto capabilities handling` it is mandatory to setup capabilities manually after each update (see below).


**setcap binaries** (recommended)

You can also set up the extra capability on :command:`bareos-sd`, :command:`bcopy`, :command:`bextract`, :command:`bls`, :command:`bscan`, :command:`bscrypto`, :command:`btape` by running the following commands:

Set the setting with our helper

.. code-block:: shell-session

   /usr/lib/bareos/scripts/bareos-config set_scsicrypto_capabilities


Set the setting manually

.. code-block:: shell-session

   setcap cap_sys_rawio=ep /usr/sbin/bareos-sd
   setcap cap_sys_rawio=ep /usr/sbin/bcopy
   setcap cap_sys_rawio=ep /usr/sbin/bextract
   setcap cap_sys_rawio=ep /usr/sbin/bls
   setcap cap_sys_rawio=ep /usr/sbin/bscan
   setcap cap_sys_rawio=ep /usr/sbin/bscrypto
   setcap cap_sys_rawio=ep /usr/sbin/btape


Remove the setting with our helper

.. code-block:: shell-session

   /usr/lib/bareos/scripts/bareos-config unset_scsicrypto_capabilities

Remove the setting manually

.. code-block:: shell-session

   setcap -r /usr/sbin/bareos-sd
   setcap -r /usr/sbin/bcopy
   setcap -r /usr/sbin/bextract
   setcap -r /usr/sbin/bls
   setcap -r /usr/sbin/bscan
   setcap -r /usr/sbin/bscrypto
   setcap -r /usr/sbin/btape


Check the setting with our helper

.. code-block:: shell-session

   /usr/lib/bareos/scripts/bareos-config check_scsicrypto_capabilities


Check the setting manually

.. code-block:: shell-session

   getcap -v /usr/sbin/bareos-sd
   getcap -v /usr/sbin/bcopy
   getcap -v /usr/sbin/bextract
   getcap -v /usr/sbin/bls
   getcap -v /usr/sbin/bscan
   getcap -v /usr/sbin/bscrypto
   getcap -v /usr/sbin/btape


:command:`getcap` and :command:`setcap` are part of libcap-progs.


.. warning::

   Adding capabilities like cap_sys_rawio to binaries can increase their abuse.
   We also recommend to restrict a bit more their ownership to root as owner and bareos as group, plus setting chmod to 0750. Doing so will restrict execution root and members of group bareos.
   All these steps are done for you by our helper.


   **systemd** (not recommended)

To add the capabilities to bareos-sd.service you can add in file :file:`/etc/systemd/system/bareos-sd.d/override.conf` a section containing the :strong:`AmbientCapabilities=CAP_SYS_RAWIO` line.
The easiest way to create this file is to use the following instructions as root.

.. code-block:: shell-session

   systemctl edit bareos-sd.service

Fill the file with the following content, then save and exit

.. code-block:: cfg

   ### Editing /etc/systemd/system/bareos-storage.service.d/override.conf
   ### Anything between here and the comment below will become the new contents of the file

   [Service]
   AmbientCapabilities=CAP_SYS_RAWIO


Reload systemd configuration and restart bareos-sd

.. code-block:: shell-session

   systemctl daemon-reload

   systemctl restart bareos-sd

   systemctl status bareos-sd
      ● bareos-storage.service - Bareos Storage Daemon service
      Loaded: loaded (/lib/systemd/system/bareos-storage.service; enabled; vendor preset: enabled)
      Drop-In: /etc/systemd/system/bareos-storage.service.d
      └─override.conf
      Active: active (running) since Tue 2022-02-01 15:12:49 CET; 5s ago
      Docs: man:bareos-sd(8)
      Main PID: 11142 (bareos-sd)
      Tasks: 2 (limit: 2298)
      Memory: 1.1M
      CPU: 8ms
      CGroup: /system.slice/bareos-storage.service
      └─11142 /usr/sbin/bareos-sd -f

      systemd[1]: Started Bareos Storage Daemon service.


To check status of capabilities of the running daemon you can use the :command:`getpcaps` followed by the pid of bareos-sd.

.. code-block:: shell-session

   root:~# getpcaps 11142
   11142: cap_sys_rawio=eip


.. warning::

   As of systemd version 249 There's no mechanism to pass restricted flag (+ep), so the result will always be full CAP_SYS_RAWIO (eip)



Solaris (USCSI ioctl interface):


The user running the storage daemon needs the following additional privileges:

.. index::
  single: Platform; Solaris; Privileges

-  :strong:`PRIV_SYS_DEVICES` (see privileges(5))

If you are running the storage daemon as another user than root (which has the :strong:`PRIV_SYS_DEVICES` privilege), you need to add it to the current set of privileges. This can be set up by setting this either as a project for the user, or as a set of extra privileges in the SMF definition starting the storage daemon. The SMF setup is the cleanest one.

For SMF make sure you have something like this in the instance block:

.. code-block:: bareosconfig

   <method_context working_directory=":default"> <method_credential user="bareos" group="bareos" privileges="basic,sys_devices"/> </method_context>

Changes in bareos-sd configuration
''''''''''''''''''''''''''''''''''

-  Set the Key Encryption Key

   -  :config:option:`sd/director/KeyEncryptionKey = passphrase`

-  Enable the loading of storage daemon plugins

   -  :config:option:`sd/storage/PluginDirectory = path_to_sd_plugins`

-  Enable the SCSI encryption option

   -  :config:option:`sd/device/DriveCryptoEnabled = yes`

-  Enable this, if you want the plugin to probe the encryption status of the drive when it needs to clear a pending key

   -  :config:option:`sd/device/QueryCryptoStatus = yes`

Changes in bareos-dir configuration
'''''''''''''''''''''''''''''''''''

-  Set the Key Encryption Key

   -  :config:option:`dir/director/KeyEncryptionKey = passphrase`

Testing
^^^^^^^

Restart the Storage Daemon and the Director. After this you can label new volumes with the encrypt option, e.g.

.. code-block:: bareosconfig

   label slots=1-5 barcodes encrypt

Disaster Recovery
^^^^^^^^^^^^^^^^^

For Disaster Recovery (DR) you need the following information:

-  Actual bareos-sd configuration files with config options enabled as described above, including, among others, a definition of a director with the Key Encryption Key used for creating the encryption keys of the volumes.

-  The actual keys used for the encryption of the volumes.

This data needs to be available as a so called crypto cache file which is used by the plugin when no connection to the director can be made to do a lookup (most likely on DR).

Most of the times the needed information, e.g. the bootstrap info, is available on recently written volumes and most of the time the encryption cache will contain the most recent data, so a recent copy of the :file:`bareos-sd.<portnr>.cryptoc` file in the working directory is enough most of the time. You can also save the info from database in a safe place and use bscrypto to populate this info (VolumeName |rarr| EncryptKey) into the crypto cache file used by
:command:`bextract` and :command:`bscan`. You can use :command:`bscrypto` with the following flags to create a new or update an existing crypto cache file e.g.:

.. code-block:: shell-session

   bscrypto -p /var/lib/bareos/bareos-sd.<portnr>.cryptoc

-  A valid BSR file containing the location of the last safe of the database makes recovery much easier. Adding a post script to the database save job could collect the needed info and make sure its stored somewhere safe.

-  Recover the database in the normal way e.g. for postgresql:

   .. code-block:: shell-session

      bextract -D <director_name> -V <volname> /dev/nst0 /tmp -b bootstrap.bsr
      /usr/lib/bareos/scripts/create_bareos_database
      /usr/lib/bareos/scripts/grant_bareos_privileges
      psql bareos < /tmp/var/lib/bareos/bareos.sql

Or something similar (change paths to follow where you installed the software or where the package put it).

.. note::

   As described at the beginning of this chapter, there are different types of key management, AME, LME and KMA. If the Library is set up for LME or KMA, it probably won’t allow our AME setup and the scsi-crypto plugin will fail to set/clear the encryption key. To be able to use AME you need to "Modify Encryption Method" and set it to something like "Application Managed". If you decide to use LME or KMA you don’t have to bother with the whole setup of AME which may for big libraries be easier, although the overhead of using AME even for very big libraries should be minimal.
