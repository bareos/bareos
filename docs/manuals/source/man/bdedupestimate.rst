bdedupestimate
==============

Synopsis
--------

**bdedupestimate** [*options*] <*devicename*>

Description
-----------

:program:`bdedupestimate` reads the given volumes with the specified device and
estimates how much data could be deduplicated on a filesystem that
deduplicates blocks of the given blocksize.

This program works on the records themselves and does not try to reconstruct the
files. This is important in cases where the maximum network buffer size is not
divisible by the chosen blocksize as in those cases the alignment of the records
will not match those of the data in the actual file.

`<https://docs.bareos.org/>`_

Internal workflow
-----------------
* :program:`bdedupestimate` reads records from each volume one by one,
  optionally decompressing compressed records.

* For each record, it splits that record into blocks of the given blocksize.

* For each block, it notes whether the block was seen already or not.

* At the end it tallies up the number of bytes of every record read and
  every unique block seen.

Options
-------

.. program:: bdedupestimate

.. option:: devicename TEXT

   Specify the input device name (either as name of a Bareos Storage Daemon
   Device resource or identical to the Archive Device in a Bareos Storage Daemon
   Device resource).

.. option:: -v,--verbose

   print out verbose messages, including intermediate reports every 100'000
   records read.

.. option:: --version

   Display program version information and exit

.. option:: -h,-?,--help

   Prints usage information

.. option:: -d,--debug-level <level>

   Set debug level to <level>.

.. option:: --dt,--debug-timestamps

   Print timestamps in debug output.

.. option:: -V,--volumes <volume> ... (REQUIRED)

   List of volumes to be analyzed.

.. option:: -c,--config <path>:PATH(existing)

   Use <path> as configuration file or directory.

.. option:: -D,--director <director>

   Specify a director name specified in the storage. Configuration file for the
   Key Encryption Key selection.

.. option:: -u,--decompress <bool>

   Specify whether compressed records should get decompressed. Compressed
   records are very unlikely to be dedupable.

.. option:: -b,--blocksize UINT:SIZE [b, kb(=1024b), ...]:POSITIVE

   Set the block size that the underlying filesystem would use to deduplicate.
   The default blocksize is 4kib.
