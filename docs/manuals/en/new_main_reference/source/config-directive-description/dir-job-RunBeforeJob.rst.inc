This is a shortcut for the :config:option:`dir/job/RunScript`\  resource, that runs a command before a job.

If the exit code of the program run is non-zero, the current Bareos job will be canceled.



   .. literalinclude:: /include/config/DirJobRunBeforeJob1.conf



is equivalent to:



   .. literalinclude:: /include/config/DirJobRunBeforeJob2.conf


