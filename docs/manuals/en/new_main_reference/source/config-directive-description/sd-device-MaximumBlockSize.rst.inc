The Storage daemon will always attempt to write blocks of the specified size (in-bytes) to the archive device. As a consequence, this statement specifies both the default block size and the maximum block size. The size written never exceed the given size. If adding data to a block would cause it to exceed the given maximum size, the block will be written to the archive device, and the new data will begin a new block.

If no value is specified or zero is specified, the Storage daemon will use a default block size of 64,512 bytes (126 \* 512).



.. warning::
   If your are using LTO drives, changing the block size after labeling the tape will result into unreadable tapes.

Please read chapter :ref:`Tapespeed and blocksizes`, to see how to tune this value in a safe manner.

