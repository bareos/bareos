Real NDMP accounting fixtures
=============================

``ndmp-bareos.json`` and ``ndmp-native.json`` contain catalog exports from
successful Full and Incremental backups made with the ndmjob emulator in
Bareos PR #2855 (commit ``b2f98af668``). The capture build also used the
``NdmpConvertFstat`` size/block conversion from accounting PR #2832
(commit ``5f1dfd5b1d``); PR #2855 alone does not preserve reported sizes.
No catalog attributes were synthesized or corrected after the backups.

Only source path prefixes were normalized to ``/ndmp-accounting/<mode>``.
Encoded ``LStat``, file-history offsets/node IDs, job levels, statuses,
timestamps and byte/file counts are preserved. Import assigns fresh IDs
and distinct Client, FileSet and Job names; media and payloads are not
needed by accounting and are not imported.

To reproduce the capture, build PR #2855 with the accounting stat
conversion and run ``test-setup`` in each of its
``ndmp-emulated-bareos`` and ``ndmp-emulated-native`` build directories.
Before running ``testrunner-01-full-backup``, replace the source contents:

================== ========== ======================
Source file        Full size  Incremental-chain size
================== ========== ======================
empty              0          0
full               1025       1025
testfile           1          1
testfile2          512         512
sub/dir/file       1000        65537
incremental        absent     513
================== ========== ======================

Keep the existing directories and ``sub/link`` symlink. After the Full,
wait at least two seconds, rewrite ``sub/dir/file`` and create
``incremental``, then run one Incremental of ``ndmjobdump`` (NDMP_BAREOS)
or ``ndmjob-native`` (NDMP_NATIVE). Export Job, Client, FileSet and File
rows joined to Path before running cleanup, which drops the test catalog.

Expected totals come from the regular source files, not from decoding the
catalog or reading accounting output. NDMP file history reports logical
sizes rather than filesystem allocation; the conversion represents them
as 512-byte blocks rounded up per file. Thus both Fulls account for five
files, 2538 logical bytes and 3584 accounted bytes. Both Incremental chains
account for six files, 67588 logical bytes and 69632 accounted bytes.
The resized file replaces its Full entry; unchanged files remain in the
chain, directories and symlinks do not count, and NDMP_BAREOS's two opaque
archive containers do not cause fallback when file history is present.

The accounting test imports Fulls first and checks the snapshot, then
imports Incrementals and checks again. It requires no running NDMP server.

Barri and Windows catalog samples
---------------------------------

``plugin-backups.json`` contains selected catalog rows from the local
PostgreSQL dump supplied for this test. It uses the successful Barri Full
job that included an opaque image and a regular file, plus a successful
Windows Full and Incremental pair. The Incremental sample includes a
changed existing file and a new file; the Full sample also contains a
zero-length file, a sparse/logically larger file, a directory and a
symlink.

The encoded ``LStat`` attributes are copied unchanged from those real
catalog records. Client/FileSet/Job identifiers and file names/paths are
anonymized; FileSet text is reduced to the plugin/filesystem declaration
needed by the test, and irrelevant checksums/FileHistory IDs are removed.
The fixture is a deliberately selected subset of each real catalog job,
not a full catalog export. Expected Windows totals are computed directly
from the selected decoded ``st_size`` values. The test verifies the Barri
opaque-image fallback and that Windows accounting uses logical size over
the Full/Incremental chain.

MSSQL VDI catalog samples
-------------------------

The ``mssql`` section contains the File rows from successful Full and
Incremental backups of two databases in ``mssqlbackup.sql``: a normal
database and one using FILESTREAM. Their encoded ``LStat`` fields and
job-level file/byte totals are preserved. Client/FileSet names, database
names, file names, and timestamps are anonymized; paths retain the
``/@MSSQL/`` namespace required to identify the virtual VDI stream, while
the database path components are replaced. FileSet text contains only the
plugin declaration and no connection credentials.

MSSQL VDI records use zero as a placeholder ``st_size`` and encode the
virtual-stream marker with mode 0700, ``st_blksize=65536`` and
``st_blocks=1``. The accounting regression imports both Full/Incremental
chains and verifies they are excluded from measured per-file totals so
subscription reporting uses the job-level estimate rather than counting
the zero placeholder as an empty file.
