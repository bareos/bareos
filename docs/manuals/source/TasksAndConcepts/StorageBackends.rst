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

**GFAPI** (GlusterFS)
   is used to access a GlusterFS storage.

**Rados** (Ceph Object Store)
   is used to access a Ceph object store.

.. _SdBackendDroplet:

Droplet Storage Backend
-----------------------

:index:`\ <single: Backend; Droplet>`
:index:`\ <single: Backend; Droplet; S3>`
:index:`\ <single: Backend; S3|see {Backend; Droplet}>`

The **bareos-storage-droplet** backend (:sinceVersion:`17.2.7: Droplet`) can be used to access Object Storage through **libdroplet**. Droplet support a number of backends, most notably S3. For details about Droplet itself see https://github.com/scality/Droplet.

Requirements
~~~~~~~~~~~~

-  The Bareos package **bareos-storage-droplet** is not available on all platforms. Please refer to :ref:`section-packages`.

-  Droplet S3:

   -  The droplet S3 backend can only be used with virtual-hosted-style buckets like http://bucket.s3_server/object. Path-style buckets are not supported. It has been tested successfully with AWS S3 and CEPH Object Gateway S3.

Installation
~~~~~~~~~~~~

Install the package **bareos-storage-droplet** by using an appropriate package management tool (eg. :command:`yum`, :command:`zypper`).

Configuration
~~~~~~~~~~~~~

The droplet backend requires a |dir| :ref:`DirectorResourceStorage`, a |sd| :ref:`StorageResourceDevice` as well as a Droplet profile file where your access– and secret–keys and other parameters for the connection to your object storage are stored.

.. _section-DropletAwsS3:

AWS S3
^^^^^^

Director
''''''''

First, we will create the new |dir| :ref:`DirectorResourceStorage`.

For the following example, we

-  choose the name :config:option:`Dir/Storage = S3_Object`\ .

-  choose :config:option:`dir/storage/MediaType = S3_Object1`\ . We name it this way, in case we later add more separated Object Storages that don’t have access to the same volumes.

-  assume the |sd| is located on the host :strong:`bareos-sd.example.com` and will offers the :ref:`StorageResourceDevice` :config:option:`Sd/Device = S3_ObjectStorage`\  (to be configured in the next section).

.. code-block:: bareosconfig
   :caption: bareos-dir.d/storage/S3\_Object.conf

   Storage {
       Name = "S3_Object"
       Address  = "bareos-sd.example.com"
       Password = "secret"
       Device = "AWS_S3_1-00"
       Media Type = "S3_Object1"
   }

These credentials are only used to connect to the |sd|. The credentials to access the object store (e.g. S3) are stored in the |sd| Droplet Profile.

Storage Daemon
''''''''''''''

As of your |sd| configuration, we need to setup a new device that acts as a link to Object Storage backend.

The name and media type must correspond to those settings in the |dir| :ref:`DirectorResourceStorage`:

-  :config:option:`sd/device/Name`\  = :config:option:`dir/storage/Device`\ 

-  :config:option:`sd/device/MediaType`\  = :config:option:`dir/storage/MediaType`\ 

.. limitation:: Droplet Backend does not support block interleaving

  The current implementation has a known Bug that may lead to bogus data on your S3 volumes when you set :config:option:`sd/device/MaximumConccurentJobs` to a value other than 1.
  Because of this the default for a backend of type Droplet is set to 1 and the |sd| will refuse to start if you set it to a value greater than 1.


A device for the usage of AWS S3 object storage with a bucket named :file:`backup-bareos` located in EU Central 1 (Frankfurt, Germany), would look like this:

.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/AWS\_S3\_1-00.conf

   Device {
     Name = "AWS_S3_1-00"
     Media Type = "S3_Object1"
     Archive Device = "AWS S3 Storage"
     Device Type = droplet
     Device Options = "profile=/etc/bareos/bareos-sd.d/device/droplet/aws.profile,bucket=backup-bareos,chunksize=100M"
     Label Media = yes                    # Lets Bareos label unlabeled media
     Random Access = yes
     Automatic Mount = yes                # When device opened, read it
     Removable Media = no
     Always Open = no
     Maximum Concurrent Jobs = 1
   }

In these examples all the backup data is placed in the :file:`bareos-backup` bucket on the defined S3 storage. In contrast to other |sd| backends, a Bareos volume is not represented by a single file. Instead a volume is a sub-directory in the defined bucket and every chunk is placed in the volume directory with the filename 0000-9999 and a size defined in the chunksize option. It is implemented this way, as S3 does not allow to append to a file. Instead it always writes full
files, so every append operation could result in reading and writing the full volume file.

Following :config:option:`sd/device/DeviceOptions`\  settings are possible:

profile
   Droplet profile path (e.g. /etc/bareos/bareos-sd.d/device/droplet/droplet.profile). Make sure the profile file is readable for user **bareos**.

acl
   Canned ACL

storageclass
   Storage Class to use.

bucket
   Bucket to store objects in.

chunksize
   Size of Volume Chunks (default = 10 Mb).

iothreads
   Number of IO-threads to use for uploads (if not set, blocking uploads are used)

ioslots
   Number of IO-slots per IO-thread (0-255, default 10). Set this to values greater than 1 for cached and to 0 for direct writing.

retries
   Number of writing tries before discarding the data. Set this to 0 for unlimited retries. Setting anything != 0 here will cause dataloss if the backend is not available, so be very careful (0-255, default = 0, which means unlimited retries).

mmap
   Use mmap to allocate Chunk memory instead of malloc().

location
   Deprecated. If required (AWS only), it has to be set in the Droplet profile.

Create the Droplet profile to be used. This profile is used later by the droplet library when accessing your cloud storage.

An example for AWS S3 could look like this:

.. code-block:: cfg
   :caption: aws.profile

   host = s3.amazonaws.com         # This parameter is only used as baseurl and will be prepended with bucket and location set in device resource to form correct url
   use_https = true
   access_key = myaccesskey
   secret_key = mysecretkey
   pricing_dir = ""                # If not empty, an droplet.csv file will be created which will record all S3 operations.
   backend = s3
   aws_auth_sign_version = 4       # Currently, AWS S3 uses version 4. The Ceph S3 gateway uses version 2.
   aws_region = eu-central-1

More arguments and the SSL parameters can be found in the documentation of the droplet library: \externalReferenceDropletDocConfigurationFile

CEPH Object Gateway S3
^^^^^^^^^^^^^^^^^^^^^^

Please note, that there is also the :ref:`SdBackendRados` backend, which can backup to CEPH directly. However, currently (17.2.7) the **Droplet** (S3) is known to outperform the **Rados** backend.

While parameters have been explained in the :ref:`section-DropletAwsS3` section, this gives an example about how to backup to a CEPH Object Gateway S3.

.. code-block:: bareosconfig
   :caption: bareos-dir.d/storage/S3\_Object.conf

   Storage {
       Name = "S3_Object"
       Address  = "bareos-sd.example.com"
       Password = "secret"
       Device = "CEPH_1-00"
       Media Type = "S3_Object1"
   }

A device for CEPH object storage could look like this:

.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/CEPH\_1-00.conf

   Device {
     Name = "CEPH_1-00"
     Media Type = "S3_Object1"
     Archive Device = "Object S3 Storage"
     Device Type = droplet
     Device Options = "profile=/etc/bareos/bareos-sd.d/droplet/ceph-rados-gateway.profile,bucket=backup-bareos,chunksize=100M"
     Label Media = yes                    # Lets Bareos label unlabeled media
     Random Access = yes
     Automatic Mount = yes                # When device opened, read it
     Removable Media = no
     Always Open = no
     Maximum Concurrent Jobs = 1
   }

The correspondig Droplet profile looks like this:

.. code-block:: cfg
   :caption: ceph-rados-gateway.profile

   host = CEPH-host.example.com
   use_https = False
   access_key = myaccesskey
   secret_key = mysecretkey
   pricing_dir = ""
   backend = s3
   aws_auth_sign_version = 2

Main differences are, that :file:`aws_region` is not required and :file:`aws_auth_sign_version = 2` instead of 4.

Troubleshooting
~~~~~~~~~~~~~~~

iothreads
^^^^^^^^^

For testing following :config:option:`sd/device/DeviceOptions`\  should be used:

-  :file:`iothreads=0`

-  :file:`retries=1`

If the S3 backend is or becomes unreachable, the |sd| will behave depending on :strong:`iothreads` and :strong:`retries`. When the |sd| is using cached writing (:strong:`iothreads >=1`) and :strong:`retries` is set to zero (unlimited tries), the job will continue running until the backend becomes available again. The job cannot be canceled in this case, as the |sd| will
continuously try to write the cached files.

Great caution should be used when using :strong:`retries>=0` combined with cached writing. If the backend becomes unavailable and the |sd| reaches the predefined tries, the job will be discarded silently yet marked as :file:`OK` in the |dir|.

You can always check the status of the writing process by using :bcommand:`status storage=...`. The current writing status will be displayed then:

.. code-block:: bconsole
   :caption: status storage

   ...
   Device "S3_ObjectStorage" (S3) is mounted with:
       Volume:      Full-0085
       Pool:        Full
       Media type:  S3_Object1
   Backend connection is working.
   Inflight chunks: 2
   Pending IO flush requests:
      /Full-0085/0002 - 10485760 (try=0)
      /Full-0085/0003 - 10485760 (try=0)
      /Full-0085/0004 - 10485760 (try=0)
   ...
   Attached Jobs: 175
   ...

:strong:`Pending IO flush requests` means that there is data to be written. :strong:`try`=0 means that this is the first try and no problem has occurred. If :strong:`try >0`, problems occurred and the storage daemon will continue trying.

Status without pending IO chunks:

.. code-block:: bconsole
   :caption: status storage

   ...
   Device "S3_ObjectStorage" (S3) is mounted with:
       Volume:      Full-0084
       Pool:        Full
       Media type:  S3_Object1
   Backend connection is working.
   No Pending IO flush requests.
   Configured device capabilities:
     EOF BSR BSF FSR FSF EOM !REM RACCESS AUTOMOUNT LABEL !ANONVOLS !ALWAYSOPEN
   Device state:
     OPENED !TAPE LABEL !MALLOC APPEND !READ EOT !WEOT !EOF !NEXTVOL !SHORT MOUNTED
     num_writers=0 reserves=0 block=8
   Attached Jobs:
   ...

For performance, :config:option:`sd/device/DeviceOptions`\  should be configured with:

-  :file:`iothreads >= 1`

-  :file:`retries = 0`

New AWS S3 Buckets
^^^^^^^^^^^^^^^^^^

As AWS S3 buckets are accessed via virtual-hosted-style buckets (like http://bucket.s3_server/object) creating a new bucket results in a new DNS entry.

As a new DNS entry is not available immediatly, Amazon solves this by using HTTP temporary redirects (code: 307) to redirect to the correct host. Unfortenatly, the Droplet library does not support HTTP redirects.

Requesting the device status only resturn a unspecific error:

.. code-block:: bconsole
   :caption: status storage

   *status storage=...
   ...
   Backend connection is not working.
   ...

Workaround:
'''''''''''

-  Wait until bucket is available a permanet hostname. This can take up to 24 hours.

-  Configure the AWS location into the profiles host entry. For the AWS location :file:`eu-central-1`, change ``host = s3.amazonaws.com`` into ``host = s3.eu-central-1.amazonaws.com``:

   .. code-block:: cfg
      :caption: Droplet profile

      ...
      host = s3.eu-central-1.amazonaws.com
      aws_region = eu-central-1
      ...

AWS S3 Logging
^^^^^^^^^^^^^^

If you use AWS S3 object storage and want to debug your bareos setup, it is recommended to turn on the server access logging in your bucket properties. You will see if bareos gets to try writing into your bucket or not.

.. _SdBackendGfapi:

GFAPI Storage Backend
---------------------

**GFAPI** (GlusterFS)

A GlusterFS Storage can be used as Storage backend of Bareos. Prerequistes are a working GlusterFS storage system and the package **bareos-storage-glusterfs**. See http://www.gluster.org/ for more information regarding GlusterFS installation and configuration and specifically `https://gluster.readthedocs.org/en/latest/Administrator Guide/Bareos/ <https://gluster.readthedocs.org/en/latest/Administrator Guide/Bareos/>`__ for Bareos integration. You can use following snippet to
configure it as storage device:



   .. literalinclude:: /include/config/SdDeviceDeviceOptionsGfapi1.conf
      :language: bareosconfig



Adapt server and volume name to your environment.

:sinceVersion:`15.2.0: GlusterFS Storage`

.. _SdBackendRados:

Rados Storage Backend
---------------------

**Rados** (Ceph Object Store)

Here you configure the Ceph object store, which is accessed by the SD using the Rados library. Prerequistes are a working Ceph object store and the package **bareos-storage-ceph**. See http://ceph.com for more information regarding Ceph installation and configuration. Assuming that you have an object store with name :file:`poolname` and your Ceph access is configured in :file:`/etc/ceph/ceph.conf`, you can use following snippet to configure it as
storage device: 

.. literalinclude:: /include/config/SdDeviceDeviceOptionsRados1.conf
   :language: bareosconfig



:sinceVersion:`15.2.0: Ceph Storage`




