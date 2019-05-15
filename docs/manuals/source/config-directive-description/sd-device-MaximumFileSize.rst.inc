No more than size bytes will be written into a given logical file on the volume. Once this size is reached, an end of file mark is written on the volume and subsequent data are written into the next file. Breaking long sequences of data blocks with file marks permits quicker positioning to the start of a given stream of data and can improve recovery from read errors on the volume. The default is one Gigabyte. This directive creates EOF marks only on tape media. However, regardless of the medium
type (tape, disk, USB ...) each time a the Maximum File Size is exceeded, a record is put into the catalog database that permits seeking to that position on the medium for restore operations. If you set this to a small value (e.g. 1MB), you will generate lots of database records (JobMedia) and may significantly increase CPU/disk overhead.

If you are configuring an modern drive like LTO-4 or newer, you probably will want to set the Maximum File Size to 20GB or bigger to avoid making the drive stop to write an EOF mark.

For more info regarding this parameter, read :ref:`Tapespeed and blocksizes`.

Note, this directive does not limit the size of Volumes that Bareos will create regardless of whether they are tape or disk volumes. It changes only the number of EOF marks on a tape and the number of block positioning records that are generated. If you want to limit the size of all Volumes for a particular device, use the use the :config:option:`dir/pool/MaximumVolumeBytes`\  directive.

