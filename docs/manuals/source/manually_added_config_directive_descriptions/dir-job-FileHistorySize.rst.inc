When using NDMP and :config:option:`dir/job/SaveFileHistory`\  is enabled, this directives controls the size of the internal temporary database (LMDB) to translate NDMP file and directory information into Bareos file and directory information.

:strong:`File History Size`\  must be greater the number of directories + files of this NDMP backup job.



.. warning::

   This uses a large memory mapped file (:strong:`File History Size` \* 256 => around 2,3 GB for the :strong:`File History Size = 10000000`\ ).
   On 32-bit systems or if a memory limit for the user running the |dir| (normally **bareos**) exists
   (verify by :command:`su - bareos -s /bin/sh -c "ulimit -a"`), this may fail.
