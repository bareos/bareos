-- update db schema from 2240 to 2250
-- start transaction
begin;

-- Add valid value 'Unlabeled' to media_volstatus
ALTER TABLE Media
  DROP CONSTRAINT IF EXISTS media_volstatus_check,
  ADD CONSTRAINT media_volstatus_check
    CHECK (VolStatus IN ('Unlabeled', 'Full', 'Archive', 'Append',
                         'Recycle', 'Purged', 'Read-Only', 'Disabled',
                         'Error', 'Busy', 'Used', 'Cleaning', 'Scratch'));

-- Add valid value 'Unlabeled' to locationlog_newvolstatus
ALTER TABLE LocationLog
  DROP CONSTRAINT IF EXISTS locationlog_newvolstatus_check,
  ADD CONSTRAINT locationlog_newvolstatus_check
    CHECK (NewVolStatus IN ('Unlabeled', 'Full', 'Archive', 'Append',
                            'Recycle', 'Purged', 'Read-Only', 'Disabled',
                            'Error', 'Busy', 'Used', 'Cleaning', 'Scratch'));

-- update the schema version
UPDATE Version SET VersionId = 2250;

commit;
set client_min_messages = warning;
analyze;
