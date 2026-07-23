-- update db schema from 2250 to 2260
-- start transaction
begin;

ALTER TABLE Job
  ADD COLUMN PrimaryDataBytes BIGINT,
  ADD COLUMN PrimaryDataSource TEXT;

-- update the schema version
UPDATE Version SET VersionId = 2260;

commit;
set client_min_messages = warning;
analyze;
