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

UPDATE Version SET VersionId = 2001;
COMMIT;

set client_min_messages = fatal;
CREATE INDEX media_poolid_idx on Media (PoolId);

ANALYSE;
