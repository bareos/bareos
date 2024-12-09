.. _WritingDplcompatWrappers:

Dplcompat Wrapper Programs
==========================

.. index::
   single: Dplcompat


:ref:`SdBackendDplcompat` can be used to access object storage through external
wrapper programs. This chapter explains the requirements for such a wrapper
program.

Overview
--------
When Dplcompat wants to perform an operation on the object storage, it will run
the wrapper program.
The wrapper program will then do the actual work.

When Dplcompat calls the program it will request a specific operation by passing
command-line parameters.
Dplcompat will also pass wrapper-specific options, that were configured in
:config:option:`sd/device/DeviceOptions` as environment variables to the
program.
All data passed to or expected from the program will be transmitted via
stdin/stdout.
On a failure, the program must exit with a non-zero return code.


Operations
----------
options
   Get a list of supported option names for the program.
testconnection
   Test if the connection to the object storage works.
list <volume>
   List the parts stored for <volume> with their sizes.
stat <volume> <part>
   Get the size of <part> in <volume>.
upload <volume> <part>
   Upload <part> for <volume> to the object storage.
download
   Download <part> of <volume> from the object storage.
remove
   Delete <part> of <volume> from the object storage.

options operation
~~~~~~~~~~~~~~~~~
This operation is used to determine which options a wrapper program supports
when initializing the device. For this call, there is no guarantee that the
|sd| passes the environment variables to configure the program.
This operation should never fail.

Command line
   ``<wrapper-program> options``
``options``
   The literal string ``options``.
Provided input
   None.
Expected output
   A list of option names, separated by newlines (``\n``). A single newline (``\n``)
   may be appended to the end of the list.
Return code
   Always zero (0).

.. code-block:: none
   :caption: example output for options operation

   fruit
   vegetable
   shape

testconnection operation
~~~~~~~~~~~~~~~~~~~~~~~~
This operation checks if the object storage works correctly with the supplied
configuation.

Command line
   ``<wrapper-program> testconnection``
``testconnection``
   The literal string ``testconnection``.
Provided input
   None.
Expected output
   None. When output is provided, it may be used for logging purposes.
Return code
   Zero on successful test, non-zero otherwise.

list operation
~~~~~~~~~~~~~~
This operation lists the parts of a volume on the object storage together with
their sizes.

Command line
   ``<wrapper-program> list <volume>``
``list``
   The literal string ``list``.
``<volume>``
   The name of the volume to list. This may contain alphanumeric characters, as
   well as colon (``:``), full stop (``.``), hyphen-minus (``-``),
   underscore (``_``), slash (``/``) and space.
Provided input
   None.
Expected output
   A list of the volume parts in no specific order.
   For every part a single line containing the name of the part, followed by a 
   space (`` ``) character, followed by the string representation of the size in
   bytes for that part must be output.
   The lines should be separated using newline (``\n``) characters.
   When there are no parts for the volume, an empty list (i.e. no output) should
   be provided.
Return code
   Zero on success, including successfully listing no parts, non-zero otherwise.

.. code-block:: none
   :caption: example output for list operation

   part00 10485760
   part03 743246
   part01 10485760
   part02 10485760

stat operation
~~~~~~~~~~~~~~
This operation gets the size of a specific volume part on the object storage.

Command line
   ``<wrapper-program> stat <volume> <part>``
``stat``
   The literal string ``stat``.
``<volume>``
   The name of a volume. This may contain alphanumeric characters, as
   well as colon (``:``), full stop (``.``), hyphen-minus (``-``),
   underscore (``_``), slash (``/``) and space.
``<part>``
   The name of a part in the volume. This contains only alphanumeric characters.
Provided input
   None.
Expected output
   The size in bytes of the storage represented as a string.
Return code
   Zero on success, non-zero otherwise, including non-existent volume or part.

.. code-block:: none
   :caption: example output for stat operation

   743246


upload operation
~~~~~~~~~~~~~~~~
This operation uploads data to the object storage.
The program is required to overwrite a part that already exists.

Command line
   ``<wrapper-program> upload <volume> <part>``
``upload``
   The literal string ``upload``.
``<volume>``
   The name of a volume. This may contain alphanumeric characters, as
   well as colon (``:``), full stop (``.``), hyphen-minus (``-``),
   underscore (``_``), slash (``/``) and space.
``<part>``
   The name of a part in the volume. This contains only alphanumeric characters.
Provided input
   Data stream to upload.
Expected output
   None. When output is provided, it may be used for logging purposes.
Return code
   Zero on success, including overwriting a part, non-zero otherwise.


download operation
~~~~~~~~~~~~~~~~~~
This operation downloads data from the object storage.

Command line
   ``<wrapper-program> download <volume> <part>``
``download``
   The literal string ``download``.
``<volume>``
   The name of a volume. This may contain alphanumeric characters, as
   well as colon (``:``), full stop (``.``), hyphen-minus (`-``),
   underscore (``_``), slash (``/``) and space.
``<part>``
   The name of a part in the volume. This contains only alphanumeric characters.
Provided input
   None.
Expected output
   Data stream of the part being downloaded.
Return code
   Zero on success, non-zero otherwise, including non-existent volume or part.


remove operation
~~~~~~~~~~~~~~~~
This operation removes data from the object storage.

Command line
   ``<wrapper-program> download <volume> <part>``
``download``
   The literal string ``download``.
``<volume>``
   The name of a volume. This may contain alphanumeric characters, as
   well as colon (``:``), full stop (``.``), hyphen-minus (``-``),
   underscore (``_``), slash (``/``) and space.
``<part>``
   The name of a part in the volume. This contains only alphanumeric characters.
Provided input
   None.
Expected output
   None. When output is provided, it may be used for logging purposes.
Return code
   Zero on success, non-zero otherwise, including non-existent volume or part.

Minimum viable example
----------------------
The following example script uses the local filesystem as an object storage.
Its only option is ``storage_path`` that points to the directory objects will be
stored in.
The objects will be stored in files named ``<volume>.<part>``.

.. code-block:: sh
   :caption: example-wrapper.sh

   #!/bin/bash
   # exit with an error when
   # a) a command fails
   # b) any command in a pipeline fails
   # c) an unset variable is encountered
   set -euo pipefail

   # expand non-matching globs to nothing (instead of the pattern itself)
   shopt -s nullglob

   case "$1" in
     options)
       # list available options
       echo storage_path
       ;;
     testconnection)
       # this will fail if $storage_path is unset or does not point to a
       # directory.
       [ -d "$storage_path" ]
       ;;
     list)
       # list all parts using globbing, due to nullglob the list is empty when
       # there is no match.
       for f in "$storage_path/$2".*; do
         base="$(basename "$f")"
         printf "%s %d\n" "${base##*.}" "$(stat --format=%s "$f")"
       done
       ;;
     stat)
       # print file size, fail if file does not exist
       stat --format=%s "$storage_path/$2.$3"
       ;;
     upload)
       # overwrite file with data from stdin
       exec cat >"$storage_path/$2.$3"
       ;;
     download)
       # print file contents to stdout, fails if file does not exist
       exec cat "$storage_path/$2.$3"
       ;;
     remove)
       # remove file, fails if file does not exist
       exec rm "$storage_path/$2.$3"
       ;;
     *)
       # catch all other operations and fail
       exit 1
       ;;
   esac

We use ``set -u`` to simlpify parameter checking.
When the script encounters an unset ``$1``, ``$2`` or ``$3``, it will
automatically fail with an error.
The same is true if the configuration option is missing.

The script also utilizes ``set -e`` to simplify error handling.
When any command fails, the whole script will immediately exit with an error.

.. warning::
   This example is not 100% accurate.
   It will, for example, not work coorectly for volume names containing a
   slash (``/``) or a full stop (``.``).
