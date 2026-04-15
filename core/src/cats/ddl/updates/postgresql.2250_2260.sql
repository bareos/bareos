-- update db schema from 2250 to 2260
-- start transaction
BEGIN;

DO $$
DECLARE
  remaining_rows bigint;
BEGIN
  IF to_regclass('public.basefiles') IS NULL THEN
    RETURN;
  END IF;

  SELECT count(*) INTO remaining_rows FROM basefiles;

  IF remaining_rows != 0 THEN
    RAISE EXCEPTION
      'Refusing to drop non-empty basefiles table during 2250 to 2260 migration (% rows remain).',
      remaining_rows;
  END IF;
END
$$ LANGUAGE 'plpgsql';

DROP TABLE IF EXISTS basefiles;

-- update the schema version
UPDATE Version SET VersionId = 2260;

COMMIT;
SET client_min_messages = warning;
ANALYZE;
