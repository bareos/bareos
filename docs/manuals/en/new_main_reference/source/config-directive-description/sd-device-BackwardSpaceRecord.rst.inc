If Yes, the archive device supports the \tt MTBSR ioctl to backspace records. If No, this call is not used and the device must be rewound and advanced forward to the desired position. This function if enabled is used at the end of a Volume after writing the end of file and any ANSI/IBM labels to determine whether or not the last block was written correctly. If you turn this function off, the test will not be done. This causes no harm as the re-read process is precautionary rather
than required.

