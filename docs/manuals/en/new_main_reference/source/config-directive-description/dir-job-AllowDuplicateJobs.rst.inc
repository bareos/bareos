\centering

.. figure:: /include/images/duplicate-real.*
   :alt: Allow Duplicate Jobs usage
   :name: fig:allowduplicatejobs
   :width: 50.0%

   Allow Duplicate Jobs usage

A duplicate job in the sense we use it here means a second or subsequent job with the same name starts. This happens most frequently when the first job runs longer than expected because no tapes are available.

If this directive is enabled duplicate jobs will be run. If the directive is set to ``no`` then only one job of a given name may run at one time. The action that Bareos takes to ensure only one job runs is determined by the directives

-  

   :config:option:`dir/job/CancelLowerLevelDuplicates`\ 

-  

   :config:option:`dir/job/CancelQueuedDuplicates`\ 

-  

   :config:option:`dir/job/CancelRunningDuplicates`\ 

If none of these directives is set to ``yes``, Allow Duplicate Jobs is set to ``no`` and two jobs are present, then the current job (the second one started) will be cancelled.

