CREATE TABLE Path
(
    PathId            SERIAL      NOT NULL,
    Path              TEXT        NOT NULL,
    PRIMARY KEY (PathId)
);

ALTER TABLE Path ALTER COLUMN Path SET STATISTICS 1000;
CREATE UNIQUE INDEX path_name_idx ON Path (Path);

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
CREATE TABLE File (
   FileId           BIGSERIAL   NOT NULL,
   FileIndex        INTEGER     NOT NULL  DEFAULT 0,
   JobId            INTEGER     NOT NULL,
   PathId           INTEGER     NOT NULL,
   DeltaSeq         SMALLINT    NOT NULL  DEFAULT 0,
   MarkId           INTEGER     NOT NULL  DEFAULT 0,
   Fhinfo           NUMERIC(20) NOT NULL  DEFAULT 0,
   Fhnode           NUMERIC(20) NOT NULL  DEFAULT 0,
   LStat            TEXT        NOT NULL,
   Md5              TEXT        NOT NULL,
   Name             TEXT        NOT NULL,
   PRIMARY KEY (FileId)
);
CREATE INDEX file_jpfid_idx ON File (JobId, PathId, Name);
-- This index is important for bvfs performance, especially
-- for .bvfs_lsdirs which is used by bareos-webui.
-- As it's a partial index, it will only contain data from
-- from accurate jobs with delete directories, so that the
-- impact on backups will be low. Nevertheless, it will
-- improve the performance, even when not using accurate.
CREATE INDEX file_pjidpart_idx ON File(PathId,JobId) WHERE FileIndex = 0 AND Name = '';

--
-- Add this if you have a good number of job
-- that run at the same time
-- ALTER SEQUENCE file_fileid_seq CACHE 1000;

--
-- Possibly add one or more of the following indexes
--  to the above File table if your Verifies are
--  too slow, but they can slow down backups.
--
-- CREATE INDEX file_pathid_idx ON file(pathid);

CREATE TABLE RestoreObject (
    RestoreObjectId   SERIAL      NOT NULL,
    ObjectName        TEXT        NOT NULL,
    RestoreObject     BYTEA       NOT NULL,
    PluginName        TEXT        NOT NULL,
    ObjectLength      INTEGER     DEFAULT 0,
    ObjectFullLength  INTEGER     DEFAULT 0,
    ObjectIndex       INTEGER     DEFAULT 0,
    ObjectType        INTEGER     DEFAULT 0,
    FileIndex         INTEGER     DEFAULT 0,
    JobId             INTEGER,
    ObjectCompression INTEGER     DEFAULT 0,
    PRIMARY KEY(RestoreObjectId)
);
CREATE INDEX restore_jobid_idx ON RestoreObject(JobId);

CREATE TABLE Job
(
    JobId             SERIAL      NOT NULL,
    Job               TEXT        NOT NULL,
    Name              TEXT        NOT NULL,
    Type              CHAR(1)     NOT NULL,
    Level             CHAR(1)     NOT NULL,
    ClientId          INTEGER     DEFAULT 0,
    JobStatus         CHAR(1)     NOT NULL,
    SchedTime         TIMESTAMP   WITHOUT TIME ZONE,
    StartTime         TIMESTAMP   WITHOUT TIME ZONE,
    EndTime           TIMESTAMP   WITHOUT TIME ZONE,
    RealEndTime       TIMESTAMP   WITHOUT TIME ZONE,
    JobTDate          BIGINT      DEFAULT 0,
    VolSessionId      INTEGER     DEFAULT 0,
    volSessionTime    INTEGER     DEFAULT 0,
    JobFiles          INTEGER     DEFAULT 0,
    JobBytes          BIGINT      DEFAULT 0,
    ReadBytes         BIGINT      DEFAULT 0,
    JobErrors         INTEGER     DEFAULT 0,
    JobMissingFiles   INTEGER     DEFAULT 0,
    PoolId            INTEGER     DEFAULT 0,
    FilesetId         INTEGER     DEFAULT 0,
    PriorJobid        INTEGER     DEFAULT 0,
    PurgedFiles       SMALLINT    DEFAULT 0,
    HasBase           SMALLINT    DEFAULT 0,
    HasCache          SMALLINT    DEFAULT 0,
    Reviewed          SMALLINT    DEFAULT 0,
    Comment           TEXT,
    PRIMARY KEY (JobId)
);

CREATE INDEX job_name_idx ON job (Name);

-- Create a table like Job for long term statistics
CREATE TABLE JobHisto (LIKE Job);
CREATE INDEX jobhisto_idx ON JobHisto (StartTime);

CREATE TABLE Location (
    LocationId        SERIAL      NOT NULL,
    Location          TEXT        NOT NULL,
    Cost              INTEGER     DEFAULT 0,
    Enabled           SMALLINT,
    PRIMARY KEY (LocationId)
);

CREATE TABLE Fileset
(
    FileSetId         SERIAL      NOT NULL,
    Fileset           TEXT        NOT NULL,
    FileSetText       TEXT        DEFAULT '',
    Md5               TEXT        NOT NULL,
    CreateTime        TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    PRIMARY KEY (filesetid)
);

CREATE INDEX fileset_name_idx ON fileset (fileset);

CREATE TABLE JobMedia
(
    JobMediaId        SERIAL      NOT NULL,
    JobId             INTEGER     NOT NULL,
    MediaId           INTEGER     NOT NULL,
    FirstIndex        INTEGER     DEFAULT 0,
    LastIndex         INTEGER     DEFAULT 0,
    StartFile         INTEGER     DEFAULT 0,
    EndFile           INTEGER     DEFAULT 0,
    StartBlock        BIGINT      DEFAULT 0,
    EndBlock          BIGINT      DEFAULT 0,
    JobBytes          NUMERIC(20) DEFAULT 0,
    VolIndex          INTEGER     DEFAULT 0,
    PRIMARY KEY (jobmediaid)
);

CREATE INDEX job_media_job_id_media_id_idx ON jobmedia (JobId, MediaId);
CREATE INDEX job_media_media_id_job_id_idx ON jobmedia (MediaId, JobId);

CREATE TABLE Media
(
    MediaId           SERIAL      NOT NULL,
    VolumeName        TEXT        NOT NULL,
    Slot              BIGINT      DEFAULT 0,
    PoolId            INTEGER     DEFAULT 0,
    MediaType         TEXT        NOT NULL,
    MediaTypeId       INTEGER     DEFAULT 0,
    LabelType         INTEGER     DEFAULT 0,
    FirstWritten      TIMESTAMP   WITHOUT TIME ZONE,
    LastWritten       TIMESTAMP   WITHOUT TIME ZONE,
    LabelDate         TIMESTAMP   WITHOUT TIME ZONE,
    VolJobs           BIGINT      DEFAULT 0,
    VolFiles          BIGINT      DEFAULT 0,
    VolBlocks         BIGINT      DEFAULT 0,
    VolMounts         BIGINT      DEFAULT 0,
    VolBytes          BIGINT      DEFAULT 0,
    VolErrors         BIGINT      DEFAULT 0,
    VolWrites         BIGINT      DEFAULT 0,
    VolCapacityBytes  BIGINT      DEFAULT 0,
    VolStatus         TEXT        NOT NULL
        CHECK (VolStatus IN ('Unlabeled', 'Full', 'Archive', 'Append',
              'Recycle', 'Purged', 'Read-Only', 'Disabled',
              'Error', 'Busy', 'Used', 'Cleaning', 'Scratch')),
    Enabled           SMALLINT    DEFAULT 1,
    Recycle           SMALLINT    DEFAULT 0,
    ActionOnPurge     SMALLINT    DEFAULT 0,
    VolRetention      BIGINT      DEFAULT 0,
    VolUseDuration    BIGINT      DEFAULT 0,
    MaxVolJobs        BIGINT      DEFAULT 0,
    MaxVolFiles       BIGINT      DEFAULT 0,
    MaxVolBytes       BIGINT      DEFAULT 0,
    InChanger         SMALLINT    DEFAULT 0,
    StorageId         INTEGER     DEFAULT 0,
    DeviceId          INTEGER     DEFAULT 0,
    MediaAddressing   SMALLINT    DEFAULT 0,
    VolReadTime       BIGINT      DEFAULT 0,
    VolWriteTime      BIGINT      DEFAULT 0,
    EndFile           BIGINT      DEFAULT 0,
    EndBlock          BIGINT      DEFAULT 0,
    LocationId        INTEGER     DEFAULT 0,
    RecycleCount      BIGINT      DEFAULT 0,
    MinBlockSize      BIGINT      DEFAULT 0,
    MaxBlockSize      BIGINT      DEFAULT 0,
    InitialWrite      TIMESTAMP   WITHOUT TIME ZONE,
    ScratchPoolId     INTEGER     DEFAULT 0,
    RecyclePoolId     INTEGER     DEFAULT 0,
    EncryptionKey     TEXT,
    Comment           TEXT,
    PRIMARY KEY (MediaId)
);

CREATE UNIQUE INDEX media_volumename_id ON Media (VolumeName);
CREATE INDEX media_poolid_idx ON Media (PoolId);

CREATE TABLE MediaType (
    MediaTypeId       SERIAL,
    MediaType         TEXT        NOT NULL,
    ReadOnly          INTEGER     DEFAULT 0,
    PRIMARY KEY(MediaTypeId)
);

CREATE TABLE Storage (
    StorageId         SERIAL,
    Name              TEXT        NOT NULL,
    AutoChanger       INTEGER     DEFAULT 0,
    PRIMARY KEY(StorageId)
);

CREATE TABLE Device (
    DeviceId                  SERIAL,
    Name                      TEXT        NOT NULL,
    MediaTypeId               INTEGER     NOT NULL,
    StorageId                 INTEGER     NOT NULL,
    DevMounts                 INTEGER     NOT NULL DEFAULT 0,
    DevReadBytes              BIGINT      NOT NULL DEFAULT 0,
    DevWriteBytes             BIGINT      NOT NULL DEFAULT 0,
    DevReadBytesSinceCleaning BIGINT      NOT NULL DEFAULT 0,
    DevWriteBytesSinceCleaning BIGINT     NOT NULL DEFAULT 0,
    DevReadTime               BIGINT      NOT NULL DEFAULT 0,
    DevWriteTime              BIGINT      NOT NULL DEFAULT 0,
    DevReadTimeSinceCleaning  BIGINT      NOT NULL DEFAULT 0,
    DevWriteTimeSinceCleaning BIGINT      NOT NULL DEFAULT 0,
    CleaningDate              TIMESTAMP   WITHOUT TIME ZONE,
    CleaningPeriod            BIGINT      NOT NULL DEFAULT 0,
    PRIMARY KEY(DeviceId)
);

CREATE TABLE Pool
(
    PoolId            SERIAL      NOT NULL,
    Name              TEXT        NOT NULL,
    NumVols           INTEGER     DEFAULT 0,
    MaxVols           INTEGER     DEFAULT 0,
    UseOnce           SMALLINT    DEFAULT 0,
    UseCatalog        SMALLINT    DEFAULT 0,
    AcceptAnyVolume   SMALLINT    DEFAULT 0,
    VolRetention      BIGINT      DEFAULT 0,
    VolUseDuration    BIGINT      DEFAULT 0,
    MaxVolJobs        INTEGER     DEFAULT 0,
    MaxVolFiles       INTEGER     DEFAULT 0,
    MaxVolBytes       BIGINT      DEFAULT 0,
    AutoPrune         SMALLINT    DEFAULT 0,
    Recycle           SMALLINT    DEFAULT 0,
    ActionOnPurge     SMALLINT    DEFAULT 0,
    PoolType          TEXT
      CHECK (PoolType IN ('Backup', 'Copy', 'Cloned', 'Archive',
                          'Migration', 'Scratch')),
    LabelType         INTEGER     DEFAULT 0,
    LabelFormat       TEXT        NOT NULL,
    Enabled           SMALLINT    DEFAULT 1,
    ScratchPoolId     INTEGER     DEFAULT 0,
    RecyclePoolId     INTEGER     DEFAULT 0,
    NextPoolId        INTEGER     DEFAULT 0,
    MinBlockSize      INTEGER     DEFAULT 0,
    MaxBlockSize      INTEGER     DEFAULT 0,
    MigrationHighBytes BIGINT     DEFAULT 0,
    MigrationLowBytes BIGINT      DEFAULT 0,
    MigrationTime     BIGINT      DEFAULT 0,
    PRIMARY KEY (PoolId)
);

CREATE INDEX pool_name_idx ON Pool (Name);

CREATE TABLE Client
(
    ClientId          SERIAL      NOT NULL,
    Name              TEXT        NOT NULL,
    UName             TEXT        NOT NULL,
    AutoPrune         SMALLINT    DEFAULT 0,
    FileRetention     BIGINT      DEFAULT 0,
    JobRetention      BIGINT      DEFAULT 0,
    PRIMARY KEY (ClientId)
);

CREATE UNIQUE INDEX client_name_idx ON Client (Name);

CREATE TABLE Log
(
    LogId             SERIAL      NOT NULL,
    JobId             INTEGER     NOT NULL,
    Time              TIMESTAMP   WITHOUT TIME ZONE,
    LogText           TEXT        NOT NULL,
    PRIMARY KEY (LogId)
);
CREATE INDEX log_name_idx ON Log (JobId);

CREATE TABLE LocationLog (
    LocLogId          SERIAL NOT NULL,
    Date              TIMESTAMP   WITHOUT TIME ZONE,
    Comment           TEXT NOT NULL,
    MediaId           INTEGER DEFAULT 0,
    LocationId        INTEGER DEFAULT 0,
    NewVolStatus              TEXT NOT NULL
        CHECK (NewVolStatus IN ('Unlabeled', 'Full', 'Archive', 'Append',
              'Recycle', 'Purged', 'Read-Only', 'Disabled',
              'Error', 'Busy', 'Used', 'Cleaning', 'Scratch')),
    newenabled        SMALLINT,
    PRIMARY KEY(LocLogId)
);

CREATE TABLE counters
(
    Counter           TEXT        NOT NULL,
    MinValue          INTEGER     DEFAULT 0,
    MaxValue          INTEGER     DEFAULT 0,
    CurrentValue      INTEGER     DEFAULT 0,
    wrapcounter       TEXT        NOT NULL,
    PRIMARY KEY (Counter)
);


CREATE TABLE PathHierarchy
(
    PathId            INTEGER     NOT NULL,
    PPathId           INTEGER     NOT NULL,
    CONSTRAINT pathhierarchy_pkey PRIMARY KEY (PathId)
);

CREATE INDEX pathhierarchy_ppathid
          ON PathHierarchy (PPathId);

CREATE TABLE PathVisibility
(
      PathId          INTEGER     NOT NULL,
      JobId           INTEGER     NOT NULL,
      Size            BIGINT      DEFAULT 0,
      Files           INTEGER     DEFAULT 0,
      CONSTRAINT pathvisibility_pkey PRIMARY KEY (JobId, PathId)
);

CREATE TABLE version
(
    VersionId         INTEGER     NOT NULL
);

CREATE TABLE Status (
    JobStatus         CHAR(1)     NOT NULL,
    JobStatusLong     TEXT,
    Severity          INTEGER,
    PRIMARY KEY (JobStatus)
);

CREATE TABLE Quota (
    ClientId          INTEGER     NOT NULL,
    GraceTime         BIGINT      DEFAULT 0,
    QuotaLimit        BIGINT      DEFAULT 0,
    PRIMARY KEY (ClientId)
);

CREATE TABLE NDMPLevelMap (
    ClientId          INTEGER     NOT NULL,
    FilesetId         INTEGER     DEFAULT 0,
    FileSystem        TEXT        NOT NULL,
    DumpLevel         INTEGER     NOT NULL,
    CONSTRAINT NDMPLevelMap_pkey PRIMARY KEY (ClientId, FilesetId, FileSystem)
);

CREATE TABLE NDMPJobEnvironment (
    JobId             INTEGER     NOT NULL,
    FileIndex         INTEGER     NOT NULL,
    EnvName           TEXT        NOT NULL,
    EnvValue          TEXT        NOT NULL,
    CONSTRAINT NDMPJobEnvironment_pkey PRIMARY KEY (JobId, FileIndex, EnvName)
);

CREATE TABLE DeviceStats (
    DeviceId          INTEGER     DEFAULT 0,
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    ReadTime          BIGINT      NOT NULL DEFAULT 0,
    WriteTime         BIGINT      NOT NULL DEFAULT 0,
    ReadBytes         BIGINT      DEFAULT 0,
    WriteBytes        BIGINT      DEFAULT 0,
    SpoolSize         BIGINT      DEFAULT 0,
    NumWaiting        SMALLINT    DEFAULT 0,
    NumWriters        SMALLINT    DEFAULT 0,
    MediaId           INTEGER     NOT NULL,
    VolCatBytes       BIGINT      DEFAULT 0,
    VolCatFiles       BIGINT      DEFAULT 0,
    VolCatBlocks      BIGINT      DEFAULT 0
);

CREATE TABLE JobStats (
    DeviceId          INTEGER     DEFAULT 0,
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    JobId             INTEGER     NOT NULL,
    JobFiles          INTEGER     DEFAULT 0,
    JobBytes          BIGINT      DEFAULT 0
);

CREATE TABLE TapeAlerts (
    DeviceId          INTEGER     DEFAULT 0,
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    AlertFlags        BIGINT      DEFAULT 0
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

-- lstat_decode part
-- This function emulate in plpgsql language the C++ code of FromBase64 function in core/src/lib/base64.cc
create or replace function bareos_frombase64(field text) returns bigint as $$
 declare
  i integer default 0;
  res bigint default 0;
  rf text default '';
  base64 text default 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  z integer := char_length(field);
 begin
  rf := substring(field from 0 for 1);
  -- skip negative
  if rf = '-' then i := i+1; end if;
  rf := substring(field from i for 1);
  while  ( rf != ' ' ) and ( i <= z ) loop
    res := res << 6;
    res := res + position(rf in base64) - 1;
    i := i + 1;
    rf := substring(field from i for 1);
  end loop;
  return res;
end
$$ language 'plpgsql' immutable;

-- This function emulate in plpgsql language the C++ code of DecodeStat
-- function in core/src/lib/attrib.cc.
-- LinkFI is decode here as bigint which differs from the C code.
-- By default we return all fields, limit parameter used in params
-- to obtain the desired values
create or replace function decode_lstat(lst text,
  params text[] = array['st_dev','st_ino','st_mode','st_nlink',
                   'st_uid','st_gid','st_rdev','st_size',
                   'st_blksize','st_blocks','st_atime','st_mtime',
                   'st_ctime','linkfi','st_flags','data']
 )
 returns table(
   st_dev bigint,
   st_ino bigint,
   st_mode bigint,
   st_nlink bigint,
   st_uid bigint,
   st_gid bigint,
   st_rdev bigint,
   st_size bigint,
   st_blksize bigint,
   st_blocks bigint,
   st_atime bigint,
   st_mtime bigint,
   st_ctime bigint,
   linkfi bigint,
   st_flags bigint,
   data bigint
 ) as $$
 declare
  fields text[];
 begin
  fields = string_to_array(lst, ' ');
  return query
  select
    case
      when 'st_dev' = any (params) then
        bareos_frombase64(fields[1])
    end as st_dev,
    case
      when 'st_ino' = any (params) then
        bareos_frombase64(fields[2])
    end as st_ino,
    case
      when 'st_mode' = any (params) then
        bareos_frombase64(fields[3])
    end as st_mode,
    case
      when 'st_nlink' = any (params) then
        bareos_frombase64(fields[4])
    end as st_nlink,
    case
      when 'st_uid' = any (params) then
        bareos_frombase64(fields[5])
    end as st_uid,
    case
      when 'st_gid' = any (params) then
        bareos_frombase64(fields[6])
    end as st_gid,
    case
      when 'st_rdev' = any (params) then
        bareos_frombase64(fields[7])
    end as st_rdev,
    case
      when 'st_size' = any (params) then
        bareos_frombase64(fields[8])
    end as st_size,
    case
      when 'st_blksize' = any (params) then
        bareos_frombase64(fields[9])
    end as st_blksize,
    case
      when 'st_blocks' = any (params) then
        bareos_frombase64(fields[10])
    end as st_blocks,
    case
      when 'st_atime' = any (params) then
        bareos_frombase64(fields[11])
    end as st_atime,
    case
      when 'st_mtime' = any (params) then
        bareos_frombase64(fields[12])
    end as st_mtime,
    case
      when 'st_ctime' = any (params) then
        bareos_frombase64(fields[13])
    end as st_ctime,
    case
      when 'linkfi' = any (params) then
        bareos_frombase64(fields[14])
    end as linkfi,
    case
      when 'st_flags' = any (params) then
        bareos_frombase64(fields[15])
    end as st_flags,
    case
      when 'data' = any (params) then
        bareos_frombase64(fields[16])
    end as data
    ;
 end
$$ language 'plpgsql' immutable;

-- set lstat/decode function parallel safe if supported
begin;
create or replace function pg_temp.exec(raw_query text) returns boolean as $$
begin
  execute raw_query;
  return true;
end
$$
language 'plpgsql';

select
    case when (select setting from pg_settings where name = 'server_version_num')::int > 100000
        then pg_temp.exec('alter function decode_lstat parallel safe;')
    else
        false
    end as decode_lstat_parallel
    ,
    case when (select setting from pg_settings where name = 'server_version_num')::int > 100000
        then pg_temp.exec('alter function bareos_frombase64 parallel safe;')
    else
        false
    end as bareos_frombase64_parallel
;
commit;

-- Initialize Version
--   DELETE should not be required,
--   but prevents errors if create script is called multiple times
DELETE FROM Version WHERE VersionId<=2250;
INSERT INTO Version (VersionId) VALUES (2250);

-- Make sure we have appropriate permissions
