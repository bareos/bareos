This is a parameter used by :ref:`plugin-autoxflate-sd` which allow you to transform a non compressed piece of data into a compressed piece of data on the storage daemon. e.g. Storage Daemon compression. You can either enable compression on the client and use the CPU cyclces there to compress your data with one of the supported compression algorithms. The value of this parameter specifies a so called io-direction currently you can use the following io-directions:

-  in - compress data streams while reading the data from a device.

-  out - compress data streams while writing the data to a device.

-  both - compress data streams both when reading and writing to a device.

Currently only plain data streams are compressed (so things that are already compressed or encrypted will not be considered for compression.) Also meta-data streams are not compressed. The compression is done in a way that the stream is transformed into a native compressed data stream. So if you enable this and send the data to a filedaemon it will know its a compressed stream and will do the decompression itself. This also means that you can turn this option on and off at any time without
having any problems with data already written.

This option could be used if your clients doesn’t have enough power to do the compression/decompression itself and you have enough network bandwidth. Or when your filesystem doesn’t have the option to transparently compress data you write to it but you want the data to be compressed when written.

