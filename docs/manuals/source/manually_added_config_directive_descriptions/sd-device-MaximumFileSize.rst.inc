No more than size bytes will be written into a given logical file on the
volume. Once this size is reached, an end-of-file mark is written on the
volume and subsequent data are written into the next file. Breaking long
sequences of data blocks with filemarks permits quicker positioning to the
start of a given stream of data and can improve recovery from read errors
on the volume.

.. note::

   Since Bareos :sinceVersion:`26.0.0: Maximum File Size default 25 GiB`,
   the default Maximum File Size is 25 GiB (previously 1 GB).

Since Bareos :sinceVersion:`26.0.0: Immediate tape filemarks`, filemarks on
tape are written in immediate mode where the operating system supports it
(Linux). The drive then does not flush its buffer to tape for each
filemark. On other platforms, or if the drive rejects the immediate
filemark, Bareos writes a regular (non-immediate) filemark instead.

Measurements on an LTO-6 drive show that each non-immediate filemark
delays writing by about 6 seconds, which is why large values were
previously recommended. With immediate filemarks no delay is measurable,
so the default does not need to be increased for write speed:

.. figure:: /include/images/tape-filemark-write-time.png

   Time in seconds to write 100 GB for different Maximum File Size
   values (in GB), with and without immediate filemarks.

A larger value means fewer positioning points, so restoring data
from the middle of a volume can take longer. The default of 25 GiB is a
compromise between filemark seek time and the time needed to read up to the
wanted data:

.. figure:: /include/images/tape-filemark-seek-time.png

   Seek time in seconds to the n-th filemark on a tape with 1 GB between
   filemarks.

This directive creates EOF marks only on tape media. However, regardless
of the medium type (tape, disk, USB, etc.), each time the Maximum File Size
is exceeded, a record is added to the catalog database to permit seeking
to that position on the medium for restore operations. Setting this to a
small value (for example, 1 MB) generates many JobMedia records and may
significantly increase CPU and disk overhead.

For more information, see :ref:`Tapespeed and blocksizes`.

This directive does not limit the size of Volumes Bareos creates, whether
they are tape or disk volumes. It changes only the number of EOF marks on
a tape and the number of block-positioning records generated. To limit the
size of all Volumes for a particular device, use
:config:option:`dir/pool/MaximumVolumeBytes`.
