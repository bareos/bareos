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

.. list-table:: Test System
   :header-rows: 0

   * - **OS**
     - Fedora 39 x86_64
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

.. note::
 The value "0" for the number of workers means the original code path used in bareos 22.
 Other numbers are the value of `MaximumWorkersPerJob` in the client configuration.


Multimedia Files
~~~~~~~~~~~~~~~~

Backup
^^^^^^^
.. figure:: /include/images/compress-test-multimedia-backup.png

   MaximumWorkersPerJob vs compression algorithms for the multimedia data backup

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

Restore
^^^^^^^
.. figure:: /include/images/compress-test-multimedia-restore.png

   MaximumWorkersPerJob vs compression algorithms for the multimedia data restore

The restore speed is not influenced by the number of threads and reaches almost
the uncompressed rate with lzo, lz4 and lz4hc. The gzip family of algorithms
slows down the restore speed by around 50%.

Linux Filesystem
~~~~~~~~~~~~~~~~

Backup
^^^^^^
.. figure:: /include/images/compress-test-rootfs-backup.png

   MaximumWorkersPerJob vs compression algorithms for the linux root fs data backup

The maximum backup speed of the Linux Filesystem is about 260MB/s.
Lzo reaches a compression rate of 46.7% while slowing down the backup speed to
about 50%. Using multiple threads increases the lzo speed form 127MB/s to
150MB/s (about 18%)

lz4 only reaches half of the speed of lzo while even reaching a worse
compression!
gzip1 is faster than lz4hc while reaching a better compression rate (56.9% vs 52.3%.)


Restore
^^^^^^^
.. figure:: /include/images/compress-test-rootfs-restore.png

   MaximumWorkersPerJob vs compression algorithms for the linux root fs data restore

On restore of the linux rootfs, lzo, lz4 and lzfast are only a bit slower than
the uncompressed backup, while the gzip algorithms are around 20% slower.


Linux Kernel Sources
~~~~~~~~~~~~~~~~~~~~

Backup
^^^^^^
.. figure:: /include/images/compress-test-kernelsource-backup.png

  MaximumWorkersPerJob vs compression algorithms for the linux kernel source data backup

The maximum backup speed of the kernel sources is 100M/s.
lzo reaches a speed of 25MB/s with a compression of 68.8%.
lz4 reaches about 20MB/s with a compression rate of 67.4% and so it is slower
than lzo and worse in compression.

The number of threads almost has no impact on the backup speed for all
algorithms.

Restore
^^^^^^^
.. figure:: /include/images/compress-test-kernelsource-restore.png

  MaximumWorkersPerJob vs compression algorithms for the linux kernel source data restore

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
lz4 family of compression algorithms, while gzip does have a significant negative
impact on restore speed.

Because of their bad performance, gzip algorithms should not be used at all.




.. _TLS_Settings:

TLS Settings
------------

.. index::
   single: TLS settings
   single: tls; performance


Bareos encrypts all network traffic by default. TLS-PSK is used to encrypt the traffic.
It is however possible to disable the TLS encryption and allow cleartext communication
between.


Disabling TLS Encryption
~~~~~~~~~~~~~~~~~~~~~~~~


We tested the influence of disabling TLS on the backup speed of our lab environment.
Only *LZO* and *No Compression* are regarded.


No TLS with no Encryption
^^^^^^^^^^^^^^^^^^^^^^^^^
.. figure:: /include/images/disable-tls-no-compression.png

   Speed comparison [MiB/s] when TLS is disabled and no compression is configured.

No TLS with LZO Encryption
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. figure:: /include/images/disable-tls-lzo-compression.png

   Speed comparison [MiB/s] when TLS is disabled and LZO compression is configured.

The impact of disabling TLS depends on the dataset being backed up.
The faster the backup itself runs(and the faster the network traffic itself
is), the bigger is the impact.  Both with cleartext  and lzo encryption we see
similar accelerations:

.. list-table:: Acceleration by disabling TLS
   :header-rows: 1

   * -
     - Backup
     - Backup LZO
     - Restore
     - Restore LZO
   * - **Multimedia files**
     - 67%
     - 67%
     - 43%
     - 38%
   * - **Linux Root Filesystem**
     - 15%
     - 28%
     - 26%
     - 40%
   * - **Kernel Source**
     - 8%
     - 8%
     - 10%
     - 38%


Conclusion
^^^^^^^^^^

Disabling the TLS encryption of the network traffic in bareos can
bring a significant acceleration.

The acceleration depends on the dataset being backed up.
The faster the network transfer is, the more influence the TLS load has.

If the backup speed is not fast enough, and traffic encryption is not required, 
disabling TLS can help to get the backups faster.




Use different TLS Ciphers
~~~~~~~~~~~~~~~~~~~~~~~~~

If disabling TLS is not an option, it still is possible to use a different cipher
that might be faster than what is chosen by default.



Use kTLS
~~~~~~~~

Newer Linux and FreeBSD versions have the possibility to use |ktls|.
|ktls| allows the cryptographic library to ask the Kernel to do the encryption
and decryption of the network traffic. If appropriate hardware(e.g. Network Card)
is available, the TLS load can be offloaded to the NIC.



.. _xattr_and_acl_support:

Extended Attribute and ACL Support
----------------------------------

Bareos is capable of backing up extended attributes, and the support for both is
enabled by default.
If the backup speed needs to be accelerated, and not backing up extended attributes
and/or ACLs, both options can be disabled which will save some time for each file
being backed up.

Disable ACL and Xattr Support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. figure:: /include/images/disable-acl-xattr.png

   Speed comparison when extended attribute and acl support is disabled. Speed is in MB/s.

.. list-table:: Acceleration by disabling Extended Attributes and ACL support.
   :header-rows: 1

   * -
     - Backup
     - Restore
   * - **Multimedia files**
     - 0%
     - 0%
   * - **Linux Root Filesystem**
     - 19%
     - 37%
   * - **Kernel Source**
     - 17%
     - 0%


Conclusion
~~~~~~~~~~

Disabling the extended attribute and acl support in the fileset can improve the
backup speed. As always, it depends on the dataset being backed up, and if the
acl and/or extended attribute information is expendable.




.. _Checksum:

File Checksums
--------------

If the FileSet option *signature* is set, Bareos calculates a checksum for every
file being backed up.

The following graphs show the results of testing the performance of the available checksums.

.. figure:: /include/images/signature-compare-multimedia-dataset.png

   Checksum performance comparison for multimedia file dataset

.. figure:: /include/images/signature-compare-rootfs-dataset.png

   Checksum performance comparison for rootfs dataset

.. figure:: /include/images/signature-compare-kernelsource-dataset.png

   Checksum performance comparison for kernel source dataset

Conclusion
~~~~~~~~~~

The results of the tests are unfortunately not clear.  SHA512 seems to be the
fastest algorithm, though the difference between the different algorithms is
not significant.

It is also strange that the restore results seem to be much less affected than
the backup results, as the restore code also recalculates the checksum to verify
that the restore successfully restored the correct data.

Theoretically, xxh128 is the by far fastest algorithm
offered and thus should be
the fastest option.
The results show that more investigation is required to understand the results.
