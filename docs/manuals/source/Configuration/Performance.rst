.. _section-Performance:

Performance considerations
==========================

.. index::
   pair: Performance; Configuration


The performance of the backup system is important. During backup, all backup
tasks need to be performed inside of the backup window.
The restore performance is important to meet service level agreements.




.. _Compression_Settings:

Compression settings
--------------------

.. index::
   single: Compression settings
   single: compression; performance


Bareos supports multiple compression algorithms.
When configured, Bareos will compress the backed up data in the |fd| before
sending it over to the |sd| during backup, and will decompress the data on
restore before writing it to the disk.

Bareos :sinceVersion:`23.0.0: Client: MaximumWorkersPerJob`, supports
deploying the compression work over multiple threads.

Lab Setup
~~~~~~~~~
To analyze the impact of the multithreading on different compression settings
and different type of data being backed up, a lab environment was set up.

Three different data sets were backed up:

* Multimedia Files
* A Linux root Filesystem.
* The Linux Kernel 6.6 Sources

.. list-table:: Datasets for backup and restore speed evaluation
   :header-rows: 1

   * -
     - Multimedia Files
     - Linux Root Filesystem
     - Linux Kernel Sources
   * - **Size**
     - 6.4G
     - 6.2G
     - 1.5G
   * - **Files**
     - 295
     - 149 118
     - 87 084
   * - **Compressibility**
     - Bad
     - Intermediate
     - Good

.. list-table:: Test Hardware
   :header-rows: 0

   * - **CPU**
     - AMD Ryzen 7 PRO 7840U w/ Radeon 780M Graphics, 16 cores
   * - **Memory**
     - 64 GB
   * - **Storage Device**
     - 2TB SSD SAMSUNG MZVL22T0HDLB nvme
   * - **Source Filesystem**
     - xfs on ssd partition
   * - **Storage Filesystem**
     - tmpfs 10G ramdisk


The test setup is configured so that the different Datasets are read from a
xfs partition on the nvme Disk.
The backup volumes are written into a 10GB tmpfs ramdisk.

Remark about the number of test workers:
0: Use the original code path as on bareos 22
1-10: Use the new code path with the given number of workers.


Multimedia Files
~~~~~~~~~~~~~~~~

.. image:: /include/images/compress-test-multimedia-backup.png

The test shows, that the system is capable of 650 M/s backup speed when backing
up the multimedia data. The lzo compression performs best, but does not
compress the data at all.  The lz4 compressor reaches only 1% of compression on
that data at about 150M/s

lz4 scales well with the number of workers and achieves a maximum speed of
500M/s with 5 workers.  The other compression algorithms can achieve higher
compression rates with much slower backup speed compared
to no compression. The impact of multithreading is high on all other algorithms
and can speedup the backup by the factor 5.

Overall, the backup speed is very slow compared to the uncompressed backup.

.. image:: /include/images/compress-test-multimedia-restore.png

The restore speed is not influenced by the number of threads and reaches almost
the uncompressed rate with lzo, lz4 and lz4hc. The gzip family of algorithms
slows down the restore speed by around 50%.

Linux Filesystem
~~~~~~~~~~~~~~~~

.. image:: /include/images/compress-test-rootfs-backup.png

The maximum backup speed of the Linux Filesystem is about 260MB/s.
Lzo reaches a compression rate of 46.7% while slowing down the backup speed to
about 50%. Using multiple threads increases the lzo speed form 127MB/s to
150MB/s (about 18%)

lz4 only reaches half of the speed of lzo while even reaching a worse
compression!
gzip1 is faster than lz4hc while reaching a better compression rate (56.9% vs 52.3%.)


.. image:: /include/images/compress-test-rootfs-restore.png

On restore of the linux rootfs, lzo, lz4 and lzfast are only a bit slower than
the uncompressed backup, while the gzip algorithms are around 20% slower.


Linux Kernel Sources
~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/compress-test-kernelsource-backup.png

The maximum backup speed of the kernel sources is 100M/s.
lzo reaches a speed of 25MB/s with a compression of 68.8%.
lz4 reaches about 20MB/s with a compression rate of 67.4% and so it is slower
than lzo and worse in compression.

The number of threads almost has no impact on the backup speed for all
algorithms.

.. image:: /include/images/compress-test-kernelsource-restore.png

Restoring the linux kernel sources reaches around 125MB/s for uncompressed backups.
All algorithms reach about 110-120MB/s. This shows that the decompression of
the data probably is not the bottleneck when restoring these files.


Conclusion
~~~~~~~~~~

Depending on the properties of the data being backed up, the backup speed can vary.
It is interesting, that the lzo algorithm shows the best performance in our tests.
When the data is not compressible, it is advisable not to enable compression at all.

The results on this test system suggest that it makes sense to use lzo compression
in most cases.

In case the lzo algorithm shows bad compression ratio on your data, it might
make sense to disable compression at all. However,in lab environment, the
bandwidth between |fd| and |sd| was not a limiting factor. In real world
environments, the bandwith probably will the bottleneck and
thus using compression will lead to an overall much better performance.

Regarding restores it can be said that the speed is not reduced by the lzo and
lz4 family of compression algorithms, while gzip does have a significant impact
on restore speed.




.. _Checksum:

File Checksums
--------------


.. _Encryption_Settings:

Encryption settings
-------------------

.. index::
   single: Encryption settings
   single: encryption; performance

- TLS
- Data encryption


