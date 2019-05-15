If Yes, the archive device is assumed to be a random access medium which supports the lseek (or lseek64 if Largefile is enabled during configuration) facility. This should be set to Yes for all file systems such as USB, and fixed files. It should be set to No for non-random access devices such as tapes and named pipes.

