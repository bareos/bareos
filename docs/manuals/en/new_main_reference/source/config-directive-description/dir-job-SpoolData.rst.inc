If this directive is set to :strong:`yes`, the Storage daemon will be requested to spool the data for this Job to disk rather than write it directly to the Volume (normally a tape).

Thus the data is written in large blocks to the Volume rather than small blocks. This directive is particularly useful when running multiple simultaneous backups to tape. Once all the data arrives or the spool files’ maximum sizes are reached, the data will be despooled and written to tape.

Spooling data prevents interleaving data from several job and reduces or eliminates tape drive stop and start commonly known as "shoe-shine".

We don’t recommend using this option if you are writing to a disk file using this option will probably just slow down the backup jobs.

NOTE: When this directive is set to yes, :config:option:`dir/job/SpoolAttributes`\  is also automatically set to yes.

For details, see :ref:`section-spooling`.

