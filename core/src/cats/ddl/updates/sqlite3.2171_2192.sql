-- update db schema from 2171 to 2192

-- Stop on error. Prevents, that tables get droped, when merging data into new table has failed.
.bail on

BEGIN;

-- adapt index
CREATE INDEX JobTDateIndex ON Job (JobTDate);

-- remove INDEX pathvisibility_jobid from pathvisibility
DROP INDEX IF EXISTS pathvisibility_jobid;

UPDATE Version SET VersionId = 2192;

COMMIT;
