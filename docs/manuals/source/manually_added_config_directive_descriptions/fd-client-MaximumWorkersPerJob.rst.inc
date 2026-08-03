.. note::
   These worker threads are mostly used to compute checksums, and to compress & encrypt data.
   If this is set to at least 1, bareos will use a separate thread for sending data.

.. warning::
   As PKI Encryption disables Parallel Send code path, the value will not be used.
