.. _checkpoints-chapter:

Checkpoints
===========

Checkpoints are a way to save the progress of a backup while it is running.

When a checkpoint is executed, the files that were successfully stored in the media at that point in time will be updated in the catalog database.

Checkpoints happen after a certain amount of time set by the user using the :config:option:`storage/storage/CheckpointInterval` directive.

Checkpoints also happen on volume changes. This means that when a volume is full, or the backup has to switch to writing to another volume for some other reason, a checkpoint is triggered saving what has been written to that volume.

The checkpoints feature is **disabled by default**.

Enabling/disabling the timed checkpoints means enabling/disabling the checkpoints on volume changes too.

Checkpoints will not be executed if the running job uses spooling, even if the checkpoints are enabled.
