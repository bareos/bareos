.. _LibcloudPlugin:

Apache Libcloud Plugin
~~~~~~~~~~~~~~~~~~~~~~

.. index::
   pair: Plugin; libcloud

The Libcloud plugin can be used to backup objects from cloud storages via the *Simple Storage Service* (**S3**) protocol. The plugin code is based on the work of Alexandre Bruyelles.

.. _LibcloudPlugin-status:

Status of Libcloud Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^

The status of the Libcloud plugin is **experimental**. It can automatically recurse nested Buckets and backup all included Objects
on a S3 storage. However, **restore of objects cannot be done directly back to the storage**. A restore will write these objects
*as files on a filesystem*.

.. _LibcloudPlugin-requirements:

Requirements of Libcloud Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To use the Apache Libcloud backend you need to have the Libcloud module available for Python 2.

The plugin needs several options to run properly, the plugin options in the fileset resource and an additional configuration file. Both is described below.

.. _LibcloudPlugin-installation:

Installation of Libcloud Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The installation is done by installing the package **bareos-filedaemon-libcloud-python-plugin**.


.. _LibcloudPlugin-configuration:

Configuration of Libcloud Plugin
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/fileset/PluginTest.conf

   FileSet {
     Name = "PluginTest"
     Description = "Test the Plugin functionality with a Python Plugin."
     Include {
       Options {
         Signature = XXH128
       }
       Plugin = "python"
                ":module_name=bareos-fd-libcloud"
                ":config_file=/etc/bareos/libcloud_config.ini"
                ":buckets_include=user_data"
                ":buckets_exclude=tmp"
     }
   }

The plugin options, separated by a colon:

module_path
   Path to the bareos modules (optional)

module_name=bareos-fd-libcloud
   This is the name of the plugin module

config_file
   The plugin needs additional parameters, this is the path to the config file (see below)

buckets_include
   Comma-separated list of buckets to include in backup

buckets_exclude
   Comma-separated list of buckets to exclude from backup


And the job as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/bareos-dir.d/job/testvm1_job.conf

   Job {
      Name = "testlibcloud_job"
      JobDefs = "DefaultJob"
      FileSet = "PluginTest"
   }

And the plugin config file as follows:

.. code-block:: bareosconfig
   :caption: /etc/bareos/libcloud_config.ini

   [host]
   hostname=127.0.0.1
   port=9000
   tls=false
   provider=S3

   [credentials]
   username=admin
   password=admin

   [misc]
   nb_worker=20
   queue_size=1000
   prefetch_size=250*1024*1024
   temporary_download_directory=/dev/shm/bareos_libcloud

.. note::

   Do not use quotes in the above config file, it is processed by the Python ConfigParser module and the quotes would not be stripped from the string.

Mandatory Plugin Options:

These options in the config file are mandatory:

hostname
   The hostname/ip address of the storage backend server

port
   The portnumber for the backend server

tls
   Use Transport encryption, if supported by the backend

provider
   The provider string, 'S3' being the default if not specified

username
   The username to use for backups

password
   The password for the backup user

nb_worker
   The number of worker processes who can preload data from objects simultaneously
   before they are given to the plugin process that does the backup

queue_size
   The maximum size in numbers of objects of the internal communication queue
   between the processes

prefetch_size
   The maximum object size in bytes that should be preloaded from the workers; objects
   larger than this size are loaded by the plugin process itself

temporary_download_directory
   The local path where the worker processes put their temporarily downloaded files to;
   the filedaemon process needs read and write access to this path


Optional Plugin Options:

This option in the config file is optional:

fail_on_download_error
   When this option is enabled, any error during a file download will fail the backup job.
   By default a warning will be issued and the next file will be backed up.

job_message_after_each_number_of_objects
   When running a backup, put a jobmessage after each count of "job_message_after_number_of_objects"
   to the joblog or no message if parameter equals 0; default is 100.
