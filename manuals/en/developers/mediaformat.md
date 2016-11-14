Storage Media Output Format
===========================

General
-------

This document describes the media format written by the Storage daemon.
The Storage daemon reads and writes in units of blocks. Blocks contain
records. Each block has a block header followed by records, and each
record has a record header followed by record data.

This chapter is intended to be a technical discussion of the Media
Format and as such is not targeted at end users but rather at developers
and system administrators that want or need to know more of the working
details of <span>**Bareos**</span>.

Definitions
-----------

Block
:   A block represents the primitive unit of information that the
    Storage daemon reads and writes to a physical device. Normally, for
    a tape device, it will be the same as a tape block. The Storage
    daemon always reads and writes blocks. A block consists of block
    header information followed by records. Clients of the Storage
    daemon (the File daemon) normally never see blocks. However, some of
    the Storage tools (bls, bscan, bextract, ...) may be use block
    header information. In older Bareos tape versions, a block could
    contain records (see record definition below) from multiple jobs.
    However, all blocks currently written by Bareos are block level
    BB02, and a given block contains records for only a single job.
    Different jobs simply have their own private blocks that are
    intermingled with the other blocks from other jobs on the Volume
    (previously the records were intermingled within the blocks). Having
    only records from a single job in any give block permitted moving
    the VolumeSessionId and VolumeSessionTime (see below) from each
    record heading to the Block header. This has two advantages: 1. a
    block can be quickly rejected based on the contents of the header
    without reading all the records. 2. because there is on the average
    more than one record per block, less data is written to the Volume
    for each job.

Record
:   A record consists of a Record Header, which is managed by the
    Storage daemon and Record Data, which is the data received from the
    Client. A record is the primitive unit of information sent to and
    from the Storage daemon by the Client (File daemon) programs. The
    details are described below.

JobId
:   A number assigned by the Director daemon for a particular job. This
    number will be unique for that particular Director (Catalog). The
    daemons use this number to keep track of individual jobs. Within the
    Storage daemon, the JobId may not be unique if several Directors are
    accessing the Storage daemon simultaneously.

Session
:   A Session is a concept used in the Storage daemon corresponds one to
    one to a Job with the exception that each session is uniquely
    identified within the Storage daemon by a unique
    SessionId/SessionTime pair (see below).

VolSessionId
:   A unique number assigned by the Storage daemon to a particular
    session (Job) it is having with a File daemon. This number by itself
    is not unique to the given Volume, but with the VolSessionTime, it
    is unique.

VolSessionTime
:   A unique number assigned by the Storage daemon to a particular
    Storage daemon execution. It is actually the Unix time\_t value of
    when the Storage daemon began execution cast to a 32 bit unsigned
    integer. The combination of the <span>**VolSessionId**</span> and
    the <span>**VolSessionTime**</span> for a given Storage daemon is
    guaranteed to be unique for each Job (or session).

FileIndex
:   A sequential number beginning at one assigned by the File daemon to
    the files within a job that are sent to the Storage daemon for
    backup. The Storage daemon ensures that this number is greater than
    zero and sequential. Note, the Storage daemon uses negative
    FileIndexes to flag Session Start and End Labels as well as End of
    Volume Labels. Thus, the combination of VolSessionId,
    VolSessionTime, and FileIndex uniquely identifies the records for a
    single file written to a Volume.

Stream
:   While writing the information for any particular file to the Volume,
    there can be any number of distinct pieces of information about that
    file, e.g. the attributes, the file data, ... The Stream indicates
    what piece of data it is, and it is an arbitrary number assigned by
    the File daemon to the parts (Unix attributes, Win32 attributes,
    data, compressed data, ...) of a file that are sent to the Storage
    daemon. The Storage daemon has no knowledge of the details of a
    Stream; it simply represents a numbered stream of bytes. The data
    for a given stream may be passed to the Storage daemon in single
    record, or in multiple records.

Block Header
:   A block header consists of a block identification (“BB02”), a block
    length in bytes (typically 64,512) a checksum, and sequential block
    number. Each block starts with a Block Header and is followed by
    Records. Current block headers also contain the VolSessionId and
    VolSessionTime for the records written to that block.

Record Header
:   A record header contains the Volume Session Id, the Volume Session
    Time, the FileIndex, the Stream, and the size of the data record
    which follows. The Record Header is always immediately followed by a
    Data Record if the size given in the Header is greater than zero.
    Note, for Block headers of level BB02 (version 1.27 and later), the
    Record header as written to tape does not contain the Volume Session
    Id and the Volume Session Time as these two fields are stored in the
    BB02 Block header. The in-memory record header does have those
    fields for convenience.

Data Record
:   A data record consists of a binary stream of bytes and is always
    preceded by a Record Header. The details of the meaning of the
    binary stream of bytes are unknown to the Storage daemon, but the
    Client programs (File daemon) defines and thus knows the details of
    each record type.

Volume Label
:   A label placed by the Storage daemon at the beginning of each
    storage volume. It contains general information about the volume. It
    is written in Record format. The Storage daemon manages Volume
    Labels, and if the client wants, he may also read them.

Begin Session Label
:   The Begin Session Label is a special record placed by the Storage
    daemon on the storage medium as the first record of an append
    session job with a File daemon. This record is useful for finding
    the beginning of a particular session (Job), since no records with
    the same VolSessionId and VolSessionTime will precede this record.
    This record is not normally visible outside of the Storage daemon.
    The Begin Session Label is similar to the Volume Label except that
    it contains additional information pertaining to the Session.

End Session Label
:   The End Session Label is a special record placed by the Storage
    daemon on the storage medium as the last record of an append session
    job with a File daemon. The End Session Record is distinguished by a
    FileIndex with a value of minus two (-2). This record is useful for
    detecting the end of a particular session since no records with the
    same VolSessionId and VolSessionTime will follow this record. This
    record is not normally visible outside of the Storage daemon. The
    End Session Label is similar to the Volume Label except that it
    contains additional information pertaining to the Session.

Storage Daemon File Output Format
---------------------------------

The file storage and tape storage formats are identical except that tape
records are by default blocked into blocks of 64,512 bytes, except for
the last block, which is the actual number of bytes written rounded up
to a multiple of 1024 whereas the last record of file storage is not
rounded up. The default block size of 64,512 bytes may be overridden by
the user (some older tape drives only support block sizes of 32K). Each
Session written to tape is terminated with an End of File mark (this
will be removed later). Sessions written to file are simply appended to
the end of the file.

Overall Format
--------------

A Bareos output file consists of Blocks of data. Each block contains a
block header followed by records. Each record consists of a record
header followed by the record data. The first record on a tape will
always be the Volume Label Record.

No Record Header will be split across Bareos blocks. However, Record
Data may be split across any number of Bareos blocks. Obviously this
will not be the case for the Volume Label which will always be smaller
than the Bareos Block size.

To simplify reading tapes, the Start of Session (SOS) and End of Session
(EOS) records are never split across blocks. If this is about to happen,
Bareos will write a short block before writing the session record
(actually, the SOS record should always be the first record in a block,
excepting perhaps the Volume label).

Due to hardware limitations, the last block written to the tape may not
be fully written. If your drive permits backspace record, Bareos will
backup over the last record written on the tape, re-read it and verify
that it was correctly written.

When a new tape is mounted Bareos will write the full contents of the
partially written block to the new tape ensuring that there is no loss
of data. When reading a tape, Bareos will discard any block that is not
totally written, thus ensuring that there is no duplication of data. In
addition, since Bareos blocks are sequentially numbered within a Job, it
is easy to ensure that no block is missing or duplicated.

Serialization
-------------

All Block Headers, Record Headers, and Label Records are written using
Bareos’s serialization routines. These routines guarantee that the data
is written to the output volume in a machine independent format.

Block Header
------------

The format of the Block Header (Bacula version 1.27 and later) is:

       uint32_t CheckSum;                /* Block check sum */
       uint32_t BlockSize;               /* Block byte size including the header */
       uint32_t BlockNumber;             /* Block number */
       char ID[4] = "BB02";              /* Identification and block level */
       uint32_t VolSessionId;            /* Session Id for Job */
       uint32_t VolSessionTime;          /* Session Time for Job */

The Block header is a fixed length and fixed format and is followed by
Record Headers and Record Data. The CheckSum field is a 32 bit checksum
of the block data and the block header but not including the CheckSum
field. The Block Header is always immediately followed by a Record
Header. If the tape is damaged, a Bareos utility will be able to recover
as much information as possible from the tape by recovering blocks which
are valid. The Block header is written using the Bareos serialization
routines and thus is guaranteed to be in machine independent format. See
below for version 2 of the block header.

Record Header
-------------

Each binary data record is preceded by a Record Header. The Record
Header is fixed length and fixed format, whereas the binary data record
is of variable length. The Record Header is written using the Bareos
serialization routines and thus is guaranteed to be in machine
independent format.

The format of the Record Header (Bacula version 1.27 or later) is:

      int32_t FileIndex;   /* File index supplied by File daemon */
      int32_t Stream;      /* Stream number supplied by File daemon */
      uint32_t DataSize;   /* size of following data record in bytes */

This record is followed by the binary Stream data of DataSize bytes,
followed by another Record Header record and the binary stream data. For
the definitive definition of this record, see record.h in the src/stored
directory.

Additional notes on the above:

The <span>**VolSessionId**</span> 
:   is a unique sequential number that is assigned by the Storage Daemon
    to a particular Job. This number is sequential since the start of
    execution of the daemon.

The <span>**VolSessionTime**</span> 
:   is the time/date that the current execution of the Storage Daemon
    started. It assures that the combination of VolSessionId and
    VolSessionTime is unique for every jobs written to the tape, even if
    there was a machine crash between two writes.

The <span>**FileIndex**</span> 
:   is a sequential file number within a job. The Storage daemon
    requires this index to be greater than zero and sequential. Note,
    however, that the File daemon may send multiple Streams for the same
    FileIndex. In addition, the Storage daemon uses negative FileIndices
    to hold the Begin Session Label, the End Session Label, and the End
    of Volume Label.

The <span>**Stream**</span> 
:   is defined by the File daemon and is used to identify separate parts
    of the data saved for each file (Unix attributes, Win32 attributes,
    file data, compressed file data, sparse file data, ...). The Storage
    Daemon has no idea of what a Stream is or what it contains except
    that the Stream is required to be a positive integer. Negative
    Stream numbers are used internally by the Storage daemon to indicate
    that the record is a continuation of the previous record (the
    previous record would not entirely fit in the block).

    For Start Session and End Session Labels (where the FileIndex is
    negative), the Storage daemon uses the Stream field to contain the
    JobId. The current stream definitions are:

        #define STREAM_UNIX_ATTRIBUTES    1    /* Generic Unix attributes */
        #define STREAM_FILE_DATA          2    /* Standard uncompressed data */
        #define STREAM_MD5_SIGNATURE      3    /* MD5 signature for the file */
        #define STREAM_GZIP_DATA          4    /* GZip compressed file data */
        /* Extended Unix attributes with Win32 Extended data.  Deprecated. */
        #define STREAM_UNIX_ATTRIBUTES_EX 5    /* Extended Unix attr for Win32 EX */
        #define STREAM_SPARSE_DATA        6    /* Sparse data stream */
        #define STREAM_SPARSE_GZIP_DATA   7
        #define STREAM_PROGRAM_NAMES      8    /* program names for program data */
        #define STREAM_PROGRAM_DATA       9    /* Data needing program */
        #define STREAM_SHA1_SIGNATURE    10    /* SHA1 signature for the file */
        #define STREAM_WIN32_DATA        11    /* Win32 BackupRead data */
        #define STREAM_WIN32_GZIP_DATA   12    /* Gzipped Win32 BackupRead data */
        #define STREAM_MACOS_FORK_DATA   13    /* Mac resource fork */
        #define STREAM_HFSPLUS_ATTRIBUTES 14   /* Mac OS extra attributes */
        #define STREAM_UNIX_ATTRIBUTES_ACCESS_ACL 15 /* Standard ACL attributes on UNIX */
        #define STREAM_UNIX_ATTRIBUTES_DEFAULT_ACL 16 /* Default ACL attributes on UNIX */

The <span>**DataSize**</span> 
:   is the size in bytes of the binary data record that follows the
    Session Record header. The Storage Daemon has no idea of the actual
    contents of the binary data record. For standard Unix files, the
    data record typically contains the file attributes or the file data.
    For a sparse file the first 64 bits of the file data contains the
    storage address for the data block.

The Record Header is never split across two blocks. If there is not
enough room in a block for the full Record Header, the block is padded
to the end with zeros and the Record Header begins in the next block.
The data record, on the other hand, may be split across multiple blocks
and even multiple physical volumes. When a data record is split, the
second (and possibly subsequent) piece of the data is preceded by a new
Record Header. Thus each piece of data is always immediately preceded by
a Record Header. When reading a record, if Bareos finds only part of the
data in the first record, it will automatically read the next record and
concatenate the data record to form a full data record.

Version BB02 Block Header
-------------------------

Each session or Job has its own private block. As a consequence, the
SessionId and SessionTime are written once in each Block Header and not
in the Record Header. So, the second and current version of the Block
Header BB02 is:

       uint32_t CheckSum;        /* Block check sum */
       uint32_t BlockSize;       /* Block byte size including the header */
       uint32_t BlockNumber;     /* Block number */
       char ID[4] = "BB02";      /* Identification and block level */
       uint32_t VolSessionId;    /* Applies to all records  */
       uint32_t VolSessionTime;  /*   contained in this block */

As with the previous version, the BB02 Block header is a fixed length
and fixed format and is followed by Record Headers and Record Data. The
CheckSum field is a 32 bit CRC checksum of the block data and the block
header but not including the CheckSum field. The Block Header is always
immediately followed by a Record Header. If the tape is damaged, a
Bareos utility will be able to recover as much information as possible
from the tape by recovering blocks which are valid. The Block header is
written using the Bareos serialization routines and thus is guaranteed
to be in machine independent format.

Version 2 Record Header
-----------------------

Version 2 Record Header is written to the medium when using Version BB02
Block Headers. The memory representation of the record is identical to
the old BB01 Record Header, but on the storage medium, the first two
fields, namely VolSessionId and VolSessionTime are not written. The
Block Header is filled with these values when the First user record is
written (i.e. non label record) so that when the block is written, it
will have the current and unique VolSessionId and VolSessionTime. On
reading each record from the Block, the VolSessionId and VolSessionTime
is filled in the Record Header from the Block Header.

Volume Label Format
-------------------

Tape volume labels are created by the Storage daemon in response to a
<span>**label**</span> command given to the Console program, or
alternatively by the <span>**btape**</span> program. created. Each
volume is labeled with the following information using the Bareos
serialization routines, which guarantee machine byte order independence.

For Bacula versions 1.27 and later, the Volume Label Format is:

      char Id[32];              /* Bacula 1.0 Immortal\n */
      uint32_t VerNum;          /* Label version number */
      /* VerNum 11 and greater Bacula 1.27 and later */
      btime_t   label_btime;    /* Time/date tape labeled */
      btime_t   write_btime;    /* Time/date tape first written */
      /* The following are 0 in VerNum 11 and greater */
      float64_t write_date;     /* Date this label written */
      float64_t write_time;     /* Time this label written */
      char VolName[128];        /* Volume name */
      char PrevVolName[128];    /* Previous Volume Name */
      char PoolName[128];       /* Pool name */
      char PoolType[128];       /* Pool type */
      char MediaType[128];      /* Type of this media */
      char HostName[128];       /* Host name of writing computer */
      char LabelProg[32];       /* Label program name */
      char ProgVersion[32];     /* Program version */
      char ProgDate[32];        /* Program build date/time */

Note, the LabelType (Volume Label, Volume PreLabel, Session Start Label,
...) is stored in the record FileIndex field of the Record Header and
does not appear in the data part of the record.

Session Label
-------------

The Session Label is written at the beginning and end of each session as
well as the last record on the physical medium. It has the following
binary format:

      char Id[32];              /* Bacula/Bareos Immortal ... */
      uint32_t VerNum;          /* Label version number */
      uint32_t JobId;           /* Job id */
      uint32_t VolumeIndex;     /* sequence no of vol */
      /* Prior to VerNum 11 */
      float64_t write_date;     /* Date this label written */
      /* VerNum 11 and greater */
      btime_t   write_btime;    /* time/date record written */
      /* The following is zero VerNum 11 and greater */
      float64_t write_time;    /* Time this label written */
      char PoolName[128];        /* Pool name */
      char PoolType[128];        /* Pool type */
      char JobName[128];         /* base Job name */
      char ClientName[128];
      /* Added in VerNum 10 */
      char Job[128];             /* Unique Job name */
      char FileSetName[128];     /* FileSet name */
      uint32_t JobType;
      uint32_t JobLevel;

In addition, the EOS label contains:

      /* The remainder are part of EOS label only */
      uint32_t JobFiles;
      uint64_t JobBytes;
      uint32_t start_block;
      uint32_t end_block;
      uint32_t start_file;
      uint32_t end_file;
      uint32_t JobErrors;

In addition, for VerNum greater than 10, the EOS label contains (in
addition to the above):

      uint32_t JobStatus          /* Job termination code */

: Note, the LabelType (Volume Label, Volume PreLabel, Session Start
Label, ...) is stored in the record FileIndex field and does not appear
in the data part of the record. Also, the Stream field of the Record
Header contains the JobId. This permits quick filtering without actually
reading all the session data in many cases.

Overall Storage Format
----------------------

                   Current Bacula/Bareos Tape Format
                         6 June 2001
               Version BB02 added 28 September 2002
               Version BB01 is the old deprecated format.
       A Bareos tape is composed of tape Blocks.  Each block
         has a Block header followed by the block data. Block
         Data consists of Records. Records consist of Record
         Headers followed by Record Data.
       :=======================================================:
       |                                                       |
       |                 Block Header (24 bytes)               |
       |                                                       |
       |-------------------------------------------------------|
       |                                                       |
       |              Record Header (12 bytes)                 |
       |                                                       |
       |-------------------------------------------------------|
       |                                                       |
       |                  Record Data                          |
       |                                                       |
       |-------------------------------------------------------|
       |                                                       |
       |              Record Header (12 bytes)                 |
       |                                                       |
       |-------------------------------------------------------|
       |                                                       |
       |                       ...                             |
       Block Header: the first item in each block. The format is
         shown below.
       Partial Data block: occurs if the data from a previous
         block spills over to this block (the normal case except
         for the first block on a tape). However, this partial
             data block is always preceded by a record header.
       Record Header: identifies the Volume Session, the Stream
         and the following Record Data size. See below for format.
       Record data: arbitrary binary data.
                        Block Header Format BB02
       :=======================================================:
       |              CheckSum         (uint32_t)              |
       |-------------------------------------------------------|
       |              BlockSize        (uint32_t)              |
       |-------------------------------------------------------|
       |              BlockNumber      (uint32_t)              |
       |-------------------------------------------------------|
       |              "BB02"           (char [4])              |
       |-------------------------------------------------------|
       |              VolSessionId     (uint32_t)              |
       |-------------------------------------------------------|
       |              VolSessionTime   (uint32_t)              |
       :=======================================================:
       BBO2: Serves to identify the block as a
         Bacula/Bareos block and also servers as a block format identifier
         should we ever need to change the format.
       BlockSize: is the size in bytes of the block. When reading
         back a block, if the BlockSize does not agree with the
         actual size read, Bareos discards the block.
       CheckSum: a checksum for the Block.
       BlockNumber: is the sequential block number on the tape.
       VolSessionId: a unique sequential number that is assigned
                     by the Storage Daemon to a particular Job.
                     This number is sequential since the start
                     of execution of the daemon.
       VolSessionTime: the time/date that the current execution
                     of the Storage Daemon started.  It assures
                     that the combination of VolSessionId and
                     VolSessionTime is unique for all jobs
                     written to the tape, even if there was a
                     machine crash between two writes.
                      Record Header Format BB02
       :=======================================================:
       |              FileIndex        (int32_t)               |
       |-------------------------------------------------------|
       |              Stream           (int32_t)               |
       |-------------------------------------------------------|
       |              DataSize         (uint32_t)              |
       :=======================================================:
       FileIndex: a sequential file number within a job.  The
                     Storage daemon enforces this index to be
                     greater than zero and sequential.  Note,
                     however, that the File daemon may send
                     multiple Streams for the same FileIndex.
                     The Storage Daemon uses negative FileIndices
                     to identify Session Start and End labels
                     as well as the End of Volume labels.
       Stream: defined by the File daemon and is intended to be
                     used to identify separate parts of the data
                     saved for each file (attributes, file data,
                     ...).  The Storage Daemon has no idea of
                     what a Stream is or what it contains.
       DataSize: the size in bytes of the binary data record
                     that follows the Session Record header.
                     The Storage Daemon has no idea of the
                     actual contents of the binary data record.
                     For standard Unix files, the data record
                     typically contains the file attributes or
                     the file data.  For a sparse file
                     the first 64 bits of the data contains
                     the storage address for the data block.
                           Volume Label
       :=======================================================:
       |              Id               (32 bytes)              |
       |-------------------------------------------------------|
       |              VerNum           (uint32_t)              |
       |-------------------------------------------------------|
       |              label_date       (float64_t)             |
       |              label_btime      (btime_t VerNum 11      |
       |-------------------------------------------------------|
       |              label_time       (float64_t)             |
       |              write_btime      (btime_t VerNum 11      |
       |-------------------------------------------------------|
       |              write_date       (float64_t)             |
       |                  0            (float64_t) VerNum 11   |
       |-------------------------------------------------------|
       |              write_time       (float64_t)             |
       |                  0            (float64_t) VerNum 11   |
       |-------------------------------------------------------|
       |              VolName          (128 bytes)             |
       |-------------------------------------------------------|
       |              PrevVolName      (128 bytes)             |
       |-------------------------------------------------------|
       |              PoolName         (128 bytes)             |
       |-------------------------------------------------------|
       |              PoolType         (128 bytes)             |
       |-------------------------------------------------------|
       |              MediaType        (128 bytes)             |
       |-------------------------------------------------------|
       |              HostName         (128 bytes)             |
       |-------------------------------------------------------|
       |              LabelProg        (32 bytes)              |
       |-------------------------------------------------------|
       |              ProgVersion      (32 bytes)              |
       |-------------------------------------------------------|
       |              ProgDate         (32 bytes)              |
       |-------------------------------------------------------|
       :=======================================================:

       Id: 32 byte identifier "Bacula 1.0 immortal\n"
       (old version also recognized:)
       Id: 32 byte identifier "Bacula 0.9 mortal\n"
       LabelType (Saved in the FileIndex of the Header record).
           PRE_LABEL -1    Volume label on unwritten tape
           VOL_LABEL -2    Volume label after tape written
           EOM_LABEL -3    Label at EOM (not currently implemented)
           SOS_LABEL -4    Start of Session label (format given below)
           EOS_LABEL -5    End of Session label (format given below)
       VerNum: 11
       label_date: Julian day tape labeled
       label_time: Julian time tape labeled
       write_date: Julian date tape first used (data written)
       write_time: Julian time tape first used (data written)
       VolName: "Physical" Volume name
       PrevVolName: The VolName of the previous tape (if this tape is
                    a continuation of the previous one).
       PoolName: Pool Name
       PoolType: Pool Type
       MediaType: Media Type
       HostName: Name of host that is first writing the tape
       LabelProg: Name of the program that labeled the tape
       ProgVersion: Version of the label program
       ProgDate: Date Label program built
                           Session Label
       :=======================================================:
       |              Id               (32 bytes)              |
       |-------------------------------------------------------|
       |              VerNum           (uint32_t)              |
       |-------------------------------------------------------|
       |              JobId            (uint32_t)              |
       |-------------------------------------------------------|
       |              write_btime      (btime_t)   VerNum 11   |
       |-------------------------------------------------------|
       |                 0             (float64_t) VerNum 11   |
       |-------------------------------------------------------|
       |              PoolName         (128 bytes)             |
       |-------------------------------------------------------|
       |              PoolType         (128 bytes)             |
       |-------------------------------------------------------|
       |              JobName          (128 bytes)             |
       |-------------------------------------------------------|
       |              ClientName       (128 bytes)             |
       |-------------------------------------------------------|
       |              Job              (128 bytes)             |
       |-------------------------------------------------------|
       |              FileSetName      (128 bytes)             |
       |-------------------------------------------------------|
       |              JobType          (uint32_t)              |
       |-------------------------------------------------------|
       |              JobLevel         (uint32_t)              |
       |-------------------------------------------------------|
       |              FileSetMD5       (50 bytes)   VerNum 11  |
       |-------------------------------------------------------|
               Additional fields in End Of Session Label
       |-------------------------------------------------------|
       |              JobFiles         (uint32_t)              |
       |-------------------------------------------------------|
       |              JobBytes         (uint32_t)              |
       |-------------------------------------------------------|
       |              start_block      (uint32_t)              |
       |-------------------------------------------------------|
       |              end_block        (uint32_t)              |
       |-------------------------------------------------------|
       |              start_file       (uint32_t)              |
       |-------------------------------------------------------|
       |              end_file         (uint32_t)              |
       |-------------------------------------------------------|
       |              JobErrors        (uint32_t)              |
       |-------------------------------------------------------|
       |              JobStatus        (uint32_t) VerNum 11    |
       :=======================================================:
       * => fields deprecated
       Id: 32 byte identifier "Bacula 1.0 immortal\n"
       LabelType (in FileIndex field of Header):
           EOM_LABEL -3     Label at EOM
           SOS_LABEL -4     Start of Session label
           EOS_LABEL -5     End of Session label
       VerNum: 11
       JobId: JobId
       write_btime: Bareos time/date this tape record written
       write_date: Julian date tape this record written - deprecated
       write_time: Julian time tape this record written - deprecated.
       PoolName: Pool Name
       PoolType: Pool Type
       MediaType: Media Type
       ClientName: Name of File daemon or Client writing this session
                   Not used for EOM_LABEL.

Unix File Attributes
--------------------

The Unix File Attributes packet consists of the following:

<span>\<</span>File-Index<span>\></span>
<span>\<</span>Type<span>\></span>
<span>\<</span>Filename<span>\></span>@<span>\<</span>File-Attributes<span>\></span>@<span>\<</span>Link<span>\></span>
@<span>\<</span>Extended-Attributes@<span>\></span> where

@
:   represents a byte containing a binary zero.

FileIndex
:   is the sequential file index starting from one assigned by the File
    daemon.

Type
:   is one of the following:

        #define FT_LNKSAVED   1    /* hard link to file already saved */
        #define FT_REGE       2    /* Regular file but empty */
        #define FT_REG        3    /* Regular file */
        #define FT_LNK        4    /* Soft Link */
        #define FT_DIR        5    /* Directory */
        #define FT_SPEC       6    /* Special file -- chr, blk, fifo, sock */
        #define FT_NOACCESS   7    /* Not able to access */
        #define FT_NOFOLLOW   8    /* Could not follow link */
        #define FT_NOSTAT     9    /* Could not stat file */
        #define FT_NOCHG     10    /* Incremental option, file not changed */
        #define FT_DIRNOCHG  11    /* Incremental option, directory not changed */
        #define FT_ISARCH    12    /* Trying to save archive file */
        #define FT_NORECURSE 13    /* No recursion into directory */
        #define FT_NOFSCHG   14    /* Different file system, prohibited */
        #define FT_NOOPEN    15    /* Could not open directory */
        #define FT_RAW       16    /* Raw block device */
        #define FT_FIFO      17    /* Raw fifo device */

Filename
:   is the fully qualified filename.

File-Attributes
:   consists of the 13 fields of the stat() buffer in ASCII base64
    format separated by spaces. These fields and their meanings are
    shown below. This stat() packet is in Unix format, and MUST be
    provided (constructed) for ALL systems.

Link
:   when the FT code is FT\_LNK or FT\_LNKSAVED, the item in question is
    a Unix link, and this field contains the fully qualified link name.
    When the FT code is not FT\_LNK or FT\_LNKSAVED, this field is null.

Extended-Attributes
:   The exact format of this field is operating system dependent. It
    contains additional or extended attributes of a system dependent
    nature. Currently, this field is used only on WIN32 systems where it
    contains a ASCII base64 representation of the
    WIN32\_FILE\_ATTRIBUTE\_DATA structure as defined by Windows. The
    fields in the base64 representation of this structure are like the
    File-Attributes separated by spaces.

The File-attributes consist of the following:

Stat Name   |  Unix                          | Windows                       | MacOS
------------|--------------------------------|-------------------------------|----------------------
st_dev      |  Device number of filesystem   | Drive number                  | vRefNum
st_ino      |  Inode number                  | Always 0                      | fileID/dirID
st_mode     |  File mode                     | File mode                     | 777 dirs/apps; 666 docs; 444 locked docs
st_nlink    |  Number of links to the file   | Number of link (only on NTFS) | Always 1
st_uid      |  Owner ID                      | Always 0                      | Always 0
st_gid      |  Group ID                      | Always 0                      | Always 0
st_rdev     |  Device ID for special files   | Drive No.                     | Always 0
st_size     |  File size in bytes            | File size in bytes            | Data fork file size in bytes
st_blksize  |  Preferred block size          | Always 0                      | Preferred block size
st_blocks   |  Number of blocks allocated    | Always 0                      | Number of blocks allocated
st_atime    |  Last access time since epoch  | Last access time since epoch  | Last access time -66 years
st_mtime    |  Last modify time since epoch  | Last modify time since epoch  | Last access time -66 years
st_ctime    |  Inode change time since epoch | File create time since epoch  | File create time -66 years

Old Depreciated Tape Format
---------------------------

The format of the Block Header (version 1.26 and earlier) is:

       uint32_t CheckSum;      /* Block check sum */
       uint32_t BlockSize;     /* Block byte size including the header */
       uint32_t BlockNumber;   /* Block number */
       char ID[4] = "BB01";    /* Identification and block level */

The format of the Record Header (version 1.26 or earlier) is:

      uint32_t VolSessionId;    /* Unique ID for this session */
      uint32_t VolSessionTime;  /* Start time/date of session */
      int32_t FileIndex;        /* File index supplied by File daemon */
      int32_t Stream;           /* Stream number supplied by File daemon */
      uint32_t DataSize;        /* size of following data record in bytes */

                   Current Bacula/Bareos Tape Format
                         6 June 2001
               Version BB01 is the old deprecated format.
       A Bareos tape is composed of tape Blocks.  Each block
         has a Block header followed by the block data. Block
         Data consists of Records. Records consist of Record
         Headers followed by Record Data.
       :=======================================================:
       |                                                       |
       |                 Block Header                          |
       |                 (16 bytes version BB01)               |
       |-------------------------------------------------------|
       |                                                       |
       |              Record Header                            |
       |               (20 bytes version BB01)                 |
       |-------------------------------------------------------|
       |                                                       |
       |                  Record Data                          |
       |                                                       |
       |-------------------------------------------------------|
       |                                                       |
       |              Record Header                            |
       |               (20 bytes version BB01)                 |
       |-------------------------------------------------------|
       |                                                       |
       |                       ...                             |
       Block Header: the first item in each block. The format is
         shown below.
       Partial Data block: occurs if the data from a previous
         block spills over to this block (the normal case except
         for the first block on a tape). However, this partial
             data block is always preceded by a record header.
       Record Header: identifies the Volume Session, the Stream
         and the following Record Data size. See below for format.
       Record data: arbitrary binary data.
                        Block Header Format BB01 (deprecated)
       :=======================================================:
       |              CheckSum         (uint32_t)              |
       |-------------------------------------------------------|
       |              BlockSize        (uint32_t)              |
       |-------------------------------------------------------|
       |              BlockNumber      (uint32_t)              |
       |-------------------------------------------------------|
       |              "BB01"           (char [4])              |
       :=======================================================:
       BBO1: Serves to identify the block as a
         Bareos block and also servers as a block format identifier
         should we ever need to change the format.
       BlockSize: is the size in bytes of the block. When reading
         back a block, if the BlockSize does not agree with the
         actual size read, Bareos discards the block.
       CheckSum: a checksum for the Block.
       BlockNumber: is the sequential block number on the tape.
       VolSessionId: a unique sequential number that is assigned
                     by the Storage Daemon to a particular Job.
                     This number is sequential since the start
                     of execution of the daemon.
       VolSessionTime: the time/date that the current execution
                     of the Storage Daemon started.  It assures
                     that the combination of VolSessionId and
                     VolSessionTime is unique for all jobs
                     written to the tape, even if there was a
                     machine crash between two writes.
                      Record Header Format BB01 (deprecated)
       :=======================================================:
       |              VolSessionId     (uint32_t)              |
       |-------------------------------------------------------|
       |              VolSessionTime   (uint32_t)              |
       |-------------------------------------------------------|
       |              FileIndex        (int32_t)               |
       |-------------------------------------------------------|
       |              Stream           (int32_t)               |
       |-------------------------------------------------------|
       |              DataSize         (uint32_t)              |
       :=======================================================:
       VolSessionId: a unique sequential number that is assigned
                     by the Storage Daemon to a particular Job.
                     This number is sequential since the start
                     of execution of the daemon.
       VolSessionTime: the time/date that the current execution
                     of the Storage Daemon started.  It assures
                     that the combination of VolSessionId and
                     VolSessionTime is unique for all jobs
                     written to the tape, even if there was a
                     machine crash between two writes.
       FileIndex: a sequential file number within a job.  The
                     Storage daemon enforces this index to be
                     greater than zero and sequential.  Note,
                     however, that the File daemon may send
                     multiple Streams for the same FileIndex.
                     The Storage Daemon uses negative FileIndices
                     to identify Session Start and End labels
                     as well as the End of Volume labels.
       Stream: defined by the File daemon and is intended to be
                     used to identify separate parts of the data
                     saved for each file (attributes, file data,
                     ...).  The Storage Daemon has no idea of
                     what a Stream is or what it contains.
       DataSize: the size in bytes of the binary data record
                     that follows the Session Record header.
                     The Storage Daemon has no idea of the
                     actual contents of the binary data record.
                     For standard Unix files, the data record
                     typically contains the file attributes or
                     the file data.  For a sparse file
                     the first 64 bits of the data contains
                     the storage address for the data block.
                           Volume Label
       :=======================================================:
       |              Id               (32 bytes)              |
       |-------------------------------------------------------|
       |              VerNum           (uint32_t)              |
       |-------------------------------------------------------|
       |              label_date       (float64_t)             |
       |-------------------------------------------------------|
       |              label_time       (float64_t)             |
       |-------------------------------------------------------|
       |              write_date       (float64_t)             |
       |-------------------------------------------------------|
       |              write_time       (float64_t)             |
       |-------------------------------------------------------|
       |              VolName          (128 bytes)             |
       |-------------------------------------------------------|
       |              PrevVolName      (128 bytes)             |
       |-------------------------------------------------------|
       |              PoolName         (128 bytes)             |
       |-------------------------------------------------------|
       |              PoolType         (128 bytes)             |
       |-------------------------------------------------------|
       |              MediaType        (128 bytes)             |
       |-------------------------------------------------------|
       |              HostName         (128 bytes)             |
       |-------------------------------------------------------|
       |              LabelProg        (32 bytes)              |
       |-------------------------------------------------------|
       |              ProgVersion      (32 bytes)              |
       |-------------------------------------------------------|
       |              ProgDate         (32 bytes)              |
       |-------------------------------------------------------|
       :=======================================================:

       Id: 32 byte identifier "Bacula 1.0 immortal\n"
       (old version also recognized:)
       Id: 32 byte identifier "Bacula 0.9 mortal\n"
       LabelType (Saved in the FileIndex of the Header record).
           PRE_LABEL -1    Volume label on unwritten tape
           VOL_LABEL -2    Volume label after tape written
           EOM_LABEL -3    Label at EOM (not currently implemented)
           SOS_LABEL -4    Start of Session label (format given below)
           EOS_LABEL -5    End of Session label (format given below)
       label_date: Julian day tape labeled
       label_time: Julian time tape labeled
       write_date: Julian date tape first used (data written)
       write_time: Julian time tape first used (data written)
       VolName: "Physical" Volume name
       PrevVolName: The VolName of the previous tape (if this tape is
                    a continuation of the previous one).
       PoolName: Pool Name
       PoolType: Pool Type
       MediaType: Media Type
       HostName: Name of host that is first writing the tape
       LabelProg: Name of the program that labeled the tape
       ProgVersion: Version of the label program
       ProgDate: Date Label program built
                           Session Label
       :=======================================================:
       |              Id               (32 bytes)              |
       |-------------------------------------------------------|
       |              VerNum           (uint32_t)              |
       |-------------------------------------------------------|
       |              JobId            (uint32_t)              |
       |-------------------------------------------------------|
       |             *write_date       (float64_t) VerNum 10   |
       |-------------------------------------------------------|
       |             *write_time       (float64_t) VerNum 10   |
       |-------------------------------------------------------|
       |              PoolName         (128 bytes)             |
       |-------------------------------------------------------|
       |              PoolType         (128 bytes)             |
       |-------------------------------------------------------|
       |              JobName          (128 bytes)             |
       |-------------------------------------------------------|
       |              ClientName       (128 bytes)             |
       |-------------------------------------------------------|
       |              Job              (128 bytes)             |
       |-------------------------------------------------------|
       |              FileSetName      (128 bytes)             |
       |-------------------------------------------------------|
       |              JobType          (uint32_t)              |
       |-------------------------------------------------------|
       |              JobLevel         (uint32_t)              |
       |-------------------------------------------------------|
       |              FileSetMD5       (50 bytes)   VerNum 11  |
       |-------------------------------------------------------|
               Additional fields in End Of Session Label
       |-------------------------------------------------------|
       |              JobFiles         (uint32_t)              |
       |-------------------------------------------------------|
       |              JobBytes         (uint32_t)              |
       |-------------------------------------------------------|
       |              start_block      (uint32_t)              |
       |-------------------------------------------------------|
       |              end_block        (uint32_t)              |
       |-------------------------------------------------------|
       |              start_file       (uint32_t)              |
       |-------------------------------------------------------|
       |              end_file         (uint32_t)              |
       |-------------------------------------------------------|
       |              JobErrors        (uint32_t)              |
       |-------------------------------------------------------|
       |              JobStatus        (uint32_t) VerNum 11    |
       :=======================================================:
       * => fields deprecated
       Id: 32 byte Bareos Identifier "Bareos 1.0 immortal\n"
       LabelType (in FileIndex field of Header):
           EOM_LABEL -3     Label at EOM
           SOS_LABEL -4     Start of Session label
           EOS_LABEL -5     End of Session label
       VerNum: 11
       JobId: JobId
       write_btime: Bareos time/date this tape record written
       write_date: Julian date tape this record written - deprecated
       write_time: Julian time tape this record written - deprecated.
       PoolName: Pool Name
       PoolType: Pool Type
       MediaType: Media Type
       ClientName: Name of File daemon or Client writing this session
                   Not used for EOM_LABEL.
