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

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>Blob </span> & <span>Filename </span>\

The <span>**Filename**</span> table shown above contains the name of
each file backed up with the path removed. If different directories or
machines contain the same filename, only one copy will be saved in this
table.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>Blob </span> & <span>Full Path </span>\

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

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>integer </span> & <span>The sequential file number in the Job
</span>\
 & <span>integer </span> & <span>Link to Job Record </span>\
 & <span>integer </span> & <span>Link to Path Record </span>\
 & <span>integer </span> & <span>Link to Filename Record </span>\
 & <span>integer </span> & <span>Used to mark files during Verify Jobs
</span>\
 & <span>tinyblob </span> & <span>File attributes in base64 encoding
</span>\
 & <span>tinyblob </span> & <span>MD5/SHA1 signature in base64 encoding
</span>\

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

<span>|l|l|p<span>2.5in</span>|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>tinyblob </span> & <span>Unique Job Name </span>\
 & <span>tinyblob </span> & <span>Job Name </span>\
 & <span>tinyint </span> & <span>Used by Bareos for purging/retention
periods </span>\
 & <span>binary(1) </span> & <span>Job Type: Backup, Copy, Clone,
Archive, Migration </span>\
 & <span>binary(1) </span> & <span>Job Level </span>\
 & <span>integer </span> & <span>Client index </span>\
 & <span>binary(1) </span> & <span>Job Termination Status </span>\
 & <span>datetime </span> & <span>Time/date when Job scheduled </span>\
 & <span>datetime </span> & <span>Time/date when Job started </span>\
 & <span>datetime </span> & <span>Time/date when Job ended </span>\
 & <span>datetime </span> & <span>Time/date when original Job ended
</span>\
 & <span>bigint </span> & <span>Start day in Unix format but 64 bits;
used for Retention period. </span>\
 & <span>integer </span> & <span>Unique Volume Session ID </span>\
 & <span>integer </span> & <span>Unique Volume Session Time </span>\
 & <span>integer </span> & <span>Number of files saved in Job </span>\
 & <span>bigint </span> & <span>Number of bytes saved in Job </span>\
 & <span>integer </span> & <span>Number of errors during Job </span>\
 & <span>integer </span> & <span>Number of files not saved (not yet
used) </span>\
 & <span>integer </span> & <span>Link to Pool Record </span>\
 & <span>integer </span> & <span>Link to FileSet Record </span>\
 & <span>integer </span> & <span>Link to prior Job Record when migrated
</span>\
 & <span>tiny integer </span> & <span>Set when all File records purged
</span>\
 & <span>tiny integer </span> & <span>Set when Base Job run </span>\

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

The Job Type (or simply Type) can have one of the following values:

<span>|l|l|</span> &\
 & <span>Backup Job </span>\
 & <span>Migrated Job </span>\
 & <span>Verify Job </span>\
 & <span>Restore Job </span>\
 & <span>Console program (not in database) </span>\
 & <span>Internal or system Job </span>\
 & <span>Admin Job </span>\
 & <span>Archive Job (not implemented) </span>\
 & <span>Copy Job </span>\
 & <span>Migration Job </span>\

Note, the Job Type values noted above are not kept in an SQL table.

The JobStatus field specifies how the job terminated, and can be one of
the following:

<span>|l|l|</span> &\
 & <span>Created but not yet running </span>\
 & <span>Running </span>\
 & <span>Blocked </span>\
 & <span>Terminated normally </span>\
 & <span>Terminated normally with warnings </span>\
 & <span>Terminated in Error </span>\
 & <span>Non-fatal error </span>\
 & <span>Fatal error </span>\
 & <span>Verify Differences </span>\
 & <span>Canceled by the user </span>\
 & <span>Incomplete Job </span>\
 & <span>Waiting on the File daemon </span>\
 & <span>Waiting on the Storage daemon </span>\
 & <span>Waiting for a new Volume to be mounted </span>\
 & <span>Waiting for a Mount </span>\
 & <span>Waiting for Storage resource </span>\
 & <span>Waiting for Job resource </span>\
 & <span>Waiting for Client resource </span>\
 & <span>Wating for Maximum jobs </span>\
 & <span>Waiting for Start Time </span>\
 & <span>Waiting for higher priority job to finish </span>\
 & <span>Doing batch insert file records </span>\
 & <span>SD despooling attributes </span>\
 & <span>Doing data despooling </span>\
 & <span>Committing data (last despool) </span>\

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>tinyblob </span> & <span>FileSet name </span>\
 & <span>tinyblob </span> & <span>MD5 checksum of FileSet </span>\
 & <span>datetime </span> & <span>Time and date Fileset created </span>\

The <span>**FileSet**</span> table contains one entry for each FileSet
that is used. The MD5 signature is kept to ensure that if the user
changes anything inside the FileSet, it will be detected and the new
FileSet will be used. This is particularly important when doing an
incremental update. If the user deletes a file or adds a file, we need
to ensure that a Full backup is done prior to the next incremental.

<span>|l|l|p<span>2.5in</span>|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>integer </span> & <span>Link to Job Record </span>\
 & <span>integer </span> & <span>Link to Media Record </span>\
 & <span>integer </span> & <span>The index (sequence number) of the
first file written for this Job to the Media </span>\
 & <span>integer </span> & <span>The index of the last file written for
this Job to the Media </span>\
 & <span>integer </span> & <span>The physical media (tape) file number
of the first block written for this Job </span>\
 & <span>integer </span> & <span>The physical media (tape) file number
of the last block written for this Job </span>\
 & <span>integer </span> & <span>The number of the first block written
for this Job </span>\
 & <span>integer </span> & <span>The number of the last block written
for this Job </span>\
 & <span>integer </span> & <span>The Volume use sequence number within
the Job </span>\

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

<span>|l|l|p<span>2.4in</span>|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>tinyblob </span> & <span>Volume name </span>\
 & <span>integer </span> & <span>Autochanger Slot number or zero
</span>\
 & <span>integer </span> & <span>Link to Pool Record </span>\
 & <span>tinyblob </span> & <span>The MediaType supplied by the user
</span>\
 & <span>integer </span> & <span>The MediaTypeId </span>\
 & <span>tinyint </span> & <span>The type of label on the Volume
</span>\
 & <span>datetime </span> & <span>Time/date when first written </span>\
 & <span>datetime </span> & <span>Time/date when last written </span>\
 & <span>datetime </span> & <span>Time/date when tape labeled </span>\
 & <span>integer </span> & <span>Number of jobs written to this media
</span>\
 & <span>integer </span> & <span>Number of files written to this media
</span>\
 & <span>integer </span> & <span>Number of blocks written to this media
</span>\
 & <span>integer </span> & <span>Number of time media mounted </span>\
 & <span>bigint </span> & <span>Number of bytes saved in Job </span>\
 & <span>integer </span> & <span>The number of parts for a Volume (DVD)
</span>\
 & <span>integer </span> & <span>Number of errors during Job </span>\
 & <span>integer </span> & <span>Number of writes to media </span>\
 & <span>bigint </span> & <span>Maximum bytes to put on this media
</span>\
 & <span>bigint </span> & <span>Capacity estimate for this volume
</span>\
 & <span>enum </span> & <span>Status of media: Full, Archive, Append,
Recycle, Read-Only, Disabled, Error, Busy </span>\
 <span>tinyint </span> & <span>Whether or not Volume can be written
</span>\
 & <span>tinyint </span> & <span>Whether or not Bareos can recycle the
Volumes: Yes, No </span>\
 & <span>tinyint </span> & <span>What happens to a Volume after purging
</span>\
 & <span>bigint </span> & <span>64 bit seconds until expiration </span>\
 & <span>bigint </span> & <span>64 bit seconds volume can be used
</span>\
 & <span>integer </span> & <span>maximum jobs to put on Volume </span>\
 & <span>integer </span> & <span>maximume EOF marks to put on Volume
</span>\
 & <span>tinyint </span> & <span>Whether or not Volume in autochanger
</span>\
 & <span>integer </span> & <span>Storage record ID </span>\
 & <span>integer </span> & <span>Device record ID </span>\
 & <span>integer </span> & <span>Method of addressing media </span>\
 & <span>bigint </span> & <span>Time Reading Volume </span>\
 & <span>bigint </span> & <span>Time Writing Volume </span>\
 & <span>integer </span> & <span>End File number of Volume </span>\
 & <span>integer </span> & <span>End block number of Volume </span>\
 & <span>integer </span> & <span>Location record ID </span>\
 & <span>integer </span> & <span>Number of times recycled </span>\
 & <span>datetime </span> & <span>When Volume first written </span>\
 & <span>integer </span> & <span>Id of Scratch Pool </span>\
 & <span>integer </span> & <span>Pool ID where to recycle Volume
</span>\
 & <span>blob </span> & <span>User text field </span>\

The <span>**Volume**</span> table (internally referred to as the Media
table) contains one entry for each volume, that is each tape, cassette
(8mm, DLT, DAT, ...), or file on which information is or was backed up.
There is one Volume record created for each of the NumVols specified in
the Pool resource record.

<span>|l|l|p<span>2.4in</span>|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>Tinyblob </span> & <span>Pool Name </span>\
 & <span>Integer </span> & <span>Number of Volumes in the Pool </span>\
 & <span>Integer </span> & <span>Maximum Volumes in the Pool </span>\
 & <span>tinyint </span> & <span>Use volume once </span>\
 & <span>tinyint </span> & <span>Set to use catalog </span>\
 & <span>tinyint </span> & <span>Accept any volume from Pool </span>\
 & <span>bigint </span> & <span>64 bit seconds to retain volume </span>\
 & <span>bigint </span> & <span>64 bit seconds volume can be used
</span>\
 & <span>integer </span> & <span>max jobs on volume </span>\
 & <span>integer </span> & <span>max EOF marks to put on Volume </span>\
 & <span>bigint </span> & <span>max bytes to write on Volume </span>\
 & <span>tinyint </span> & <span>yes|no for autopruning </span>\
 & <span>tinyint </span> & <span>yes|no for allowing auto recycling of
Volume </span>\
 & <span>tinyint </span> & <span>Default Volume ActionOnPurge </span>\
 & <span>enum </span> & <span>Backup, Copy, Cloned, Archive, Migration
</span>\
 & <span>tinyint </span> & <span>Type of label ANSI/Bareos </span>\
 & <span>Tinyblob </span> & <span>Label format </span>\
 <span>tinyint </span> & <span>Whether or not Volume can be written
</span>\
 & <span>integer </span> & <span>Id of Scratch Pool </span>\
 & <span>integer </span> & <span>Pool ID where to recycle Volume
</span>\
 & <span>integer </span> & <span>Pool ID of next Pool </span>\
 & <span>bigint </span> & <span>High water mark for migration </span>\
 & <span>bigint </span> & <span>Low water mark for migration </span>\
 & <span>bigint </span> & <span>Time before migration </span>\

The <span>**Pool**</span> table contains one entry for each media pool
controlled by Bareos in this database. One media record exists for each
of the NumVols contained in the Pool. The PoolType is a Bareos defined
keyword. The MediaType is defined by the administrator, and corresponds
to the MediaType specified in the Director’s Storage definition record.
The CurrentVol is the sequence number of the Media record for the
current volume.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>TinyBlob </span> & <span>File Services Name </span>\
 & <span>TinyBlob </span> & <span>uname -a from Client (not yet used)
</span>\
 & <span>tinyint </span> & <span>yes|no for autopruning </span>\
 & <span>bigint </span> & <span>64 bit seconds to retain Files </span>\
 & <span>bigint </span> & <span>64 bit seconds to retain Job </span>\

The <span>**Client**</span> table contains one entry for each machine
backed up by Bareos in this database. Normally the Name is a fully
qualified domain name.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Unique Id </span>\
 & <span>tinyblob </span> & <span>Resource name of Storage device
</span>\
 & <span>tinyint </span> & <span>Set if it is an autochanger </span>\

The <span>**Storage**</span> table contains one entry for each Storage
used.

<span>|l|l|l|</span>\
 & &\
 & <span>tinyblob </span> & <span>Counter name </span>\
 & <span>integer </span> & <span>Start/Min value for counter </span>\
 & <span>integer </span> & <span>Max value for counter </span>\
 & <span>integer </span> & <span>Current counter value </span>\
 & <span>tinyblob </span> & <span>Name of another counter </span>\

The <span>**Counter**</span> table contains one entry for each permanent
counter defined by the user.

<span>|l|l|p<span>2.5in</span>|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>tinyblob </span> & <span>Unique Job Name </span>\
 & <span>tinyblob </span> & <span>Job Name </span>\
 & <span>binary(1) </span> & <span>Job Type: Backup, Copy, Clone,
Archive, Migration </span>\
 & <span>binary(1) </span> & <span>Job Level </span>\
 & <span>integer </span> & <span>Client index </span>\
 & <span>binary(1) </span> & <span>Job Termination Status </span>\
 & <span>datetime </span> & <span>Time/date when Job scheduled </span>\
 & <span>datetime </span> & <span>Time/date when Job started </span>\
 & <span>datetime </span> & <span>Time/date when Job ended </span>\
 & <span>datetime </span> & <span>Time/date when original Job ended
</span>\
 & <span>bigint </span> & <span>Start day in Unix format but 64 bits;
used for Retention period. </span>\
 & <span>integer </span> & <span>Unique Volume Session ID </span>\
 & <span>integer </span> & <span>Unique Volume Session Time </span>\
 & <span>integer </span> & <span>Number of files saved in Job </span>\
 & <span>bigint </span> & <span>Number of bytes saved in Job </span>\
 & <span>integer </span> & <span>Number of errors during Job </span>\
 & <span>integer </span> & <span>Number of files not saved (not yet
used) </span>\
 & <span>integer </span> & <span>Link to Pool Record </span>\
 & <span>integer </span> & <span>Link to FileSet Record </span>\
 & <span>integer </span> & <span>Link to prior Job Record when migrated
</span>\
 & <span>tiny integer </span> & <span>Set when all File records purged
</span>\
 & <span>tiny integer </span> & <span>Set when Base Job run </span>\

The <span>bf JobHisto</span> table is the same as the Job table, but it
keeps long term statistics (i.e. it is not pruned with the Job).

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>integer </span> & <span>Points to Job record </span>\
 & <span>datetime </span> & <span>Time/date log record created </span>\
 & <span>blob </span> & <span>Log text </span>\

The <span>**Log**</span> table contains a log of all Job output.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>tinyblob </span> & <span>Text defining location </span>\
 & <span>integer </span> & <span>Relative cost of obtaining Volume
</span>\
 & <span>tinyint </span> & <span>Whether or not Volume is enabled
</span>\

The <span>**Location**</span> table defines where a Volume is
physically.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>datetime </span> & <span>Time/date log record created </span>\
 & <span>integer </span> & <span>Points to Media record </span>\
 & <span>integer </span> & <span>Points to Location record </span>\
 & <span>integer </span> & <span>enum: Full, Archive, Append, Recycle,
Purged Read-only, Disabled, Error, Busy, Used, Cleaning </span>\
 & <span>tinyint </span> & <span>Whether or not Volume is enabled
</span>\

The <span>**Log**</span> table contains a log of all Job output.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\

The <span>**Version**</span> table defines the Bareos database version
number. Bareos checks this number before reading the database to ensure
that it is compatible with the Bareos binary file.

<span>|l|l|l|</span>\
 & &\
 & <span>integer </span> & <span>Primary Key </span>\
 & <span>integer </span> & <span>JobId of Base Job </span>\
 & <span>integer </span> & <span>Reference to Job </span>\
 & <span>integer </span> & <span>Reference to File </span>\
 & <span>integer </span> & <span>File Index number </span>\

The <span>**BaseFiles**</span> table contains all the File references
for a particular JobId that point to a Base file – i.e. they were
previously saved and hence were not saved in the current JobId but in
BaseJobId under FileId. FileIndex is the index of the file, and is used
for optimization of Restore jobs to prevent the need to read the FileId
record when creating the in memory tree. This record is not yet
implemented.

### MySQL Table Definition

The commands used to create the MySQL tables are as follows:

    USE bareos;
    CREATE TABLE Filename (
      FilenameId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
      Name BLOB NOT NULL,
      PRIMARY KEY(FilenameId),
      INDEX (Name(30))
      );
    CREATE TABLE Path (
       PathId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       Path BLOB NOT NULL,
       PRIMARY KEY(PathId),
       INDEX (Path(50))
       );
    CREATE TABLE File (
       FileId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       FileIndex INTEGER UNSIGNED NOT NULL DEFAULT 0,
       JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
       PathId INTEGER UNSIGNED NOT NULL REFERENCES Path,
       FilenameId INTEGER UNSIGNED NOT NULL REFERENCES Filename,
       MarkId INTEGER UNSIGNED NOT NULL DEFAULT 0,
       LStat TINYBLOB NOT NULL,
       MD5 TINYBLOB NOT NULL,
       PRIMARY KEY(FileId),
       INDEX (JobId),
       INDEX (PathId),
       INDEX (FilenameId)
       );
    CREATE TABLE Job (
       JobId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       Job TINYBLOB NOT NULL,
       Name TINYBLOB NOT NULL,
       Type BINARY(1) NOT NULL,
       Level BINARY(1) NOT NULL,
       ClientId INTEGER NOT NULL REFERENCES Client,
       JobStatus BINARY(1) NOT NULL,
       SchedTime DATETIME NOT NULL,
       StartTime DATETIME NOT NULL,
       EndTime DATETIME NOT NULL,
       JobTDate BIGINT UNSIGNED NOT NULL,
       VolSessionId INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolSessionTime INTEGER UNSIGNED NOT NULL DEFAULT 0,
       JobFiles INTEGER UNSIGNED NOT NULL DEFAULT 0,
       JobBytes BIGINT UNSIGNED NOT NULL,
       JobErrors INTEGER UNSIGNED NOT NULL DEFAULT 0,
       JobMissingFiles INTEGER UNSIGNED NOT NULL DEFAULT 0,
       PoolId INTEGER UNSIGNED NOT NULL REFERENCES Pool,
       FileSetId INTEGER UNSIGNED NOT NULL REFERENCES FileSet,
       PurgedFiles TINYINT NOT NULL DEFAULT 0,
       HasBase TINYINT NOT NULL DEFAULT 0,
       PRIMARY KEY(JobId),
       INDEX (Name(128))
       );
    CREATE TABLE FileSet (
       FileSetId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       FileSet TINYBLOB NOT NULL,
       MD5 TINYBLOB NOT NULL,
       CreateTime DATETIME NOT NULL,
       PRIMARY KEY(FileSetId)
       );
    CREATE TABLE JobMedia (
       JobMediaId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
       MediaId INTEGER UNSIGNED NOT NULL REFERENCES Media,
       FirstIndex INTEGER UNSIGNED NOT NULL DEFAULT 0,
       LastIndex INTEGER UNSIGNED NOT NULL DEFAULT 0,
       StartFile INTEGER UNSIGNED NOT NULL DEFAULT 0,
       EndFile INTEGER UNSIGNED NOT NULL DEFAULT 0,
       StartBlock INTEGER UNSIGNED NOT NULL DEFAULT 0,
       EndBlock INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolIndex INTEGER UNSIGNED NOT NULL DEFAULT 0,
       PRIMARY KEY(JobMediaId),
       INDEX (JobId, MediaId)
       );
    CREATE TABLE Media (
       MediaId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       VolumeName TINYBLOB NOT NULL,
       Slot INTEGER NOT NULL DEFAULT 0,
       PoolId INTEGER UNSIGNED NOT NULL REFERENCES Pool,
       MediaType TINYBLOB NOT NULL,
       FirstWritten DATETIME NOT NULL,
       LastWritten DATETIME NOT NULL,
       LabelDate DATETIME NOT NULL,
       VolJobs INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolFiles INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolBlocks INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolMounts INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolBytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
       VolErrors INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolWrites INTEGER UNSIGNED NOT NULL DEFAULT 0,
       VolCapacityBytes BIGINT UNSIGNED NOT NULL,
       VolStatus ENUM('Full', 'Archive', 'Append', 'Recycle', 'Purged',
        'Read-Only', 'Disabled', 'Error', 'Busy', 'Used', 'Cleaning') NOT NULL,
       Recycle TINYINT NOT NULL DEFAULT 0,
       VolRetention BIGINT UNSIGNED NOT NULL DEFAULT 0,
       VolUseDuration BIGINT UNSIGNED NOT NULL DEFAULT 0,
       MaxVolJobs INTEGER UNSIGNED NOT NULL DEFAULT 0,
       MaxVolFiles INTEGER UNSIGNED NOT NULL DEFAULT 0,
       MaxVolBytes BIGINT UNSIGNED NOT NULL DEFAULT 0,
       InChanger TINYINT NOT NULL DEFAULT 0,
       MediaAddressing TINYINT NOT NULL DEFAULT 0,
       VolReadTime BIGINT UNSIGNED NOT NULL DEFAULT 0,
       VolWriteTime BIGINT UNSIGNED NOT NULL DEFAULT 0,
       PRIMARY KEY(MediaId),
       INDEX (PoolId)
       );
    CREATE TABLE Pool (
       PoolId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       Name TINYBLOB NOT NULL,
       NumVols INTEGER UNSIGNED NOT NULL DEFAULT 0,
       MaxVols INTEGER UNSIGNED NOT NULL DEFAULT 0,
       UseOnce TINYINT NOT NULL,
       UseCatalog TINYINT NOT NULL,
       AcceptAnyVolume TINYINT DEFAULT 0,
       VolRetention BIGINT UNSIGNED NOT NULL,
       VolUseDuration BIGINT UNSIGNED NOT NULL,
       MaxVolJobs INTEGER UNSIGNED NOT NULL DEFAULT 0,
       MaxVolFiles INTEGER UNSIGNED NOT NULL DEFAULT 0,
       MaxVolBytes BIGINT UNSIGNED NOT NULL,
       AutoPrune TINYINT DEFAULT 0,
       Recycle TINYINT DEFAULT 0,
       PoolType ENUM('Backup', 'Copy', 'Cloned', 'Archive', 'Migration', 'Scratch') NOT NULL,
       LabelFormat TINYBLOB,
       Enabled TINYINT DEFAULT 1,
       ScratchPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
       RecyclePoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
       UNIQUE (Name(128)),
       PRIMARY KEY (PoolId)
       );
    CREATE TABLE Client (
       ClientId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
       Name TINYBLOB NOT NULL,
       Uname TINYBLOB NOT NULL,       /* full uname -a of client */
       AutoPrune TINYINT DEFAULT 0,
       FileRetention BIGINT UNSIGNED NOT NULL,
       JobRetention  BIGINT UNSIGNED NOT NULL,
       UNIQUE (Name(128)),
       PRIMARY KEY(ClientId)
       );
    CREATE TABLE BaseFiles (
       BaseId INTEGER UNSIGNED AUTO_INCREMENT,
       BaseJobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
       JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
       FileId INTEGER UNSIGNED NOT NULL REFERENCES File,
       FileIndex INTEGER UNSIGNED,
       PRIMARY KEY(BaseId)
       );
    CREATE TABLE UnsavedFiles (
       UnsavedId INTEGER UNSIGNED AUTO_INCREMENT,
       JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
       PathId INTEGER UNSIGNED NOT NULL REFERENCES Path,
       FilenameId INTEGER UNSIGNED NOT NULL REFERENCES Filename,
       PRIMARY KEY (UnsavedId)
       );
    CREATE TABLE Version (
       VersionId INTEGER UNSIGNED NOT NULL
       );
    -- Initialize Version
    INSERT INTO Version (VersionId) VALUES (7);
    CREATE TABLE Counters (
       Counter TINYBLOB NOT NULL,
       MinValue INTEGER,
       MaxValue INTEGER,
       CurrentValue INTEGER,
       WrapCounter TINYBLOB NOT NULL,
       PRIMARY KEY (Counter(128))
       );
