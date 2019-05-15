This directive specifies the maximum number of Jobs that can be written to the Volume. If you specify zero (the default), there is no limit. Otherwise, when the number of Jobs backed up to the Volume equals positive-integer the Volume will be marked Used. When the Volume is marked Used it can no longer be used for appending Jobs, much like the Full status but it can be recycled if recycling is enabled, and thus used again. By setting MaximumVolumeJobs to one, you get the same effect as setting
UseVolumeOnce = yes.

The value defined by this directive in the bareos-dir.conf file is the default value used when a Volume is created. Once the volume is created, changing the value in the bareos-dir.conf file will not change what is stored for the Volume. To change the value for an existing Volume you must use the update command in the Console.

If you are running multiple simultaneous jobs, this directive may not work correctly because when a drive is reserved for a job, this directive is not taken into account, so multiple jobs may try to start writing to the Volume. At some point, when the Media record is updated, multiple simultaneous jobs may fail since the Volume can no longer be written.

