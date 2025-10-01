.. _scsitapealert-sd:

scsitapealert-sd
~~~~~~~~~~~~~~~~

.. index::
   single: Plugin; scsitapealert-sd
   single: TapeAlert
   single: Tape; Alert

This plugin is part of the **bareos-storage-tape** package.
The plugin retrieves TapeAlert flags of every :config:option:`sd/device` that
has :config:option:`sd/device/DriveTapeAlertEnabled` set.
For every flag set, a detailed message will be written to the joblog.

The plugin directly queries the TapeAlert page from the device.
This requires raw access to the device.
On Linux, when the |sd| is run as a normal user (the default), the process
needs the capability ``CAP_SYS_RAWIO`` to facilitate this.
The :ref:`setup_scsi_capabilities` section of the plugin :ref:`scsicrypto-sd`
explains how to set this up.

.. note::

    When you see the error message
    ``Unable to perform SG_IO ioctl on fd X: ERR=Permission denied`` your |sd|
    probably lacks the ``CAP_SYS_RAWIO`` capability.
