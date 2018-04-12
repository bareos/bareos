-- Use to migrate from Bacula Enterprise 8
-- Only works with Bareos branch, that uses denormalized file table
-- This script is derived from postgresql.14_2001.sql
-- TODO: adapt to new database version number scheme

BEGIN; -- Necessary for Bareos core

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

ALTER TABLE Media DROP COLUMN VolParts RESTRICT;
ALTER TABLE Media ADD COLUMN EncryptionKey text;

-- Used for the denormalized file / filename table
ALTER TABLE file RENAME filename to name;

-- INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
--   ('I', 'Incomplete job', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('L', 'Committing data', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('W', 'Terminated with warnings', 20);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('l', 'Doing data despooling', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('q', 'Queued waiting for device', 15);

UPDATE Version SET VersionId = 2001;
COMMIT;

set client_min_messages = fatal;
CREATE INDEX media_poolid_idx on Media (PoolId);


CREATE TABLE NDMPJobEnvironment (
    JobId             INTEGER     NOT NULL,
    FileIndex         INTEGER     NOT NULL,
    EnvName           TEXT        NOT NULL,
    EnvValue          TEXT        NOT NULL,
    CONSTRAINT NDMPJobEnvironment_pkey PRIMARY KEY (JobId, FileIndex, EnvName)
);

ALTER TABLE Media ADD COLUMN MinBlockSize INTEGER DEFAULT 0;
ALTER TABLE Media ADD COLUMN MaxBlockSize INTEGER DEFAULT 0;

ALTER TABLE Pool ADD COLUMN MinBlockSize INTEGER DEFAULT 0;
ALTER TABLE Pool ADD COLUMN MaxBlockSize INTEGER DEFAULT 0;

UPDATE Version SET VersionId = 2002;
COMMIT;

set client_min_messages = fatal;


--
-- We drop and recreate the stats table which was not used yet.
--
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

--
-- We drop and recreate the stats table which was not used yet.
--
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

DROP TABLE CDImages;

UPDATE Version SET VersionId = 2003;
COMMIT;

set client_min_messages = fatal;


ALTER TABLE FileSet ADD COLUMN FileSetText TEXT DEFAULT '';
UPDATE Version SET VersionId = 2004;
COMMIT;

set client_min_messages = fatal;


ANALYSE;
