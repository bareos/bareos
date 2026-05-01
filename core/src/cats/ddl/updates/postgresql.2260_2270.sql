-- update db schema from 2260 to 2270
-- start transaction
begin;

ALTER TABLE Job ADD COLUMN BaseId BIGINT DEFAULT 0;
ALTER TABLE Job ADD COLUMN ContentId BIGINT DEFAULT 0;
ALTER TABLE Job ADD COLUMN ExpireTime TIMESTAMP WITH TIME ZONE DEFAULT NULL;
ALTER TABLE JobHisto ADD COLUMN BaseId BIGINT DEFAULT 0;
ALTER TABLE JobHisto ADD COLUMN ContentId BIGINT DEFAULT 0;
ALTER TABLE JobHisto ADD COLUMN ExpireTime TIMESTAMP WITH TIME ZONE DEFAULT NULL;

UPDATE Job SET ContentId = JobId WHERE ContentId = 0;
UPDATE JobHisto SET ContentId = JobId WHERE ContentId = 0;

-- update the schema version
UPDATE Version SET VersionId = 2270;

commit;
set client_min_messages = warning;
analyze;
