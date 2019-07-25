Catalog Services
================

General
-------

This chapter is intended to be a technical discussion of the catalog
services and as such is not targeted at end users but rather at
developers and system administrators who want or need to know more of
the working details of **Bareos**.

Currently, we prefer the usage of PostgreSQL. Therefore our support
for SQLite or other databases could be discontinued in the future.
PostgreSQL was chosen because it is a full-featured, very mature database,
and because Dan Langille did the Bareos driver for it.

SQLite was chosen because it is small, efficient, and can be directly
embedded in **Bareos** thus requiring much less effort from the system
administrator or person building **Bareos**. In our testing SQLite has
performed very well, and for the functions that we use, it has never
encountered any errors except that it does not appear to handle
databases larger than 2GBytes. That said, we would not recommend it for
serious production use. Nonetheless SQLite is very suitable for test
environments.

Bareos **requires** one of the three databases (MySQL, PostgreSQL, or SQLite)
to run. Therefore it is mandatory to specify one of them for the cmake
configuration step, i.e.: ``-Dpostgresql=yes``.

Filenames and Maximum Filename Length
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In general, either MySQL, PostgreSQL or SQLite permit storing arbitrary
long path names and file names in the catalog database. In practice,
there still may be one or two places in the catalog interface code that
restrict the maximum path length to 512 characters and the maximum file
name length to 512 characters. These restrictions are believed to have
been removed. Please note, these restrictions apply only to the catalog
database and thus to your ability to list online the files saved during
any job. All information received and stored by the Storage daemon
(normally on tape) allows and handles arbitrarily long path and
filenames.

Database Table Design
~~~~~~~~~~~~~~~~~~~~~

All discussions that follow pertain to the PostgreSQL database.

Because the catalog database may contain very large amounts of data for
large sites, we have made a modest attempt to normalize the data tables
to reduce redundant information. While reducing the size of the database
significantly, it does, unfortunately, add some complications to the
structures.

In simple terms, the catalog database must contain a record of all Jobs
run by Bareos, and for each Job, it must maintain a list of all files
saved, with their File Attributes (permissions, create date, …), and the
location and Media on which the file is stored. This is seemingly a
simple task, but it represents a huge amount of interlinked data.
The data stored in the File records, which allows the user or
administrator to obtain a list of all files backed up during a job,
is by far the largest volume of information put into the catalog database.

Although the catalog database has been designed to handle backup data
for multiple clients, some users may want to maintain multiple
databases, one for each machine to be backed up. This reduces the risk
of confusion of accidental restoring a file to the wrong machine as well
as reducing the amount of data in a single database, thus increasing
efficiency and reducing the impact of a lost or damaged database.

Database Tables
---------------

Path
~~~~

The **Path** table contains shown above the path or directory names of all
directories on the system or systems.

+-------------+-----------+-------------+
| Column Name | Data Type | Remark      |
+=============+===========+=============+
| PathId      | serial    | Primary Key |
+-------------+-----------+-------------+
| Path        | text      | Full Path   |
+-------------+-----------+-------------+

The filename and any disk name are stripped off. As with the filename,
only one copy of each directory name is kept regardless of how many machines
or drives have the same directory. These path names should be stored in Unix
path name format.

File
~~~~

The **File** table contains one entry for each file backed up by Bareos.

+-------------+---------------+---------------------------------------+
| Column Name | Data Type     | Remark                                |
+=============+===============+=======================================+
| FileId      | serial        | Primary Key                           |
+-------------+---------------+---------------------------------------+
| FileIndex   | integer       | The sequential file number in the Job |
+-------------+---------------+---------------------------------------+
| JobId       | integer       | Link to Job Record                    |
+-------------+---------------+---------------------------------------+
| PathId      | integer       | Link to Path Record                   |
+-------------+---------------+---------------------------------------+
| DeltaSeq    | smallint      |                                       |
+-------------+---------------+---------------------------------------+
| MarkId      | integer       | Used to mark files during Verify Jobs |
+-------------+---------------+---------------------------------------+
| Fhinofo     | numeric(20)   |                                       |
+-------------+---------------+---------------------------------------+
| Fhnode      | numeric(20)   |                                       |
+-------------+---------------+---------------------------------------+
| LStat       | text          | File attributes in base64 encoding    |
+-------------+---------------+---------------------------------------+
| MD5         | text          | MD5/SHA1 signature in base64 encoding |
+-------------+---------------+---------------------------------------+
| name        | text          |                                       |
+-------------+---------------+---------------------------------------+

File that is backed up multiple times (as is normal) will have multiple
entries in the File table. This will probably
be the table with the most number of records. Consequently, it is
essential to keep the size of this record to an absolute minimum. At the
same time, this table must contain all the information (or pointers to
the information) about the file and where it is backed up.

This table contains by far the largest amount of information in the
catalog database, both from the stand point of number of records, and
the stand point of total database size.

As MD5 hash (also termed message digests) consists of 128-bit (16-byte).
A typical format (eg. ``md5sum``, …) to represent them is as a sequence
of 32 hexadecimal digits. However, in the **MD5** column, the digest is
represented as base64.

To compare a Bareos digest with command line tools, you have to use

::

    openssl dgst -md5 -binary $PATH_OF_YOUR_FILE | base64

Please note, even the table column is named **MD5**, it is used to store
any kind of digest (MD5, SHA1, …). This is not directly indicated by the
value, however, you can determine the type depending of the length of
the digest.

Job
~~~

The **Job** table contains one record for each Job run by Bareos.

+----------------+----------------+------------------------------------------+
| Column         | Data Type      | Remark                                   |
| Name           |                |                                          |
+================+================+==========================================+
| JobId          | serial         | Primary Key                              |
+----------------+----------------+------------------------------------------+
| Job            | text           | Unique Job Name                          |
+----------------+----------------+------------------------------------------+
| Name           | text           | Job Name                                 |
+----------------+----------------+------------------------------------------+
| Type           | char(1)        | Job Type: Backup, Copy, Clone, Archive,  |
|                |                | Migration                                |
+----------------+----------------+------------------------------------------+
| Level          | char(1)        | Job Level                                |
+----------------+----------------+------------------------------------------+
| ClientId       | integer        | Client index                             |
+----------------+----------------+------------------------------------------+
| JobStatus      | char(1)        | Job Termination Status                   |
+----------------+----------------+------------------------------------------+
| SchedTime      | timestamp      | Time/date when Job scheduled             |
+----------------+----------------+------------------------------------------+
| StartTime      | timestamp      | Time/date when Job started               |
+----------------+----------------+------------------------------------------+
| EndTime        | timestamp      | Time/date when Job ended                 |
+----------------+----------------+------------------------------------------+
| RealEndTime    | timestamp      | Time/date when original Job ended        |
+----------------+----------------+------------------------------------------+
| JobTDate       | bigint         | Start day in Unix format but 64 bits;    |
|                |                | used for Retention period.               |
+----------------+----------------+------------------------------------------+
| VolSessionId   | integer        | Unique Volume Session ID                 |
+----------------+----------------+------------------------------------------+
| VolSessionTime | integer        | Unique Volume Session Time               |
+----------------+----------------+------------------------------------------+
| JobFiles       | integer        | Number of files saved in Job             |
+----------------+----------------+------------------------------------------+
| JobBytes       | bigint         | Number of bytes saved in Job             |
+----------------+----------------+------------------------------------------+
| ReadBytes      | bigint         |                                          |
+----------------+----------------+------------------------------------------+
| JobErrors      | integer        | Number of errors during Job              |
+----------------+----------------+------------------------------------------+
| JobMissinFiles | integer        | Number of files not saved (not yet used) |
+----------------+----------------+------------------------------------------+
| PoolId         | integer        | Link to Pool Record                      |
+----------------+----------------+------------------------------------------+
| FileSetId      | integer        | Link to FileSet Record                   |
+----------------+----------------+------------------------------------------+
| PriorJobId     | integer        | Link to prior Job Record when migrated   |
+----------------+----------------+------------------------------------------+
| PurgedFiles    | smallint       | Set when all File records purged         |
+----------------+----------------+------------------------------------------+
| HasBase        | smallint       | Set when Base Job run                    |
+----------------+----------------+------------------------------------------+
| HasCache       | smallint       |                                          |
+----------------+----------------+------------------------------------------+
| Reviewed       | smallint       |                                          |
+----------------+----------------+------------------------------------------+
| Comment        | text           |                                          |
+----------------+----------------+------------------------------------------+

The Name field of the Job record corresponds to the Name resource record
given in the Director’s configuration file.

The Job field contains a combination of the Name and the schedule time
of the Job by the Director. Thus for a given Director, even with
multiple catalog databases, the Job will contain a unique name that
represents the Job.

For a given Storage daemon, the VolSessionId and VolSessionTime form a
unique identification of the Job. This will be the case even if multiple
Directors are using the same Storage daemon.

The JobStatus field specifies how the job terminated.

FileSet
~~~~~~~

The **FileSet** table contains one entry for each FileSet that is used.

+-------------+-----------+-------------------------------+
| Column Name | Data Type | Remark                        |
+=============+===========+===============================+
| FileSetId   | serial    | Primary Key                   |
+-------------+-----------+-------------------------------+
| FileSet     | text      | FileSet name                  |
+-------------+-----------+-------------------------------+
| FileSetText | text      |                               |
+-------------+-----------+-------------------------------+
| MD5         | text      | MD5 checksum of FileSet       |
+-------------+-----------+-------------------------------+
| CreateTime  | timestamp | Time and date Fileset created |
+-------------+-----------+-------------------------------+

The MD5 signature is kept to ensure that if the user changes anything
inside the FileSet, it will be detected and the new FileSet will be
used. This is particularly important when doing an incremental update.
If the user deletes a file or adds a file, we need to ensure that a Full
backup is done prior to the next incremental.

JobMedia
~~~~~~~~

The **JobMedia** table contains one entry at the following: start of the
job, start of each new tape file mark, start of each new tape, end of
the job. You will have 2 or more JobMedia records per Job.

+-------------+-------------+---------------------------------------------------+
| Column      | Data        | Remark                                            |
| Name        | Type        |                                                   |
+=============+=============+===================================================+
| JobMediaId  | serial      | Primary Key                                       |
+-------------+-------------+---------------------------------------------------+
| JobId       | integer     | Link to Job Record                                |
+-------------+-------------+---------------------------------------------------+
| MediaId     | integer     | Link to Media Record                              |
+-------------+-------------+---------------------------------------------------+
| FirstIndex  | integer     | The index (sequence number) of the first file     |
|             |             | written for this Job to the Media                 |
+-------------+-------------+---------------------------------------------------+
| LastIndex   | integer     | The index of the last file written for this Job   |
|             |             | to the Media                                      |
+-------------+-------------+---------------------------------------------------+
| StartFile   | integer     | | *Tape*: The physical media file mark number of  |
|             |             |   the first block written for this Job.           |
|             |             | | *Other*: Upper 32-bit of the position of the    |
|             |             |   first block written for this Job.               |
+-------------+-------------+---------------------------------------------------+
| EndFile     | integer     | | *Tape*: The physical media file mark number of  |
|             |             |   the last block written for this Job             |
|             |             | | *Other*: Upper 32-bit of the position of the    |
|             |             |   last block written for this Job                 |
+-------------+-------------+---------------------------------------------------+
| StartBlock  | integer     | | *Tape*: The number of the first block written   |
|             |             |   for this Job                                    |
|             |             | | *Other*: Lower 32-bit of the position of the    |
|             |             |   first block written for this Job                |
+-------------+-------------+---------------------------------------------------+
| Endblock    | integer     | | *Tape*: The number of the last block written for|
|             |             |   this Job                                        |
|             |             | | *Other*: Lower 32-bit of the position of the    |
|             |             |   last block written for this Job                 |
+-------------+-------------+---------------------------------------------------+
| JobBytes    | numeric(20) | The Volume use sequence number within the Job     |
+-------------+-------------+---------------------------------------------------+
| VolIndex    | integer     | The Volume use sequence number within the Job     |
+-------------+-------------+---------------------------------------------------+

Device
~~~~~~

This is the device table. It contains information about reading and or writing devices.

+----------------------------+-------------+---------------------------------------+
| Column                     | Data        | Remark                                |
| Name                       | Type        |                                       |
+============================+=============+=======================================+
| DeviceId                   | serial      |                                       |
+----------------------------+-------------+---------------------------------------+
| Name                       | text        |                                       |
+----------------------------+-------------+---------------------------------------+
| MediaTypeId                | integer     |                                       |
+----------------------------+-------------+---------------------------------------+
| StorageId                  | integer     |                                       |
+----------------------------+-------------+---------------------------------------+
| DevMounts                  | integer     |                                       |
+----------------------------+-------------+---------------------------------------+
| DevReadBytes               | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevWriteBytes              | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevReadBytesSinceCleaning  | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevWriteBytesSinceCleaning | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevReadTime                | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevWriteTime               | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevReadTimeSinceCleaning   | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| DevWriteTimeSinceCleaning  | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+
| CleaningDate               | timestamp   |                                       |
+----------------------------+-------------+---------------------------------------+
| CleaningPeriod             | bigint      |                                       |
+----------------------------+-------------+---------------------------------------+


Tape Volume
^^^^^^^^^^^

The number ob records depends on the “Maximum File Size” specified in
the Device resource. This record allows Bareos to efficiently position
close to any given file in a backup. For restoring a full Job, these
records are not very important, but if you want to retrieve a single
file that was written near the end of a 100GB backup, the JobMedia
records can speed it up by orders of magnitude by permitting forward
spacing files and blocks rather than reading the whole 100GB backup.

Other Volume
^^^^^^^^^^^^

StartFile and StartBlock are both 32-bit integer values. However, as the
position on a disk volume is specified in bytes, we need this to be a
64-bit value.

Therefore, the start position is calculated as:

::

    StartPosition = StartFile * 4294967296 + StartBlock

The end position of a job on a volume can be determined by:

::

    EndPosition = EndFile * 4294967296 + EndBlock

Be aware, that you can not assume, that the job size on a volume is
``EndPosition - StartPosition``. When interleaving is used other jobs
can also be stored between Start- and EndPosition.

::

    EndPosition - StartPosition >= JobSizeOnThisMedia

Media (Volume)
~~~~~~~~~~~~~~

The **Media** table contains one entry for each volume, that is each tape
or file on which information is or was backed up. There is one volume record
created for each of the NumVols specified in the Pool resource record.

+------------------+-----------+-----------------------------------------+
| Column Name      | Data      | Remark                                  |
|                  | Type      |                                         |
+==================+===========+=========================================+
| MediaId          | serial    | Primary Key                             |
+------------------+-----------+-----------------------------------------+
| VolumeName       | text      | Volume name                             |
+------------------+-----------+-----------------------------------------+
| Slot             | integer   | Autochanger Slot number or zero         |
+------------------+-----------+-----------------------------------------+
| PoolId           | integer   | Link to Pool Record                     |
+------------------+-----------+-----------------------------------------+
| MediaType        | text      | The MediaType supplied by the user      |
+------------------+-----------+-----------------------------------------+
| MediaTypeId      | integer   | The MediaTypeId                         |
+------------------+-----------+-----------------------------------------+
| LabelType        | integer   | The type of label on the Volume         |
+------------------+-----------+-----------------------------------------+
| FirstWritten     | timestamp | Time/date when first written            |
+------------------+-----------+-----------------------------------------+
| LastWritten      | timestamp | Time/date when last written             |
+------------------+-----------+-----------------------------------------+
| LabelDate        | timestamp | Time/date when tape labeled             |
+------------------+-----------+-----------------------------------------+
| VolJobs          | integer   | Number of jobs written to this media    |
+------------------+-----------+-----------------------------------------+
| VolFiles         | integer   | Number of files written to this media   |
+------------------+-----------+-----------------------------------------+
| VolBlocks        | integer   | Number of blocks written to this media  |
+------------------+-----------+-----------------------------------------+
| VolMounts        | integer   | Number of time media mounted            |
+------------------+-----------+-----------------------------------------+
| VolBytes         | bigint    | Number of bytes saved in Job            |
+------------------+-----------+-----------------------------------------+
| VolErrors        | integer   | Number of errors during Job             |
+------------------+-----------+-----------------------------------------+
| VolWrites        | integer   | Number of writes to media               |
+------------------+-----------+-----------------------------------------+
| VolCapacityBytes | bigint    | Capacity estimate for this volume       |
+------------------+-----------+-----------------------------------------+
| VolStatus        | text      | | Status of media:                      |
|                  |           | | Full, Archive, Append,                |
|                  |           |   Recycle, Read-Only, Disabled, Error,  |
|                  |           |   Busy                                  |
+------------------+-----------+-----------------------------------------+
| Enabled          | smallint  | Whether or not Volume can be written    |
+------------------+-----------+-----------------------------------------+
| Recycle          | smallint  | Whether or not Bareos can recycle the   |
|                  |           | Volumes: Yes, No                        |
+------------------+-----------+-----------------------------------------+
| ActionOnPurge    | smallint  | What happens to a Volume after purging  |
+------------------+-----------+-----------------------------------------+
| VolRetention     | bigint    | 64 bit seconds until expiration         |
+------------------+-----------+-----------------------------------------+
| VolUseDureation  | bigint    | 64 bit seconds volume can be used       |
+------------------+-----------+-----------------------------------------+
| MaxVolJobs       | integer   | maximum jobs to put on Volume           |
+------------------+-----------+-----------------------------------------+
| MaxVolFiles      | integer   | maximume EOF marks to put on Volume     |
+------------------+-----------+-----------------------------------------+
| MaxVolBytes      | bigint    | Maximum bytes to put on this media      |
+------------------+-----------+-----------------------------------------+
| InChanger        | smallint  | Whether or not Volume in autochanger    |
+------------------+-----------+-----------------------------------------+
| StorageId        | integer   | Storage record ID                       |
+------------------+-----------+-----------------------------------------+
| DeviceId         | integer   | Device record ID                        |
+------------------+-----------+-----------------------------------------+
| MediaAddressing  | smallint  | Method of addressing media              |
+------------------+-----------+-----------------------------------------+
| VolReadTime      | bigint    | Time Reading Volume                     |
+------------------+-----------+-----------------------------------------+
| VolWriteTime     | bigint    | Time Writing Volume                     |
+------------------+-----------+-----------------------------------------+
| EndFile          | integer   | End File number of Volume               |
+------------------+-----------+-----------------------------------------+
| EndBlock         | bigint    | End block number of Volume              |
+------------------+-----------+-----------------------------------------+
| LocationId       | integer   | Location record ID                      |
+------------------+-----------+-----------------------------------------+
| RecycleCount     | integer   | Number of times recycled                |
+------------------+-----------+-----------------------------------------+
| MinBlockSize     | integer   | Minimum block size on this media        |
+------------------+-----------+-----------------------------------------+
| MaxBlockSize     | integer   | Maximum block size on this media        |
+------------------+-----------+-----------------------------------------+
| InitialWrite     | timestamp | When Volume first written               |
+------------------+-----------+-----------------------------------------+
| ScratchPoolId    | integer   | Id of Scratch Pool                      |
+------------------+-----------+-----------------------------------------+
| RecyclePoolId    | integer   | Pool ID where to recycle Volume         |
+------------------+-----------+-----------------------------------------+
| EncryptionKey    | text      | Key used for encryptoion                |
+------------------+-----------+-----------------------------------------+
| Comment          | text      | User text field                         |
+------------------+-----------+-----------------------------------------+

Pool
~~~~

The **Pool** table contains one entry for each media pool controlled by
Bareos in this database.

+-----------------------+-----------------------+-----------------------+
| Column Name           | Data Type             | Remark                |
+=======================+=======================+=======================+
| PoolId                | serial                | Primary Key           |
+-----------------------+-----------------------+-----------------------+
| Name                  | text                  | Pool Name             |
+-----------------------+-----------------------+-----------------------+
| NumVols               | integer               | Number of Volumes in  |
|                       |                       | the Pool              |
+-----------------------+-----------------------+-----------------------+
| MaxVols               | integer               | Maximum Volumes in    |
|                       |                       | the Pool              |
+-----------------------+-----------------------+-----------------------+
| UseOnce               | smallint              | Use volume once       |
+-----------------------+-----------------------+-----------------------+
| UseCatalog            | smallint              | Set to use catalog    |
+-----------------------+-----------------------+-----------------------+
| AcceptAnyVolume       | smallint              | Accept any volume     |
|                       |                       | from Pool             |
+-----------------------+-----------------------+-----------------------+
| VolRetention          | bigint                | 64 bit seconds to     |
|                       |                       | retain volume         |
+-----------------------+-----------------------+-----------------------+
| VolUseDuration        | bigint                | 64 bit seconds volume |
|                       |                       | can be used           |
+-----------------------+-----------------------+-----------------------+
| MaxVolJobs            | integer               | max jobs on volume    |
+-----------------------+-----------------------+-----------------------+
| MaxVolFiles           | integer               | max EOF marks to put  |
|                       |                       | on Volume             |
+-----------------------+-----------------------+-----------------------+
| MaxVolBytes           | bigint                | max bytes to write on |
|                       |                       | Volume                |
+-----------------------+-----------------------+-----------------------+
| AutoPrune             | smallint              | yes or no for         |
|                       |                       | autopruning           |
+-----------------------+-----------------------+-----------------------+
| Recycle               | smallint              | yes or no for         |
|                       |                       | allowing auto         |
|                       |                       | recycling of Volume   |
+-----------------------+-----------------------+-----------------------+
| ActionOnPurge         | smallint              | Default Volume        |
|                       |                       | ActionOnPurge         |
+-----------------------+-----------------------+-----------------------+
| PoolType              | text                  | Backup, Copy, Cloned, |
|                       |                       | Archive, Migration    |
+-----------------------+-----------------------+-----------------------+
| LabelType             | integer               | Type of label         |
|                       |                       | ANSI/Bareos           |
+-----------------------+-----------------------+-----------------------+
| LabelFormat           | text                  | Label format          |
+-----------------------+-----------------------+-----------------------+
| Enabled               | smallint              | Whether or not Volume |
|                       |                       | can be written        |
+-----------------------+-----------------------+-----------------------+
| ScratchPoolId         | integer               | Id of Scratch Pool    |
+-----------------------+-----------------------+-----------------------+
| RecyclePoolId         | integer               | Pool ID where to      |
|                       |                       | recycle Volume        |
+-----------------------+-----------------------+-----------------------+
| NextPoolId            | integer               | Pool ID of next Pool  |
+-----------------------+-----------------------+-----------------------+
| MinBlockSize          | integer               | Pool ID of next Pool  |
+-----------------------+-----------------------+-----------------------+
| MaxBlockSize          | integer               | Pool ID of next Pool  |
+-----------------------+-----------------------+-----------------------+
| MigrationHighBytes    | bigint                | High water mark for   |
|                       |                       | migration             |
+-----------------------+-----------------------+-----------------------+
| MigrationLowBytes     | bigint                | Low water mark for    |
|                       |                       | migration             |
+-----------------------+-----------------------+-----------------------+
| MigrationTime         | bigint                | Time before migration |
+-----------------------+-----------------------+-----------------------+

In the **Media** table one or more records exist
for each of the Volumes contained in the Pool. The MediaType is defined
by the administrator, and corresponds to the MediaType specified in the
Director’s Storage definition record.

Client
~~~~~~

The **Client** table contains one entry for each machine backed up by
Bareos in this database. Normally the Name is a fully qualified domain
name.

+---------------+-----------+-------------------------------------+
| Column Name   | Data Type | Remark                              |
+===============+===========+=====================================+
| ClientId      | serial    | Primary Key                         |
+---------------+-----------+-------------------------------------+
| Name          | text      | File Services Name                  |
+---------------+-----------+-------------------------------------+
| UName         | text      | uname -a from Client (not yet used) |
+---------------+-----------+-------------------------------------+
| AutoPrune     | smallint  | yes or no for autopruning           |
+---------------+-----------+-------------------------------------+
| FileRetention | bigint    | 64 bit seconds to retain Files      |
+---------------+-----------+-------------------------------------+
| JobRentention | bigint    | 64 bit seconds to retain Job        |
+---------------+-----------+-------------------------------------+

Storage
~~~~~~~

The **Storage** table contains one entry for each Storage used.

+-------------+-----------+---------------------------------+
| Column Name | Data Type | Remark                          |
+=============+===========+=================================+
| StorageId   | serial    | Unique Id                       |
+-------------+-----------+---------------------------------+
| Name        | text      | Resource name of Storage device |
+-------------+-----------+---------------------------------+
| AutoChanger | integer   | Set if it is an autochanger     |
+-------------+-----------+---------------------------------+

Counters
~~~~~~~~

The **Counter** table contains one entry for each permanent counter
defined by the user.

+--------------+-----------+-----------------------------+
| Column Name  | Data Type | Remark                      |
+==============+===========+=============================+
| Counter      | serial    | Counter name                |
+--------------+-----------+-----------------------------+
| MinValue     | integer   | Start/Min value for counter |
+--------------+-----------+-----------------------------+
| MaxValue     | integer   | Max value for counter       |
+--------------+-----------+-----------------------------+
| CurrentValue | integer   | Current counter value       |
+--------------+-----------+-----------------------------+
| WrapCounter  | text      | Name of another counter     |
+--------------+-----------+-----------------------------+

Log
~~~

The **Log** table contains a log of all Job output.

+-------------+-----------+------------------------------+
| Column Name | Data Type | Remark                       |
+=============+===========+==============================+
| LogId       | serial    | Primary Key                  |
+-------------+-----------+------------------------------+
| JobId       | integer   | Points to Job record         |
+-------------+-----------+------------------------------+
| Time        | timestamp | Time/date log record created |
+-------------+-----------+------------------------------+
| LogText     | text      | Log text                     |
+-------------+-----------+------------------------------+

Location
~~~~~~~~

The **Location** table defines where a Volume is physically.

+-------------+-----------+-----------------------------------+
| Column Name | Data Type | Remark                            |
+=============+===========+===================================+
| LocationId  | serial    | Primary Key                       |
+-------------+-----------+-----------------------------------+
| Location    | text      | Text defining location            |
+-------------+-----------+-----------------------------------+
| Cost        | integer   | Relative cost of obtaining Volume |
+-------------+-----------+-----------------------------------+
| Enabled     | smallint  | Whether or not Volume is enabled  |
+-------------+-----------+-----------------------------------+

LocationLog
~~~~~~~~~~~

The **LocationLog** table contains a log of all Job output.

+-------------+-----------+-----------------------------------------------+
| Column      | Data      | Remark                                        |
| Name        | Type      |                                               |
+=============+===========+===============================================+
| LocLogId    | serial    | Primary Key                                   |
+-------------+-----------+-----------------------------------------------+
| Date        | timestamp | Time/date log record created                  |
+-------------+-----------+-----------------------------------------------+
| Comment     | text      | Time/date log record created                  |
+-------------+-----------+-----------------------------------------------+
| MediaId     | integer   | Points to Media record                        |
+-------------+-----------+-----------------------------------------------+
| LocationId  | integer   | Points to Location record                     |
+-------------+-----------+-----------------------------------------------+
| NewVolStaus | integer   | enum: Full, Archive, Append, Recycle, Purged  |
|             |           | Read-only, Disabled, Error, Busy, Used,       |
|             |           | Cleaning                                      |
+-------------+-----------+-----------------------------------------------+
| Enabled     | tinyint   | Whether or not Volume is enabled              |
+-------------+-----------+-----------------------------------------------+

Version
~~~~~~~

The **Version** table defines the Bareos database version number. Bareos
checks this number before reading the database to ensure that it is
compatible with the Bareos binary file.

+-------------+-----------+-------------+
| Column Name | Data Type | Remark      |
+=============+===========+=============+
| VersionId   | integer   | Primary Key |
+-------------+-----------+-------------+

BaseFiles
~~~~~~~~~

The **BaseFiles** table contains all the File references for a
particular JobId that point to a Base file.

+-------------+-----------+-------------------+
| Column Name | Data Type | Remark            |
+=============+===========+===================+
| BaseId      | serial    | Primary Key       |
+-------------+-----------+-------------------+
| JobId       | integer   | Reference to Job  |
+-------------+-----------+-------------------+
| FileId      | bigint    | Reference to File |
+-------------+-----------+-------------------+
| FileIndex   | integer   | File Index number |
+-------------+-----------+-------------------+
| BaseJobId   | integer   | JobId of Base Job |
+-------------+-----------+-------------------+

For example they were previously saved and hence were not saved in
the current JobId but in BaseJobId under FileId. FileIndex is the
index of the file, and is used for optimization of Restore jobs to
prevent the need to read the FileId record when creating the in
memory tree. This record is not yet implemented.

UML Diagram of Database Schema
------------------------------

.. uml:: catalog/schema_bareos_postgresql.puml

