-- Can be replaced by
-- ALTER TABLE Job ADD COLUMN (ReadBytes BIGINT UNSIGNED DEFAULT 0);

BEGIN TRANSACTION;
CREATE TEMPORARY TABLE job_backup AS SELECT * FROM Job;
DROP TABLE Job;

CREATE TABLE Job
(
   JobId INTEGER,
   Job VARCHAR(128) NOT NULL,
   Name VARCHAR(128) NOT NULL,
   Type CHAR NOT NULL,
   Level CHAR NOT NULL,
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   JobStatus CHAR NOT NULL,
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

INSERT INTO Job (JobId, Job, Name, Type, Level, ClientId, JobStatus,
SchedTime, StartTime, EndTime, RealEndTime, JobTDate, VolSessionId,
VolSessionTime, JobFiles, JobBytes, JobErrors, JobMissingFiles,
PoolId, FileSetId, PriorJobId, PurgedFiles, HasBase) SELECT
JobId, Job, Name, Type, Level, ClientId, JobStatus, SchedTime, StartTime,
EndTime, RealEndTime, JobTDate, VolSessionId, VolSessionTime, JobFiles,
JobBytes, JobErrors, JobMissingFiles, PoolId, FileSetId, PriorJobId,
PurgedFiles, HasBase FROM Job_backup;

DROP TABLE Job_backup;


-- ----------------------------------------------------------------
-- New ActionOnPurge field

CREATE TEMPORARY TABLE pool_backup AS SELECT * FROM Pool;
DROP TABLE Pool;

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

INSERT INTO Pool (PoolId, Name, NumVols, MaxVols, UseOnce, UseCatalog,
AcceptAnyVolume, VolRetention, VolUseDuration, MaxVolJobs, MaxVolFiles,
MaxVolBytes, AutoPrune, Recycle, PoolType, LabelType,
LabelFormat, Enabled, ScratchPoolId, RecyclePoolId, NextPoolId,
MigrationHighBytes, MigrationLowBytes, MigrationTime)
SELECT PoolId, Name, NumVols, MaxVols, UseOnce, UseCatalog, AcceptAnyVolume,
VolRetention, VolUseDuration, MaxVolJobs, MaxVolFiles, MaxVolBytes, AutoPrune,
Recycle, PoolType, LabelType, LabelFormat, Enabled,
ScratchPoolId, RecyclePoolId, NextPoolId, MigrationHighBytes,
MigrationLowBytes, MigrationTime FROM pool_backup;

DROP TABLE pool_backup;

-- ----------------------------------------------------------------
-- New ActionOnPurge field

CREATE TEMPORARY TABLE media_backup AS SELECT * FROM Media;
DROP TABLE Media;

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

INSERT INTO Media (
   MediaId, VolumeName, Slot, PoolId, MediaType, MediaTypeId,
   LabelType, FirstWritten, LastWritten, LabelDate, VolJobs,
   VolFiles, VolBlocks, VolMounts, VolBytes, VolParts, VolErrors,
   VolWrites, VolCapacityBytes, VolStatus, Enabled, Recycle,
   VolRetention, VolUseDuration, MaxVolJobs,
   MaxVolFiles, MaxVolBytes, InChanger, StorageId, DeviceId,
   MediaAddressing, VolReadTime, VolWriteTime, EndFile, EndBlock,
   LocationId, RecycleCount, InitialWrite, ScratchPoolId,
   RecyclePoolId, Comment)
SELECT MediaId, VolumeName, Slot, PoolId, MediaType, MediaTypeId,
   LabelType, FirstWritten, LastWritten, LabelDate, VolJobs,
   VolFiles, VolBlocks, VolMounts, VolBytes, VolParts, VolErrors,
   VolWrites, VolCapacityBytes, VolStatus, Enabled, Recycle,
   VolRetention, VolUseDuration, MaxVolJobs,
   MaxVolFiles, MaxVolBytes, InChanger, StorageId, DeviceId,
   MediaAddressing, VolReadTime, VolWriteTime, EndFile, EndBlock,
   LocationId, RecycleCount, InitialWrite, ScratchPoolId,
   RecyclePoolId, Comment FROM media_backup;

DROP TABLE media_backup;

UPDATE Version SET VersionId=11;
COMMIT;

DROP TABLE IF EXISTS JobHistory;

-- Create a table like Job for long term statistics
CREATE TABLE JobHisto (
   JobId INTEGER,
   Job VARCHAR(128) NOT NULL,
   Name VARCHAR(128) NOT NULL,
   Type CHAR NOT NULL,
   Level CHAR NOT NULL,
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   JobStatus CHAR NOT NULL,
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
   HasBase TINYINT DEFAULT 0
);

CREATE INDEX inx61 ON JobHisto (StartTime);
