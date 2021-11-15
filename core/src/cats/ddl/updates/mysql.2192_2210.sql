-- update db schema from 2171 to 2192

BEGIN;

-- adapt index
ALTER TABLE JobMedia ADD INDEX (MediaId, JobId);

UPDATE Version SET VersionId = 2210;

COMMIT;
ANALYZE TABLE JobMedia;
