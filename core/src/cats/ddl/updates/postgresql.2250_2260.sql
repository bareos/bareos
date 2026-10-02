-- update db schema from 2250 to 2260
-- start transaction
begin;

ALTER TABLE Path SET (autovacuum_vacuum_scale_factor = 0.02);
ALTER TABLE File SET (autovacuum_vacuum_scale_factor = 0.02);

-- update the schema version
UPDATE Version SET VersionId = 2260;

commit;
set client_min_messages = warning;
analyze;
