.. _checkpoints-chapter:

Checkpoints
===========

Usually, the information about files backed up by a job are stored into the database at the end of a **successful** job.
This results in failed or cancelled jobs not having any file information.
You cannot recover the data from these jobs via the usual restore procedure. Especially for very long running jobs with many files, this fact is unsatisfactory.
To avoid this problem, we introduced the Checkpoints feature.

Checkpoints are a way to save the progress of a backup while it is running.

When a checkpoint is executed, the files that were successfully stored in the media at that point in time will be updated in the catalog database.

Checkpoints happen after a certain amount of time set by the user using the :config:option:`storage/storage/CheckpointInterval` directive.
When the interval is too high and the backup job fails, the files that have been backed up since the last Checkpoint will be missing in the catalog.
On the other hand, updating the catalog too often will create a higher load on the system and might reduce the backup performance and clutter debug messages.
For that reason, we suggest to start with a value of 15 minutes. A reasonable range is between 1 and 30 minutes.

Checkpoints also happen on volume changes. This means that when a volume is full, or the backup has to switch to writing to another volume for some other reason, a checkpoint is triggered saving what has been written to that volume.

Based on the Checkpoints Feature, a resume of cancelled or broken Backupjobs is conceivable in the future.

The checkpoints feature is **disabled by default**.

Enabling/disabling the timed checkpoints means enabling/disabling the checkpoints on volume changes too.



.. warning::

   As the functionality of Checkpoints does not make sense with how spooling works, checkpoints are not executed on jobs using spooling, even if the checkpoints are enabled.
