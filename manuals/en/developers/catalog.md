Catalog Services
================

General
-------

This chapter is intended to be a technical discussion of the Catalog
services and as such is not targeted at end users but rather at
developers and system administrators that want or need to know more of
the working details of <span>**Bareos**</span>.

The <span>**Bareos Catalog**</span> services consist of the programs
that provide the SQL database engine for storage and retrieval of all
information concerning files that were backed up and their locations on
the storage media.

We have investigated the possibility of using the following SQL engines
for Bareos: Beagle, mSQL, GNU SQL, PostgreSQL, SQLite, Oracle, and
MySQL. Each presents certain problems with either licensing or maturity.
At present, we have chosen for development purposes to use MySQL,
PostgreSQL and SQLite. MySQL was chosen because it is fast, proven to be
reliable, widely used, and actively being developed. MySQL is released
under the GNU GPL license. PostgreSQL was chosen because it is a
full-featured, very mature database, and because Dan Langille did the
Bareos driver for it. PostgreSQL is distributed under the BSD license.
SQLite was chosen because it is small, efficient, and can be directly
embedded in <span>**Bareos**</span> thus requiring much less effort from
the system administrator or person building <span>**Bareos**</span>. In
our testing SQLite has performed very well, and for the functions that
we use, it has never encountered any errors except that it does not
appear to handle databases larger than 2GBytes. That said, we would not
recommend it for serious production use.

The Bareos SQL code has been written in a manner that will allow it to
be easily modified to support any of the current SQL database systems on
the market (for example: mSQL, iODBC, unixODBC, Solid, OpenLink ODBC,
EasySoft ODBC, InterBase, Oracle8, Oracle7, and DB2).

If you do not specify either <span>**`--`with-mysql**</span> or
<span>**`--`with-postgresql**</span> or <span>**`--`with-sqlite**</span>
on the ./configure line, Bareos will use its minimalist internal
database. This database is kept for build reasons but is no longer
supported. Bareos <span>**requires**</span> one of the three databases
(MySQL, PostgreSQL, or SQLite) to run.

### Filenames and Maximum Filename Length

In general, either MySQL, PostgreSQL or SQLite permit storing arbitrary
long path names and file names in the catalog database. In practice,
there still may be one or two places in the Catalog interface code that
restrict the maximum path length to 512 characters and the maximum file
name length to 512 characters. These restrictions are believed to have
been removed. Please note, these restrictions apply only to the Catalog
database and thus to your ability to list online the files saved during
any job. All information received and stored by the Storage daemon
(normally on tape) allows and handles arbitrarily long path and
filenames.

### Database Table Design

All discussions that follow pertain to the MySQL database. The details
for the PostgreSQL and SQLite databases are essentially identical except
for that all fields in the SQLite database are stored as ASCII text and
some of the database creation statements are a bit different. The
details of the internal Bareos catalog are not discussed here.

Because the Catalog database may contain very large amounts of data for
large sites, we have made a modest attempt to normalize the data tables
to reduce redundant information. While reducing the size of the database
significantly, it does, unfortunately, add some complications to the
structures.

In simple terms, the Catalog database must contain a record of all Jobs
run by Bareos, and for each Job, it must maintain a list of all files
saved, with their File Attributes (permissions, create date, ...), and
the location and Media on which the file is stored. This is seemingly a
simple task, but it represents a huge amount interlinked data. Note: the
list of files and their attributes is not maintained when using the
internal Bareos database. The data stored in the File records, which
allows the user or administrator to obtain a list of all files backed up
during a job, is by far the largest volume of information put into the
Catalog database.

Although the Catalog database has been designed to handle backup data
for multiple clients, some users may want to maintain multiple
databases, one for each machine to be backed up. This reduces the risk
of confusion of accidental restoring a file to the wrong machine as well
as reducing the amount of data in a single database, thus increasing
efficiency and reducing the impact of a lost or damaged database.

Sequence of Creation of Records for a Save Job
----------------------------------------------

Start with StartDate, ClientName, Filename, Path, Attributes, MediaName,
MediaCoordinates. (PartNumber, NumParts). In the steps below, “Create
new” means to create a new record whether or not it is unique. “Create
unique” means each record in the database should be unique. Thus, one
must first search to see if the record exists, and only if not should a
new one be created, otherwise the existing RecordId should be used.

1.  Create new Job record with StartDate; save JobId

2.  Create unique Media record; save MediaId

3.  Create unique Client record; save ClientId

4.  Create unique Filename record; save FilenameId

5.  Create unique Path record; save PathId

6.  Create unique Attribute record; save AttributeId store ClientId,
    FilenameId, PathId, and Attributes

7.  Create new File record store JobId, AttributeId, MediaCoordinates,
    etc

8.  Repeat steps 4 through 8 for each file

9.  Create a JobMedia record; save MediaId

10. Update Job record filling in EndDate and other Job statistics

Database Tables
---------------

### Filename

Column Name | Data Type | Remark
------------|-----------|------------
FilenameId  | integer   | Primary Key
Name        | Blob      | Filename

The <span>**Filename**</span> table shown above contains the name of
each file backed up with the path removed. If different directories or
machines contain the same filename, only one copy will be saved in this
table.

### Path

Column Name | Data Type | Remark
------------|-----------|------------
PathId      | integer   | Primary Key
Path        | Blob      | Full Path

The <span>**Path**</span> table contains shown above the path or
directory names of all directories on the system or systems. The
filename and any MSDOS disk name are stripped off. As with the filename,
only one copy of each directory name is kept regardless of how many
machines or drives have the same directory. These path names should be
stored in Unix path name format.

Some simple testing on a Linux file system indicates that separating the
filename and the path may be more complication than is warranted by the
space savings. For example, this system has a total of 89,097 files,
60,467 of which have unique filenames, and there are 4,374 unique paths.

Finding all those files and doing two stats() per file takes an average
wall clock time of 1 min 35 seconds on a 400MHz machine running RedHat
6.1 Linux.

Finding all those files and putting them directly into a MySQL database
with the path and filename defined as TEXT, which is variable length up
to 65,535 characters takes 19 mins 31 seconds and creates a 27.6 MByte
database.

Doing the same thing, but inserting them into Blob fields with the
filename indexed on the first 30 characters and the path name indexed on
the 255 (max) characters takes 5 mins 18 seconds and creates a 5.24 MB
database. Rerunning the job (with the database already created) takes
about 2 mins 50 seconds.

Running the same as the last one (Path and Filename Blob), but Filename
indexed on the first 30 characters and the Path on the first 50
characters (linear search done there after) takes 5 mins on the average
and creates a 3.4 MB database. Rerunning with the data already in the DB
takes 3 mins 35 seconds.

Finally, saving only the full path name rather than splitting the path
and the file, and indexing it on the first 50 characters takes 6 mins 43
seconds and creates a 7.35 MB database.

### File

Column Name | Data Type | Remark
------------|-----------|---------------------------------------------
FileId | integer   | Primary Key
FileIndex | integer   | The sequential file number in the Job
JobId | integer   | Link to Job Record
PathId | integer   | Link to Path Record
FilenameId | integer   | Link to Filename Record
DeltaSeq | smallint   | 
MarkId      | integer   | Used to mark files during Verify Jobs
LStat | tinyblob  | File attributes in base64 encoding
MD5 | tinyblob  | MD5/SHA1 signature in base64 encoding

The <span>**File**</span> table shown above contains one entry for each
file backed up by Bareos. Thus a file that is backed up multiple times
(as is normal) will have multiple entries in the File table. This will
probably be the table with the most number of records. Consequently, it
is essential to keep the size of this record to an absolute minimum. At
the same time, this table must contain all the information (or pointers
to the information) about the file and where it is backed up. Since a
file may be backed up many times without having changed, the path and
filename are stored in separate tables.

This table contains by far the largest amount of information in the
Catalog database, both from the stand point of number of records, and
the stand point of total database size. As a consequence, the user must
take care to periodically reduce the number of File records using the
<span>**retention**</span> command in the Console program.

### Job / JobHisto

Column Name | Data Type         | Remark
------------|-------------------|----------------------------------------------
JobId       | integer           |       Primary Key        
Job         | tinyblob          |       Unique Job Name        
Name        | tinyblob          |       Job Name        
Type        | binary(1)         | Job Type: Backup, Copy, Clone, Archive, Migration
Level       | binary(1)         |       Job Level        
ClientId    | integer           |       Client index        
JobStatus   | binary(1)         |       Job Termination Status        
SchedTime   | datetime          |       Time/date when Job scheduled        
StartTime   | datetime          |       Time/date when Job started        
EndTime     | datetime          |       Time/date when Job ended        
ReadEndTime | datetime          | Time/date when original Job ended
JobTDate    | bigint            | Start day in Unix format but 64 bits; used for Retention period.
VolSessionId | integer           |       Unique Volume Session ID        
VolSessionTime | integer           |       Unique Volume Session Time        
JobFiles    | integer           |       Number of files saved in Job        
JobBytes    | bigint            |       Number of bytes saved in Job        
JobErrors   | integer           |       Number of errors during Job        
JobMissingFiles | integer           | Number of files not saved (not yet used)
PoolId      | integer           |       Link to Pool Record        
FileSetId   | integer           |       Link to FileSet Record        
PrioJobId   | integer           | Link to prior Job Record when migrated
PurgedFiles | tiny integer      | Set when all File records purged
HasBase     | tiny integer      |       Set when Base Job run        

The <span>**Job**</span> table contains one record for each Job run by
Bareos. Thus normally, there will be one per day per machine added to
the database. Note, the JobId is used to index Job records in the
database, and it often is shown to the user in the Console program.
However, care must be taken with its use as it is not unique from
database to database. For example, the user may have a database for
Client data saved on machine Rufus and another database for Client data
saved on machine Roxie. In this case, the two database will each have
JobIds that match those in another database. For a unique reference to a
Job, see Job below.

The Name field of the Job record corresponds to the Name resource record
given in the Director’s configuration file. Thus it is a generic name,
and it will be normal to find many Jobs (or even all Jobs) with the same
Name.

The Job field contains a combination of the Name and the schedule time
of the Job by the Director. Thus for a given Director, even with
multiple Catalog databases, the Job will contain a unique name that
represents the Job.

For a given Storage daemon, the VolSessionId and VolSessionTime form a
unique identification of the Job. This will be the case even if multiple
Directors are using the same Storage daemon.

The JobStatus field specifies how the job terminated.

The <span>JobHisto</span> table is the same as the Job table, but it
keeps long term statistics (i.e. it is not pruned with the Job).

### FileSet

Column Name | Data Type | Remark
------------|-----------|------------------------------
FileSetId   | integer   | Primary Key
FileSet | tinyblob  | FileSet name
MD5 | tinyblob  | MD5 checksum of FileSet
CreateTime | datetime  | Time and date Fileset created

The <span>**FileSet**</span> table contains one entry for each FileSet
that is used. The MD5 signature is kept to ensure that if the user
changes anything inside the FileSet, it will be detected and the new
FileSet will be used. This is particularly important when doing an
incremental update. If the user deletes a file or adds a file, we need
to ensure that a Full backup is done prior to the next incremental.


### JobMedia

Column Name | Data Type | Remark
------------|-----------|----------------------------------------------------------------
JobMediaId  | integer   | Primary Key
JobId       | integer   | Link to Job Record
MediaId     | integer   | Link to Media Record
FirstIndex  | integer   | The index (sequence number) of the first file written for this Job to the Media
LastIndex   | integer   | The index of the last file written for this Job to the Media
StartFile   | integer   | Tape: the physical media file mark number of the first block written for this Job. 
|           |           | Other: upper 32-bit of the position of the first block written for this Job. 
EndFile     | integer   | Tape: the physical media file mark number of the last block written for this Job.
|           |           | Other: upper 32-bit of the position of the last block written for this Job.
StartBlock  | integer   | Tape: the number of the first block written for this Job
|           |           | Other: lower 32-bit of the position of the first block written for this Job.
Endblock    | integer   | Tape: the number of the last block written for this Job
|           |           | Other: lower 32-bit of the position of the last block written for this Job.
VolIndex    | integer   | The Volume use sequence number within the Job

The <span>**JobMedia**</span> table contains one entry at the following:
start of the job, start of each new tape file mark, start of each new tape,
end of the job.
You will have 2 or more JobMedia records per Job.



#### Tape Volume

The number ob records depends on the “Maximum File Size” specified in
the Device resource. This record allows Bareos to efficiently position
close to any given file in a backup. For restoring a full
Job, these records are not very important, but if you want to retrieve a
single file that was written near the end of a 100GB backup, the
JobMedia records can speed it up by orders of magnitude by permitting
forward spacing files and blocks rather than reading the whole 100GB
backup.

#### Other Volume

StartFile and StartBlock are both 32-bit integer values.
However, as the position on a disk volume is specified in bytes, we need this to be a 64-bit value.

Therefore, the start position is calculated as:

```
StartPosition = StartFile * 4294967296 + StartBlock
```

The end position of a job on a volume can be determined by:

```
EndPosition = EndFile * 4294967296 + EndBlock
```

Be aware, that you can not assume, that the job size on a volume is `EndPosition - StartPosition`.
When interleaving is used other jobs can also be stored between Start- and EndPosition.

```
EndPosition - StartPosition >= JobSizeOnThisMedia
```



### Volume (Media)

Column Name      | Data Type | Remark
-----------------|-----------|----------------------------------------
MediaId          | integer   | Primary Key 
VolumeName       | tinyblob  | Volume name 
Slot             | integer   | Autochanger Slot number or zero
PoolId           | integer   | Link to Pool Record 
MediaType        | tinyblob  | The MediaType supplied by the user
MediaTypeId      | integer   | The MediaTypeId 
LabelType        | tinyint   | The type of label on the Volume
FirstWritten     | datetime  | Time/date when first written 
LastWritten      | datetime  | Time/date when last written 
LabelDate        | datetime  | Time/date when tape labeled 
VolJobs          | integer   | Number of jobs written to this media
VolFiles         | integer   | Number of files written to this media
VolBlocks        | integer   | Number of blocks written to this media
VolMounts        | integer   | Number of time media mounted 
VolBytes         | bigint    | Number of bytes saved in Job 
VolParts         | integer   | The number of parts for a Volume (DVD)
VolErrors        | integer   | Number of errors during Job 
VolWrites        | integer   | Number of writes to media 
MaxVolBytes      | bigint    | Maximum bytes to put on this media
VolCapacityBytes | bigint    | Capacity estimate for this volume
VolStatus        | enum      | Status of media: Full, Archive, Append, Recycle, Read-Only, Disabled, Error, Busy 
Enabled          | tinyint   | Whether or not Volume can be written
Recycle          | tinyint   | Whether or not Bareos can recycle the Volumes: Yes, No 
ActionOnPurge    | tinyint   | What happens to a Volume after purging
VolRetention     | bigint    | 64 bit seconds until expiration 
VolUseDureation  | bigint    | 64 bit seconds volume can be used
MaxVolJobs       | integer   | maximum jobs to put on Volume 
MaxVolFiles      | integer   | maximume EOF marks to put on Volume
InChanger        | tinyint   | Whether or not Volume in autochanger
StorageId        | integer   | Storage record ID 
DeviceId         | integer   | Device record ID 
MediaAddressing  | integer   | Method of addressing media 
VolReadTime      | bigint    | Time Reading Volume 
VolWriteTime     | bigint    | Time Writing Volume 
EndFile          | integer   | End File number of Volume 
EndBlock         | integer   | End block number of Volume 
LocationId       | integer   | Location record ID 
RecycleCount     | integer   | Number of times recycled 
InitialWrite     | datetime  | When Volume first written 
ScratchPoolId    | integer   | Id of Scratch Pool 
RecyclePoolId    | integer   | Pool ID where to recycle Volume
Comment          | blob      | User text field 

The <span>**Volume**</span> table (internally referred to as the Media
table) contains one entry for each volume, that is each tape, cassette
(8mm, DLT, DAT, ...), or file on which information is or was backed up.
There is one Volume record created for each of the NumVols specified in
the Pool resource record.

### Pool

Column Name | Data Type | Remark
------------|-----------|----------------------------------------------
PoolId | integer   | Primary Key
Name | Tinyblob  | Pool Name
NumVols | Integer   | Number of Volumes in the Pool
MaxVols | Integer   | Maximum Volumes in the Pool
UseOnce | tinyint   | Use volume once
UseCatalog | tinyint   | Set to use catalog
AcceptAnyVolume | tinyint   | Accept any volume from Pool
VolRetention | bigint    | 64 bit seconds to retain volume
VolUseDuration | bigint    | 64 bit seconds volume can be used
MaxVolJobs | integer   | max jobs on volume
MaxVolFiles | integer   | max EOF marks to put on Volume
MaxVolBytes | bigint    | max bytes to write on Volume
AutoPrune | tinyint   | yes or no for autopruning
Recycle | tinyint   | yes or no for allowing auto recycling of Volume
ActionOnPurge | tinyint   | Default Volume ActionOnPurge
PoolType | enum      | Backup, Copy, Cloned, Archive, Migration
LabelType | tinyint   | Type of label ANSI/Bareos
LabelFormat | Tinyblob  | Label format
Enabled | tinyint   | Whether or not Volume can be written
ScratchPoolId | integer   | Id of Scratch Pool
RecyclePoolId | integer   | Pool ID where to recycle Volume
NextPoolId | integer   | Pool ID of next Pool
MigrationHighBytes | bigint    | High water mark for migration
MigrationLowBytes | bigint    | Low water mark for migration
MigrationTime | bigint    | Time before migration

The <span>**Pool**</span> table contains one entry for each media pool
controlled by Bareos in this database. One media record exists for each
of the NumVols contained in the Pool. The PoolType is a Bareos defined
keyword. The MediaType is defined by the administrator, and corresponds
to the MediaType specified in the Director’s Storage definition record.
The CurrentVol is the sequence number of the Media record for the
current volume.

### Client

Column Name | Data Type | Remark
------------|-----------|---------------------------------------
ClientId    | integer   | Primary Key
Name        | TinyBlob  | File Services Name
UName       | TinyBlob  | uname -a from Client (not yet used)
AutoPrune   | tinyint   | yes or no for autopruning
FileRetention | bigint    | 64 bit seconds to retain Files
JobRentention | bigint    | 64 bit seconds to retain Job

The <span>**Client**</span> table contains one entry for each machine
backed up by Bareos in this database. Normally the Name is a fully
qualified domain name.


### Storage

Column Name | Data Type | Remark
------------|-----------|----------------------------------
StorageId   | integer   | Unique Id
Name        | tinyblob  | Resource name of Storage device
AutoChanger | tinyint   | Set if it is an autochanger

The <span>**Storage**</span> table contains one entry for each Storage
used.

### Counter

Column Name | Data Type | Remark
------------|-----------|-----------------------------
Counter     | tinyblob  | Counter name
MinValue    | integer   | Start/Min value for counter
MaxValue    | integer   | Max value for counter
CurrentValue | integer   | Current counter value
WrapCounter | tinyblob  | Name of another counter

The <span>**Counter**</span> table contains one entry for each permanent
counter defined by the user.


### Log

Column Name | Data Type | Remark
------------|-----------|---------------------------------------
LogIdId     | integer   | Primary Key
JobId       | integer   | Points to Job record
Time        | datetime  |  Time/date log record created
LogText     | blob      | Log text

The <span>**Log**</span> table contains a log of all Job output.

### Location

Column Name | Data Type | Remark
------------|-----------|-----------------------------------------
LocationId  | integer   | Primary Key
Location    | tinyblob  | Text defining location
Cost        | integer   | Relative cost of obtaining Volume
Enabled     | tinyint   | Whether or not Volume is enabled

The <span>**Location**</span> table defines where a Volume is
physically.

### LocationLog

Column Name | Data Type | Remark
------------|-----------|-----------------------------------------------
LocLogId    | integer   | Primary Key
Date        | datetime  | Time/date log record created
MediaId     | integer   | Points to Media record
LocationId  | integer   | Points to Location record
NewVolStatus | integer   | enum: Full, Archive, Append, Recycle, Purged Read-only, Disabled, Error, Busy, Used, Cleaning
Enabled     | tinyint   | Whether or not Volume is enabled

The <span>**LocationLog**</span> table contains a log of all Job output.


### Version

Column Name | Data Type | Remark
------------|-----------|-------------
VersionId   | integer   | Primary Key

The <span>**Version**</span> table defines the Bareos database version
number. Bareos checks this number before reading the database to ensure
that it is compatible with the Bareos binary file.

### BaseFiles

Column Name | Data Type | Remark
------------|-----------|-----------------------
BaseId      | integer   | Primary Key
BaseJobId   | integer   | JobId of Base Job
JobId       | integer   | Reference to Job
FileId      | integer   | Reference to File
FileIndex   | integer   | File Index number

The <span>**BaseFiles**</span> table contains all the File references
for a particular JobId that point to a Base file – i.e. they were
previously saved and hence were not saved in the current JobId but in
BaseJobId under FileId. FileIndex is the index of the file, and is used
for optimization of Restore jobs to prevent the need to read the FileId
record when creating the in memory tree. This record is not yet
implemented.
