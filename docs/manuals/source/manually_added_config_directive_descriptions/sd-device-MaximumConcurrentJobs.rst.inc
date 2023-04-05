This directive specifies the maximum number of Jobs that can run concurrently on a specified Device. Using this directive, it is possible to have different Jobs using multiple drives, because when the Maximum Concurrent Jobs limit is reached, the Storage Daemon will start new Jobs on any other available compatible drive. This facilitates writing to multiple drives with multiple Jobs that all use the same Pool.

.. warning::

   If :config:option:`sd/device/DeviceType` is "Droplet" then Maximum Concurrent Jobs is limited to 1.

.. warning::


   When using devices that are *not* of type *tape*, it is highly recommended to set `Maximum Concurrent Jobs` to 1 {the default since Bareo 23) to avoid intermixing jobs. Intermixed jobs on a Volume will significantly reduce performance during restore. Instead, define as many devices as required (e.g. multiply disk devices via :config:option:`sd/device/Count`) and parallelize by storing each job to an individual device at once.
