-- update db schema from 2171 to 2192

-- start transaction
BEGIN;

ALTER TABLE BaseFiles
ALTER COLUMN BaseId TYPE BIGINT;

DROP INDEX IF EXISTS pathvisibility_jobid;

UPDATE Version SET VersionId = 2192;

COMMIT;

set client_min_messages = fatal;

ANALYSE;
