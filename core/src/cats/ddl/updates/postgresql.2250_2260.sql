-- update db schema from 2250 to 2260
-- start transaction
BEGIN;

DO $$
DECLARE
  remaining_rows bigint;
BEGIN
  IF to_regclass('basefiles') IS NULL THEN
    RETURN;
  END IF;

  SELECT count(*) INTO remaining_rows FROM basefiles;

  IF remaining_rows != 0 THEN
    RAISE EXCEPTION
      'Refusing to drop non-empty basefiles table during 2250 to 2260 migration (% rows remain). See the "Updating the database scheme" chapter in the Bareos documentation for guidance.',
      remaining_rows;
  END IF;
END
$$ LANGUAGE 'plpgsql';

ALTER TABLE Path SET (autovacuum_vacuum_scale_factor = 0.02);
ALTER TABLE File SET (autovacuum_vacuum_scale_factor = 0.02);

CREATE INDEX job_starttime_idx ON job (StartTime);

DROP TABLE IF EXISTS basefiles;
ALTER TABLE Job DROP COLUMN IF EXISTS HasBase;
ALTER TABLE JobHisto DROP COLUMN IF EXISTS HasBase;

UPDATE Version SET VersionId = 2260;

COMMIT;
SET client_min_messages = warning;
ANALYZE;
