

.. _PruneStatistics:

 The :strong:`Statistics Retention`\  directive defines the length of time that Bareos will keep statistics job records in the Catalog database after the Job End time (in the catalog **JobHisto** table). When this time period expires, and if user runs :bcommand:`prune stats` command, Bareos will prune (remove) Job records that are older than the specified period.

Theses statistics records aren’t use for restore purpose, but mainly for capacity planning, billings, etc. See chapter :ref:`section-JobStatistics` for additional information.

