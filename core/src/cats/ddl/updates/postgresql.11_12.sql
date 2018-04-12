BEGIN; -- Necessary for Bacula core

ALTER TABLE JobMedia DROP Copy ;
ALTER TABLE Job ADD COLUMN HasCache smallint default 0;
ALTER TABLE Job ADD COLUMN Reviewed smallint default 0;
ALTER TABLE Job ADD COLUMN Comment text;
ALTER TABLE JobHisto ADD COLUMN HasCache smallint default 0;
ALTER TABLE JobHisto ADD COLUMN Reviewed smallint default 0;
ALTER TABLE JobHisto ADD COLUMN Comment text;
UPDATE Version SET VersionId=12;
COMMIT;

BEGIN; -- Can conflict with previous Bweb installation
ALTER TABLE Status ADD COLUMN Severity int;
UPDATE Status SET Severity = 15;
UPDATE Status SET Severity = 100 where JobStatus = 'f';
UPDATE Status SET Severity = 90 where JobStatus = 'A';
UPDATE Status SET Severity = 10 where JobStatus = 'T';
UPDATE Status SET Severity = 20 where JobStatus = 'e';
UPDATE Status SET Severity = 25 where JobStatus = 'E';
COMMIT;

BEGIN; -- Can already exists if using 3.1.x release
CREATE TABLE PathHierarchy
(
     PathId integer NOT NULL,
     PPathId integer NOT NULL,
     CONSTRAINT pathhierarchy_pkey PRIMARY KEY (PathId)
);

CREATE INDEX pathhierarchy_ppathid
	  ON PathHierarchy (PPathId);

CREATE TABLE PathVisibility
(
      PathId integer NOT NULL,
      JobId integer NOT NULL,
      Size int8 DEFAULT 0,
      Files int4 DEFAULT 0,
      CONSTRAINT pathvisibility_pkey PRIMARY KEY (JobId, PathId)
);

CREATE INDEX pathvisibility_jobid
	  ON PathVisibility (JobId);

COMMIT;

CREATE INDEX basefiles_jobid_idx ON BaseFiles ( JobId );

-- suppress output for index modification
SET client_min_messages TO 'fatal';

-- Remove bad PostgreSQL index
DROP INDEX file_fp_idx;

-- Create the good one
-- If you want to create this index during production, you can use
-- CREATE INDEX CONCURRENTLY file_jpf_idx ON File (JobId, PathId, FilenameId)
-- to make it without locks (require PostgreSQL 8.2 version)

CREATE INDEX file_jpfid_idx on File (JobId, PathId, FilenameId);

-- restore output
SET client_min_messages TO DEFAULT;

ANALYSE;
