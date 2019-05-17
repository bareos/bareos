Selection Patterns is only used for Copy and Migration jobs, see :ref:`MigrationChapter`. The interpretation of its value depends on the selected :config:option:`dir/job/SelectionType`\ .

For the OldestVolume and SmallestVolume, this Selection pattern is not used (ignored).

For the Client, Volume, and Job keywords, this pattern must be a valid regular expression that will filter the appropriate item names found in the Pool.

For the SQLQuery keyword, this pattern must be a valid :command:`SELECT` SQL statement that returns JobIds.

