TCP/IP Network Protocol
=======================

General
-------

This document describes the TCP/IP protocol used by Bareos to
communicate between the various daemons and services. The definitive
definition of the protocol can be found in src/lib/bsock.h,
src/lib/bnet.c and src/lib/bnet\_server.c.

Bareos’s network protocol is basically a “packet oriented” protocol
built on a standard TCP/IP streams. At the lowest level all packet
transfers are done with read() and write() requests on system sockets.
Pipes are not used as they are considered unreliable for large serial
data transfers between various hosts.

Using the routines described below (bnet\_open, bnet\_write, bnet\_recv,
and bnet\_close) guarantees that the number of bytes you write into the
socket will be received as a single record on the other end regardless
of how many low level write() and read() calls are needed. All data
transferred are considered to be binary data.

bnet and Threads
----------------

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

bnet\_open
----------

To establish a connection to a server, use the subroutine:

BSOCK \*bnet\_open(void \*jcr, char \*host, char \*service, int port,
int \*fatal) bnet\_open(), if successful, returns the Bareos sock
descriptor pointer to be used in subsequent bnet\_send() and
bnet\_read() requests. If not successful, bnet\_open() returns a NULL.
If fatal is set on return, it means that a fatal error occurred and that
you should not repeatedly call bnet\_open(). Any error message will
generally be sent to the JCR.

bnet\_send
----------

To send a packet, one uses the subroutine:

int bnet\_send(BSOCK \*sock) This routine is equivalent to a write()
except that it handles the low level details. The data to be sent is
expected to be in sock-<span>\></span>msg and be
sock-<span>\></span>msglen bytes. To send a packet, bnet\_send() first
writes four bytes in network byte order than indicate the size of the
following data packet. It returns:

     Returns 0 on failure
     Returns 1 on success

In the case of a failure, an error message will be sent to the JCR
contained within the bsock packet.

bnet\_fsend
-----------

This form uses:

int bnet\_fsend(BSOCK \*sock, char \*format, ...) and it allows you to
send a formatted messages somewhat like fprintf(). The return status is
the same as bnet\_send.

is\_bnet\_error
---------------

Fro additional error information, you can call
<span>**is\_bnet\_error(BSOCK \*bsock)**</span> which will return 0 if
there is no error or non-zero if there is an error on the last
transmission.

is\_bnet\_stop
--------------

The <span>**is\_bnet\_stop(BSOCK \*bsock)**</span>
function will return 0 if there no errors and you can continue sending.
It will return non-zero if there are errors or the line is closed (no
more transmissions should be sent).

bnet\_recv
----------

To read a packet, one uses the subroutine:

int bnet\_recv(BSOCK \*sock) This routine is similar to a read() except
that it handles the low level details. bnet\_read() first reads packet
length that follows as four bytes in network byte order. The data is
read into sock-<span>\></span>msg and is sock-<span>\></span>msglen
bytes. If the sock-<span>\></span>msg is not large enough, bnet\_recv()
realloc() the buffer. It will return an error (-2) if maxbytes is less
than the record size sent.

It returns:

   * \>0: number of bytes read
   * 0: on end of file
   * -1: on hard end of file (i.e. network connection close)
   * -2: on error

It should be noted that bnet\_recv() is a blocking read.

bnet\_sig {#sec:bnet_sig}
---------

To send a “signal” from one daemon to another, one uses the subroutine:

int bnet\_sig(BSOCK \*sock, SIGNAL) where SIGNAL is one of the
following:

   * BNET\_EOF - deprecated use BNET\_EOD
   * BNET\_EOD - End of data stream, new data may follow
   * BNET\_EOD\_POLL - End of data and poll all in one
   * BNET\_STATUS - Request full status
   * BNET\_TERMINATE - Conversation terminated, doing close()
   * BNET\_POLL - Poll request, I’m hanging on a read
   * BNET\_HEARTBEAT - Heartbeat Response requested
   * BNET\_HB\_RESPONSE - Only response permitted to HB
   * BNET\_PROMPT - Prompt for UA

bnet\_strerror
--------------

Returns a formated string corresponding to the last error that occurred.

bnet\_close
-----------

The connection with the server remains open until closed by the
subroutine:

void bnet\_close(BSOCK \*sock)

Becoming a Server
-----------------

The bnet\_open() and bnet\_close() routines described above are used on
the client side to establish a connection and terminate a connection
with the server. To become a server (i.e. wait for a connection from a
client), use the routine <span>**bnet\_thread\_server**</span>. The
calling sequence is a bit complicated, please refer to the code in
bnet\_server.c and the code at the beginning of each daemon as examples
of how to call it.

Higher Level Conventions
------------------------

Within Bareos, we have established the convention that any time a single
record is passed, it is sent with bnet\_send() and read with
bnet\_recv(). Thus the normal exchange between the server (S) and the
client (C) are:

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

    while ((n=bnet_recv(bsock)) > 0) {
       act on request
    }
    if (n < 0)
       error

The client will perform the following:

    bnet_send(bsock);
    bnet_send(bsock);
    ...
    bnet_sig(bsock, BNET_EOD);

Thus the client will send multiple packets and signal to the server when
all the packets have been sent by sending a zero length record.
