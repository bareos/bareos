--
-- Note, we use BLOB rather than TEXT because in MySQL,
--  BLOBs are identical to TEXT except that BLOB is case
--  sensitive in sorts, which is what we want, and TEXT
--  is case insensitive.
--
CREATE TABLE Path (
   PathId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Path BLOB NOT NULL,
   PRIMARY KEY(PathId),
   INDEX (Path(255))
);

-- We strongly recommend to avoid the temptation to add new indexes.
-- In general, these will cause very significant performance
-- problems in other areas.  A better approach is to carefully check
-- that all your memory configuration parameters are
-- suitable for the size of your installation. If you backup
-- millions of files, you need to adapt the database memory
-- configuration parameters concerning sorting, joining and global
-- memory.  By default, sort and join parameters are very small
-- (sometimes 8Kb), and having sufficient memory specified by those
-- parameters is extremely important to run fast.

-- In File table
-- FileIndex is 0 for FT_DELETED files
-- Name is '' for directories
-- The index INDEX (PathId, JobId, FileIndex) is
-- important for bvfs performance, especially
-- for .bvfs_lsdirs which is used by bareos-webui.
CREATE TABLE File (
   FileId           BIGINT    UNSIGNED  NOT NULL  AUTO_INCREMENT,
   FileIndex        INTEGER   UNSIGNED            DEFAULT 0,
   JobId            INTEGER   UNSIGNED  NOT NULL  REFERENCES Job,
   PathId           INTEGER   UNSIGNED  NOT NULL  REFERENCES Path,
   DeltaSeq         SMALLINT  UNSIGNED            DEFAULT 0,
   MarkId           INTEGER   UNSIGNED            DEFAULT 0,
   LStat            TINYBLOB            NOT NULL,
   MD5              TINYBLOB            NOT NULL,
   Fhinfo           NUMERIC(20)                   DEFAULT 0,
   Fhnode           NUMERIC(20)                   DEFAULT 0,
   Name             BLOB                NOT NULL,
   PRIMARY KEY (FileId),
   INDEX (JobId, PathId, Name(255)),
   INDEX (PathId, JobId, FileIndex)
);

--
-- Possibly add one or more of the following indexes
--  to the above File table if your Verifies are
--  too slow, but they can slow down backups.
--
--  INDEX (PathId),

CREATE TABLE RestoreObject (
   RestoreObjectId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   ObjectName BLOB NOT NULL,
   RestoreObject LONGBLOB NOT NULL,
   PluginName TINYBLOB NOT NULL,
   ObjectLength INTEGER DEFAULT 0,
   ObjectFullLength INTEGER DEFAULT 0,
   ObjectIndex INTEGER DEFAULT 0,
   ObjectType INTEGER DEFAULT 0,
   FileIndex INTEGER UNSIGNED DEFAULT 0,
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   ObjectCompression INTEGER DEFAULT 0,
   PRIMARY KEY(RestoreObjectId),
   INDEX (JobId)
);

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
   HasCache TINYINT DEFAULT 0,
   Reviewed TINYINT DEFAULT 0,
   Comment BLOB,
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
   HasCache TINYINT DEFAULT 0,
   Reviewed TINYINT DEFAULT 0,
   Comment BLOB,
   INDEX (JobId),
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
   FileSetText BLOB NOT NULL,
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
   JobBytes NUMERIC(20) DEFAULT 0,
   VolIndex INTEGER UNSIGNED DEFAULT 0,
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
   VolErrors INTEGER UNSIGNED DEFAULT 0,
   VolWrites INTEGER UNSIGNED DEFAULT 0,
   VolCapacityBytes BIGINT UNSIGNED DEFAULT 0,
   VolStatus ENUM('Full', 'Archive', 'Append', 'Recycle', 'Purged',
                  'Read-Only', 'Disabled', 'Error', 'Busy', 'Used',
                  'Cleaning') NOT NULL,
   Enabled TINYINT DEFAULT 1,
   Recycle TINYINT DEFAULT 0,
   ActionOnPurge     TINYINT	DEFAULT 0,
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
   MinBlockSize INTEGER UNSIGNED DEFAULT 0,
   MaxBlockSize INTEGER UNSIGNED DEFAULT 0,
   InitialWrite DATETIME DEFAULT 0,
   ScratchPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   RecyclePoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   EncryptionKey TINYBLOB,
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
   ActionOnPurge     TINYINT	DEFAULT 0,
   PoolType ENUM('Backup', 'Copy', 'Cloned', 'Archive', 'Migration', 'Scratch') NOT NULL,
   LabelType TINYINT DEFAULT 0,
   LabelFormat TINYBLOB,
   Enabled TINYINT DEFAULT 1,
   ScratchPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   RecyclePoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   NextPoolId INTEGER UNSIGNED DEFAULT 0 REFERENCES Pool,
   MinBlockSize INTEGER UNSIGNED DEFAULT 0,
   MaxBlockSize INTEGER UNSIGNED DEFAULT 0,
   MigrationHighBytes BIGINT UNSIGNED DEFAULT 0,
   MigrationLowBytes BIGINT UNSIGNED DEFAULT 0,
   MigrationTime BIGINT UNSIGNED DEFAULT 0,
   UNIQUE (Name(128)),
   PRIMARY KEY (PoolId)
);

CREATE TABLE Client (
   ClientId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   Name TINYBLOB NOT NULL,
   Uname TINYBLOB NOT NULL,	  /* full uname -a of client */
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

CREATE INDEX basefiles_jobid_idx ON BaseFiles ( JobId );

-- This table seems to be obsolete
-- CREATE TABLE UnsavedFiles (
--    UnsavedId INTEGER UNSIGNED AUTO_INCREMENT,
--    JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
--    PathId INTEGER UNSIGNED NOT NULL REFERENCES Path,
--    FilenameId INTEGER UNSIGNED NOT NULL REFERENCES Filename,
--    PRIMARY KEY (UnsavedId)
-- );

CREATE TABLE Counters (
   Counter TINYBLOB NOT NULL,
   `MinValue` INTEGER DEFAULT 0,
   `MaxValue` INTEGER DEFAULT 0,
   CurrentValue INTEGER DEFAULT 0,
   WrapCounter TINYBLOB NOT NULL,
   PRIMARY KEY (Counter(128))
);

CREATE TABLE Status (
   JobStatus CHAR(1) BINARY NOT NULL,
   JobStatusLong BLOB,
   Severity INT,
   PRIMARY KEY (JobStatus)
);

CREATE TABLE PathHierarchy
(
   PathId integer NOT NULL,
   PPathId integer NOT NULL,
   CONSTRAINT pathhierarchy_pkey PRIMARY KEY (PathId)
);

CREATE INDEX pathhierarchy_ppathid
	  ON PathHierarchy (PPathId);

CREATE TABLE PathVisibility
(
   PathId integer NOT NULL,
   JobId integer NOT NULL,
   Size int8 DEFAULT 0,
   Files int4 DEFAULT 0,
   CONSTRAINT pathvisibility_pkey PRIMARY KEY (JobId, PathId)
);
CREATE INDEX pathvisibility_jobid
	     ON PathVisibility (JobId);

CREATE TABLE Version (
   VersionId INTEGER UNSIGNED NOT NULL
);

CREATE TABLE Quota (
   ClientId INTEGER DEFAULT 0 REFERENCES Client,
   GraceTime BIGINT DEFAULT 0,
   QuotaLimit BIGINT UNSIGNED DEFAULT 0,
   PRIMARY KEY (ClientId)
);

CREATE TABLE NDMPLevelMap (
   ClientId INTEGER DEFAULT 0 REFERENCES Client,
   FileSetId INTEGER UNSIGNED DEFAULT 0 REFERENCES FileSet,
   FileSystem TINYBLOB NOT NULL,
   DumpLevel INTEGER NOT NULL,
   CONSTRAINT NDMPLevelMap_pkey PRIMARY KEY (ClientId, FilesetId, FileSystem(256))
);

CREATE TABLE NDMPJobEnvironment (
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   FileIndex INTEGER UNSIGNED NOT NULL,
   EnvName TINYBLOB NOT NULL,
   EnvValue TINYBLOB NOT NULL,
   CONSTRAINT NDMPJobEnvironment_pkey PRIMARY KEY (JobId, FileIndex, EnvName(256))
);

CREATE TABLE DeviceStats (
   DeviceId INTEGER UNSIGNED DEFAULT 0 REFERENCES Device,
   SampleTime DATETIME NOT NULL,
   ReadTime BIGINT UNSIGNED DEFAULT 0,
   WriteTime BIGINT UNSIGNED DEFAULT 0,
   ReadBytes BIGINT UNSIGNED DEFAULT 0,
   WriteBytes BIGINT UNSIGNED DEFAULT 0,
   SpoolSize BIGINT UNSIGNED DEFAULT 0,
   NumWaiting INTEGER DEFAULT 0,
   NumWriters INTEGER DEFAULT 0,
   MediaId INTEGER UNSIGNED DEFAULT 0 REFERENCES Media,
   VolCatBytes BIGINT UNSIGNED DEFAULT 0,
   VolCatFiles BIGINT UNSIGNED DEFAULT 0,
   VolCatBlocks BIGINT UNSIGNED DEFAULT 0
);

CREATE TABLE JobStats (
   DeviceId INTEGER UNSIGNED DEFAULT 0 REFERENCES Device,
   SampleTime DATETIME NOT NULL,
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   JobFiles INTEGER UNSIGNED DEFAULT 0,
   JobBytes BIGINT UNSIGNED DEFAULT 0
);

CREATE TABLE TapeAlerts (
   DeviceId INTEGER UNSIGNED DEFAULT 0 REFERENCES Device,
   SampleTime DATETIME NOT NULL,
   AlertFlags BIGINT UNSIGNED DEFAULT 0
);

INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('C', 'Created, not yet running', 15),
   ('R', 'Running', 15),
   ('B', 'Blocked', 15),
   ('T', 'Completed successfully', 10),
   ('E', 'Terminated with errors', 25),
   ('e', 'Non-fatal error', 20),
   ('f', 'Fatal error', 100),
   ('D', 'Verify found differences', 15),
   ('A', 'Canceled by user', 90),
   ('I', 'Incomplete job', 15),
   ('L', 'Committing data', 15),
   ('W', 'Terminated with warnings', 20),
   ('l', 'Doing data despooling', 15),
   ('q', 'Queued waiting for device', 15),
   ('F', 'Waiting for Client', 15),
   ('S', 'Waiting for Storage daemon', 15),
   ('m', 'Waiting for new media', 15),
   ('M', 'Waiting for media mount', 15),
   ('s', 'Waiting for storage resource', 15),
   ('j', 'Waiting for job resource', 15),
   ('c', 'Waiting for client resource', 15),
   ('d', 'Waiting on maximum jobs', 15),
   ('t', 'Waiting on start time', 15),
   ('p', 'Waiting on higher priority jobs', 15),
   ('i', 'Doing batch insert file records', 15),
   ('a', 'SD despooling attributes', 15);

-- Initialize Version
--   DELETE should not be required,
--   but prevents errors if create script is called multiple times
DELETE FROM Version WHERE VersionId<=2171;
INSERT INTO Version (VersionId) VALUES (2171);
