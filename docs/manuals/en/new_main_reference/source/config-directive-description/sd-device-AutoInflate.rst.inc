This is a parameter used by :ref:`plugin-autoxflate-sd` which allow you to transform a compressed piece of data into a non compressed piece of data on the storage daemon. e.g. Storage Daemon decompression. You can either enable decompression on the client and use the CPU cyclces there to decompress your data with one of the supported compression algorithms. The value of this parameter specifies a so called io-direction currently you can use the following io-directions:

-  in - decompress data streams while reading the data from a device.

-  out - decompress data streams while writing the data to a device.

-  both - decompress data streams both when reading and writing to a device.

This option allows you to write uncompressed data to for instance a tape drive that has hardware compression even when you compress your data on the client with for instance a low cpu load compression method (LZ4 for instance) to transfer less data over the network. It also allows you to restore data in a compression format that the client might not support but the storage daemon does. This only works on normal compressed datastreams not on encrypted datastreams or meta data streams.

