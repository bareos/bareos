-- update db schema from 2192 to 2210
-- start transaction
BEGIN;
CREATE INDEX job_media_media_id_job_id_idx ON jobmedia (mediaid, jobid);
UPDATE Version SET VersionId = 2210;
COMMIT;
set client_min_messages = warning;
ANALYSE;
