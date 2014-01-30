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

UPDATE Version SET VersionId = 2001;
COMMIT;

set client_min_messages = fatal;
CREATE INDEX media_poolid_idx on Media (PoolId);

ANALYSE;
