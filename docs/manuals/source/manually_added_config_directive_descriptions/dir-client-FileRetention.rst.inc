The File Retention directive defines the length of time that Bareos will keep File records in the Catalog database after the End time of the Job corresponding to the File records. When this time period expires and :config:option:`dir/client/AutoPrune = yes`\ , Bareos will prune (remove) File records that are older than the specified File Retention period. Note, this affects only records in the catalog database. It does not affect your archive backups.

File records may actually be retained for a shorter period than you specify on this directive if you specify either a shorter :config:option:`dir/client/JobRetention`\  or a shorter :config:option:`dir/pool/VolumeRetention`\  period. The shortest retention period of the three takes precedence.

The default is 60 days.

