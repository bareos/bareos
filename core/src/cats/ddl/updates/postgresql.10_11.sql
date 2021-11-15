-- The alter table operation can be faster with a big maintenance_work_mem
-- Uncomment and adapt this value to your environment
-- SET maintenance_work_mem = '1GB';

BEGIN;
ALTER TABLE file ALTER fileid TYPE bigint ;
ALTER TABLE basefiles ALTER fileid TYPE bigint;
ALTER TABLE job ADD COLUMN readbytes bigint default 0;
ALTER TABLE media ADD COLUMN ActionOnPurge smallint default 0;
ALTER TABLE pool ADD COLUMN ActionOnPurge smallint default 0;

-- Create a table like Job for long term statistics
DROP TABLE IF EXISTS JobHistory;
CREATE TABLE JobHisto (LIKE Job);
CREATE INDEX jobhisto_idx ON JobHisto ( starttime );

UPDATE Version SET VersionId=11;
COMMIT;

-- vacuum analyse;
