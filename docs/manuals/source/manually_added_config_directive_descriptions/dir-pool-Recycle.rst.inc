This directive specifies whether or not Purged Volumes may be recycled. If it is set to :strong:`yes` and Bareos needs a volume but finds none that are appendable, it will search for and recycle (reuse) Purged Volumes (i.e. volumes with all the Jobs and Files expired and thus deleted from the Catalog). If the Volume is recycled, all previous data written to that Volume will be overwritten. If Recycle is set to :strong:`no`, the Volume will not be recycled, and hence,
the data will remain valid. If you want to reuse (re-write) the Volume, and the recycle flag is no (0 in the catalog), you may manually set the recycle flag (update command) for a Volume to be reused.

Please note that the value defined by this directive in the configuration file is the default value used when a Volume is created. Once the volume is created, changing the value in the configuration file will not change what is stored for the Volume. To change the value for an existing Volume you must use the :bcommand:`update volume` command.

When all Job and File records have been pruned or purged from the catalog for a particular Volume, if that Volume is marked as Append, Full, Used, or Error, it will then be marked as Purged. Only Volumes marked as Purged will be considered to be converted to the Recycled state if the :strong:`Recycle`\  directive is set to :strong:`yes`.

