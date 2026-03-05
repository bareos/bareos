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
