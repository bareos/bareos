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

### Installing and Configuring MySQL

For the details of installing and configuring MySQL, please see the
chapter of this manual.

### Installing and Configuring PostgreSQL

For the details of installing and configuring PostgreSQL, please see the
chapter of this manual.

### Installing and Configuring SQLite

For the details of installing and configuring SQLite, please see the
chapter of this manual.

### Internal Bareos Catalog

Please see the chapter of this manual for more details.

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

Data Type | Remark
----------|------------
integer   | Primary Key
Blob      | Filename

The <span>**Filename**</span> table shown above contains the name of
each file backed up with the path removed. If different directories or
machines contain the same filename, only one copy will be saved in this
table.

Data Type | Remark
----------|------------
integer   | Primary Key
Blob      | Full Path

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

Data Type | Remark
----------|---------------------------------------------
integer   | Primary Key
integer   | The sequential file number in the Job
integer   | Link to Job Record
integer   | Link to Path Record
integer   | Link to Filename Record
integer   | Used to mark files during Verify Jobs
tinyblob  | File attributes in base64 encoding
tinyblob  | MD5/SHA1 signature in base64 encoding

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

Data Type         | Remark
------------------|------------------------------------------------------------------
integer           |       Primary Key        
tinyblob          |       Unique Job Name        
tinyblob          |       Job Name        
tinyint           | Used by Bareos for purging/retention periods
binary(1)         | Job Type: Backup, Copy, Clone, Archive, Migration
binary(1)         |       Job Level        
integer           |       Client index        
binary(1)         |       Job Termination Status        
datetime          |       Time/date when Job scheduled        
datetime          |       Time/date when Job started        
datetime          |       Time/date when Job ended        
datetime          | Time/date when original Job ended
bigint            | Start day in Unix format but 64 bits; used for Retention period.
integer           |       Unique Volume Session ID        
integer           |       Unique Volume Session Time        
integer           |       Number of files saved in Job        
bigint            |       Number of bytes saved in Job        
integer           |       Number of errors during Job        
integer           | Number of files not saved (not yet used)
integer           |       Link to Pool Record        
integer           |       Link to FileSet Record        
integer           | Link to prior Job Record when migrated
tiny integer      | Set when all File records purged
tiny integer      |       Set when Base Job run        

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

The JobStatus field specifies how the job terminated, and can be one of
the following:

  * Created but not yet running
  * Running
  * Blocked 
  * Terminated normally 
  * Terminated normally with warnings 
  * Terminated in Error 
  * Non-fatal error 
  * Fatal error 
  * Verify Differences 
  * Canceled by the user 
  * Incomplete Job 
  * Waiting on the File daemon 
  * Waiting on the Storage daemon 
  * Waiting for a new Volume to be mounted 
  * Waiting for a Mount 
  * Waiting for Storage resource 
  * Waiting for Job resource 
  * Waiting for Client resource 
  * Wating for Maximum jobs 
  * Waiting for Start Time 
  * Waiting for higher priority job to finish 
  * Doing batch insert file records 
  * SD despooling attributes 
  * Doing data despooling 
  * Committing data (last despool) 

Data Type | Remark
----------|------------------------------
integer   | Primary Key
tinyblob  | FileSet name
tinyblob  | MD5 checksum of FileSet
datetime  | Time and date Fileset created

The <span>**FileSet**</span> table contains one entry for each FileSet
that is used. The MD5 signature is kept to ensure that if the user
changes anything inside the FileSet, it will be detected and the new
FileSet will be used. This is particularly important when doing an
incremental update. If the user deletes a file or adds a file, we need
to ensure that a Full backup is done prior to the next incremental.

Data Type | Remark
----------|---------------------------------------------------------------------------------
integer   | Primary Key
integer   | Link to Job Record
integer   | Link to Media Record
integer   | The index (sequence number) of the first file written for this Job to the Media
integer   | The index of the last file written for this Job to the Media
integer   | The physical media (tape) file number of the first block written for this Job
integer   | The physical media (tape) file number of the last block written for this Job
integer   | The number of the first block written for this Job
integer   | The number of the last block written for this Job
integer   | The Volume use sequence number within the Job

The <span>**JobMedia**</span> table contains one entry at the following:
start of the job, start of each new tape file, start of each new tape,
end of the job. Since by default, a new tape file is written every 2GB,
in general, you will have more than 2 JobMedia records per Job. The
number can be varied by changing the “Maximum File Size” specified in
the Device resource. This record allows Bareos to efficiently position
close to (within 2GB) any given file in a backup. For restoring a full
Job, these records are not very important, but if you want to retrieve a
single file that was written near the end of a 100GB backup, the
JobMedia records can speed it up by orders of magnitude by permitting
forward spacing files and blocks rather than reading the whole 100GB
backup.

Data Type | Remark
----------|----------------------------------------
integer   | Primary Key 
tinyblob  | Volume name 
integer   | Autochanger Slot number or zero
integer   | Link to Pool Record 
tinyblob  | The MediaType supplied by the user
integer   | The MediaTypeId 
tinyint   | The type of label on the Volume
datetime  | Time/date when first written 
datetime  | Time/date when last written 
datetime  | Time/date when tape labeled 
integer   | Number of jobs written to this media
integer   | Number of files written to this media
integer   | Number of blocks written to this media
integer   | Number of time media mounted 
bigint    | Number of bytes saved in Job 
integer   | The number of parts for a Volume (DVD)
integer   | Number of errors during Job 
integer   | Number of writes to media 
bigint    | Maximum bytes to put on this media
bigint    | Capacity estimate for this volume
enum      | Status of media: Full, Archive, Append, Recycle, Read-Only, Disabled, Error, Busy 
tinyint   | Whether or not Volume can be written
tinyint   | Whether or not Bareos can recycle the Volumes: Yes, No 
tinyint   | What happens to a Volume after purging
bigint    | 64 bit seconds until expiration 
bigint    | 64 bit seconds volume can be used
integer   | maximum jobs to put on Volume 
integer   | maximume EOF marks to put on Volume
tinyint   | Whether or not Volume in autochanger
integer   | Storage record ID 
integer   | Device record ID 
integer   | Method of addressing media 
bigint    | Time Reading Volume 
bigint    | Time Writing Volume 
integer   | End File number of Volume 
integer   | End block number of Volume 
integer   | Location record ID 
integer   | Number of times recycled 
datetime  | When Volume first written 
integer   | Id of Scratch Pool 
integer   | Pool ID where to recycle Volume
blob      | User text field 

The <span>**Volume**</span> table (internally referred to as the Media
table) contains one entry for each volume, that is each tape, cassette
(8mm, DLT, DAT, ...), or file on which information is or was backed up.
There is one Volume record created for each of the NumVols specified in
the Pool resource record.

Data Type | Remark
----------|----------------------------------------------
integer   | Primary Key
Tinyblob  | Pool Name
Integer   | Number of Volumes in the Pool
Integer   | Maximum Volumes in the Pool
tinyint   | Use volume once
tinyint   | Set to use catalog
tinyint   | Accept any volume from Pool
bigint    | 64 bit seconds to retain volume
bigint    | 64 bit seconds volume can be used
integer   | max jobs on volume
integer   | max EOF marks to put on Volume
bigint    | max bytes to write on Volume
tinyint   | yes|no for autopruning
tinyint   | yes|no for allowing auto recycling of Volume
tinyint   | Default Volume ActionOnPurge
enum      | Backup, Copy, Cloned, Archive, Migration
tinyint   | Type of label ANSI/Bareos
Tinyblob  | Label format
tinyint   | Whether or not Volume can be written
integer   | Id of Scratch Pool
integer   | Pool ID where to recycle Volume
integer   | Pool ID of next Pool
bigint    | High water mark for migration
bigint    | Low water mark for migration
bigint    | Time before migration

The <span>**Pool**</span> table contains one entry for each media pool
controlled by Bareos in this database. One media record exists for each
of the NumVols contained in the Pool. The PoolType is a Bareos defined
keyword. The MediaType is defined by the administrator, and corresponds
to the MediaType specified in the Director’s Storage definition record.
The CurrentVol is the sequence number of the Media record for the
current volume.

Data Type | Remark
----------|---------------------------------------
integer   | Primary Key
TinyBlob  | File Services Name
TinyBlob  | uname -a from Client (not yet used)
tinyint   | yes|no for autopruning
bigint    | 64 bit seconds to retain Files
bigint    | 64 bit seconds to retain Job

The <span>**Client**</span> table contains one entry for each machine
backed up by Bareos in this database. Normally the Name is a fully
qualified domain name.

Data Type | Remark
----------|----------------------------------
integer   | Unique Id
tinyblob  | Resource name of Storage device
tinyint   | Set if it is an autochanger

The <span>**Storage**</span> table contains one entry for each Storage
used.

Data Type | Remark
----------|-----------------------------
tinyblob  | Counter name
integer   | Start/Min value for counter
integer   | Max value for counter
integer   | Current counter value
tinyblob  | Name of another counter

The <span>**Counter**</span> table contains one entry for each permanent
counter defined by the user.

Data Type    | Remark
-------------|---------------------------------------------------
integer      | Primary Key
tinyblob     | Unique Job Name
tinyblob     | Job Name
binary(1)    | Job Type: Backup, Copy, Clone, Archive, Migration
binary(1)    | Job Level
integer      | Client index
binary(1)    | Job Termination Status
datetime     | Time/date when Job scheduled
datetime     | Time/date when Job started
datetime     | Time/date when Job ended
datetime     | Time/date when original Job ended
bigint       | Start day in Unix format but 64 bits; used for Retention period.
integer      | Unique Volume Session ID
integer      | Unique Volume Session Time
integer      | Number of files saved in Job
bigint       | Number of bytes saved in Job
integer      | Number of errors during Job
integer      | Number of files not saved (not yet used)
integer      | Link to Pool Record
integer      | Link to FileSet Record
integer      | Link to prior Job Record when migrated
tiny integer | Set when all File records purged
tiny integer | Set when Base Job run

The <span>bf JobHisto</span> table is the same as the Job table, but it
keeps long term statistics (i.e. it is not pruned with the Job).

Data Type | Remark
----------|---------------------------------------
integer   | Primary Key
integer   | Points to Job record
datetime  |  Time/date log record created
blob      | Log text

The <span>**Log**</span> table contains a log of all Job output.

Data Type | Remark
----------|-----------------------------------------
integer   | Primary Key
tinyblob  | Text defining location
integer   | Relative cost of obtaining Volume
tinyint   | Whether or not Volume is enabled

The <span>**Location**</span> table defines where a Volume is
physically.

Data Type | Remark
----------|-----------------------------------------------
integer   | Primary Key
datetime  | Time/date log record created
integer   |Points to Media record
integer   | Points to Location record
integer   | enum: Full, Archive, Append, Recycle, Purged Read-only, Disabled, Error, Busy, Used, Cleaning
tinyint   | Whether or not Volume is enabled

The <span>**Log**</span> table contains a log of all Job output.

Data Type | Remark
----------|-------------
integer   | Primary Key

The <span>**Version**</span> table defines the Bareos database version
number. Bareos checks this number before reading the database to ensure
that it is compatible with the Bareos binary file.

Data Type | Remark
----------|-----------------------
integer   | Primary Key
integer   | JobId of Base Job
integer   | Reference to Job
integer   | Reference to File
integer   | File Index number

The <span>**BaseFiles**</span> table contains all the File references
for a particular JobId that point to a Base file – i.e. they were
previously saved and hence were not saved in the current JobId but in
BaseJobId under FileId. FileIndex is the index of the file, and is used
for optimization of Restore jobs to prevent the need to read the FileId
record when creating the in memory tree. This record is not yet
implemented.
