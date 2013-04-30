USE ${db_name};
--
-- Note, we use BLOB rather than TEXT because in MySQL,
--  BLOBs are identical to TEXT except that BLOB is case
--  sensitive in sorts, which is what we want, and TEXT
--  is case insensitive.
--
CREATE TABLE Filename (
  FilenameId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
  Name BLOB NOT NULL,
  PRIMARY KEY(FilenameId),
  INDEX (Name(255))
  );

CREATE TABLE Path (
   PathId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Path BLOB NOT NULL,
   PRIMARY KEY(PathId),
   INDEX (Path(255))
   );


CREATE TABLE File (
   FileId BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
   FileIndex INTEGER UNSIGNED DEFAULT 0,
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   PathId INTEGER UNSIGNED NOT NULL REFERENCES Path,
   FilenameId INTEGER UNSIGNED NOT NULL REFERENCES Filename,
   MarkId INTEGER UNSIGNED DEFAULT 0,
   LStat TINYBLOB NOT NULL,
   MD5 TINYBLOB,
   PRIMARY KEY(FileId),
   INDEX (JobId),
   INDEX (JobId, PathId, FilenameId)
   );

--
-- Possibly add one or more of the following indexes
--  to the above File table if your Verifies are
--  too slow.
--
--  INDEX (PathId),
--  INDEX (FilenameId),
--  INDEX (FilenameId, PathId)
--  INDEX (JobId),
--

CREATE TABLE MediaType (
   MediaTypeId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   MediaType TINYBLOB NOT NULL,
   ReadOnly TINYINT DEFAULT 0,
   PRIMARY KEY(MediaTypeId)
   );

CREATE TABLE Storage (
   StorageId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Name TINYBLOB NOT NULL,
   AutoChanger TINYINT DEFAULT 0,
   PRIMARY KEY(StorageId)
   );

CREATE TABLE Device (
   DeviceId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Name TINYBLOB NOT NULL,
   MediaTypeId INTEGER UNSIGNED DEFAULT 0 REFERENCES MediaType,
   StorageId INTEGER UNSIGNED DEFAULT 0 REFERENCES Storage,
   DevMounts INTEGER UNSIGNED DEFAULT 0,
   DevReadBytes BIGINT UNSIGNED DEFAULT 0,
   DevWriteBytes BIGINT UNSIGNED DEFAULT 0,
   DevReadBytesSinceCleaning BIGINT UNSIGNED DEFAULT 0,
   DevWriteBytesSinceCleaning BIGINT UNSIGNED DEFAULT 0,
   DevReadTime BIGINT UNSIGNED DEFAULT 0,
   DevWriteTime BIGINT UNSIGNED DEFAULT 0,
   DevReadTimeSinceCleaning BIGINT UNSIGNED DEFAULT 0,
   DevWriteTimeSinceCleaning BIGINT UNSIGNED DEFAULT 0,
   CleaningDate DATETIME DEFAULT 0,
   CleaningPeriod BIGINT UNSIGNED DEFAULT 0,
   PRIMARY KEY(DeviceId)
   );


CREATE TABLE Job (
   JobId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Job TINYBLOB NOT NULL,
   Name TINYBLOB NOT NULL,
   Type BINARY(1) NOT NULL,
   Level BINARY(1) NOT NULL,
   ClientId INTEGER DEFAULT 0 REFERENCES Client,
   JobStatus BINARY(1) NOT NULL,
   SchedTime DATETIME DEFAULT 0,
   StartTime DATETIME DEFAULT 0,
   EndTime DATETIME DEFAULT 0,
   RealEndTime DATETIME DEFAULT 0,
   JobTDate BIGINT UNSIGNED DEFAULT 0,
   VolSessionId INTEGER UNSIGNED DEFAULT 0,
   VolSessionTime INTEGER UNSIGNED DEFAULT 0,
   JobFiles INTEGER UNSIGNED DEFAULT 0,
   JobBytes BIGINT UNSIGNED DEFAULT 0,
   ReadBytes BIGINT UNSIGNED DEFAULT 0,
   JobErrors INTEGER UNSIGNED DEFAULT 0,
   JobMissingFiles INTEGER UNSIGNED DEFAULT 0,
   PoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   FileSetId INTEGER UNSIGNED DEFAULT 0 REFERENCES FileSet,
   PriorJobId INTEGER UNSIGNED DEFAULT 0 REFERENCES Job,
   PurgedFiles TINYINT DEFAULT 0,
   HasBase TINYINT DEFAULT 0,
   PRIMARY KEY(JobId),
   INDEX (Name(128))
   );

-- Create a table like Job for long term statistics 
CREATE TABLE JobHisto (
   JobId INTEGER UNSIGNED NOT NULL,
   Job TINYBLOB NOT NULL,
   Name TINYBLOB NOT NULL,
   Type BINARY(1) NOT NULL,
   Level BINARY(1) NOT NULL,
   ClientId INTEGER DEFAULT 0,
   JobStatus BINARY(1) NOT NULL,
   SchedTime DATETIME DEFAULT 0,
   StartTime DATETIME DEFAULT 0,
   EndTime DATETIME DEFAULT 0,
   RealEndTime DATETIME DEFAULT 0,
   JobTDate BIGINT UNSIGNED DEFAULT 0,
   VolSessionId INTEGER UNSIGNED DEFAULT 0,
   VolSessionTime INTEGER UNSIGNED DEFAULT 0,
   JobFiles INTEGER UNSIGNED DEFAULT 0,
   JobBytes BIGINT UNSIGNED DEFAULT 0,
   ReadBytes BIGINT UNSIGNED DEFAULT 0,
   JobErrors INTEGER UNSIGNED DEFAULT 0,
   JobMissingFiles INTEGER UNSIGNED DEFAULT 0,
   PoolId INTEGER UNSIGNED DEFAULT 0,
   FileSetId INTEGER UNSIGNED DEFAULT 0,
   PriorJobId INTEGER UNSIGNED DEFAULT 0,
   PurgedFiles TINYINT DEFAULT 0,
   HasBase TINYINT DEFAULT 0,
   INDEX (StartTime)
   );

CREATE TABLE Location (
   LocationId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Location TINYBLOB NOT NULL,
   Cost INTEGER DEFAULT 0,
   Enabled TINYINT,
   PRIMARY KEY(LocationId)
   );

CREATE TABLE LocationLog (
   LocLogId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Date DATETIME DEFAULT 0,
   Comment BLOB NOT NULL,
   MediaId INTEGER UNSIGNED DEFAULT 0 REFERENCES Media,
   LocationId INTEGER UNSIGNED DEFAULT 0 REFERENCES Location,
   NewVolStatus ENUM('Full', 'Archive', 'Append', 'Recycle', 'Purged',
    'Read-Only', 'Disabled', 'Error', 'Busy', 'Used', 'Cleaning') NOT NULL,
   NewEnabled TINYINT,
   PRIMARY KEY(LocLogId)
);


CREATE TABLE FileSet (
   FileSetId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   FileSet TINYBLOB NOT NULL,
   MD5 TINYBLOB,
   CreateTime DATETIME DEFAULT 0,
   PRIMARY KEY(FileSetId)
   );

CREATE TABLE JobMedia (
   JobMediaId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   MediaId INTEGER UNSIGNED NOT NULL REFERENCES Media,
   FirstIndex INTEGER UNSIGNED DEFAULT 0,
   LastIndex INTEGER UNSIGNED DEFAULT 0,
   StartFile INTEGER UNSIGNED DEFAULT 0,
   EndFile INTEGER UNSIGNED DEFAULT 0,
   StartBlock INTEGER UNSIGNED DEFAULT 0,
   EndBlock INTEGER UNSIGNED DEFAULT 0,
   VolIndex INTEGER UNSIGNED DEFAULT 0,
   Copy INTEGER UNSIGNED DEFAULT 0,
   Stripe INTEGER UNSIGNED DEFAULT 0,
   PRIMARY KEY(JobMediaId),
   INDEX (JobId, MediaId)
   );


CREATE TABLE Media (
   MediaId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   VolumeName TINYBLOB NOT NULL,
   Slot INTEGER DEFAULT 0,
   PoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   MediaType TINYBLOB NOT NULL,
   MediaTypeId INTEGER UNSIGNED DEFAULT 0 REFERENCES MediaType,
   LabelType TINYINT DEFAULT 0,
   FirstWritten DATETIME DEFAULT 0,
   LastWritten DATETIME DEFAULT 0,
   LabelDate DATETIME DEFAULT 0,
   VolJobs INTEGER UNSIGNED DEFAULT 0,
   VolFiles INTEGER UNSIGNED DEFAULT 0,
   VolBlocks INTEGER UNSIGNED DEFAULT 0,
   VolMounts INTEGER UNSIGNED DEFAULT 0,
   VolBytes BIGINT UNSIGNED DEFAULT 0,
   VolParts INTEGER UNSIGNED DEFAULT 0,
   VolErrors INTEGER UNSIGNED DEFAULT 0,
   VolWrites INTEGER UNSIGNED DEFAULT 0,
   VolCapacityBytes BIGINT UNSIGNED DEFAULT 0,
   VolStatus ENUM('Full', 'Archive', 'Append', 'Recycle', 'Purged',
    'Read-Only', 'Disabled', 'Error', 'Busy', 'Used', 'Cleaning') NOT NULL,
   Enabled TINYINT DEFAULT 1,
   Recycle TINYINT DEFAULT 0,
   ActionOnPurge     TINYINT    DEFAULT 0,
   VolRetention BIGINT UNSIGNED DEFAULT 0,
   VolUseDuration BIGINT UNSIGNED DEFAULT 0,
   MaxVolJobs INTEGER UNSIGNED DEFAULT 0,
   MaxVolFiles INTEGER UNSIGNED DEFAULT 0,
   MaxVolBytes BIGINT UNSIGNED DEFAULT 0,
   InChanger TINYINT DEFAULT 0,
   StorageId INTEGER UNSIGNED DEFAULT 0 REFERENCES Storage,
   DeviceId INTEGER UNSIGNED DEFAULT 0 REFERENCES Device,
   MediaAddressing TINYINT DEFAULT 0,
   VolReadTime BIGINT UNSIGNED DEFAULT 0,
   VolWriteTime BIGINT UNSIGNED DEFAULT 0,
   EndFile INTEGER UNSIGNED DEFAULT 0,
   EndBlock INTEGER UNSIGNED DEFAULT 0,
   LocationId INTEGER UNSIGNED DEFAULT 0 REFERENCES Location,
   RecycleCount INTEGER UNSIGNED DEFAULT 0,
   InitialWrite DATETIME DEFAULT 0,
   ScratchPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   RecyclePoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   Comment BLOB,
   PRIMARY KEY(MediaId),
   UNIQUE (VolumeName(128)),
   INDEX (PoolId)
   );

CREATE TABLE Pool (
   PoolId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Name TINYBLOB NOT NULL,
   NumVols INTEGER UNSIGNED DEFAULT 0,
   MaxVols INTEGER UNSIGNED DEFAULT 0,
   UseOnce TINYINT DEFAULT 0,
   UseCatalog TINYINT DEFAULT 0,
   AcceptAnyVolume TINYINT DEFAULT 0,
   VolRetention BIGINT UNSIGNED DEFAULT 0,
   VolUseDuration BIGINT UNSIGNED DEFAULT 0,
   MaxVolJobs INTEGER UNSIGNED DEFAULT 0,
   MaxVolFiles INTEGER UNSIGNED DEFAULT 0,
   MaxVolBytes BIGINT UNSIGNED DEFAULT 0,
   AutoPrune TINYINT DEFAULT 0,
   Recycle TINYINT DEFAULT 0,
   ActionOnPurge     TINYINT    DEFAULT 0,
   PoolType ENUM('Backup', 'Copy', 'Cloned', 'Archive', 'Migration', 'Scratch') NOT NULL,
   LabelType TINYINT DEFAULT 0,
   LabelFormat TINYBLOB,
   Enabled TINYINT DEFAULT 1,
   ScratchPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   RecyclePoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   NextPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   MigrationHighBytes BIGINT UNSIGNED DEFAULT 0,
   MigrationLowBytes BIGINT UNSIGNED DEFAULT 0,
   MigrationTime BIGINT UNSIGNED DEFAULT 0,
   UNIQUE (Name(128)),
   PRIMARY KEY (PoolId)
   );


CREATE TABLE Client (
   ClientId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Name TINYBLOB NOT NULL,
   Uname TINYBLOB NOT NULL,       /* full uname -a of client */
   AutoPrune TINYINT DEFAULT 0,
   FileRetention BIGINT UNSIGNED DEFAULT 0,
   JobRetention  BIGINT UNSIGNED DEFAULT 0,
   UNIQUE (Name(128)),
   PRIMARY KEY(ClientId)
   );

CREATE TABLE Log (
   LogId INTEGER UNSIGNED AUTO_INCREMENT,
   JobId INTEGER UNSIGNED DEFAULT 0 REFERENCES Job,
   Time DATETIME DEFAULT 0,
   LogText BLOB NOT NULL,
   PRIMARY KEY(LogId),
   INDEX (JobId)
   );


CREATE TABLE BaseFiles (
   BaseId INTEGER UNSIGNED AUTO_INCREMENT,
   BaseJobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   FileId BIGINT UNSIGNED NOT NULL REFERENCES File,
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



CREATE TABLE Counters (
   Counter TINYBLOB NOT NULL,
   MinValue INTEGER DEFAULT 0,
   MaxValue INTEGER DEFAULT 0,
   CurrentValue INTEGER DEFAULT 0,
   WrapCounter TINYBLOB NOT NULL,
   PRIMARY KEY (Counter(128))
   );

CREATE TABLE CDImages (
   MediaId INTEGER UNSIGNED NOT NULL,
   LastBurn DATETIME NOT NULL,
   PRIMARY KEY (MediaId)
   );

CREATE TABLE Status (
   JobStatus CHAR(1) BINARY NOT NULL,
   JobStatusLong BLOB, 
   PRIMARY KEY (JobStatus)
   );

INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('C', 'Created, not yet running'),
   ('R', 'Running'),
   ('B', 'Blocked'),
   ('T', 'Completed successfully'),
   ('E', 'Terminated with errors'),
   ('e', 'Non-fatal error'),
   ('f', 'Fatal error'),
   ('D', 'Verify found differences'),
   ('A', 'Canceled by user'),
   ('F', 'Waiting for Client'),
   ('S', 'Waiting for Storage daemon'),
   ('m', 'Waiting for new media'),
   ('M', 'Waiting for media mount'),
   ('s', 'Waiting for storage resource'),
   ('j', 'Waiting for job resource'),
   ('c', 'Waiting for client resource'),
   ('d', 'Waiting on maximum jobs'),
   ('t', 'Waiting on start time'),
   ('p', 'Waiting on higher priority jobs'),
   ('i', 'Doing batch insert file records'),
   ('a', 'SD despooling attributes');

CREATE TABLE Version (
   VersionId INTEGER UNSIGNED NOT NULL 
   );

-- Initialize Version            
INSERT INTO Version (VersionId) VALUES (11);

