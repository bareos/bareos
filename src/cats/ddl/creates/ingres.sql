--
-- When using batch insert make sure you adhere to the following
-- minimum Ingres version:
--
-- Ingres 9.2 or higher with the fix for bug 123652 and bug 117256
--   For Solaris x86 this is Ingres 9.2. + patch 13785 or higher
--   For Linux x86_64 this is Ingres 9.2 + patch 13790 or higher
--

--
-- As it seems the upper limit for a VARCHAR/VARBYTE column is 32000 bytes
-- We could have used a BLOB as storage type which can go up to 2 Gb but
-- you cannot create indexes based on a BLOB (and we don't seem to be able
-- to limit the part of the field which should be used for the index to lets
-- say 255 chars like we can in MySQL) and the storage for a BLOB is also far
-- from optimal so for now we decided that the upper limit for File and Path
-- fields is the 32000 bytes.
--
-- To use the big VARBYTE colums we need the bigger page sizes enabled in
-- ingres (which at this time supports 2K, 4K, 8K, 16K, 32K, 64K)
-- Make sure you have the following settings in your
-- ${II_SYSTEM}/ingres/files/config.dat
--
-- ii.<nodename>.dbms.private.*.cache.p8k_status: ON
-- ii.<nodename>.dbms.private.*.cache.p16k_status: ON
-- ii.<nodename>.dbms.private.*.cache.p32k_status: ON
-- ii.<nodename>.dbms.private.*.cache.p64k_status: ON
-- ii.<nodename>.rcp.dmf_cache_size8k: 200
-- ii.<nodename>.rcp.dmf_cache_size16k: 200
-- ii.<nodename>.rcp.dmf_cache_size32k: 200
-- ii.<nodename>.rcp.dmf_cache_size64k: 200
--

SET AUTOCOMMIT ON\g

CREATE SEQUENCE Path_Seq;
CREATE TABLE path
(
   Pathid		INTEGER 	NOT NULL DEFAULT Path_Seq.nextval,
   Path 		VARBYTE(32000)	NOT NULL,
   PRIMARY KEY (Pathid)
);

CREATE UNIQUE INDEX (path_name_idx ON path (Path) WITH STRUCTURE=HASH,PAGE_SIZE=32768);

CREATE SEQUENCE File_Seq;
CREATE TABLE file
(
   FileId		BIGINT		NOT NULL DEFAULT File_Seq.nextval,
   FileIndex		INTEGER 	NOT NULL DEFAULT 0,
   JobId		INTEGER 	NOT NULL,
   PathId		INTEGER 	NOT NULL,
   Name 		VARBYTE(32000)	NOT NULL,
   DeltaSeq		INTEGER 	NOT NULL DEFAULT 0,
   MarkId		INTEGER 	NOT NULL DEFAULT 0,
   LStat		VARBYTE(255)	NOT NULL,
   Md5			VARBYTE(255)	NOT NULL,
   PRIMARY KEY (fileid)
);

CREATE INDEX file_jpfid_idx ON File (jobid, pathid, name);

-- If you need performances, you can remove this index
-- the database engine is able to use the composite index
-- to find all records with a given JobId
CREATE INDEX file_jobid_idx ON File(jobid);

CREATE SEQUENCE RestoreObject_Seq;
CREATE TABLE RestoreObject (
   RestoreObjectId	INTEGER 	NOT NULL DEFAULT RestoreObject_Seq.nextval,
   ObjectName		VARBYTE(128)	NOT NULL,
   RestoreObject	BLOB		NOT NULL,
   PluginName		VARBYTE(128)	NOT NULL,
   ObjectLength 	INTEGER 	DEFAULT 0,
   ObjectFullLength	INTEGER 	DEFAULT 0,
   ObjectIndex		INTEGER 	DEFAULT 0,
   ObjectType		INTEGER 	DEFAULT 0,
   FileIndex		INTEGER 	DEFAULT 0,
   JobId		INTEGER,
   ObjectCompression	INTEGER 	DEFAULT 0,
   PRIMARY KEY (RestoreObjectId)
);

CREATE INDEX restore_jobid_idx on RestoreObject(JobId);

CREATE SEQUENCE Job_Seq;
CREATE TABLE Job
(
   JobId		INTEGER 	NOT NULL DEFAULT Job_Seq.nextval,
   Job			VARBYTE(128)	NOT NULL,
   Name 		VARBYTE(128)	NOT NULL,
   Type 		CHAR(1) 	NOT NULL,
   Level		CHAR(1) 	NOT NULL,
   ClientId		INTEGER 	DEFAULT 0,
   JobStatus		CHAR(1) 	NOT NULL,
   SchedTime		TIMESTAMP WITHOUT TIME ZONE,
   StartTime		TIMESTAMP WITHOUT TIME ZONE,
   EndTime		TIMESTAMP WITHOUT TIME ZONE,
   RealEndTime		TIMESTAMP WITHOUT TIME ZONE,
   JobTDate		BIGINT		DEFAULT 0,
   VolSessionId 	INTEGER 	DEFAULT 0,
   volSessionTime	INTEGER 	DEFAULT 0,
   JobFiles		INTEGER 	DEFAULT 0,
   JobBytes		BIGINT		DEFAULT 0,
   ReadBytes		BIGINT		DEFAULT 0,
   JobErrors		INTEGER 	DEFAULT 0,
   JobMissingFiles	INTEGER 	DEFAULT 0,
   PoolId		INTEGER 	DEFAULT 0,
   FilesetId		INTEGER 	DEFAULT 0,
   PriorJobid		INTEGER 	DEFAULT 0,
   PurgedFiles		SMALLINT	DEFAULT 0,
   HasBase		SMALLINT	DEFAULT 0,
   HasCache		SMALLINT	DEFAULT 0,
   Reviewed		SMALLINT	DEFAULT 0,
   Comment		VARBYTE(4096),
   PRIMARY KEY (JobId)
);

CREATE INDEX job_name_idx ON Job (Name);

-- Create a table like Job for long term statistics
CREATE SEQUENCE JobHisto_Seq;
CREATE TABLE JobHisto
(
   JobId		INTEGER 	NOT NULL DEFAULT JobHisto_Seq.nextval,
   Job			VARBYTE(128)	NOT NULL,
   Name 		VARBYTE(128)	NOT NULL,
   Type 		CHAR(1) 	NOT NULL,
   Level		CHAR(1) 	NOT NULL,
   ClientId		INTEGER 	DEFAULT 0,
   JobStatus		CHAR(1) 	NOT NULL,
   SchedTime		TIMESTAMP WITHOUT TIME ZONE,
   StartTime		TIMESTAMP WITHOUT TIME ZONE,
   EndTime		TIMESTAMP WITHOUT TIME ZONE,
   RealEndTime		TIMESTAMP WITHOUT TIME ZONE,
   JobTDate		BIGINT		DEFAULT 0,
   VolSessionId 	INTEGER 	DEFAULT 0,
   volSessionTime	INTEGER 	DEFAULT 0,
   JobFiles		INTEGER 	DEFAULT 0,
   JobBytes		BIGINT		DEFAULT 0,
   ReadBytes		BIGINT		DEFAULT 0,
   JobErrors		INTEGER 	DEFAULT 0,
   JobMissingFiles	INTEGER 	DEFAULT 0,
   PoolId		INTEGER 	DEFAULT 0,
   FilesetId		INTEGER 	DEFAULT 0,
   PriorJobid		INTEGER 	DEFAULT 0,
   PurgedFiles		SMALLINT	DEFAULT 0,
   HasBase		SMALLINT	DEFAULT 0,
   HasCache		SMALLINT	DEFAULT 0,
   Reviewed		SMALLINT	DEFAULT 0,
   Comment		VARBYTE(4096),
   PRIMARY KEY (JobId)
);

CREATE INDEX jobhisto_idx ON JobHisto (StartTime);

CREATE SEQUENCE Location_Seq;
CREATE TABLE Location (
   LocationId		INTEGER 	NOT NULL DEFAULT Location_Seq.nextval,
   Location		VARBYTE(128)	NOT NULL,
   Cost 		INTEGER 	DEFAULT 0,
   Enabled		SMALLINT,
   PRIMARY KEY (LocationId)
);

CREATE SEQUENCE Fileset_Seq;
CREATE TABLE Fileset
(
   FilesetId		INTEGER 	NOT NULL DEFAULT Fileset_Seq.nextval,
   Fileset		VARBYTE(128)	NOT NULL,
   Md5			VARBYTE(128)	NOT NULL,
   FileSetText		VARBYTE(4096),
   Createtime		TIMESTAMP WITHOUT TIME ZONE NOT NULL,
   PRIMARY KEY (filesetid)
);

CREATE INDEX fileset_name_idx ON Fileset (fileset);

CREATE SEQUENCE JobMedia_Seq;
CREATE TABLE JobMedia
(
   JobMediaId		INTEGER 	NOT NULL DEFAULT JobMedia_Seq.nextval,
   JobId		INTEGER 	NOT NULL,
   MediaId		INTEGER 	NOT NULL,
   FirstIndex		INTEGER 	DEFAULT 0,
   LastIndex		INTEGER 	DEFAULT 0,
   StartFile		INTEGER 	DEFAULT 0,
   EndFile		INTEGER 	DEFAULT 0,
   StartBlock		BIGINT		DEFAULT 0,
   EndBlock		BIGINT		DEFAULT 0,
   VolIndex		INTEGER 	DEFAULT 0,
   PRIMARY KEY (jobmediaid)
);

CREATE INDEX job_media_job_id_media_id_idx ON JobMedia (jobid, mediaid);

CREATE SEQUENCE Media_Seq;
CREATE TABLE Media
(
   MediaId		INTEGER 	NOT NULL DEFAULT Media_Seq.nextval,
   VolumeName		VARBYTE(128)	NOT NULL,
   Slot			INTEGER 	DEFAULT 0,
   PoolId		INTEGER 	DEFAULT 0,
   MediaType		VARBYTE(128)	NOT NULL,
   MediaTypeId 		INTEGER 	DEFAULT 0,
   LabelType		INTEGER 	DEFAULT 0,
   FirstWritten		TIMESTAMP WITHOUT TIME ZONE,
   LastWritten 		TIMESTAMP WITHOUT TIME ZONE,
   LabelDate		TIMESTAMP WITHOUT TIME ZONE,
   VolJobs		INTEGER 	DEFAULT 0,
   VolFiles		INTEGER 	DEFAULT 0,
   VolBlocks		INTEGER 	DEFAULT 0,
   VolMounts		INTEGER 	DEFAULT 0,
   VolBytes		BIGINT		DEFAULT 0,
   VolErrors		INTEGER 	DEFAULT 0,
   VolWrites		INTEGER 	DEFAULT 0,
   VolCapacitybytes	BIGINT		DEFAULT 0,
   VolStatus		VARBYTE(128)	NOT NULL
	CHECK (volstatus in ('Full','Archive','Append',
	      'Recycle','Purged','Read-Only','Disabled',
	      'Error','Busy','Used','Cleaning','Scratch')),
   Enabled		SMALLINT	DEFAULT 1,
   Recycle		SMALLINT	DEFAULT 0,
   ActionOnPurge	SMALLINT	DEFAULT 0,
   VolRetention		BIGINT		DEFAULT 0,
   VolUseDuration	BIGINT		DEFAULT 0,
   MaxVolJobs		INTEGER 	DEFAULT 0,
   MaxVolFiles 		INTEGER 	DEFAULT 0,
   MaxVolBytes 		BIGINT		DEFAULT 0,
   InChanger		SMALLINT	DEFAULT 0,
   StorageId		BIGINT		DEFAULT 0,
   DeviceId		INTEGER 	DEFAULT 0,
   MediaAddressing	SMALLINT	DEFAULT 0,
   VolReadTime 		BIGINT		DEFAULT 0,
   VolWriteTime		BIGINT		DEFAULT 0,
   EndFile		INTEGER 	DEFAULT 0,
   EndBlock		BIGINT		DEFAULT 0,
   LocationId		INTEGER 	DEFAULT 0,
   RecycleCount		INTEGER 	DEFAULT 0,
   MinBlockSize		INTEGER		DEFAULT 0,
   MaxBlockSize		INTEGER		DEFAULT 0,
   InitialWrite		TIMESTAMP WITHOUT TIME ZONE,
   ScratchPoolId	INTEGER 	DEFAULT 0,
   RecyclePoolId	INTEGER 	DEFAULT 0,
   EncryptionKey	VARBYTE(128),
   Comment		VARBYTE(4096),
   PRIMARY KEY (mediaid)
);

CREATE UNIQUE INDEX media_volumename_id ON Media (VolumeName);
CREATE INDEX media_poolid_idx ON Media (PoolId);

CREATE SEQUENCE MediaType_Seq;
CREATE TABLE MediaType (
   MediaTypeId		INTEGER 	NOT NULL DEFAULT MediaType_Seq.nextval,
   MediaType		VARBYTE(128)	NOT NULL,
   ReadOnly		INTEGER 	NOT NULL DEFAULT 0,
   PRIMARY KEY (MediaTypeId)
);

CREATE SEQUENCE Storage_Seq;
CREATE TABLE Storage (
   StorageId		INTEGER 	NOT NULL DEFAULT Storage_Seq.nextval,
   Name 		VARBYTE(128)	NOT NULL,
   AutoChanger		INTEGER 	NOT NULL DEFAULT 0,
   PRIMARY KEY (StorageId)
);

CREATE SEQUENCE Device_Seq;
CREATE TABLE Device (
   DeviceId			INTEGER 	NOT NULL DEFAULT Device_Seq.nextval,
   Name 			VARBYTE(128)	NOT NULL,
   MediaTypeId			INTEGER 	NOT NULL,
   StorageId			INTEGER 	NOT NULL,
   DevMounts			INTEGER 	NOT NULL DEFAULT 0,
   DevReadBytes 		BIGINT		NOT NULL DEFAULT 0,
   DevWriteBytes		BIGINT		NOT NULL DEFAULT 0,
   DevReadBytesSinceCleaning	BIGINT		NOT NULL DEFAULT 0,
   DevWriteBytesSinceCleaning	BIGINT		NOT NULL DEFAULT 0,
   DevReadTime			BIGINT		NOT NULL DEFAULT 0,
   DevWriteTime 		BIGINT		NOT NULL DEFAULT 0,
   DevReadTimeSinceCleaning	BIGINT		NOT NULL DEFAULT 0,
   DevWriteTimeSinceCleaning	BIGINT		NOT NULL DEFAULT 0,
   CleaningDate 		TIMESTAMP WITHOUT TIME ZONE,
   CleaningPeriod		BIGINT		NOT NULL DEFAULT 0,
   PRIMARY KEY (DeviceId)
);

CREATE SEQUENCE Pool_Seq;
CREATE TABLE Pool
(
   PoolId		INTEGER 	NOT NULL DEFAULT pool_Seq.nextval,
   Name 		VARBYTE(128)	NOT NULL,
   NumVols		INTEGER 	DEFAULT 0,
   MaxVols		INTEGER 	DEFAULT 0,
   UseOnce		SMALLINT	DEFAULT 0,
   UseCatalog		SMALLINT	DEFAULT 0,
   AcceptAnyVolume	SMALLINT	DEFAULT 0,
   VolRetention 	BIGINT		DEFAULT 0,
   VolUseDuration	BIGINT		DEFAULT 0,
   MaxVolJobs		INTEGER 	DEFAULT 0,
   MaxVolFiles		INTEGER 	DEFAULT 0,
   MaxVolBytes		BIGINT		DEFAULT 0,
   AutoPrune		SMALLINT	DEFAULT 0,
   Recycle		SMALLINT	DEFAULT 0,
   ActionOnPurge	SMALLINT	DEFAULT 0,
   PoolType		VARBYTE(32)
	CHECK (pooltype in ('Backup','Copy','Cloned','Archive','Migration','Scratch')),
   LabelType		INTEGER 	DEFAULT 0,
   LabelFormat		VARBYTE(128)	NOT NULL,
   Enabled		SMALLINT	DEFAULT 1,
   ScratchPoolId	INTEGER 	DEFAULT 0,
   RecyclePoolId	INTEGER 	DEFAULT 0,
   NextPoolId		INTEGER 	DEFAULT 0,
   MinBlockSize		INTEGER		DEFAULT 0,
   MaxBlockSize		INTEGER		DEFAULT 0,
   MigrationHighBytes	BIGINT		DEFAULT 0,
   MigrationLowBytes	BIGINT		DEFAULT 0,
   MigrationTime	BIGINT		DEFAULT 0,
   PRIMARY KEY (poolid)
);

CREATE INDEX pool_name_idx ON Pool (name);

CREATE SEQUENCE Client_Seq;
CREATE TABLE Client
(
   ClientId		INTEGER 	NOT NULL DEFAULT Client_Seq.nextval,
   Name 		VARBYTE(128)	NOT NULL,
   Uname		VARBYTE(256)	NOT NULL,
   AutoPrune		SMALLINT	DEFAULT 0,
   FileRetention	BIGINT		DEFAULT 0,
   JobRetention 	BIGINT		DEFAULT 0,
   PRIMARY KEY (clientid)
);

CREATE UNIQUE INDEX client_name_idx ON Client (Name);

CREATE SEQUENCE Log_Seq;
CREATE TABLE Log
(
   LogId		INTEGER 	NOT NULL DEFAULT Log_Seq.nextval,
   JobId		INTEGER 	NOT NULL,
   Time 		TIMESTAMP WITHOUT TIME ZONE,
   LogText		VARBYTE(4096)		NOT NULL,
   PRIMARY KEY (LogId)
);
CREATE INDEX log_name_idx ON Log (JobId);

CREATE SEQUENCE LocationLog_Seq;
CREATE TABLE LocationLog (
   LocLogId		INTEGER 	NOT NULL DEFAULT LocationLog_Seq.nextval,
   Date 		TIMESTAMP WITHOUT TIME ZONE,
   Comment		VARBYTE(4096)	NOT NULL,
   MediaId		INTEGER 	DEFAULT 0,
   LocationId		INTEGER 	DEFAULT 0,
   NewVolStatus 	VARBYTE(32)	NOT NULL
	CHECK (newvolstatus in ('Full','Archive','Append',
	      'Recycle','Purged','Read-Only','Disabled',
	      'Error','Busy','Used','Cleaning','Scratch')),
   NewEnabled		SMALLINT,
   PRIMARY KEY (LocLogId)
);

CREATE TABLE Counters
(
   Counter		VARBYTE(128)	NOT NULL,
   MinValue		INTEGER 	DEFAULT 0,
   MaxValue		INTEGER 	DEFAULT 0,
   CurrentValue 	INTEGER 	DEFAULT 0,
   WrapCounter		VARBYTE(128)	NOT NULL,
   PRIMARY KEY (counter)
);

CREATE SEQUENCE BaseFiles_Seq;
CREATE TABLE BaseFiles
(
   BaseId		INTEGER 	NOT NULL DEFAULT BaseFiles_Seq.nextval,
   JobId		INTEGER 	NOT NULL,
   FileId		BIGINT		NOT NULL,
   FileIndex		INTEGER,
   BaseJobid		INTEGER,
   PRIMARY KEY (BaseId)
);

CREATE INDEX basefiles_jobid_idx ON BaseFiles (JobId);

-- This table seems to be obsolete
-- CREATE TABLE UnsavedFiles
-- (
--    UnsavedId		INTEGER 	NOT NULL,
--    JobId		INTEGER 	NOT NULL,
--    PathId		INTEGER 	NOT NULL,
--    FilenameId		INTEGER 	NOT NULL,
--    PRIMARY KEY (UnsavedId)
-- );

CREATE TABLE PathHierarchy
(
   PathId		INTEGER 	NOT NULL,
   PPathId		INTEGER 	NOT NULL,
   CONSTRAINT pathhierarchy_pkey PRIMARY KEY (PathId)
);

CREATE INDEX pathhierarchy_ppathid ON PathHierarchy (PPathId);

CREATE TABLE PathVisibility
(
   PathId		INTEGER 	NOT NULL,
   JobId		INTEGER 	NOT NULL,
   Size 		BIGINT		DEFAULT 0,
   Files		INTEGER 	DEFAULT 0,
   CONSTRAINT pathvisibility_pkey PRIMARY KEY (JobId, PathId)
);
CREATE INDEX pathvisibility_jobid ON PathVisibility (JobId);

CREATE TABLE version
(
   versionid		INTEGER 	NOT NULL
);

CREATE TABLE Status (
   JobStatus		CHAR(1) 	NOT NULL,
   JobStatusLong	VARBYTE(128),
   Severity		INTEGER,
   PRIMARY KEY (JobStatus)
);

CREATE TABLE Quota
(
   ClientId		INTEGER 	NOT NULL,
   GraceTime		INTEGER		DEFAULT 0,
   QuotaLimit		BIGINT		DEFAULT 0,
   PRIMARY KEY (ClientId)
);

CREATE TABLE NDMPLevelMap
(
   ClientId		INTEGER 	NOT NULL,
   FilesetId		INTEGER 	DEFAULT 0,
   FileSystem 		VARBYTE(256)	NOT NULL,
   DumpLevel		INTEGER 	DEFAULT 0,
   CONSTRAINT NDMPLevelMap_pkey PRIMARY KEY (ClientId, FilesetId, FileSystem)
);

CREATE TABLE NDMPJobEnvironment
(
   JobId		INTEGER 	NOT NULL,
   FileIndex		INTEGER 	NOT NULL,
   EnvName    		VARBYTE(256)	NOT NULL,
   EnvValue   		VARBYTE(256)	NOT NULL,
   CONSTRAINT NDMPJobEnvironment_pkey PRIMARY KEY (JobId, FileIndex, EnvName)
);

CREATE TABLE DeviceStats (
   DeviceId		INTEGER 	DEFAULT 0,
   SampleTime		TIMESTAMP WITHOUT TIME ZONE NOT NULL,
   ReadTime		BIGINT		DEFAULT 0,
   WriteTime		BIGINT		DEFAULT 0,
   ReadBytes		BIGINT		DEFAULT 0,
   WriteBytes		BIGINT		DEFAULT 0,
   SpoolSize		BIGINT  	DEFAULT 0,
   NumWaiting		INTEGER 	DEFAULT 0,
   NumWriters		INTEGER 	DEFAULT 0,
   MediaId		INTEGER 	DEFAULT 0,
   VolCatBytes		BIGINT		DEFAULT 0,
   VolCatFiles		BIGINT		DEFAULT 0,
   VolCatBlocks		BIGINT		DEFAULT 0
);

CREATE TABLE JobStats (
   DeviceId		INTEGER 	DEFAULT 0,
   SampleTime		TIMESTAMP WITHOUT TIME ZONE NOT NULL,
   JobId		INTEGER 	NOT NULL,
   JobFiles		INTEGER 	DEFAULT 0,
   JobBytes		BIGINT		DEFAULT 0
);

CREATE TABLE TapeAlerts (
   DeviceId		INTEGER 	DEFAULT 0,
   SampleTime		TIMESTAMP WITHOUT TIME ZONE NOT NULL,
   AlertFlags		BIGINT		DEFAULT 0
);
\g

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
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('m', 'Waiting for new media',15);
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

INSERT INTO Version (VersionId) VALUES (2170);

-- Make sure we have appropriate permissions
\g
