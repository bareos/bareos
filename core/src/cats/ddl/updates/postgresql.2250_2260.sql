-- update db schema from 2250 to 2260
-- start transaction
begin;

CREATE TABLE SubscriptionAccountingSnapshot (
    SnapshotId        SMALLINT    PRIMARY KEY,
    LastAttempt       TIMESTAMP   WITHOUT TIME ZONE,
    LastSuccess       TIMESTAMP   WITHOUT TIME ZONE,
    LastError         TEXT
);

INSERT INTO SubscriptionAccountingSnapshot (SnapshotId)
VALUES (1);

CREATE TABLE SubscriptionAccounting (
    ClientName        TEXT        NOT NULL,
    FileSetName       TEXT        NOT NULL,
    Excluded          BOOLEAN     NOT NULL,
    ExclusionReason   TEXT,
    Files             NUMERIC(20, 0) NOT NULL DEFAULT 0,
    Bytes             NUMERIC(20, 0) NOT NULL DEFAULT 0,
    LogicalBytes      NUMERIC(20, 0) NOT NULL DEFAULT 0,
    Rule              TEXT,
    JobsInChain       INTEGER     NOT NULL DEFAULT 0,
    PRIMARY KEY (ClientName, FileSetName)
);

UPDATE Version SET VersionId = 2260;

commit;
set client_min_messages = warning;
analyze;
