If set to :strong:`yes`, Bareos will automatically apply the :config:option:`dir/client/FileRetention`\  period and the :config:option:`dir/client/JobRetention`\  period for the client at the end of the job.

Pruning affects only information in the catalog and not data stored in the backup archives (on Volumes), but if pruning deletes all data referring to a certain volume, the volume is regarded as empty and will possibly be overwritten before the volume retention has expired.

