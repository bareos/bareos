Selection Patterns is only used for Copy and Migration jobs, see :ref:`MigrationChapter`. The interpretation of its value depends on the selected :config:option:`dir/job/SelectionType`\ .

For the OldestVolume and SmallestVolume, this Selection pattern is not used (ignored).

For the Client, Volume, and Job keywords, this pattern must be a valid regular expression that will filter the appropriate item names found in the Pool.

For the SQLQuery keyword, this pattern must be a valid :command:`SELECT` SQL statement that returns JobIds. The returned JobIds must be from the pool specified in the Pool statement. This is because Bareos will always use the storage associated with that pool when reading a job that is to be copied or migrated. If you want to copy or migrate jobs from different pools with the SQLQuery selection type, use separate Jobs and matching SQL statements for each pool.

Example:

.. literalinclude:: /include/config/DirSelectionPatterns-sql1.conf
   :language: bareosconfig
