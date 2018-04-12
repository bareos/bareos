BEGIN;

ALTER TABLE Job ADD COLUMN HasCache TINYINT DEFAULT 0;
ALTER TABLE Job ADD COLUMN Reviewed TINYINT DEFAULT 0;
ALTER TABLE Job ADD COLUMN Comment TEXT;
ALTER TABLE JobHisto ADD COLUMN HasCache TINYINT DEFAULT 0;
ALTER TABLE JobHisto ADD COLUMN Reviewed TINYINT DEFAULT 0;
ALTER TABLE JobHisto ADD COLUMN Comment TEXT;

ALTER TABLE Status ADD COLUMN Severity int;
UPDATE Status SET Severity = 15;
UPDATE Status SET Severity = 100 where JobStatus = 'f';
UPDATE Status SET Severity = 90 where JobStatus = 'A';
UPDATE Status SET Severity = 10 where JobStatus = 'T';
UPDATE Status SET Severity = 20 where JobStatus = 'e';
UPDATE Status SET Severity = 25 where JobStatus = 'E';

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

CREATE INDEX basefiles_jobid_idx ON BaseFiles ( JobId );

UPDATE Version SET VersionId=12;
COMMIT;

DROP INDEX inx4;
DROP INDEX IF EXISTS inx9;
CREATE INDEX file_jpf_idx ON File (JobId, PathId, FilenameId);
