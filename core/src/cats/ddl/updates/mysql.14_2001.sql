CREATE TABLE Quota (
   ClientId INTEGER DEFAULT 0 REFERENCES Client,
   GraceTime BIGINT DEFAULT 0,
   QuotaLimit BIGINT UNSIGNED DEFAULT 0,
   PRIMARY KEY (ClientId)
);

CREATE TABLE NDMPLevelMap (
   ClientId INTEGER DEFAULT 0 REFERENCES Client,
   FileSetId INTEGER UNSIGNED DEFAULT 0 REFERENCES FileSet,
   FileSystem TINYBLOB NOT NULL,
   DumpLevel INTEGER NOT NULL,
   CONSTRAINT NDMPLevelMap_pkey PRIMARY KEY (ClientId, FilesetId, FileSystem(256))
);

ALTER TABLE Media DROP COLUMN VolParts;
ALTER TABLE Media ADD COLUMN EncryptionKey TINYBLOB;

INSERT INTO Status (JobStatus,JobStatusLong,Severity) VALUES
   ('I', 'Incomplete job', 15),
   ('L', 'Committing data', 15),
   ('W', 'Terminated with warnings', 20),
   ('l', 'Doing data despooling', 15),
   ('q', 'Queued waiting for device', 15);

UPDATE Version SET VersionId = 2001;
