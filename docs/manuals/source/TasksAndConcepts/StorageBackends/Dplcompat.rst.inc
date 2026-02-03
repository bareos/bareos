.. _SdBackendDplcompat:

Dplcompat Storage Backend
-------------------------

.. index::
   single: Backend; Dplcompat
   single: Backend; Dplcompat; S3
   single: Backend; S3|see {Backend; Dplcompat}

The **bareos-storage-dplcompat** backend (:sinceVersion:`24.0.0: Dplcompat`) can
be used to access Object Storage through external wrapper programs.
For information on bundled wrappers, see :ref:`section-DplcompatWrappers`.
To learn how to implement your own wrapper, see :ref:`WritingDplcompatWrappers`.

Installation
~~~~~~~~~~~~

Install the package **bareos-storage-dplcompat**.

Configuration
~~~~~~~~~~~~~

The droplet backend requires a |dir| :ref:`DirectorResourceStorage`, a |sd|
:ref:`StorageResourceDevice` as well as a wrapper script that facilitates access
to the object storage.
This wrapper script may need additional configuration options, that are passed
using :config:option:`sd/device/DeviceOptions`.

Storage Daemon
^^^^^^^^^^^^^^
The following :config:option:`sd/device/DeviceOptions`\  settings configure
dplcompat itself:

chunksize
   Size of Volume Chunks in Bytes (default: 10 MB).

iothreads
   Number of IO-threads to use for uploads (if not set, blocking uploads are
   used)

ioslots
   Number of IO-slots per IO-thread (0-255, default 10). Set this to values
   greater than 1 for cached and to 0 for direct writing.

retries
   Number of writing tries before discarding the data (0-255, default: 0).
   Setting this to a value greater zero will cause data-loss if the backend is
   not available.

program
   The wrapper program to use. Either an absolute path or the name of a program
   in :config:option:`sd/storage/ScriptsDirectory`.

program_timeout
   Timeout in seconds after which the wrapper program is presumed dead if it
   does not respond to I/O operations (default: 30).


.. tip::
   Due to the nature of Dplcompat, it benefits from large chunksizes, because
   that reduces the number of wrapper processes the plugin spawns.


.. warning::
   The SD will allocate up to :math:`iothreads * ioslots * chunksize` bytes of
   memory for the device. With larger chunksize settings, this escalates pretty
   quickly. Make sure your have enough memory or reduce the number of ioslots.


Example
'''''''
We want to configure a device **ObjectStore**:

* Upload up to four chunks of 256 MB in parallel.
* Limit to at most 2 chunks per upload-thread.
* Use the wrapper program ``acme-wrapper``.
* Pass options ``fruit`` with value ``banana`` and ``vegetable`` with value
  ``carrot`` to the wrapper program.

.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/ObjectStore.conf

   Device {
     Name = ObjectStore
     Media Type = Objects
     Archive Device = Object Storage # currently unused, but required
     Device Options = "iothreads=4"
                      ",ioslots=2"
                      ",chunksize=262144000"
                      ",program=acme-wrapper"
                      ",fruit=banana"
                      ",vegetable=carrot"
     Device Type = dplcompat
     LabelMedia = yes
     Random Access = yes
     AutomaticMount = yes
     RemovableMedia = no
     AlwaysOpen = no
   }

.. note::
   For improved readability the :config:option:`sd/device/DeviceOptions` was
   split into multiple lines.
   This is still a single value containing a comma-separated list of key-value
   pairs.

Director
^^^^^^^^
To connect the director to the storage, a :ref:`DirectorResourceStorage` must be
configured.

Example
'''''''
Based on the example above, a matching :ref:`DirectorResourceStorage` would be

.. code-block:: bareosconfig
   :caption: bareos-dir.d/storage/ObjectStore.conf

   Storage {
     Name = ObjectStore
     Media Type = Objects
     Address  = "bareos-sd.example.com"
     Password = "secret"
     Device = ObjectStore
   }

.. _section-DplcompatWrappers:

Bundled Wrapper Programs
~~~~~~~~~~~~~~~~~~~~~~~~
The scripts in this section are bundled with Dplcompat.

s3cmd-wrapper.sh
^^^^^^^^^^^^^^^^
This program wraps ``s3cmd``. It connects Dplcompat to S3 compatible storages.

Prerequisites
'''''''''''''
This wrapper program requires a properly configured ``s3cmd``.

You can install ``s3cmd`` with your system's package manager or using ``pip``
with or without a ``venv``.

Configuring ``s3cmd`` can be done interactively using ``s3cmd --configure``.
The resulting configuration file (usually ``$HOME/.s3cfg``) should be copied
into the Bareos configuration directory and its owner should be set to the user
that runs the |sd|.

When running ``s3cmd`` as the user that also runs the |sd| and providing the
configuration file, you should be able to access your S3 bucket.
If the command ``s3cmd --config /etc/bareos/s3cmd.cfg info s3://your-bucket``
returns successfully, the configuration should be fine.
The bucket must allow read/write access to the access key or token that is used,
of course.

Configuration
'''''''''''''
s3cmd_prog
   Configure the full path to the s3cmd program to use (default: autodetected)

s3cfg
   Full path to the s3cfg file to read the configuration from (required).

bucket
   Name of the S3 bucket to use (default: "backup")

base_url 
   Override the base URL used to generate the object URLs from. When this is
   set, the bucket option is ignored. (no default)

storage_class
   Defines the storage class of newly uploaded objects. When this is not set,
   the bucket's policy applies.

Example
'''''''
The following example configures a :ref:`StorageResourceDevice` as follows:

* use ``s3cmd-wrapper.sh``
* read ``s3cmd`` configuration from ``/etc/bareos/s3cmd.cfg``
* use the bucket named "bareos"
* set the storage class to "ONEZONE_IA"

.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/S3Cmd-Example.conf

   Device {
     Name = S3Cmd-Example
     Media Type = S3
     Archive Device = Object Storage # currently unused, but required
     Device Options = "iothreads=4"
                      ",ioslots=2"
                      ",chunksize=262144000"
                      ",program=s3cmd-wrapper.sh"
                      ",s3cfg=/etc/bareos/s3cmd.cfg"
                      ",bucket=bareos"
                      ",storage_class=ONEZONE_IA"
     Device Type = dplcompat
     LabelMedia = yes
     Random Access = yes
     AutomaticMount = yes
     RemovableMedia = no
     AlwaysOpen = no
   }
