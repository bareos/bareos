-- update db schema from 2170 to 2171

-- start transaction
BEGIN;

-- NDMP Fhinfo fields for NDMP DAR
ALTER TABLE File ADD COLUMN Fhinfo NUMERIC(20) NOT NULL DEFAULT 0;
ALTER TABLE File ADD COLUMN Fhnode NUMERIC(20) NOT NULL DEFAULT 0;

-- Add JobMedia JobBytes info, also used by NDMP DAR
ALTER TABLE JobMedia ADD COLUMN JobBytes NUMERIC(20) DEFAULT 0;

UPDATE Version SET VersionId = 2171;

COMMIT;

set client_min_messages = fatal;

ANALYSE;
