-- update db schema from 2171 to 2192

-- Stop on error. Prevents, that tables get droped, when merging data into new table has failed.
.bail on

BEGIN;

-- adapt index
CREATE INDEX JobMediaTMediaIdJobIdIndex ON JobMedia (MediaId,JobId);

UPDATE Version SET VersionId = 2210;

COMMIT;
