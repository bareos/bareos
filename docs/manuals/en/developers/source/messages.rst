Message Classes
===============

Currently, there are following classes of messages:

-  Memory Messages
-  Debug Messages
-  Job Messages
-  Queued Job Messages
-  Error Messages

Memory Messages
~~~~~~~~~~~~~~~

Memory messages are messages that are edited into a memory buffer.
Generally they are used in low level routines. These routines do not
generally have access to the Job Control Record and so they return
messages reformatted in a memory buffer.

::

    Mmsg(resultmessage, "3603 JobId=%u device %s is busy reading.\n", jobid);

Debug Messages
~~~~~~~~~~~~~~

Debug messages are designed to be turned on at a specified debug level
and are always sent to STDOUT. There are designed to only be used in the
development debug process. They are coded as:

::

    DmsgN(level, message, arg1, ...)

where

-  ``N`` is a number indicating how many arguments are to be substituted
   into the message (i.e. it is a count of the number arguments you have
   in your message – generally the number of percent signs (%)).
-  ``level`` is the debug level at which you wish the message to be
   printed.
-  ``message`` is the message to be printed, and
-  ``arg1, ...`` are the arguments to be substituted.

Since not all compilers support #defines with varargs, you must
explicitly specify how many arguments you have.

When the debug message is printed, it will automatically be prefixed by
the name of the daemon which is running, the filename where the Dmsg is,
and the line number within the file.

Some actual examples are:

::

    Dmsg2(20, “MD5len=%d MD5=%s\n”, strlen(buf), buf);
    Dmsg1(9, “Created client %s record\n”, client->hdr.name);

Job Messages
~~~~~~~~~~~~

Job messages are messages that pertain to a particular job such as a
file that could not be saved, or the number of files and bytes that were
saved. They are coded as:

::

    Jmsg(JCR, ERROR_CODE, 0, message, arg1, ...);

where

-  ``JCR`` is the **Job Control Record**. If ``JCR == NULL`` and the
   ``JCR`` can not be determined, the message is treated as Daemon
   message, with fallback to output to ``STDOUT``.
-  ``ERROR_CODE`` indicates the severity or class of error
-  ``0`` might be a timestamp, but is generally 0 (timestamp will be set
   to current time)
-  ``message`` is the message to be printed, and
-  ``arg1, ...`` are the arguments to be substituted.

``ERROR_CODE`` is one of the following:

+-----+----------------------------------------------------------------+
| ERR | Description                                                    |
| OR_ |                                                                |
| COD |                                                                |
| E   |                                                                |
+=====+================================================================+
| M_A | Causes the daemon to immediately abort. This should be used    |
| BOR | only in extrem e cases. It attempts to produce a traceback.    |
| T   |                                                                |
+-----+----------------------------------------------------------------+
| M_E | Causes the daemon to immediately terminate. This should be     |
| RRO | used only in extreme cases. It does not produce a traceback.   |
| R_T |                                                                |
| ERM |                                                                |
+-----+----------------------------------------------------------------+
| M_F | Causes the daemon to terminate the current job, but the daemon |
| ATA | keeps running.                                                 |
| L   |                                                                |
+-----+----------------------------------------------------------------+
| M_E | Reports the error. The daemon and the job continue running.    |
| RRO |                                                                |
| R   |                                                                |
+-----+----------------------------------------------------------------+
| M_W | Reports an warning message. The daemon and the job continue    |
| ARN | running.                                                       |
| ING |                                                                |
+-----+----------------------------------------------------------------+
| M_I | Reports an informational message.                              |
| NFO |                                                                |
+-----+----------------------------------------------------------------+

The Jmsg() takes varargs so can have any number of arguments for
substituted in a printf like format. Output from the Jmsg() will go to
the Job report.

If the Jmsg is followed with a number such as Jmsg1(…), the number
indicates the number of arguments to be substituted (varargs is not
standard for ``#defines``), and what is more important is that the file
and line number will be prefixed to the message. This permits a sort of
debug from user’s output.

::

    Jmsg1(jcr, M_WARNING, 0, "Plugin=%s not found.\n", cmd);
    Jmsg1(jcr, M_ERROR, 0, "%s exists but is not a directory.\n", path);
    Jmsg0(NULL, M_ERROR_TERM, 0, "Failed to find config filename.\n");

The `Message
Ressource <http://doc.bareos.org/master/html/bareos-manual-main-reference.html#MessagesChapter>`__
configuration defines how and to what destinations will be sent.

Special Cases
^^^^^^^^^^^^^

-  ``JCR == NULL``: in this case, it is tried to identify the ``JCR``
   automatically. If no ``JCR`` is found, the message is treated as
   Daemon message, with fallback to output to ``STDOUT``.
-  ``JCR.JobId == 0`` and ``JCR.dir_socket != NULL``: a socket
   connection to the Director exists (normally on the File or Storage
   Daemon). The message is send directly to the Director via this socket
   connection.

Queued Job Messages
~~~~~~~~~~~~~~~~~~~

Queued Job messages are similar to Jmsg()s except that the message is
Queued rather than immediately dispatched. This is necessary within the
network subroutines and in the message editing routines. This is to
prevent recursive loops, and to ensure that messages can be delivered
even in the event of a network error.

::

    Qmsg(jcr, M_INFO, 0, "File skipped. Not newer: %s\n", attr->ofname);

Error Messages
~~~~~~~~~~~~~~

Error messages are messages that are related to the daemon as a whole
rather than a particular job. For example, an out of memory condition my
generate an error message. They should be very rarely needed. In
general, you should be using Job and Job Queued messages (Jmsg and
Qmsg). They are coded as:

::

    EmsgN(ERROR_CODE, 0, message, arg1, ...)

As with debug messages, you must explicitly code the of arguments to be
substituted in the message.

Some actual examples are:

::

    Emsg1(M_ABORT, 0, “Cannot create message thread: %s\n”, strerror(status));
    Emsg3(M_WARNING, 0, “Connect to File daemon %s at %s:%d failed. Retrying ...\n”, client->hdr.name, client->address, client->port);
    Emsg3(M_FATAL, 0, “Bad response from File daemon to %s command: %d %s\n”, cmd, n, strerror(errno));

In essence, a ``EmsgN(ERROR_CODE, 0, message, arg1, ...)`` call resolves
to:

::

    DmsgN(10, message, arg1, ...)
    JmsgN(NULL, ERROR_CODE, 0, message, arg1, ...)
