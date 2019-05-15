The specified device must be the generic SCSI device of the autochanger.

The changer device is additional to the the :config:option:`sd/device/ArchiveDevice`\ . This is because most autochangers are controlled through a different device than is used for reading and writing the tapes. For example, on Linux, one normally uses the generic SCSI interface for controlling the autochanger, but the standard SCSI interface for reading and writing the tapes.

On Linux, for the :config:option:`sd/device/ArchiveDevice = /dev/nst0`\ , you would typically have :config:option:`sd/device/ChangerDevice = /dev/sg0`\ .

On FreeBSD systems, the changer device will typically be on :file:`/dev/pass0` through :file:`/dev/passN`.

On Solaris, the changer device will typically be some file under :file:`/dev/rdsk`.

Please ensure that your Storage daemon has permission to access this device.

It can be overwritten per device using the :config:option:`sd/device/ChangerDevice`\  directive.

