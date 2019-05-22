This directive instructs the Director to search for the oldest used Volume in the Pool when another Volume is requested by the Storage daemon and none are available. The catalog is then purged irrespective of retention periods of all Files and Jobs written to this Volume. The Volume is then recycled and will be used as the next Volume to be written. This directive overrides any Job, File, or Volume retention periods that you may have specified.

This directive can be useful if you have a fixed number of Volumes in the Pool and you want to cycle through them and reusing the oldest one when all Volumes are full, but you don’t want to worry about setting proper retention periods. However, by using this option you risk losing valuable data.

In most cases, you should use :config:option:`dir/pool/RecycleOldestVolume`\  instead.



   .. warning::

      
      Be aware that :strong:`Purge Oldest Volume`\  disregards all retention
      periods. If you have only a single Volume defined and you turn this
      variable on, that Volume will always be immediately overwritten when it
      fills!  So at a minimum, ensure that you have a decent number of Volumes
      in your Pool before running any jobs.  If you want retention periods to
      apply do not use this directive.\\
      We :strong:`highly` recommend against using this directive, because it is
      sure that some day, Bareos will purge a Volume that contains current
      data.
      
