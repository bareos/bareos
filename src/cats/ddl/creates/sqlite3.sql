--
-- Sqlite:
--
-- make this as close to MySQL as possible.
-- sqlite:
--   INTEGER PRIMARY KEY is autoincremental by default.
--   index name must be unique per database, so name it to table_column_column_...

CREATE TABLE Path (
   PathId INTEGER,
   Path TEXT DEFAULT '',
   PRIMARY KEY(PathId)
);
CREATE INDEX inx2 ON Path (Path);

-- In File table
-- FileIndex is 0 for FT_DELETED files
-- Name is '' for directories
CREATE TABLE File (
   FileId           INTEGER             NOT NULL,
   FileIndex        INTEGER   UNSIGNED            DEFAULT 0,
   JobId            INTEGER   UNSIGNED  NOT NULL  REFERENCES Job,
   PathId           INTEGER   UNSIGNED  NOT NULL  REFERENCES Path,
   Name             BLOB                NOT NULL,
   DeltaSeq         SMALLINT  UNSIGNED            DEFAULT 0,
   MarkId           INTEGER   UNSIGNED            DEFAULT 0,
   LStat            TINYBLOB            NOT NULL,
   MD5              TINYBLOB            NOT NULL,
   Fhinfo           TEXT                          DEFAULT 0,
   Fhnode           TEXT                          DEFAULT 0,
   PRIMARY KEY (FileId)
);
CREATE INDEX File_JobId ON File (JobId);
CREATE INDEX File_JobId_PathId_Name ON File (JobId, PathId, Name);


CREATE TABLE RestoreObject (
   RestoreObjectId INTEGER,
   ObjectName TEXT DEFAULT '',
   RestoreObject TEXT DEFAULT '',
   PluginName TEXT DEFAULT '',
   ObjectLength INTEGER DEFAULT 0,
   ObjectFullLength INTEGER DEFAULT 0,
   ObjectIndex INTEGER DEFAULT 0,
   ObjectType INTEGER DEFAULT 0,
   FileIndex INTEGER UNSIGNED DEFAULT 0,
   ObjectCompression INTEGER DEFAULT 0,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   PRIMARY KEY(RestoreObjectId)
);
CREATE INDEX restore_jobid_idx ON RestoreObject (JobId);

CREATE TABLE Job (
   JobId INTEGER,
   Job VARCHAR(128) NOT NULL,
   Name VARCHAR(128) NOT NULL,
   Type CHAR(1) NOT NULL,
   Level CHAR(1) NOT NULL,
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   JobStatus CHAR(1) NOT NULL,
   SchedTime DATETIME NOT NULL,
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
   PoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   FileSetId INTEGER UNSIGNED REFERENCES FileSet DEFAULT 0,
   PriorJobId INTEGER UNSIGNED REFERENCES Job DEFAULT 0,
   PurgedFiles TINYINT DEFAULT 0,
   HasBase TINYINT DEFAULT 0,
   HasCache TINYINT DEFAULT 0,
   Reviewed TINYINT DEFAULT 0,
   Comment TEXT,
   PRIMARY KEY(JobId)
);
CREATE INDEX inx6 ON Job (Name);

-- Create a table like Job for long term statistics
CREATE TABLE JobHisto (
   JobId INTEGER,
   Job VARCHAR(128) NOT NULL,
   Name VARCHAR(128) NOT NULL,
   Type CHAR(1) NOT NULL,
   Level CHAR(1) NOT NULL,
   ClientId INTEGER DEFAULT 0,
   JobStatus CHAR(1) NOT NULL,
   SchedTime DATETIME NOT NULL,
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
   Comment TEXT
);
CREATE INDEX inx61 ON JobHisto (StartTime);

CREATE TABLE Location (
   LocationId INTEGER,
   Location TEXT NOT NULL,
   Cost INTEGER DEFAULT 0,
   Enabled TINYINT,
   PRIMARY KEY(LocationId)
);

CREATE TABLE LocationLog (
   LocLogId INTEGER,
   Date DATETIME NOT NULL,
   Comment TEXT NOT NULL,
   MediaId INTEGER UNSIGNED REFERENCES Media DEFAULT 0,
   LocationId INTEGER UNSIGNED REFERENCES LocationId DEFAULT 0,
   NewVolStatus VARCHAR(20) NOT NULL,
   NewEnabled TINYINT NOT NULL,
   PRIMARY KEY(LocLogId)
);

CREATE TABLE Log (
   LogId INTEGER,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   Time DATETIME NOT NULL,
   LogText TEXT NOT NULL,
   PRIMARY KEY(LogId)
);
CREATE INDEX LogInx1 ON Log (JobId);

CREATE TABLE FileSet (
   FileSetId INTEGER,
   FileSet VARCHAR(128) NOT NULL,
   MD5 VARCHAR(25) NOT NULL,
   CreateTime DATETIME DEFAULT 0,
   FileSetText TEXT DEFAULT '',
   PRIMARY KEY(FileSetId)
);

CREATE TABLE JobMedia (
   JobMediaId INTEGER,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   MediaId INTEGER UNSIGNED REFERENCES Media NOT NULL,
   FirstIndex INTEGER UNSIGNED NOT NULL,
   LastIndex INTEGER UNSIGNED NOT NULL,
   StartFile INTEGER UNSIGNED DEFAULT 0,
   EndFile INTEGER UNSIGNED DEFAULT 0,
   StartBlock INTEGER UNSIGNED DEFAULT 0,
   EndBlock INTEGER UNSIGNED DEFAULT 0,
   JobBytes TEXT DEFAULT 0,
   VolIndex INTEGER UNSIGNED DEFAULT 0,
   PRIMARY KEY(JobMediaId)
);
CREATE INDEX inx7 ON JobMedia (JobId, MediaId);


CREATE TABLE Media (
   MediaId INTEGER,
   VolumeName VARCHAR(128) NOT NULL,
   Slot INTEGER DEFAULT 0,
   PoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   MediaType VARCHAR(128) NOT NULL,
   MediaTypeId INTEGER UNSIGNED REFERENCES MediaType DEFAULT 0,
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
   VolStatus VARCHAR(20) NOT NULL,
   Enabled TINYINT DEFAULT 1,
   Recycle TINYINT DEFAULT 0,
   ActionOnPurge     TINYINT	DEFAULT 0,
   VolRetention BIGINT UNSIGNED DEFAULT 0,
   VolUseDuration BIGINT UNSIGNED DEFAULT 0,
   MaxVolJobs INTEGER UNSIGNED DEFAULT 0,
   MaxVolFiles INTEGER UNSIGNED DEFAULT 0,
   MaxVolBytes BIGINT UNSIGNED DEFAULT 0,
   InChanger TINYINT DEFAULT 0,
   StorageId INTEGER UNSIGNED REFERENCES Storage DEFAULT 0,
   DeviceId INTEGER UNSIGNED REFERENCES Device DEFAULT 0,
   MediaAddressing TINYINT DEFAULT 0,
   VolReadTime BIGINT UNSIGNED DEFAULT 0,
   VolWriteTime BIGINT UNSIGNED DEFAULT 0,
   EndFile INTEGER UNSIGNED DEFAULT 0,
   EndBlock INTEGER UNSIGNED DEFAULT 0,
   LocationId INTEGER UNSIGNED REFERENCES Location DEFAULT 0,
   RecycleCount INTEGER UNSIGNED DEFAULT 0,
   MinBlockSize INTEGER UNSIGNED DEFAULT 0,
   MaxBlockSize INTEGER UNSIGNED DEFAULT 0,
   InitialWrite DATETIME DEFAULT 0,
   ScratchPoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   RecyclePoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   EncryptionKey VARCHAR(128),
   Comment TEXT,
   PRIMARY KEY(MediaId)
);
CREATE INDEX inx8 ON Media (PoolId);

CREATE TABLE MediaType (
   MediaTypeId INTEGER,
   MediaType VARCHAR(128) NOT NULL,
   ReadOnly TINYINT DEFAULT 0,
   PRIMARY KEY(MediaTypeId)
);

CREATE TABLE Storage (
   StorageId INTEGER,
   Name VARCHAR(128) NOT NULL,
   AutoChanger TINYINT DEFAULT 0,
   PRIMARY KEY(StorageId)
);

CREATE TABLE Device (
   DeviceId INTEGER,
   Name VARCHAR(128) NOT NULL,
   MediaTypeId INTEGER UNSIGNED REFERENCES MediaType NOT NULL,
   StorageId INTEGER UNSIGNED REFERENCES Storage,
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

CREATE TABLE Pool (
   PoolId INTEGER,
   Name VARCHAR(128) NOT NULL,
   NumVols INTEGER UNSIGNED DEFAULT 0,
   MaxVols INTEGER UNSIGNED DEFAULT 0,
   UseOnce TINYINT DEFAULT 0,
   UseCatalog TINYINT DEFAULT 1,
   AcceptAnyVolume TINYINT DEFAULT 0,
   VolRetention BIGINT UNSIGNED DEFAULT 0,
   VolUseDuration BIGINT UNSIGNED DEFAULT 0,
   MaxVolJobs INTEGER UNSIGNED DEFAULT 0,
   MaxVolFiles INTEGER UNSIGNED DEFAULT 0,
   MaxVolBytes BIGINT UNSIGNED DEFAULT 0,
   AutoPrune TINYINT DEFAULT 0,
   Recycle TINYINT DEFAULT 0,
   ActionOnPurge     TINYINT	DEFAULT 0,
   PoolType VARCHAR(20) NOT NULL,
   LabelType TINYINT DEFAULT 0,
   LabelFormat VARCHAR(128) NOT NULL,
   Enabled TINYINT DEFAULT 1,
   ScratchPoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   RecyclePoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   NextPoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   MinBlockSize INTEGER UNSIGNED DEFAULT 0,
   MaxBlockSize INTEGER UNSIGNED DEFAULT 0,
   MigrationHighBytes BIGINT UNSIGNED DEFAULT 0,
   MigrationLowBytes BIGINT UNSIGNED DEFAULT 0,
   MigrationTime BIGINT UNSIGNED DEFAULT 0,
   UNIQUE (Name),
   PRIMARY KEY (PoolId)
);

CREATE TABLE Client (
   ClientId INTEGER,
   Name VARCHAR(128) NOT NULL,
   Uname VARCHAR(255) NOT NULL,   -- uname -a field
   AutoPrune TINYINT DEFAULT 0,
   FileRetention BIGINT UNSIGNED DEFAULT 0,
   JobRetention  BIGINT UNSIGNED DEFAULT 0,
   UNIQUE (Name),
   PRIMARY KEY(ClientId)
);

CREATE TABLE BaseFiles (
   BaseId INTEGER,
   BaseJobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   FileId INTEGER UNSIGNED REFERENCES File NOT NULL,
   FileIndex INTEGER UNSIGNED,
   PRIMARY KEY(BaseId)
);

CREATE INDEX basefiles_jobid_idx ON BaseFiles ( JobId );

-- This table seems to be obsolete
-- CREATE TABLE UnsavedFiles (
--    UnsavedId INTEGER,
--    JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
--    PathId INTEGER UNSIGNED REFERENCES Path NOT NULL,
--    FilenameId INTEGER UNSIGNED REFERENCES Filename NOT NULL,
--    PRIMARY KEY (UnsavedId)
-- );

CREATE TABLE NextId (
   id INTEGER UNSIGNED DEFAULT 0,
   TableName TEXT NOT NULL,
   PRIMARY KEY (TableName)
);

-- Initialize JobId to start at 1
INSERT INTO NextId (id, TableName) VALUES (1, 'Job');

CREATE TABLE Version (
   VersionId INTEGER UNSIGNED NOT NULL
);

CREATE TABLE Counters (
   Counter TEXT NOT NULL,
   MinValue INTEGER DEFAULT 0,
   MaxValue INTEGER DEFAULT 0,
   CurrentValue INTEGER DEFAULT 0,
   WrapCounter TEXT NOT NULL,
   PRIMARY KEY (Counter)
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

CREATE TABLE Status (
   JobStatus CHAR(1) NOT NULL,
   JobStatusLong BLOB,
   Severity INT,
   PRIMARY KEY (JobStatus)
);

CREATE TABLE Quota (
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   GraceTime INTEGER UNSIGNED DEFAULT 0,
   QuotaLimit BIGINT UNSIGNED DEFAULT 0,
   PRIMARY KEY (ClientId)
);

CREATE TABLE NDMPLevelMap (
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   FileSetId INTEGER UNSIGNED REFERENCES FileSet DEFAULT 0,
   FileSystem TEXT DEFAULT '',
   DumpLevel INTEGER UNSIGNED DEFAULT 0,
   CONSTRAINT NDMPLevelMap_pkey PRIMARY KEY (ClientId, FilesetId, FileSystem)
);

CREATE TABLE NDMPJobEnvironment (
   JobId integer NOT NULL,
   FileIndex INTEGER UNSIGNED NOT NULL,
   EnvName TEXT NOT NULL,
   EnvValue TEXT NOT NULL,
   CONSTRAINT NDMPJobEnvironment_pkey PRIMARY KEY (JobId, FileIndex, EnvName)
);

CREATE TABLE DeviceStats (
   DeviceId INTEGER UNSIGNED REFERENCES Device DEFAULT 0,
   SampleTime DATETIME NOT NULL,
   ReadTime BIGINT UNSIGNED DEFAULT 0,
   WriteTime BIGINT UNSIGNED DEFAULT 0,
   ReadBytes BIGINT UNSIGNED DEFAULT 0,
   WriteBytes BIGINT UNSIGNED DEFAULT 0,
   SpoolSize BIGINT UNSIGNED DEFAULT 0,
   NumWaiting INTEGER DEFAULT 0,
   NumWriters INTEGER DEFAULT 0,
   MediaId INTEGER UNSIGNED REFERENCES Media DEFAULT 0,
   VolCatBytes BIGINT UNSIGNED DEFAULT 0,
   VolCatFiles BIGINT UNSIGNED DEFAULT 0,
   VolCatBlocks BIGINT UNSIGNED DEFAULT 0
);

CREATE TABLE JobStats (
   DeviceId INTEGER UNSIGNED REFERENCES Device DEFAULT 0,
   SampleTime DATETIME NOT NULL,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   JobFiles INTEGER UNSIGNED DEFAULT 0,
   JobBytes BIGINT UNSIGNED DEFAULT 0
);

CREATE TABLE TapeAlerts (
   DeviceId INTEGER UNSIGNED REFERENCES Device DEFAULT 0,
   SampleTime DATETIME NOT NULL,
   AlertFlags BIGINT UNSIGNED DEFAULT 0
);

INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('C', 'Created, not yet running',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('R', 'Running',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('B', 'Blocked',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('T', 'Completed successfully', 10);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('E', 'Terminated with errors', 25);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('e', 'Non-fatal error',20);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('f', 'Fatal error',100);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('D', 'Verify found differences',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('A', 'Canceled by user',90);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('I', 'Incomplete job', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('L', 'Committing data', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('W', 'Terminated with warnings', 20);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('l', 'Doing data despooling', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('q', 'Queued waiting for device', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('F', 'Waiting for Client',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('S', 'Waiting for Storage daemon',15);
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('m', 'Waiting for new media');
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('M', 'Waiting for media mount',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('s', 'Waiting for storage resource',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('j', 'Waiting for job resource',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('c', 'Waiting for client resource',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('d', 'Waiting on maximum jobs',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('t', 'Waiting on start time',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('p', 'Waiting on higher priority jobs',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('a', 'SD despooling attributes',15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('i', 'Doing batch insert file records',15);

-- Initialize Version
--   DELETE should not be required,
--   but prevents errors if create script is called multiple times
DELETE FROM Version WHERE VersionId<=2171;
INSERT INTO Version (VersionId) VALUES (2171);

PRAGMA default_cache_size = 100000;
PRAGMA synchronous = NORMAL;
