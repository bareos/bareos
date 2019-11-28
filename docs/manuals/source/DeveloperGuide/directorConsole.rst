Director Console Output
=======================

The console output should handle different `API modes <#sec:ApiMode>`__.

As the initial design of the Director Console connection did allow
various forms of output (in fact: free text with some helper functions)
the introduction of another API mode (API mode JSON) did require to
change the creation of output.

-  Output can be send to a console connection (UserAgent Socket) or the
   command is executed in a job
-  free text (with the use of some helper functions)
-  -  :ref:`Signals <section-signals>`
-  Different Message types

   -  Types

      -  UAContext::send_msg
      -  UAContext::info_msg
      -  UAContext::warning_msg
      -  UAContext::error_msg

   -  indicated by a signal packet and than the text packet

-  see `Daemon Protocols <#DaemonProtocol>`__

The ``OUTPUT_FORMATTER`` class have been introduced to consolidate the
interface to generate Console output.

The basic idea is to provide an object, array and key-value based
interface.

object_key_value
~~~~~~~~~~~~~~~~

A main member function of ``OUTPUT_FORMATTER`` are the
object_key_value(…) functions, like

::

    void OUTPUT_FORMATTER::object_key_value(const char *key, const char *key_fmt, const char *value, const char *value_fmt, int wrap = -1);

API mode 0 and 1 get the key and value, and write them as defined in the
key_fmt and value_fmt strings. If the key_fmt string is not given, the
key will not be written. If the value_fmt string is not given, the value
will not be written.

In API mode 2 (JSON), OUTPUT_FORMATTER stores the key value pair in its
internal JSON object, to delivers it, when the response object is
finished. The keys will be transformed to lower case strings.

decoration
~~~~~~~~~~

Additional output can be created by the
``void OUTPUT_FORMATTER::decoration(const char *fmt, ...)`` function.
This strings will only be written in API mode 0 and is intended to make
an output prettier.

messages
~~~~~~~~

For messages, the UAContext function should be used:

-  UAContext::send_msg
-  UAContext::info_msg
-  UAContext::warning_msg
-  UAContext::error_msg

Internally, these redirect to the
``void OUTPUT_FORMATTER::message(const char *type, POOL_MEM &message)``
function.

-  API mode 0

   -  packet 1: message is send to the user console

-  API mode 1

   -  packet 1: message signal is send
   -  packet 2: message is send to the user console

-  API mode 2:

   -  packet 1: message signal is send (TODO: this might change)
   -  message is stored in the corresponding message object

      -  if an error message is send, the result of the command will
         switch to error

Objects and Arrays
~~~~~~~~~~~~~~~~~~

To structure data, the ``OUTPUT_FORMATTER`` class offers functions:

-  ``void object_start(const char *name = NULL);``
-  ``void object_end(const char *name = NULL);``
-  ``void array_start(const char *name);``
-  ``void array_end(const char *name);``

These functions define the structure of the reult object in API mode 2,
but are ignored in API mode 0 and 1.

named objects
^^^^^^^^^^^^^

-  named objects (object_start(NAME))

   -  can be added to objects (named and nameless objects)

nameless objects
^^^^^^^^^^^^^^^^

-  nameless objects (object_start(NULL))

   -  can be added to arrays

arrays
^^^^^^

-  arrays (array_start(NAME))

   -  can be added by name to an object
   -  contain nameless objects (object_start(NULL))

Example
~~~~~~~

::

    LockRes();
    ua->send->array_start("storages");
    foreach_res(store, R_STORAGE) {
        if (acl_access_ok(ua, Storage_ACL, store->name())) {
            ua->send->object_start();
            ua->send->object_key_value("name", store->name(), "%s\n");
            ua->send->object_end();
        }
    }
    ua->send->array_end("storages");
    UnlockRes();

results to

::

    *.api 2
    {
      "jsonrpc": "2.0",
      "id": null,
      "result": {
        "api": 2
      }
    }
    *.storages
    {
      "jsonrpc": "2.0",
      "id": null,
      "result": {
        "storages": [
          {
            "name": "File"
          },
          {
            "name": "myTapeLibrary"
          }
        ]
      }
    }

Example with 3 level structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    ua->send->array_start("files");
    for(int i=0; file[i]; i++) {
        ua->send->object_start();
        ua->send->object_key_value("Name", "%s=", file[i]->name, "%s");
        ua->send->object_key_value("Type", "%s=", file[i]->type, "%s");
        decode_stat(file[i]->lstat, &statp, sizeof(statp), LinkFI);
        ua->send->object_start("stat");
        ua->send->object_key_value("dev", "%s=", statp.st_dev, "%s");
        ua->send->object_key_value("ino", "%s=", statp.st_ino, "%s");
        ua->send->object_key_value("mode", "%s=", statp.st_mode, "%s");
        ...
        ua->send->object_end("stat");
        ua->send->object_end();
    }
    ua->send->array_end("files");
