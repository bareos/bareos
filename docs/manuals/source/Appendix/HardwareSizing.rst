
Hardware sizing
===============

Database system
---------------

PostgreSQL version
~~~~~~~~~~~~~~~~~~

It is always advisable to use the **latest** currently **supported** |postgresql| on your platform.  With identical contents, we have observed that |postgresql| 12 required *half of the space* of a |postgresql| 10 installation.

PostgreSQL setup
~~~~~~~~~~~~~~~~

The database files should be stored on **SSD storage**.
The database configuration should be optimized for the amount of memory that the database has available.
A good starting point is using `pgtune <https://pgtune.leopard.in.ua/>`_, a website that allows to set some basic information and generates a |postgresql| configuration.
For pgtune the DB type of the Bareos catalog  database is a "Data Warehouse".


The file table
~~~~~~~~~~~~~~

The by far **biggest table** in the Bareos catalog database is the **file** table.
Typically this is about **90-95%** of the database's total size.
The **size of the file table** depends on the number of files that are stored and the average length of a filename (without path).

Roughly :math:`\frac{1}{3}` of the file table size is consumed by its **indexes**.
To have optimum performance, the **memory available** for the Bareos catalog database should be at least the **size of the file table indexes**.

Database size estimation
~~~~~~~~~~~~~~~~~~~~~~~~

Depending on the number of files and the average length of filenames, the **database size** can be **estimated** using the following calculations:

To **calculate the number of files** in the DB, the number of files being backed up from all systems needs to be multiplied by the number of times that they will be kept in the database.

The **amount of data per file** in the DB, depends on the size of filenames that are being backed up, but we have analyzed some real-world examples and found that values between **250 and 350 bytes per row** are usual.

So the calculation of the size of the file table can be approximated with the following formula:

.. math::
   \begin{split}
   s &= n_f \cdot n_b \cdot 300 \frac{\mbox{bytes}}{\mbox{row}} \\
   s &: \mbox{storage required for file table} \\
   n_f &: \mbox{number of files in a (full) backup} \\
   n_b &: \mbox{number of (full) backups} \\
   \end{split}

*Example:* If **200.000 files** are backed up during a full backup, a full backup is run **every week** and the retention of the backups is **4 weeks**, the total amount of files would be

.. math::
   \begin{split}
   n_f &= 200.000\ \mbox{Files} \\
   n_b &= 4\ \mbox{Full Backups} \\
   s &= n_f \cdot n_b \cdot 300 \frac{\mbox{bytes}}{\mbox{row}} \\
     &= 200.000\ \mbox{Files} \cdot 4\ \mbox{Full Backups} \cdot 300 \frac{\mbox{bytes}}{\mbox{row}} \\
     &= 240.000.000\ \mbox{bytes} \\
     &= 240\ \mbox{GB} \\
   \end{split}

About :math:`\frac{1}{3}` of the DB Size should be available as RAM, so about 80 GB.


CPU considerations
------------------

During backups with Bareos, the amount of CPU consumed is influenced by different parameters.

System being backed up
----------------------

* Often is the backup speed limited by the I/O speed that can be read from the original filesystem.
* The I/O speed of the filesystems being backed up or the network bandwidth are often not fast enough to saturate the CPU.
* The **TLS communication encryption** (enabled by default) also consumes CPU power, especially when the I/O rate is very fast.
* If **data encryption** is configured, the encryption is calculated on the source system and will consume CPU power there.
* If **signatures** are enabled in your fileset, this will require additional CPU cycles to be calculated on the source system.
* If data **compression** is configured, the compression is also executed on the source system and will consume CPU power there.

Storage Daemon System
---------------------

* The |sd| receives the data from the clients and stores it to the storage media.
* The |sd| receives the data stream coming from the Clients and  stores the data to local storage media.
* Most CPU Power is consumed by the decryption of the TLS stream and by calculating Checksums that are verified before storing data to the storage media.

Depending on the available I/O throughput and the number of parallel jobs, different optimizations can be made:

* For a relative small number of clients that send data at very high I/O rates, it makes sense to disable Hyperthreading Technologies so that less cores can operate at higher speed.
* For a large number of clients with non-exceptional I/O rates, more CPU cores will provide better overall performance with parallel backup jobs than fast CPU cores.

As a starting point, reserve 512 MB of memory and :math:`\frac{1}{4}` CPU core per concurrent job planned.

Director
--------

The |dir| itself has comparatively low CPU and RAM requirements.
Most of the really expensive calculations are done by the database engine.

It is recommended to run the |dir| service together with the database server on the same machine, which minimizes the latency and overhead of the communication with the database.
