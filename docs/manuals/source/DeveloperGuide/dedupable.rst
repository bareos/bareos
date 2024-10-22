Dedupable Backend
=================

The dedupable storage backend allows bareos to store its data in
a way that is easier for deduping filesystems to deduplicate.
The problems with the simple file backend in that regard are
* interleaving of backup payload data and bareos meta data, and
* unaligned payload data.
This means that to support deduping filesystems we have to
essentially parse the interleaved data stream out into separate data
& metadata streams and write them to disk in a way where we
maximize aligned writes.


The layout of the data stream is described in
:ref:`storage-media-output-format`, but the basic gist is:

* data is stored in blocks, which start with a :ref:`sd-block-header`

* each message from the fd is prefixed by a :ref:`sd-record-header`
  and put into a block.  If the record would not fit the block,
  then it is split into multiple parts, each part having its own
  :ref:`sd-record-header`, each written to a different block.

The basic parsing algorithm this stream is simply:

.. uml:: dedupable/parsing.puml

.. note:: Keep in mind that split records can only happen at the
          end of a block.

Now that we have separated out all block headers, record headers
and data, we still need to store them in a sensible way.

The first observation is that we do not really need to care about
record headers anymore.  Their only only use is in delimiting the
backup payload.  As such we can treat them as normal data from now on.

This means that we think of our data like so:
Our data consists of **blocks**.  Each block consists **parts**,
which are data of a known size.
This means we need to store

* information on when blocks begin and end, essentially storing which parts they contain,

* information on when parts begin and end, and

* the data itself

The information about blocks is stored as a contiguous array of
block headers inside the volumes *blocks* file,
similarly the information about parts is stored as a contiguous array
of part headers in the volumes *parts* file.

The question remains on how we store the rest of the data.

Block Level Deduping Filesystems
--------------------------------

There are a lot of ways to deduplicate file data.  One common approach
is block based deduplication.

Files inside a file system are commonly split up into blocks of a
certain size.  This is done to make it easier to allocate memory
on disk, similar as to how the operating system splits up your
virtual memory space into pages to allocate them inside the
physical ram.

A block based deduplicating filesystem would use these blocks for
deduplication: Instead of writing multiple copies of the same block
to disk, it would only write a single copy and reference it multiple
times.

These are the kind of filesystems we would like to support. Examples
of filesystems with such functionality are zfs, btrfs, and block based
filesystems on top of vdo.

Storing data in a dedupable way
-------------------------------

.. note:: in this section *block* always refers to filesystem blocks,
          not to the blocks in our data stream.

To store our data in a dedupable manner, we need to ensure that
each backed up file is written to the volume in such a way that
the volumes blocks can be deduplicated.
There are two problems with this however:

#. the storage daemon has no idea about files in the backup stream,
   it only knows about records.
#. Not all files consist of all full blocks. E.g. on a filesystem
   with 128k block size, you can still have files with sizes < 128k.

The general idea we solved problem 1 is to store the records
themselves in a way that is dedup friendly. This means that if the
client is consistent in how he divides file data into records, then
those records are dedupable in our volume.

There are two ways to solve 2.:

* pad all small blocks to their full size, or

* store small blocks separately to full blocks.

We have decided to use the second approach here, as the backup stream
contains a lot of extremely small parts anyways, e.g. record headers,
plugin meta data, file permissions, and so on, which would be wasteful
to store in a padded manner.

So how does this look in practice:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
As discussed above, we just need to figure out how to store the
payload of a single record!

Lets say the payload has size *X*, and our filesystem uses blocks
of size *Y*. We split the payload into two sections: one section,
the aligned section, has size equal to the largest multiple of *Y*
smaller than *X* and starts at the beginning of the payload.  The
other section is the rest.  E.g. if the payload has size 13K and
the filesystem has block size 4K, then the aligned section will
consist of the first 12K = 4K * 3 bytes, and the rest section will
contain the last 1K.


We then write the aligned section to the *aligned.data* file
and the rest to the *unaligned.data* file, as we do not want to pad
it.  As a single record payload may be associated with multiple parts,
we need to make sure that this split into section is compatible with
them as each part may only refer to data in a single data file.
If this is not the case, then we simply split the part containing the
section border in two.

.. note::
   In the current implementation we do not need to explicitly split
   up parts along the aligned/unaligned border, as the parts are
   directly generated with that split in mind.

As we only store full blocks to the *aligned.data* file, each of them
will be dedupable.  This means that if the maximum record payload
size, which is equal to :config:option:`fd/client/MaximumNetworkBufferSize`,
is divisible by our filesystem block size, then most data is
potentially dedupable.

.. warning:: If the maximum record payload size is *smaller* than the filesystem
             block size, then nothing will be written to the *aligned.data* file
             and almost no data will be deduped.

Volume Structure
----------------

As described above, each volume, which is in fact a directory,
will contain at least four files:

* the *blocks* file, containing information about the sd blocks,

* the *parts* file, containing information about each part of the blocks,

* the *aligned.data* file, containing data that can be deduped, and

* the *unaligned.data* file, that contains all data that cannot be
  easily deduped.

In fact each volume also contains a final, fifth file called
*config*.  In this file the volume writes down some important
information about itself.  E.g. the block size for which it was
generated, also how many blocks, parts, and how much data is actually
contained in the other four files.
Without this file, the other files are basically just blobs of data.

Each of the other four files is basically just an array of their
respective types written to disk (in network byte order).
Whenever a file needs to grow, we grow it by at least 2MiB, so that
we do not have to constantly grow it during a back up, as growing a
file can be quite the slow operation.
