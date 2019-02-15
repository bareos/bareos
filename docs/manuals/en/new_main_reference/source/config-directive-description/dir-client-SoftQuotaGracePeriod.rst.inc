Time allowed for a client to be over its :config:option:`dir/client/SoftQuota`\  before it will be enforced.

When the amount of data backed up by the client outruns the value specified by the Soft Quota directive, the next start of a backup job will start the soft quota grace time period. This is written to the job log:



   .. literalinclude:: /_static/config/DirClientSoftQuotaGracePeriod1.conf



In the Job Overview, the value of Grace Expiry Date: will then change from ``Soft Quota was never exceeded`` to the date when the grace time expires, e.g. ``11-Dec-2012 04:09:05``.

During that period, it is possible to do backups even if the total amount of stored data exceeds the limit specified by soft quota.

If in this state, the job log will write:



   .. literalinclude:: /_static/config/DirClientSoftQuotaGracePeriod2.conf



After the grace time expires, in the next backup job of the client, the value for Burst Quota will be set to the value that the client has stored at this point in time. Also, the job will be terminated. The following information in the job log shows what happened:



   .. literalinclude:: /_static/config/DirClientSoftQuotaGracePeriod3.conf



At this point, it is not possible to do any backup of the client. To be able to do more backups, the amount of stored data for this client has to fall under the burst quota value.

