-- update db schema from 2171 to 2192

BEGIN;

-- adapt index
ALTER TABLE Job ADD INDEX (JobTDate);

-- change BaseFiles.BaseId data type
ALTER TABLE BaseFiles MODIFY BaseId BIGINT;

-- remove INDEX pathvisibility_jobid from pathvisibility
DROP INDEX pathvisibility_jobid ON PathVisibility;

UPDATE Version SET VersionId = 2192;

COMMIT;

ANALYZE TABLE Job;
ANALYZE TABLE PathVisibility;
