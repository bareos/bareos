This directive specifies the maximum number of Jobs that can run concurrently on a specified Device. Using this directive, it is possible to have different Jobs using multiple drives, because when the Maximum Concurrent Jobs limit is reached, the Storage Daemon will start new Jobs on any other available compatible drive. This facilitates writing to multiple drives with multiple Jobs that all use the same Pool.

.. warning::

   If :config:option:`sd/device/DeviceType` is "Droplet" then Maximum Concurrent Jobs is limited to 1.

.. warning::

   When using non-tape devices, it is highly recommended to set Maximum Concurrent Jobs to 1 to avoid intermixing jobs. Intermixed jobs on a Volume will significantly reduce performance during restore.
