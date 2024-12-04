Storage Backends
================

A Bareos Storage Daemon can use various storage backends:

**Tape**
   is used to access tape device and thus has sequential access.

**File**
   tells Bareos that the device is a file. It may either be a file defined on fixed medium or a removable filesystem such as USB. All files must be random access devices.

**Fifo**
   is a first-in-first-out sequential access read-only or write-only device.

**Droplet**
   is used to access an object store supported by **libdroplet**, most notably S3. For details, refer to :ref:`SdBackendDroplet`.

**Dplcompat**
   replacement for Droplet with compatible storage format (i.e. you can switch from Droplet to Dplcompat and back), see :ref:`SdBackendDplcompat`.

**GFAPI** (GlusterFS)
   is used to access a GlusterFS storage.

**Dedupable**
   is used to support filesystem block-based deduplication, see :ref:`SdBackendDedupable`.

.. include:: StorageBackends/Droplet.rst.inc
.. include:: StorageBackends/Dplcompat.rst.inc

.. _SdBackendGfapi:

GFAPI Storage Backend
---------------------

**GFAPI** (GlusterFS)

A GlusterFS Storage can be used as Storage backend of Bareos. Prerequisites are a working
GlusterFS storage system and the package **bareos-storage-glusterfs**.
See https://www.gluster.org/ for more information regarding GlusterFS installation and
configuration and specifically `https://docs.gluster.org/en/latest/Administrator-Guide/Bareos/ <https://docs.gluster.org/en/latest/Administrator-Guide/Bareos/>`__
for Bareos integration. You can use following snippet to configure it as storage device:



   .. literalinclude:: /include/config/SdDeviceDeviceOptionsGfapi1.conf
      :language: bareosconfig



Adapt server and volume name to your environment.

:sinceVersion:`15.2.0: GlusterFS Storage`

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
