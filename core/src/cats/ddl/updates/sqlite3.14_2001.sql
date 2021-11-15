BEGIN;

CREATE TABLE Quota (
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   GraceTime INTEGER UNSIGNED DEFAULT 0,
   QuotaLimit BIGINT UNSIGNED DEFAULT 0,
   PRIMARY KEY (ClientId)
);

CREATE TABLE NDMPLevelMap (
   ClientId INTEGER REFERENCES Client DEFAULT 0,
   FileSetId INTEGER UNSIGNED REFERENCES FileSet DEFAULT 0,
   FileSystem TEXT DEFAULT '',
   DumpLevel INTEGER UNSIGNED DEFAULT 0,
   CONSTRAINT NDMPLevelMap_pkey PRIMARY KEY (ClientId, FilesetId, FileSystem)
);

ALTER TABLE Media ADD COLUMN EncryptionKey VARCHAR(128);

INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('I', 'Incomplete job', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('L', 'Committing data', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('W', 'Terminated with warnings', 20);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('l', 'Doing data despooling', 15);
INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('q', 'Queued waiting for device', 15);

UPDATE Version SET VersionId = 2001;
COMMIT;
