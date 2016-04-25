Storage Daemon Design
=====================

This chapter is intended to be a technical discussion of the Storage
daemon services and as such is not targeted at end users but rather at
developers and system administrators that want or need to know more of
the working details of <span>**Bareos**</span>.

This document is somewhat out of date.

SD Design Introduction
----------------------

The Bareos Storage daemon provides storage resources to a Bareos
installation. An individual Storage daemon is associated with a physical
permanent storage device (for example, a tape drive, CD writer, tape
changer or jukebox, etc.), and may employ auxiliary storage resources
(such as space on a hard disk file system) to increase performance
and/or optimize use of the permanent storage medium.

Any number of storage daemons may be run on a given machine; each
associated with an individual storage device connected to it, and Bareos
operations may employ storage daemons on any number of hosts connected
by a network, local or remote. The ability to employ remote storage
daemons (with appropriate security measures) permits automatic off-site
backup, possibly to publicly available backup repositories.

SD Development Outline
----------------------

In order to provide a high performance backup and restore solution that
scales to very large capacity devices and networks, the storage daemon
must be able to extract as much performance from the storage device and
network with which it interacts. In order to accomplish this, storage
daemons will eventually have to sacrifice simplicity and painless
portability in favor of techniques which improve performance. My goal in
designing the storage daemon protocol and developing the initial
prototype storage daemon is to provide for these additions in the
future, while implementing an initial storage daemon which is very
simple and portable to almost any POSIX-like environment. This original
storage daemon (and its evolved descendants) can serve as a portable
solution for non-demanding backup requirements (such as single servers
of modest size, individual machines, or small local networks), while
serving as the starting point for development of higher performance
configurable derivatives which use techniques such as POSIX threads,
shared memory, asynchronous I/O, buffering to high-speed intermediate
media, and support for tape changers and jukeboxes.

SD Connections and Sessions
---------------------------

A client connects to a storage server by initiating a conventional TCP
connection. The storage server accepts the connection unless its maximum
number of connections has been reached or the specified host is not
granted access to the storage server. Once a connection has been opened,
the client may make any number of Query requests, and/or initiate (if
permitted), one or more Append sessions (which transmit data to be
stored by the storage daemon) and/or Read sessions (which retrieve data
from the storage daemon).

Most requests and replies sent across the connection are simple ASCII
strings, with status replies prefixed by a four digit status code for
easier parsing. Binary data appear in blocks stored and retrieved from
the storage. Any request may result in a single-line status reply of
“<span>3201 Notificationpending</span>”, which indicates the client must
send a “Query notification” request to retrieve one or more
notifications posted to it. Once the notifications have been returned,
the client may then resubmit the request which resulted in the 3201
status.

The following descriptions omit common error codes, yet to be defined,
which can occur from most or many requests due to events like media
errors, restarting of the storage daemon, etc. These details will be
filled in, along with a comprehensive list of status codes along with
which requests can produce them in an update to this document.

### SD Append Requests

<span>append open session = <span>\<</span>JobId<span>\></span> [ <span>\<</span>Password<span>\></span> ] </span>
:   A data append session is opened with the Job ID given by
    <span>*JobId*</span> with client password (if required) given by
    <span>*Password*</span>. If the session is successfully opened, a
    status of <span>3000 OK</span> is returned with a
    “<span>ticket = </span><span>*number*</span>” reply used to identify
    subsequent messages in the session. If too many sessions are open,
    or a conflicting session (for example, a read in progress when
    simultaneous read and append sessions are not permitted), a status
    of “<span>3502 Volume busy</span>” is returned. If no volume is
    mounted, or the volume mounted cannot be appended to, a status of
    “<span>3503 Volume not mounted</span>” is returned.

append data = <span>\<</span>ticket-number<span>\></span> 
:   If the append data is accepted, a status of <span>3000 OK data
    address = <span>\<</span>IPaddress<span>\></span> port =
    <span>\<</span>port<span>\></span></span> is returned, where the
    <span>IPaddress</span> and <span>port</span> specify the IP address
    and port number of the data channel. Error status codes are
    <span>3504 Invalid ticket number</span> and
    <span>3505 Session aborted</span>, the latter of which indicates the
    entire append session has failed due to a daemon or media error.

    Once the File daemon has established the connection to the data
    channel opened by the Storage daemon, it will transfer a header
    packet followed by any number of data packets. The header packet is
    of the form:

    <span>file-index<span>\></span>
    <span>\<</span>stream-id<span>\></span>
    <span>\<</span>info<span>\></span></span>

    The details are specified in the section of this document.

\*append abort session = <span>\<</span>ticket-number<span>\></span> 
:   The open append session with ticket <span>*ticket-number*</span> is
    aborted; any blocks not yet written to permanent media are
    discarded. Subsequent attempts to append data to the session will
    receive an error status of <span>3505Session aborted</span>.

append end session = <span>\<</span>ticket-number<span>\></span> 
:   The open append session with ticket <span>*ticket-number*</span> is
    marked complete; no further blocks may be appended. The storage
    daemon will give priority to saving any buffered blocks from this
    session to permanent media as soon as possible.

append close session = <span>\<</span>ticket-number<span>\></span> 
:   The append session with ticket <span>*ticket*</span> is closed. This
    message does not receive an <span>3000 OK</span> reply until all of
    the content of the session are stored on permanent media, at which
    time said reply is given, followed by a list of volumes, from first
    to last, which contain blocks from the session, along with the first
    and last file and block on each containing session data and the
    volume session key identifying data from that session in lines with
    the following format:

    <span>Volume-id<span>\></span>
    <span>\<</span>start-file<span>\></span>
    <span>\<</span>start-block<span>\></span>
    <span>\<</span>end-file<span>\></span>
    <span>\<</span>end-block<span>\></span>
    <span>\<</span>volume-session-id<span>\></span></span>where
    <span>*Volume-id*</span> is the volume label,
    <span>*start-file*</span> and <span>*start-block*</span> are the
    file and block containing the first data from that session on the
    volume, <span>*end-file*</span> and <span>*end-block*</span> are the
    file and block with the last data from the session on the volume and
    <span>*volume-session-id*</span> is the volume session ID for blocks
    from the session stored on that volume.

### SD Read Requests

Read open session = <span>\<</span>JobId<span>\></span> <span>\<</span>Volume-id<span>\></span> <span>\<</span>start-file<span>\></span> <span>\<</span>start-block<span>\></span> <span>\<</span>end-file<span>\></span> <span>\<</span>end-block<span>\></span> <span>\<</span>volume-session-id<span>\></span> <span>\<</span>password<span>\></span> 
:   where <span>*Volume-id*</span> is the volume label,
    <span>*start-file*</span> and <span>*start-block*</span> are the
    file and block containing the first data from that session on the
    volume, <span>*end-file*</span> and <span>*end-block*</span> are the
    file and block with the last data from the session on the volume and
    <span>*volume-session-id*</span> is the volume session ID for blocks
    from the session stored on that volume.

    If the session is successfully opened, a status of

    <span>\`\`</span>

    is returned with a reply used to identify subsequent messages in the
    session. If too many sessions are open, or a conflicting session
    (for example, an append in progress when simultaneous read and
    append sessions are not permitted), a status of
    ”<span>3502 Volume busy</span>“ is returned. If no volume is
    mounted, or the volume mounted cannot be appended to, a status of
    ”<span>3503 Volume not mounted</span>“ is returned. If no block with
    the given volume session ID and the correct client ID number appears
    in the given first file and block for the volume, a status of
    ”<span>3505 Session notfound</span>\`\` is returned.

Read data = <span>\<</span>Ticket<span>\></span> <span>\></span> <span>\<</span>Block<span>\></span> 
:   The specified Block of data from open read session with the
    specified Ticket number is returned, with a status of
    <span>3000 OK</span> followed by a
    ”<span>Length = </span><span>*size*</span>“ line giving the length
    in bytes of the block data which immediately follows. Blocks must be
    retrieved in ascending order, but blocks may be skipped. If a block
    number greater than the largest stored on the volume is requested, a
    status of ”<span>3201 End of volume</span>“ is returned. If a block
    number greater than the largest in the file is requested, a status
    of ”<span>3401 End of file</span>\`\` is returned.

Read close session = <span>\<</span>Ticket<span>\></span> 
:   The read session with Ticket number is closed. A read session may be
    closed at any time; you needn’t read all its blocks before closing
    it.

<span>*by January 30th, MM *</span>

SD Data Structures
------------------

In the Storage daemon, there is a Device resource (i.e. from conf file)
that describes each physical device. When the physical device is used it
is controled by the DEVICE structure (defined in dev.h), and typically
refered to as dev in the C++ code. Anyone writing or reading a physical
device must ultimately get a lock on the DEVICE structure – this
controls the device. However, multiple Jobs (defined by a JCR structure
src/jcr.h) can be writing a physical DEVICE at the same time (of course
they are sequenced by locking the DEVICE structure). There are a lot of
job dependent “device” variables that may be different for each Job such
as spooling (one job may spool and another may not, and when a job is
spooling, it must have an i/o packet open, each job has its own record
and block structures, ...), so there is a device control record or DCR
that is the primary way of interfacing to the physical device. The DCR
contains all the job specific data as well as a pointer to the Device
resource (DEVRES structure) and the physical DEVICE structure.

Now if a job is writing to two devices (it could be writing two separate
streams to the same device), it must have two DCRs. Today, the code only
permits one. This won’t be hard to change, but it is new code.

Today three jobs (threads), two physical devices each job writes to only
one device:

      Job1 -> DCR1 -> DEVICE1
      Job2 -> DCR2 -> DEVICE1
      Job3 -> DCR3 -> DEVICE2

To be implemented three jobs, three physical devices, but job1 is
writing simultaneously to three devices:

      Job1 -> DCR1 -> DEVICE1
              -> DCR4 -> DEVICE2
              -> DCR5 -> DEVICE3
      Job2 -> DCR2 -> DEVICE1
      Job3 -> DCR3 -> DEVICE2

      Job = job control record
      DCR = Job contorl data for a specific device
      DEVICE = Device only control data
