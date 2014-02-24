BEGIN; -- Necessary for Bareos core

CREATE TABLE NDMPJobEnvironment (
    JobId             INTEGER     NOT NULL,
    FileIndex         INTEGER     NOT NULL,
    EnvName           TEXT        NOT NULL,
    EnvValue          TEXT        NOT NULL,
    CONSTRAINT NDMPJobEnvironment_pkey PRIMARY KEY (JobId, FileIndex, EnvName)
);

CREATE TABLE DeviceStats (
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    ReadTime          BIGINT      NOT NULL DEFAULT 0,
    WriteTime         BIGINT      NOT NULL DEFAULT 0,
    ReadBytes         BIGINT      DEFAULT 0,
    WriteBytes        BIGINT      DEFAULT 0,
    Spool             SMALLINT    DEFAULT 0,
    Waiting           SMALLINT    DEFAULT 0,
    Writers           SMALLINT    DEFAULT 0,
    MediaId           INTEGER     NOT NULL,
    VolCatBytes       BIGINT      DEFAULT 0,
    VolCatFiles       BIGINT      DEFAULT 0,
    VolCatBlocks      BIGINT      DEFAULT 0
);

CREATE TABLE JobStats (
    SampleTime        TIMESTAMP   WITHOUT TIME ZONE NOT NULL,
    JobId             INTEGER     NOT NULL,
    JobFiles          INTEGER     DEFAULT 0,
    JobBytes          BIGINT      DEFAULT 0
);

ALTER TABLE Media ADD COLUMN MinBlockSize INTEGER DEFAULT 0;
ALTER TABLE Media ADD COLUMN MaxBlockSize INTEGER DEFAULT 0;

ALTER TABLE Pool ADD COLUMN MinBlockSize INTEGER DEFAULT 0;
ALTER TABLE Pool ADD COLUMN MaxBlockSize INTEGER DEFAULT 0;

UPDATE Version SET VersionId = 2002;
COMMIT;

set client_min_messages = fatal;

ANALYSE;
