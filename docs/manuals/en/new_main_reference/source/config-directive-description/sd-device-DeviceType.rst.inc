The Device Type specification allows you to explicitly define the kind of device you want to use. It may be one of the following:

**Tape**
   is used to access tape device and thus has sequential access. Tape devices are controlled using ioctl() calls.

**File**
   tells Bareos that the device is a file. It may either be a file defined on fixed medium or a removable filesystem such as USB. All files must be random access devices.

**Fifo**
   is a first-in-first-out sequential access read-only or write-only device.

**GFAPI** (GlusterFS)
   is used to access a GlusterFS storage. It must be configured using :config:option:`sd/device/DeviceOptions`\ . For details, refer to :ref:`SdBackendGfapi`.

   :index:`Version >= 14.2.2 <pair: bareos-14.2.2; GlusterFS (gfapi)>`

**Rados** (Ceph Object Store)
   is used to access a Ceph object store. It must be configured using :config:option:`sd/device/DeviceOptions`\ . For details, refer to :ref:`SdBackendRados`.

   :index:`Version >= 14.2.2 <pair: bareos-14.2.2; Ceph (Rados)>`

**Droplet**
   is used to access an object store supported by **libdroplet**, most notably S3. For details, refer to :ref:`SdBackendDroplet`.

   :index:`Version >= 17.2.7 <pair: bareos-17.2.7; Droplet>`

The Device Type directive is not required in all cases. If it is not specified, Bareos will attempt to guess what kind of device has been specified using the :config:option:`sd/device/ArchiveDevice`\  specification supplied. There are several advantages to explicitly specifying the Device Type. First, on some systems, block and character devices have the same type. Secondly, if you explicitly specify the Device Type, the mount point need not be defined until the device is
opened. This is the case with most removable devices such as USB. If the Device Type is not explicitly specified, then the mount point must exist when the Storage daemon starts.

