If Yes the storage daemon will not try to rewind the device on closing the device e.g. when shutting down the Storage daemon. This allows you to do an emergency shutdown of the Daemon without the need to wait for the device to rewind. On restarting and opening the device it will get a rewind anyhow and this way services don’t have to wait forever for a tape to spool back.

