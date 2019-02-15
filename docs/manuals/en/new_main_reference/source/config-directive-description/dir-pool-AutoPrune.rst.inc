If :strong:`Auto Prune=yes`, the :config:option:`dir/pool/VolumeRetention`\  period is automatically applied when a new Volume is needed and no appendable Volumes exist in the Pool. Volume pruning causes expired Jobs (older than the :config:option:`dir/pool/VolumeRetention`\  period) to be deleted from the Catalog and permits possible recycling of the Volume.

