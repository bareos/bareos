-- update db schema from 2250 to 2260
-- start transaction
begin;

alter table Path set (autovacuum_vacuum_scale_factor = 0.02);
alter table File set (autovacuum_vacuum_scale_factor = 0.02);

-- update the schema version
update Version set VersionId = 2260;

commit;
set client_min_messages = warning;
analyze;
