.. _BootstrapChapter:

The Bootstrap File
==================

:index:`[TAG=File->Bootstrap] <pair: File; Bootstrap>` :index:`[TAG=Bootstrap->File] <pair: Bootstrap; File>`

.. TODO: This chapter is going to be rewritten (by Philipp).

The information in this chapter is provided so that you may either create your own bootstrap files, or so that you can edit a bootstrap file produced by Bareos. However, normally the bootstrap file will be automatically created for you during the :strong:`restore` in the Console program, or by using a :config:option:`dir/job/WriteBootstrap`\  record in your Backup Jobs, and thus you will never need to know the details of this file.

The bootstrap file contains ASCII information that permits precise specification of what files should be restored, what volume they are on, and where they are on the volume. It is a relatively compact form of specifying the information, is human readable, and can be edited with any text editor.

Bootstrap File Format
---------------------

:index:`[TAG=Bootstrap->File Format] <pair: Bootstrap; File Format>`

The general format of a bootstrap file is:

<keyword>= <value>

Where each keyword and the value specify which files to restore. More precisely the keyword and their values serve to limit which files will be restored and thus act as a filter. The absence of a keyword means that all records will be accepted.

Blank lines and lines beginning with a pound sign (#) in the bootstrap file are ignored.

There are keywords which permit filtering by Volume, Client, Job, FileIndex, Session Id, Session Time, ...

The more keywords that are specified, the more selective the specification of which files to restore will be. In fact, each keyword is ANDed with other keywords that may be present.

For example,



::

   Volume = Test-001
   VolSessionId = 1
   VolSessionTime = 108927638



directs the Storage daemon (or the bextract program) to restore only those files on Volume Test-001 AND having VolumeSessionId equal to one AND having VolumeSession time equal to 108927638.

The full set of permitted keywords presented in the order in which they are matched against the Volume records are:

Volume
   :index:`[TAG=Bootstrap->Volume] <pair: Bootstrap; Volume>` The value field specifies what Volume the following commands apply to. Each Volume specification becomes the current Volume, to which all the following commands apply until a new current Volume (if any) is specified. If the Volume name contains spaces, it should be enclosed in quotes. At lease one Volume specification is required.

Count
   :index:`[TAG=Bootstrap->Count] <pair: Bootstrap; Count>` The value is the total number of files that will be restored for this Volume. This allows the Storage daemon to know when to stop reading the Volume. This value is optional.

VolFile
   :index:`[TAG=Bootstrap->VolFile] <pair: Bootstrap; VolFile>` The value is a file number, a list of file numbers, or a range of file numbers to match on the current Volume. The file number represents the physical file on the Volume where the data is stored. For a tape volume, this record is used to position to the correct starting file, and once the tape is past the last specified file, reading will stop.

VolBlock
   :index:`[TAG=Bootstrap->VolBlock] <pair: Bootstrap; VolBlock>` The value is a block number, a list of block numbers, or a range of block numbers to match on the current Volume. The block number represents the physical block within the file on the Volume where the data is stored.

VolSessionTime
   :index:`[TAG=Bootstrap->VolSessionTime] <pair: Bootstrap; VolSessionTime>` The value specifies a Volume Session Time to be matched from the current volume.

VolSessionId
   :index:`[TAG=Bootstrap->VolSessionId] <pair: Bootstrap; VolSessionId>` The value specifies a VolSessionId, a list of volume session ids, or a range of volume session ids to be matched from the current Volume. Each VolSessionId and VolSessionTime pair corresponds to a unique Job that is backed up on the Volume.

JobId
   :index:`[TAG=Bootstrap->JobId] <pair: Bootstrap; JobId>` The value specifies a JobId, list of JobIds, or range of JobIds to be selected from the current Volume. Note, the JobId may not be unique if you have multiple Directors, or if you have reinitialized your database. The JobId filter works only if you do not run multiple simultaneous jobs. This value is optional and not used by Bareos to restore files.

Job
   :index:`[TAG=Bootstrap->Job] <pair: Bootstrap; Job>` The value specifies a Job name or list of Job names to be matched on the current Volume. The Job corresponds to a unique VolSessionId and VolSessionTime pair. However, the Job is perhaps a bit more readable by humans. Standard regular expressions (wildcards) may be used to match Job names. The Job filter works only if you do not run multiple simultaneous jobs. This value is optional and not used by Bareos to restore files.

Client
   :index:`[TAG=Bootstrap->Client] <pair: Bootstrap; Client>` The value specifies a Client name or list of Clients to will be matched on the current Volume. Standard regular expressions (wildcards) may be used to match Client names. The Client filter works only if you do not run multiple simultaneous jobs. This value is optional and not used by Bareos to restore files.

FileIndex
   :index:`[TAG=Bootstrap->FileIndex] <pair: Bootstrap; FileIndex>` The value specifies a FileIndex, list of FileIndexes, or range of FileIndexes to be selected from the current Volume. Each file (data) stored on a Volume within a Session has a unique FileIndex. For each Session, the first file written is assigned FileIndex equal to one and incremented for each file backed up.

   This for a given Volume, the triple VolSessionId, VolSessionTime, and FileIndex uniquely identifies a file stored on the Volume. Multiple copies of the same file may be stored on the same Volume, but for each file, the triple VolSessionId, VolSessionTime, and FileIndex will be unique. This triple is stored in the Catalog database for each file.

   To restore a particular file, this value (or a range of FileIndexes) is required.

FileRegex
   :index:`[TAG=Bootstrap->FileRegex] <pair: Bootstrap; FileRegex>` The value is a regular expression. When specified, only matching filenames will be restored.

   ::

         FileRegex=^/etc/passwd(.old)?

Slot
   :index:`[TAG=Bootstrap->Slot] <pair: Bootstrap; Slot>` The value specifies the autochanger slot. There may be only a single Slot specification for each Volume.

Stream
   :index:`[TAG=Bootstrap->Stream] <pair: Bootstrap; Stream>` The value specifies a Stream, a list of Streams, or a range of Streams to be selected from the current Volume. Unless you really know what you are doing (the internals of Bareos), you should avoid this specification. This value is optional and not used by Bareos to restore files.

The Volume record is a bit special in that it must be the first record. The other keyword records may appear in any order and any number following a Volume record.

Multiple Volume records may be specified in the same bootstrap file, but each one starts a new set of filter criteria for the Volume.

In processing the bootstrap file within the current Volume, each filter specified by a keyword is ANDed with the next. Thus,



::

   Volume = Test-01
   Client = "My machine"
   FileIndex = 1



will match records on Volume Test-01 AND Client records for My machine AND FileIndex equal to one.

Multiple occurrences of the same record are ORed together. Thus,



::

   Volume = Test-01
   Client = "My machine"
   Client = "Backup machine"
   FileIndex = 1



will match records on Volume Test-01 AND (Client records for My machine OR Backup machine) AND FileIndex equal to one.

For integer values, you may supply a range or a list, and for all other values except Volumes, you may specify a list. A list is equivalent to multiple records of the same keyword. For example,



::

   Volume = Test-01
   Client = "My machine", "Backup machine"
   FileIndex = 1-20, 35



will match records on Volume Test-01 AND (Client records for My machine OR Backup machine) AND (FileIndex 1 OR 2 OR 3 ... OR 20 OR 35).

As previously mentioned above, there may be multiple Volume records in the same bootstrap file. Each new Volume definition begins a new set of filter conditions that apply to that Volume and will be ORed with any other Volume definitions.

As an example, suppose we query for the current set of tapes to restore all files on Client Rufus using the query command in the console program:



::

   Using default Catalog name=MySQL DB=bareos
   *query
   Available queries:
        1: List Job totals:
        2: List where a file is saved:
        3: List where the most recent copies of a file are saved:
        4: List total files/bytes by Job:
        5: List total files/bytes by Volume:
        6: List last 10 Full Backups for a Client:
        7: List Volumes used by selected JobId:
        8: List Volumes to Restore All Files:
   Choose a query (1-8): 8
   Enter Client Name: Rufus
   +-------+------------------+------------+-----------+----------+------------+
   | JobId | StartTime        | VolumeName | StartFile | VolSesId | VolSesTime |
   +-------+------------------+------------+-----------+----------+------------+
   | 154   | 2002-05-30 12:08 | test-02    | 0         | 1        | 1022753312 |
   | 202   | 2002-06-15 10:16 | test-02    | 0         | 2        | 1024128917 |
   | 203   | 2002-06-15 11:12 | test-02    | 3         | 1        | 1024132350 |
   | 204   | 2002-06-18 08:11 | test-02    | 4         | 1        | 1024380678 |
   +-------+------------------+------------+-----------+----------+------------+



The output shows us that there are four Jobs that must be restored. The first one is a Full backup, and the following three are all Incremental backups.

The following bootstrap file will restore those files:



::

   Volume=test-02
   VolSessionId=1
   VolSessionTime=1022753312
   Volume=test-02
   VolSessionId=2
   VolSessionTime=1024128917
   Volume=test-02
   VolSessionId=1
   VolSessionTime=1024132350
   Volume=test-02
   VolSessionId=1
   VolSessionTime=1024380678



As a final example, assume that the initial Full save spanned two Volumes. The output from query might look like:



::

   +-------+------------------+------------+-----------+----------+------------+
   | JobId | StartTime        | VolumeName | StartFile | VolSesId | VolSesTime |
   +-------+------------------+------------+-----------+----------+------------+
   | 242   | 2002-06-25 16:50 | File0003   | 0         | 1        | 1025016612 |
   | 242   | 2002-06-25 16:50 | File0004   | 0         | 1        | 1025016612 |
   | 243   | 2002-06-25 16:52 | File0005   | 0         | 2        | 1025016612 |
   | 246   | 2002-06-25 19:19 | File0006   | 0         | 2        | 1025025494 |
   +-------+------------------+------------+-----------+----------+------------+



and the following bootstrap file would restore those files:



::

   Volume=File0003
   VolSessionId=1
   VolSessionTime=1025016612
   Volume=File0004
   VolSessionId=1
   VolSessionTime=1025016612
   Volume=File0005
   VolSessionId=2
   VolSessionTime=1025016612
   Volume=File0006
   VolSessionId=2
   VolSessionTime=1025025494



Automatic Generation of Bootstrap Files
---------------------------------------

:index:`[TAG=Files->Automatic Generation of Bootstrap] <pair: Files; Automatic Generation of Bootstrap>` :index:`[TAG=Bootstrap->Automatic Generation] <pair: Bootstrap; Automatic Generation>`

One thing that is probably worth knowing: the bootstrap files that are generated automatically at the end of the job are not as optimized as those generated by the restore command. This is because during Incremental and Differential jobs, the records pertaining to the files written for the Job are appended to the end of the bootstrap file. As consequence, all the files saved to an Incremental or Differential job will be restored first by the Full save, then by any Incremental or Differential
saves.

When the bootstrap file is generated for the restore command, only one copy (the most recent) of each file is restored.

So if you have spare cycles on your machine, you could optimize the bootstrap files by doing the following:



::

      bconsole
      restore client=xxx select all
      done
      no
      quit
      Backup bootstrap file.



The above will not work if you have multiple FileSets because that will be an extra prompt. However, the restore client=xxx select all builds the in-memory tree, selecting everything and creates the bootstrap file.

The no answers the Do you want to run this (yes/mod/no) question.

Bootstrap for bscan
-------------------

:index:`[TAG=bscan] <single: bscan>` :index:`[TAG=bscan->bootstrap] <pair: bscan; bootstrap>` :index:`[TAG=Bootstrap->bscan] <pair: Bootstrap; bscan>` :index:`[TAG=Command->bscan] <pair: Command; bscan>` 

.. _bscanBootstrap:



If you have a very large number of Volumes to scan with bscan, you may exceed the command line limit (511 characters). In that case, you can create a simple bootstrap file that consists of only the volume names. An example might be:



::

   Volume="Vol001"
   Volume="Vol002"
   Volume="Vol003"
   Volume="Vol004"
   Volume="Vol005"



Bootstrap Example
-----------------

:index:`[TAG=Example->Bootstrap] <pair: Example; Bootstrap>` :index:`[TAG=Bootstrap->Example] <pair: Bootstrap; Example>`

If you want to extract or copy a single Job, you can do it by selecting by JobId (code not tested) or better yet, if you know the VolSessionTime and the VolSessionId (printed on Job report and in Catalog), specifying this is by far the best. Using the VolSessionTime and VolSessionId is the way Bareos does restores. A bsr file might look like the following:



::

   Volume="Vol001"
   VolSessionId=10
   VolSessionTime=1080847820



If you know how many files are backed up (on the job report), you can enormously speed up the selection by adding (let’s assume there are 157 files):



::

   FileIndex=1-157
   Count=157



Finally, if you know the File number where the Job starts, you can also cause bcopy to forward space to the right file without reading every record:



::

   VolFile=20



There is nothing magic or complicated about a BSR file. Parsing it and properly applying it within Bareos \*is\* magic, but you don’t need to worry about that.

If you want to see a \*real\* bsr file, simply fire up the restore command in the console program, select something, then answer no when it prompts to run the job. Then look at the file restore.bsr in your working directory. 


