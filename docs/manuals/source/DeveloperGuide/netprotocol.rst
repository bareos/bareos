TCP/IP Network Protocol
=======================

Introduction
------------

This document describes the TCP/IP protocol used by Bareos to
communicate between the various daemons and services. The definitive
definition of the protocol can be found in src/lib/bsock.h,
src/lib/bnet.c and src/lib/bnet_server.c.

Bareos’s network protocol is basically a “packet oriented” protocol
built on a standard TCP/IP streams. At the lowest level all packet
transfers are done with read() and write() requests on system sockets.
Pipes are not used as they are considered unreliable for large serial
data transfers between various hosts.

The chapter :ref:`DeveloperGuideNetworkSequenceDiagrams` contains message sequences of backup, restore and copy jobs as examples, respectively.

Network Routines
----------------

Using the routines described below (bnet_open, bnet_write, bnet_recv,
and bnet_close) guarantees that the number of bytes you write into the
socket will be received as a single record on the other end regardless
of how many low level write() and read() calls are needed. All data
transferred are considered to be binary data.

bnet and Threads
~~~~~~~~~~~~~~~~

These bnet routines work fine in a threaded environment. However, they
assume that there is only one reader or writer on the socket at any
time. It is highly recommended that only a single thread access any
BSOCK packet. The exception to this rule is when the socket is first
opened and it is waiting for a job to start. The wait in the Storage
daemon is done in one thread and then passed to another thread for
subsequent handling.

If you envision having two threads using the same BSOCK, think twice,
then you must implement some locking mechanism. However, it probably
would not be appropriate to put locks inside the bnet subroutines for
efficiency reasons.

bnet_open
~~~~~~~~~

To establish a connection to a server, use the subroutine:

BSOCK \*bnet_open(void \*jcr, char \*host, char \*service, int port, int
\*fatal) bnet_open(), if successful, returns the Bareos sock descriptor
pointer to be used in subsequent bnet_send() and bnet_read() requests.
If not successful, bnet_open() returns a NULL. If fatal is set on
return, it means that a fatal error occurred and that you should not
repeatedly call bnet_open(). Any error message will generally be sent to
the JCR.

bnet_send
~~~~~~~~~

To send a packet, one uses the subroutine:

int bnet_send(BSOCK \*sock) This routine is equivalent to a write()
except that it handles the low level details. The data to be sent is
expected to be in sock->msg and be sock->msglen bytes. To send a packet,
bnet_send() first writes four bytes in network byte order than indicate
the size of the following data packet. It returns:

::

     Returns 0 on failure
     Returns 1 on success

In the case of a failure, an error message will be sent to the JCR
contained within the bsock packet.

bnet_fsend
~~~~~~~~~~

This form uses:

int bnet_fsend(BSOCK \*sock, char \*format, …) and it allows you to send
a formatted messages somewhat like fprintf(). The return status is the
same as bnet_send.

is_bnet_error
~~~~~~~~~~~~~

Fro additional error information, you can call **is_bnet_error(BSOCK
\*bsock)** which will return 0 if there is no error or non-zero if there
is an error on the last transmission.

is_bnet_stop
~~~~~~~~~~~~

The **is_bnet_stop(BSOCK \*bsock)** function will return 0 if there no
errors and you can continue sending. It will return non-zero if there
are errors or the line is closed (no more transmissions should be sent).

bnet_recv
~~~~~~~~~

To read a packet, one uses the subroutine:

int bnet_recv(BSOCK \*sock) This routine is similar to a read() except
that it handles the low level details. bnet_read() first reads packet
length that follows as four bytes in network byte order. The data is
read into sock->msg and is sock->msglen bytes. If the sock->msg is not
large enough, bnet_recv() realloc() the buffer. It will return an error
(-2) if maxbytes is less than the record size sent.

It returns:

-  >0: number of bytes read
-  0: on end of file
-  -1: on hard end of file (i.e. network connection close)
-  -2: on error

It should be noted that bnet_recv() is a blocking read.

.. _sec:bnet_sig:

bnet_sig
~~~~~~~~

To send a “signal” from one daemon to another, one uses the subroutine:

int bnet_sig(BSOCK \*sock, SIGNAL) where SIGNAL is one of the following:

-  BNET_EOF - deprecated use BNET_EOD
-  BNET_EOD - End of data stream, new data may follow
-  BNET_EOD_POLL - End of data and poll all in one
-  BNET_STATUS - Request full status
-  BNET_TERMINATE - Conversation terminated, doing close()
-  BNET_POLL - Poll request, I’m hanging on a read
-  BNET_HEARTBEAT - Heartbeat Response requested
-  BNET_HB_RESPONSE - Only response permitted to HB
-  BNET_PROMPT - Prompt for UA

bnet_strerror
~~~~~~~~~~~~~

Returns a formated string corresponding to the last error that occurred.

bnet_close
~~~~~~~~~~

The connection with the server remains open until closed by the
subroutine:

void bnet_close(BSOCK \*sock)

Becoming a Server
~~~~~~~~~~~~~~~~~

The bnet_open() and bnet_close() routines described above are used on
the client side to establish a connection and terminate a connection
with the server. To become a server (i.e. wait for a connection from a
client), use the routine **bnet_thread_server**. The calling sequence is
a bit complicated, please refer to the code in bnet_server.c and the
code at the beginning of each daemon as examples of how to call it.

Higher Level Conventions
------------------------

Within Bareos, we have established the convention that any time a single
record is passed, it is sent with bnet_send() and read with bnet_recv().
Thus the normal exchange between the server (S) and the client (C) are:

::

    S: wait for connection            C: attempt connection
    S: accept connection              C: bnet_send() send request
    S: bnet_recv() wait for request
    S: act on request
    S: bnet_send() send ack            C: bnet_recv() wait for ack

Thus a single command is sent, acted upon by the server, and then
acknowledged.

In certain cases, such as the transfer of the data for a file, all the
information or data cannot be sent in a single packet. In this case, the
convention is that the client will send a command to the server, who
knows that more than one packet will be returned. In this case, the
server will enter a loop:

::

    while ((n=bnet_recv(bsock)) > 0) {
       act on request
    }
    if (n < 0)
       error

The client will perform the following:

::

    bnet_send(bsock);
    bnet_send(bsock);
    ...
    bnet_sig(bsock, BNET_EOD);

Thus the client will send multiple packets and signal to the server when
all the packets have been sent by sending a zero length record.

Network startup activity
------------------------

The following diagram shows the activity on a Bareos TCP server (i.e. a |dir| or a |sd|) when a TCP client initiates a network connection. The connection protocol has two possible handshake modes. Cleartext first or TLS first. Depending on the configuration, the Bareos version and the type of client (i.e. |fd| or |bconsole|) the handshake mode will be chosen. See chapter :ref:`section-TlsTechnicalDocumentation` for detailed information. 

.. uml::
  :caption: Network startup activity (Bareos TCP-Server)

  start

  :accept TCP socket;

  if (evaluation of cleartext hello \nis successfus) then (yes)
    if (is cleartext hello ?) then (yes)
      if (is R_CLIENT ?) then (yes)
        if (TLS required) then (yes)
          :Close connection;
          stop
        else (no)
          :Do cleartext \nhandshake;
        endif
      elseif (is R_CONSOLE AND \nconsole version < 18.2 ?) then (yes)
          :Do cleartext \nhandshake;
      elseif (TLS not configured) then (yes)
        :Do cleartext \nhandshake;
      else
        :Close connection;
        stop
      endif
      :Do cleartext handshake;
    else (no)
      :Do TLS handshake \nas a server;
      if (TLS handshake successful ?) then (no)
        :Close connection;
        stop
      else (yes)
      endif
    endif
  else (no)
    :Close connection;
    stop
  endif

  :Handle client message;
  end

Debugging
---------

Developers who want to debug the message flow can use the network dump feature. This feature is only for experts and developers and is **not intended for use in a production system**. Therefore no help will be provided on the command line. The arguments however can be mixed with the standard arguments.

.. code-block:: shell-session
   :caption: Network Dump Command Line Options

   Usage: [bconsole|bareos-dir|bareos-fd|bareos-sd]
          -z <filename>    name and path of the dumpfile
                           (setting this switches on the network dump)
          -zp              use the plantuml notation
          -z <stackdepth>  for additional stacktrace, stackdepth=-1: print all frames,
                                                      stackdepth=n:  print up to n frames

.. _DeveloperGuideNetworkSequenceDiagrams:

Network Sequence Diagrams
-------------------------

This chapter contains diagrams of the network traffic for the most common jobs. The jobs are simplified as much as possible to show the essence of the network traffic. Refer to chapter :ref:`DeveloperGuideJobExecution` for sequence diagrams with the internal program flow.

Backup of a Single File
~~~~~~~~~~~~~~~~~~~~~~~

Comprehensive Diagram
^^^^^^^^^^^^^^^^^^^^^

.. uml:: netprotocol/backup_of_one_file_comprehensive.puml

Detailed Diagram
^^^^^^^^^^^^^^^^

.. uml:: netprotocol/backup_of_one_file.puml

Restore of a Single File
~~~~~~~~~~~~~~~~~~~~~~~~

Comprehensive Diagram
^^^^^^^^^^^^^^^^^^^^^

.. uml:: netprotocol/restore_of_one_file_comprehensive.puml

Detailed Diagram
^^^^^^^^^^^^^^^^

.. uml:: netprotocol/restore_of_one_file.puml

