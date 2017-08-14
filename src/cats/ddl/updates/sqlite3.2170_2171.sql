-- update db schema from 2170 to 2171

BEGIN;

-- NDMP Fhinfo fields for NDMP DAR
ALTER TABLE File ADD COLUMN Fhinfo TEXT DEFAULT 0;
ALTER TABLE File ADD COLUMN Fhnode TEXT DEFAULT 0;

-- Add JobMedia JobBytes info, also used by NDMP DAR
ALTER TABLE JobMedia ADD COLUMN JobBytes TEXT DEFAULT 0;

UPDATE Version SET VersionId = 2171;


COMMIT;
