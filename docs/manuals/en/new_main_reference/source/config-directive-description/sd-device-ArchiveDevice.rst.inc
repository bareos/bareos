Specifies where to read and write the backup data. The type of the Archive Device can be specified by the :config:option:`sd/device/DeviceType`\  directive. If Device Type is not specified, Bareos tries to guess the Device Type accordingly to the type of the specified Archive Device file type.

There are different types that are supported:

device
   Usually the device file name of a removable storage device (tape drive), for example :file:`/dev/nst0` or :file:`/dev/rmt/0mbn`, preferably in the "non-rewind" variant. In addition, on systems such as Sun, which have multiple tape access methods, you must be sure to specify to use Berkeley I/O conventions with the device. The b in the Solaris (Sun) archive specification :file:`/dev/rmt/0mbn` is what is needed in this case. Bareos does not support SysV tape drive behavior.

directory
   If a directory is specified, it is used as file storage. The directory must be existing and be specified as absolute path. Bareos will write to file storage in the specified directory and the filename used will be the Volume name as specified in the Catalog. If you want to write into more than one directory (i.e. to spread the load to different disk drives), you will need to define two Device resources, each containing an Archive Device with a different directory.

fifo
   

.. _SetupFifo:

 A FIFO is a special kind of file that connects two programs via kernel memory. If a FIFO device is specified for a backup operation, you must have a program that reads what Bareos writes into the FIFO. When the Storage daemon starts the job, it will wait for :config:option:`sd/device/MaximumOpenWait`\  seconds for the read program to start reading, and then time it out and terminate the job. As a consequence, it is best to start the read
   program at the beginning of the job perhaps with the :config:option:`dir/job/RunBeforeJob`\  directive. For this kind of device, you always want to specify :config:option:`sd/device/AlwaysOpen`\  = no, because you want the Storage daemon to open it only when a job starts. Since a FIFO is a one way device, Bareos will not attempt to read a label of a FIFO device, but will simply write on it. To create a FIFO Volume in the catalog, use the add command
   rather than the label command to avoid attempting to write a label.

   

      .. literalinclude:: /_static/config/SdDeviceArchiveDevice1.conf



   During a restore operation, if the Archive Device is a FIFO, Bareos will attempt to read from the FIFO, so you must have an external program that writes into the FIFO. Bareos will wait :config:option:`sd/device/MaximumOpenWait`\  seconds for the program to begin writing and will then time it out and terminate the job. As noted above, you may use the :config:option:`dir/job/RunBeforeJob`\  to start the writer program at the beginning of the job.

   A FIFO device can also be used to test your configuration, see the :ref:`Howto section <TestUsingFifoDevice>`.

GlusterFS Storage
   

.. _GlusterArchiveType:

 don’t use this directive, but only :config:option:`sd/device/DeviceType`\  and :config:option:`sd/device/DeviceOptions`\  (this behavior have changed with :index:`Version >= 15.2.0 <pair: bareos-15.2.0; Device Options>`).

Ceph Object Store
   

.. _CephArchiveType:

 don’t use this directive, but only :config:option:`sd/device/DeviceType`\  and :config:option:`sd/device/DeviceOptions`\ . (this behavior have changed with :index:`Version >= 15.2.0 <pair: bareos-15.2.0; Device Options>`).

