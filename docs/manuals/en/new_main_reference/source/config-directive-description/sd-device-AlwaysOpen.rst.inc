If Yes, Bareos will always keep the device open unless specifically unmounted by the Console program. This permits Bareos to ensure that the tape drive is always available, and properly positioned. If you set AlwaysOpen to no Bareos will only open the drive when necessary, and at the end of the Job if no other Jobs are using the drive, it will be freed. The next time Bareos wants to append to a tape on a drive that was freed, Bareos will rewind the tape and position it to the end. To avoid
unnecessary tape positioning and to minimize unnecessary operator intervention, it is highly recommended that Always Open = yes. This also ensures that the drive is available when Bareos needs it.

If you have Always Open = yes (recommended) and you want to use the drive for something else, simply use the unmount command in the Console program to release the drive. However, don’t forget to remount the drive with mount when the drive is available or the next Bareos job will block.

For File storage, this directive is ignored. For a FIFO storage device, you must set this to No.

Please note that if you set this directive to No Bareos will release the tape drive between each job, and thus the next job will rewind the tape and position it to the end of the data. This can be a very time consuming operation. In addition, with this directive set to no, certain multiple drive autochanger operations will fail. We strongly recommend to keep Always Open set to Yes

