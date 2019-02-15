The Job Retention directive defines the length of time that Bareos will keep Job records in the Catalog database after the Job End time. When this time period expires and \resourceDirectiveValue{Dir}{Client}{Auto Prune}{yes} Bareos will prune (remove) Job records that are older than the specified File Retention period. As with the other retention periods, this affects only records in the catalog and not data in your archive backup.

If a Job record is selected for pruning, all associated File and JobMedia records will also be pruned regardless of the File Retention period set. As a consequence, you normally will set the File retention period to be less than the Job retention period. The Job retention period can actually be less than the value you specify here if you set the :config:option:`dir/pool/Volume
Retention`\  directive to a smaller duration. This is because the Job retention period and the Volume retention period are independently applied, so the smaller of the two takes precedence.

The default is 180 days.

