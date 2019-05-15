This directive specifies the maximum number of Jobs with the current Storage resource that can run concurrently. Note, this directive limits only Jobs for Jobs using this Storage daemon. Any other restrictions on the maximum concurrent jobs such as in the Director, Job or Client resources will also apply in addition to any limit specified here.

If you set the Storage daemon’s number of concurrent jobs greater than one, we recommend that you read :ref:`ConcurrentJobs` and/or turn data spooling on as documented in :ref:`SpoolingChapter`.

