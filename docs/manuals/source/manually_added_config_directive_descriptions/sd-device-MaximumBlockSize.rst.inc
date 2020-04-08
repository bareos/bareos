The Storage daemon will always attempt to write blocks of the specified size (in bytes) to the archive device.
As a consequence, this statement specifies both the default block size and the maximum block size.
The size written never exceeds the given size.
If adding data to a block would cause it to exceed the given maximum size, the block will be written to the archive device, and the new data will begin a new block.

If no value is specified or zero is specified, the Storage daemon will use a default block size of 64,512 bytes (126 \* 512).

Please read chapter :ref:`Tapespeed and blocksizes`, to see how to tune this value in a safe manner.

.. limitation:: Setting Maximum Block Size for non-tapes is not supported

   This setting has only been tested with tape drives.
   The use with every other storage backend is untested and therefore unsupported and discouraged.
      
.. warning::
   After setting this value the device may write volumes with the new setting.
   Such volumes can only be read by a device with the same or a larger maximum block size configured.
   You must make sure that all devices with the same :config:option:`sd/device/MediaType` have the same value applied.
