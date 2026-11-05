-- update db schema from 2260 to 2261
-- start transaction
BEGIN;

CREATE TABLE SubscriptionAccountingSnapshot (
    SnapshotId        SMALLINT    PRIMARY KEY,
    LastAttempt       TIMESTAMP   WITHOUT TIME ZONE,
    LastSuccess       TIMESTAMP   WITHOUT TIME ZONE,
    LastError         TEXT
);

INSERT INTO SubscriptionAccountingSnapshot (SnapshotId)
VALUES (1);

ALTER TABLE Job
    ADD COLUMN AccountingValid BOOLEAN NOT NULL DEFAULT FALSE,
    ADD COLUMN AccountingExcluded BOOLEAN,
    ADD COLUMN AccountingFiles NUMERIC(20, 0),
    ADD COLUMN AccountingBytes NUMERIC(20, 0),
    ADD COLUMN AccountingLogicalBytes NUMERIC(20, 0),
    ADD COLUMN AccountingOpaqueFiles NUMERIC(20, 0),
    ADD COLUMN AccountingNdmpArchives NUMERIC(20, 0),
    ADD COLUMN AccountingRule TEXT,
    ADD COLUMN AccountingExclusionReason TEXT,
    ADD COLUMN AccountingJobsInChain INTEGER;

CREATE INDEX job_accounting_client_fileset_idx
    ON Job (ClientId, FileSetId, JobTDate DESC, JobId DESC);

UPDATE Version SET VersionId = 2261;

COMMIT;
SET client_min_messages = warning;
ANALYZE;
