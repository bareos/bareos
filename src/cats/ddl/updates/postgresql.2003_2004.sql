BEGIN; -- Necessary for Bareos core

ALTER TABLE FileSet ADD COLUMN FileSetText TEXT DEFAULT '';
UPDATE Version SET VersionId = 2004;
COMMIT;

set client_min_messages = fatal;

ANALYSE;
