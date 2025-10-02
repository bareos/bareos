The Device Type specification allows you to explicitly define the kind of device you want to use.
See :ref:`SdBackends` for options.

The Device Type directive is not required in all cases. If it is not specified, Bareos will attempt to guess what kind of device has been specified using the :config:option:`sd/device/ArchiveDevice`\  specification supplied. There are several advantages to explicitly specifying the Device Type. First, on some systems, block and character devices have the same type. Secondly, if you explicitly specify the Device Type, the mount point need not be defined until the device is
opened. This is the case with most removable devices such as USB. If the Device Type is not explicitly specified, then the mount point must exist when the Storage daemon starts.
