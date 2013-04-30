
CREATE TABLE Filename (
  FilenameId INTEGER,
  Name TEXT DEFAULT '',
  PRIMARY KEY(FilenameId) 
  );

CREATE INDEX inx1 ON Filename (Name);

CREATE TABLE Path (
   PathId INTEGER,
   Path TEXT DEFAULT '',
   PRIMARY KEY(PathId) 
   );

CREATE INDEX inx2 ON Path (Path);


CREATE TABLE File (
   FileId INTEGER,
   FileIndex INTEGER UNSIGNED NOT NULL,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   PathId INTEGER UNSIGNED REFERENCES Path NOT NULL,
   FilenameId INTEGER UNSIGNED REFERENCES Filename NOT NULL,
   MarkId INTEGER UNSIGNED DEFAULT 0,
   LStat VARCHAR(255) NOT NULL,
   MD5 VARCHAR(255) NOT NULL,
   PRIMARY KEY(FileId) 
   );

CREATE INDEX inx3 ON File (JobId);
CREATE INDEX inx4 ON File (FilenameId, PathId);
--
-- Possibly add one or more of the following indexes
--  if your Verifies are too slow.
--
-- CREATE INDEX inx4 ON File (PathId);
-- CREATE INDEX inx5 ON File (FileNameId);
-- CREATE INDEX inx9 ON File (JobId, PathId, FilenameId);

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
   HasBase TINYINT DEFAULT 0
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
   VolIndex INTEGER UNSIGNED DEFAULT 0,
   Copy     INTEGER UNSIGNED DEFAULT 0,
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
   VolParts INTEGER UNSIGNED DEFAULT 0,
   VolErrors INTEGER UNSIGNED DEFAULT 0,
   VolWrites INTEGER UNSIGNED DEFAULT 0,
   VolCapacityBytes BIGINT UNSIGNED DEFAULT 0,
   VolStatus VARCHAR(20) NOT NULL,
   Enabled TINYINT DEFAULT 1,
   Recycle TINYINT DEFAULT 0,
   ActionOnPurge     TINYINT    DEFAULT 0,
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
   InitialWrite DATETIME DEFAULT 0,
   ScratchPoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   RecyclePoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
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
   ActionOnPurge     TINYINT    DEFAULT 0,
   PoolType VARCHAR(20) NOT NULL,
   LabelType TINYINT DEFAULT 0,
   LabelFormat VARCHAR(128) NOT NULL,
   Enabled TINYINT DEFAULT 1,
   ScratchPoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   RecyclePoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
   NextPoolId INTEGER UNSIGNED REFERENCES Pool DEFAULT 0,
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

CREATE TABLE UnsavedFiles (
   UnsavedId INTEGER,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   PathId INTEGER UNSIGNED REFERENCES Path NOT NULL,
   FilenameId INTEGER UNSIGNED REFERENCES Filename NOT NULL,
   PRIMARY KEY (UnsavedId)
   );


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

CREATE TABLE CDImages (
   MediaId INTEGER UNSIGNED NOT NULL,
   LastBurn DATETIME NOT NULL,
   PRIMARY KEY (MediaId)
   );


CREATE TABLE Status (
   JobStatus CHAR(1) NOT NULL,
   JobStatusLong BLOB, 
   PRIMARY KEY (JobStatus)
   );

INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('C', 'Created, not yet running');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('R', 'Running');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('B', 'Blocked');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('T', 'Completed successfully');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('E', 'Terminated with errors');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('e', 'Non-fatal error');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('f', 'Fatal error');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('D', 'Verify found differences');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('A', 'Canceled by user');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('F', 'Waiting for Client');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('S', 'Waiting for Storage daemon');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('m', 'Waiting for new media');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('M', 'Waiting for media mount');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('s', 'Waiting for storage resource');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('j', 'Waiting for job resource');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('c', 'Waiting for client resource');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('d', 'Waiting on maximum jobs');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('t', 'Waiting on start time');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('p', 'Waiting on higher priority jobs');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('a', 'SD despooling attributes');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('i', 'Doing batch insert file records');


-- Initialize Version            
INSERT INTO Version (VersionId) VALUES (11);


PRAGMA default_cache_size = 100000;
PRAGMA synchronous = NORMAL;
