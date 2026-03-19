barri image format
==================

The barri image format is created and used by both the **barri-cli** command line tools as well as the **barri-fd** plugin.

A valid image file is simply a list of `block`s.  Each `block` has the following structure:

======  ==== ====== ==================
Offset  Size Type   Content
======  ==== ====== ==================
0x00    8    string type identifier
0x08    \*          type specific data
======  ==== ====== ==================

Each value is stored with little endian byte order. Unless specified, blocks are *not* padded, i.e. a block that contains an 8 byte integer, may have a size thats not divisible by 8.
Generally, the type id is a human readable string descriping the block type. The first *block* is always a :ref:`DeveloperGuideBarriFileHeader` block, followed by any number of :ref:`disks <DeveloperGuideBarriDisk>`.  Each disk consists of one *disk header* block, followed by its partition information and data blocks.

.. _DeveloperGuideBarriFileHeader:

file header
-----------

======  ==== ====== =================
Offset  Size Type   Content
======  ==== ====== =================
0x00    8    string "barrifil"
0x08    2    uint   major version
0x0A    1    uint   minor version
0x0B    1    uint   patch version
0x0C    4    uint   disk count
0x10    8    uint   payload size
======  ==== ====== =================

The file header contains the number of disks contained in the image, as well as the total payload size.
The current version of the image format is 1.0.0.

.. _DeveloperGuideBarriDisk:

disk
----

As mentioned above, each disk is started by a *disk header*, followed by the :ref:`DeveloperGuideBarriPartTable`.
The partition table is followed by a number of :ref:`DeveloperGuideBarriExtent` equal to ``extent count``.
The disk header look looks like so:

======  ==== ====== =================
Offset  Size Type   Content
======  ==== ====== =================
0x00    8    string "barridsk"
0x08    8    uint   disk size
0x10    8    uint   total extent size
0x18    4    uint   media type
0x1C    4    uint   bytes per sector
0x20    4    uint   extent count
======  ==== ====== =================

Here the ``disk size`` is the size of the actual disk.  The ``total extent size`` is the amount of data actually contained in this image for this disk. This data is split over multiple so called *extents*.  ``extent count`` specifies how many of them there are for this disk in this image.  ``media type`` and ``bytes per sector`` specify the disk format.


.. _DeveloperGuideBarriPartTable:

partition table
---------------

======  ====== ====== =================
Offset  Size   Type   Content
======  ====== ====== =================
0x000   8      string "barritbl"
0x008   4      uint   partition count
0x00C   1      uint   partition type
0x00D   482           additional data
0x1EF   \*            table entries
======  ====== ====== =================

The data stored in ``additional data`` depends on what kind of partition we have.

raw partition table
^^^^^^^^^^^^^^^^^^^

======  ====== ====== =================
Offset  Size   Type   Content
======  ====== ====== =================
0x00    482    zero   reserved
======  ====== ====== =================

Partition tables for raw partitions do not contain any data.  ``partition count`` is always 0.

gpt partition table
^^^^^^^^^^^^^^^^^^^

The additional data for a gpt partition table has the following format:

======  ====== ====== ===================
Offset  Size   Type   Content
======  ====== ====== ===================
0x00    4      uint   max partition count
0x04    8      uint   data offset
0x0C    8      uint   data length
0x14    16     guid   disk guid
0x24    446    bytes  bootstrap
======  ====== ====== ===================

A gpt partition on the actual disk starts with a header in the second block.  This header tells the operating system which part of the disk can contain partitions.  This data is saved via ``data offset`` and ``data length``.  ``max partition count`` is the maximum amount of partitions that this disk can contain.  This is used in determining the size of the actual partition table.
``disk guid`` is the guid assigned to this disk.  This is a way of identifying it.
The ``bootstrap`` is the actual code that sits at the start of the disk that loads up the actual bootloader thats stored somewhere else on the disk.

See https://en.wikipedia.org/wiki/GUID_Partition_Table#Partition_table_header_(LBA_1)
for reference.

The partition table entries look like so:

======  ====== ======= ===================
Offset  Size   Type    Content
======  ====== ======= ===================
0x00    16     guid    type
0x10    16     guid    id
0x20    8      uint    attributes
0x28    72     wstring name
======  ====== ======= ===================

``name`` is a string made out of at most 36 utf-16le code units.  Regardless of its actual size, we always write the full 72 bytes.


mbr partition table
^^^^^^^^^^^^^^^^^^^

======  ====== ====== =================
Offset  Size   Type   Content
======  ====== ====== =================
0x00    4      uint   checksum
0x04    8      uint   signature
0x0C    8      zero
0x14    16     zero
0x24    446    bytes  bootstrap
======  ====== ====== =================

The ``bootstrap`` is the actual code that sits at the start of the disk that loads up the actual bootloader thats stored somewhere else on the disk.

The partition table entries look like so:

======  ====== ======= ===================
Offset  Size   Type    Content
======  ====== ======= ===================
0x00    16     guid    id
0x10    4      uint    hidden sector count
0x14    1      uint    type
0x15    1      bool    bootable
0x16    1      bool    recognized
======  ====== ======= ===================

.. _DeveloperGuideBarriExtent:

extent
------

Similar to :ref:`DeveloperGuideBarriPartTable` blocks, extent blocks are also not fixed size.

======  ====== ====== =================
Offset  Size   Type   Content
======  ====== ====== =================
0x00    8      string "barrixtn"
0x08    8      uint   offset
0x10    8      uint   length
0x18    length bytes  extent data
======  ====== ====== =================

``offset`` specifies where in the disk to store this extent.  ``length`` specifies the amount of data contained in this extent.
