No more than size bytes will be written into a given logical file on the
volume. Once this size is reached, an end-of-file mark is written on the
volume and subsequent data are written into the next file. Breaking long
sequences of data blocks with filemarks permits quicker positioning to the
start of a given stream of data and can improve recovery from read errors
on the volume.

.. note::

   Since Bareos 26, the default Maximum File Size has changed.

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
