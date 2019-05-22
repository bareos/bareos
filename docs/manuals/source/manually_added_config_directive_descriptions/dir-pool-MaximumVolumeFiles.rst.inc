This directive specifies the maximum number of files that can be written to the Volume. If you specify zero (the default), there is no limit. Otherwise, when the number of files written to the Volume equals positive-integer the Volume will be marked Used. When the Volume is marked Used it can no longer be used for appending Jobs, much like the Full status but it can be recycled if recycling is enabled and thus used again. This value is checked and the Used status is set only at the end of a job
that writes to the particular volume.

The value defined by this directive in the bareos-dir.conf file is the default value used when a Volume is created. Once the volume is created, changing the value in the bareos-dir.conf file will not change what is stored for the Volume. To change the value for an existing Volume you must use the update command in the Console.

