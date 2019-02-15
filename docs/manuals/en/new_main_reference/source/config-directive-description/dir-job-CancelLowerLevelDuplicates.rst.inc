If :config:option:`dir/job/AllowDuplicateJobs`\  is set to ``no`` and this directive is set to ``yes``, Bareos will choose between duplicated jobs the one with the highest level. For example, it will cancel a previous Incremental to run a Full backup. It works only for Backup jobs. If the levels of the duplicated jobs are the same, nothing is done and the directives
:config:option:`dir/job/CancelQueuedDuplicates`\  and :config:option:`dir/job/CancelRunningDuplicates`\  will be examined.

