Bareos Memory Management
========================

General
-------

This document describes the memory management routines that are used in
Bareos and is meant to be a technical discussion for developers rather
than part of the user manual.

Since Bareos may be called upon to handle filenames of varying and more
or less arbitrary length, special attention needs to be used in the code
to ensure that memory buffers are sufficiently large. There are four
possibilities for memory usage within **Bareos**. Each will be described
in turn. They are:

-  Statically allocated memory.

-  Dynamically allocated memory using malloc() and free().

-  Non-pooled memory.

-  Pooled memory.

Statically Allocated Memory
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Statically allocated memory is of the form:

::

    char buffer[MAXSTRING];

The use of this kind of memory is discouraged except when you are 100%
sure that the strings to be used will be of a fixed length. One example
of where this is appropriate is for **Bareos** resource names, which are
currently limited to 127 characters (MAX_NAME_LENGTH). Although this
maximum size may change, particularly to accommodate Unicode, it will
remain a relatively small value.

Dynamically Allocated Memory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Dynamically allocated memory is obtained using the standard malloc()
routines. As in:

::

    char *buf;
    buf = malloc(256);

This kind of memory can be released with:

::

    free(buf);

It is recommended to use this kind of memory only when you are sure that
you know the memory size needed and the memory will be used for short
periods of time – that is it would not be appropriate to use statically
allocated memory. An example might be to obtain a large memory buffer
for reading and writing files.

Pooled and Non-pooled Memory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to facility the handling of arbitrary length filenames and to
efficiently handle a high volume of dynamic memory usage, we have
implemented routines between the C code and the malloc routines. The
first is called “Pooled” memory, and is memory, which once allocated and
then released, is not returned to the system memory pool, but rather
retained in a Bareos memory pool. The next request to acquire pooled
memory will return any free memory block. In addition, each memory block
has its current size associated with the block allowing for easy
checking if the buffer is of sufficient size. This kind of memory would
normally be used in high volume situations (lots of malloc()s and
free()s) where the buffer length may have to frequently change to adapt
to varying filename lengths.

The non-pooled memory is handled by routines similar to those used for
pooled memory, allowing for easy size checking. However, non-pooled
memory is returned to the system rather than being saved in the Bareos
pool. This kind of memory would normally be used in low volume
situations (few malloc()s and free()s), but where the size of the buffer
might have to be adjusted frequently.

Types of Memory Pool:
'''''''''''''''''''''

Currently there are three memory pool types:

-  PM_NOPOOL – non-pooled memory.

-  PM_FNAME – a filename pool.

-  PM_MESSAGE – a message buffer pool.

-  PM_EMSG – error message buffer pool.

Getting Memory:
'''''''''''''''

To get memory, one uses:

::

    void *get_pool_memory(pool);

where **pool** is one of the above mentioned pool names. The size of the
memory returned will be determined by the system to be most appropriate
for the application.

If you wish non-pooled memory, you may alternatively call:

::

    void *get_memory(size_t size);

The buffer length will be set to the size specified, and it will be
assigned to the PM_NOPOOL pool (no pooling).

Releasing Memory:
'''''''''''''''''

To free memory acquired by either of the above two calls, use:

::

    void free_pool_memory(void *buffer);

where buffer is the memory buffer returned when the memory was acquired.
If the memory was originally allocated as type PM_NOPOOL, it will be
released to the system, otherwise, it will be placed on the appropriate
Bareos memory pool free chain to be used in a subsequent call for memory
from that pool.

Determining the Memory Size:
''''''''''''''''''''''''''''

To determine the memory buffer size, use:

::

    size_t sizeof_pool_memory(void *buffer);

Resizing Pool Memory:
'''''''''''''''''''''

To resize pool memory, use:

::

    void *realloc_pool_memory(void *buffer);

The buffer will be reallocated, and the contents of the original buffer
will be preserved, but the address of the buffer may change.

Automatic Size Adjustment:
''''''''''''''''''''''''''

To have the system check and if necessary adjust the size of your pooled
memory buffer, use:

::

    void *check_pool_memory_size(void *buffer, size_t new-size);

where **new-size** is the buffer length needed. Note, if the buffer is
already equal to or larger than **new-size** no buffer size change will
occur. However, if a buffer size change is needed, the original contents
of the buffer will be preserved, but the buffer address may change. Many
of the low level Bareos subroutines expect to be passed a pool memory
buffer and use this call to ensure the buffer they use is sufficiently
large.

Releasing All Pooled Memory:
''''''''''''''''''''''''''''

In order to avoid orphaned buffer error messages when terminating the
program, use:

::

    void close_memory_pool();

to free all unused memory retained in the Bareos memory pool. Note, any
memory not returned to the pool via free_pool_memory() will not be
released by this call.

Pooled Memory Statistics:
'''''''''''''''''''''''''

For debugging purposes and performance tuning, the following call will
print the current memory pool statistics:

::

    void print_memory_pool_stats();

an example output is:

::

    Pool  Maxsize  Maxused  Inuse
       0      256        0      0
       1      256        1      0
       2      256        1      0
