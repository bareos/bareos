.. _DaemonProtocol:

Daemon Protocol
===============

General
-------

This document describes the protocols used between the various daemons.
As Bareos has developed, it has become quite out of date. The general
idea still holds true, but the details of the fields for each command,
and indeed the commands themselves have changed considerably.

It is intended to be a technical discussion of the general daemon
protocols and as such is not targeted at end users but rather at
developers and system administrators that want or need to know more of
the working details of **Bareos**.

Low Level Network Protocol
--------------------------

At the lowest level, the network protocol is handled by **BSOCK**
packets which contain a lot of information about the status of the
network connection: who is at the other end, etc. Each basic **Bareos**
network read or write actually consists of two low level network
read/writes. The first write always sends four bytes of data in machine
independent byte order. If data is to follow, the first four bytes are a
positive non-zero integer indicating the length of the data that follow
in the subsequent write. If the four byte integer is zero or negative,
it indicates a special request, a sort of network signaling capability.
In this case, no data packet will follow. The low level BSOCK routines
expect that only a single thread is accessing the socket at a time. It
is advised that multiple threads do not read/write the same socket. If
you must do this, you must provide some sort of locking mechanism. It
would not be appropriate for efficiency reasons to make every call to
the BSOCK routines lock and unlock the packet.

General Daemon Protocol
-----------------------

In general, all the daemons follow the following global rules. There may
be exceptions depending on the specific case. Normally, one daemon will
be sending commands to another daemon (specifically, the Director to the
Storage daemon and the Director to the File daemon).

-  Commands are always ASCII commands that are upper/lower case
   dependent as well as space sensitive.

-  All binary data is converted into ASCII (either with printf
   statements or using base64 encoding).

-  All responses to commands sent are always prefixed with a return
   numeric code where codes in the 1000’s are reserved for the Director,
   the 2000’s are reserved for the File daemon, and the 3000’s are
   reserved for the Storage daemon.

-  Any response that is not prefixed with a numeric code is a command
   (or subcommand if you like) coming from the other end. For example,
   while the Director is corresponding with the Storage daemon, the
   Storage daemon can request Catalog services from the Director. This
   convention permits each side to send commands to the other daemon
   while simultaneously responding to commands.

-  Any response that is of zero length, depending on the context, either
   terminates the data stream being sent or terminates command mode
   prior to closing the connection.

-  Any response that is of negative length is a special sign that
   normally requires a response. For example, during data transfer from
   the File daemon to the Storage daemon, normally the File daemon sends
   continuously without intervening reads. However, periodically, the
   File daemon will send a packet of length -1 indicating that the
   current data stream is complete and that the Storage daemon should
   respond to the packet with an OK, ABORT JOB, PAUSE, etc. This permits
   the File daemon to efficiently send data while at the same time
   occasionally “polling” the Storage daemon for his status or any
   special requests.

   Currently, these negative lengths are specific to the daemon, but
   shortly, the range 0 to -999 will be standard daemon wide signals,
   while -1000 to -1999 will be for Director user, -2000 to -2999 for
   the File daemon, and -3000 to -3999 for the Storage daemon.

The Protocol Used Between the Director and the Storage Daemon
-------------------------------------------------------------

Before sending commands to the File daemon, the Director opens a Message
channel with the Storage daemon, identifies itself and presents its
password. If the password check is OK, the Storage daemon accepts the
Director. The Director then passes the Storage daemon, the JobId to be
run as well as the File daemon authorization (append, read all, or read
for a specific session). The Storage daemon will then pass back to the
Director a enabling key for this JobId that must be presented by the
File daemon when opening the job. Until this process is complete, the
Storage daemon is not available for use by File daemons.

::

    SD: listens
    DR: makes connection
    DR: Hello <Director-name> calling <password>
    SD: 3000 OK Hello
    DR: JobId=nnn Allow=(append, read) Session=(*, SessionId)
                        (Session not implemented yet)
    SD: 3000 OK Job Authorization=<password>
    DR: use device=<device-name> media_type=<media-type>
            pool_name=<pool-name> pool_type=<pool_type>
    SD: 3000 OK use device

For the Director to be authorized, the <Director-name> and the
<password> must match the values in one of the Storage daemon’s Director
resources (there may be several Directors that can access a single
Storage daemon).

The Protocol Used Between the Director and the File Daemon
----------------------------------------------------------

A typical conversation might look like the following:

::

    FD: listens
    DR: makes connection
    DR: Hello <Director-name> calling <password>
    FD: 2000 OK Hello
    DR: JobId=nnn Authorization=<password>
    FD: 2000 OK Job
    DR: storage address = <Storage daemon address> port = <port-number>
              name = <DeviceName> mediatype = <MediaType>
    FD: 2000 OK storage
    DR: include
    DR: <directory1>
    DR: <directory2>
        ...
    DR: Null packet
    FD: 2000 OK include
    DR: exclude
    DR: <directory1>
    DR: <directory2>
        ...
    DR: Null packet
    FD: 2000 OK exclude
    DR: full
    FD: 2000 OK full
    DR: save
    FD: 2000 OK save
    FD: Attribute record for each file as sent to the
        Storage daemon (described above).
    FD: Null packet
    FD: <append close responses from Storage daemon>
        e.g.
        3000 OK Volumes = <number of volumes>
        3001 Volume = <volume-id> <start file> <start block>
             <end file> <end block> <volume session-id>
        3002 Volume data = <date/time of last write> <Number bytes written>
             <number errors>
        ... additional Volume / Volume data pairs for volumes 2 .. n
    FD: Null packet
    FD: close socket

The Save Protocol Between the File Daemon and the Storage Daemon
----------------------------------------------------------------

Once the Director has send a **save** command to the File daemon, the
File daemon will contact the Storage daemon to begin the save.

In what follows: FD: refers to information set via the network from the
File daemon to the Storage daemon, and SD: refers to information set
from the Storage daemon to the File daemon.

Command and Control Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Command and control information is exchanged in human readable ASCII
commands.

::

    FD: listens
    SD: makes connection
    FD: append open session = <JobId> [<password>]
    SD: 3000 OK ticket = <number>
    FD: append data <ticket-number>
    SD: 3000 OK data address = <IPaddress> port = <port>

Data Information
~~~~~~~~~~~~~~~~

The Data information consists of the file attributes and data to the
Storage daemon. For the most part, the data information is sent one way:
from the File daemon to the Storage daemon. This allows the File daemon
to transfer information as fast as possible without a lot of handshaking
and network overhead.

However, from time to time, the File daemon needs to do a sort of
checkpoint of the situation to ensure that everything is going well with
the Storage daemon. To do so, the File daemon sends a packet with a
negative length indicating that he wishes the Storage daemon to respond
by sending a packet of information to the File daemon. The File daemon
then waits to receive a packet from the Storage daemon before
continuing.

All data sent are in binary format except for the header packet, which
is in ASCII. There are two packet types used data transfer mode: a
header packet, the contents of which are known to the Storage daemon,
and a data packet, the contents of which are never examined by the
Storage daemon.

The first data packet to the Storage daemon will be an ASCII header
packet consisting of the following data.

<File-Index> <Stream-Id> <Info> where **<File-Index>** is a sequential
number beginning from one that increments with each file (or directory)
sent.

where **<Stream-Id>** will be 1 for the Attributes record and 2 for
uncompressed File data. 3 is reserved for the MD5 signature for the
file.

where **<Info>** transmit information about the Stream to the Storage
Daemon. It is a character string field where each character has a
meaning. The only character currently defined is 0 (zero), which is
simply a place holder (a no op). In the future, there may be codes
indicating compressed data, encrypted data, etc.

Immediately following the header packet, the Storage daemon will expect
any number of data packets. The series of data packets is terminated by
a zero length packet, which indicates to the Storage daemon that the
next packet will be another header packet. As previously mentioned, a
negative length packet is a request for the Storage daemon to
temporarily enter command mode and send a reply to the File daemon. Thus
an actual conversation might contain the following exchanges:

::

    FD: <1 1 0> (header packet)
    FD: <data packet containing file-attributes>
    FD: Null packet
    FD: <1 2 0>
    FD: <multiple data packets containing the file data>
    FD: Packet length = -1
    SD: 3000 OK
    FD: <2 1 0>
    FD: <data packet containing file-attributes>
    FD: Null packet
    FD: <2 2 0>
    FD: <multiple data packets containing the file data>
    FD: Null packet
    FD: Null packet
    FD: append end session <ticket-number>
    SD: 3000 OK end
    FD: append close session <ticket-number>
    SD: 3000 OK Volumes = <number of volumes>
    SD: 3001 Volume = <volumeid> <start file> <start block>
         <end file> <end block> <volume session-id>
    SD: 3002 Volume data = <date/time of last write> <Number bytes written>
         <number errors>
    SD: ... additional Volume / Volume data pairs for
         volumes 2 .. n
    FD: close socket

The information returned to the File daemon by the Storage daemon in
response to the **append close session** is transmit in turn to the
Director.
