You may turn off the Block Checksum (CRC32) code that Bareos uses when writing blocks to a Volume. Doing so can reduce the Storage daemon CPU usage slightly. It will also permit Bareos to read a Volume that has corrupted data.

It is not recommend to turn this off, particularly on older tape drives or for disk Volumes where doing so may allow corrupted data to go undetected.

