.. _SdBackends:

Storage Backends
================

A Bareos Storage Daemon can use various storage backends:

**Tape**
   is used to access tape device and thus has sequential access.

**File**
   tells Bareos that the device is a file. It may either be a file defined on fixed medium or a removable filesystem such as USB. All files must be random access devices.

**Fifo**
   is a first-in-first-out sequential access read-only or write-only device.

**Dplcompat**
   replacement for Droplet with compatible storage format (i.e. you can switch from Droplet to Dplcompat and back), see :ref:`SdBackendDplcompat`.

**Dedupable**
   is used to support filesystem block-based deduplication, see :ref:`SdBackendDedupable`.

..
   Hint: check REGISTER_SD_BACKEND in /stored/backends to find allowed types

The :config:option:`sd/device/DeviceType` directive is not required in all
cases. If it is not specified, Bareos will attempt to guess what kind of device
has been specified using the :config:option:`sd/device/ArchiveDevice`\
specification supplied. There are several advantages to explicitly specifying
the Device Type. First, on some systems, block and character devices have the
same type. Secondly, if you explicitly specify the Device Type, the mount point
need not be defined until the device is opened. This is the case with most
removable devices such as USB. If the Device Type is not explicitly specified,
then the mount point must exist when the Storage daemon starts.



.. include:: StorageBackends/Dplcompat.rst.inc

.. _SdBackendDedupable:

Dedupable Backend
-----------------

**Dedupable**

A dedupable device splits up the data it receives into bareos specific data and file data
to store them separately.  It tries to store the file data blocks it receives only at certain intervals
in its storage. This should make it possible for your filesystem to deduplicate its the actual file data.

For this to work correctly, the device option **BlockSize** needs to be set to the block size
your filesystem uses to deduplicate.  It is also important that :config:option:`fd/client/MaximumNetworkBufferSize`\ is
divisible by this size.

   .. literalinclude:: /include/config/SdDeviceDeviceOptionsDedupable.conf
      :language: bareosconfig
      :caption: example configuration

:sinceVersion:`23.1.0: Dedupable Storage`
