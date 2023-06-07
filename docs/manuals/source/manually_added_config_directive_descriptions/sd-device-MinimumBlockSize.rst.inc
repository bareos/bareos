This statement applies only to non-random access devices (e.g. tape drives). Blocks written by the storage daemon to a non-random archive device will never be smaller than the given size. The Storage daemon will attempt to efficiently fill blocks with data received from active sessions but will, if necessary, add padding to a block to achieve the required minimum size.

To force the block size to be fixed, as is the case for some non-random access devices (tape drives), set the Minimum block size and the Maximum block size to the same value. This is usually not required.
The default is that the minimum block size is zero and maximum block size is 1 MiB (1.048.576 bytes).
This results in a default block size of 1 MiB (1.048.576 bytes).

For example, suppose you want a fixed block size of 100K bytes, then you would specify:



   .. literalinclude:: /include/config/SdDeviceMinimumBlockSize1.conf
      :language: bareosconfig



Please note that if you specify a fixed block size as shown above, the tape drive must either be in variable block size mode, or if it is in fixed block size mode, the block size (generally defined by :command:`mt`) must be identical to the size specified in Bareos – otherwise when you attempt to re-read your Volumes, you will get an error.

If you want the block size to be variable but with a 63K minimum and 200K maximum (and default as well), you would specify:



   .. literalinclude:: /include/config/SdDeviceMinimumBlockSize2.conf
      :language: bareosconfig
