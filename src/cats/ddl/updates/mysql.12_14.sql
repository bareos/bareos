CREATE TABLE RestoreObject (
   RestoreObjectId INTEGER UNSIGNED NOT NULL AUTO_INCREMENT,
   ObjectName BLOB NOT NULL,
   RestoreObject LONGBLOB NOT NULL,
   PluginName TINYBLOB NOT NULL,
   ObjectLength INTEGER DEFAULT 0,
   ObjectFullLength INTEGER DEFAULT 0,
   ObjectIndex INTEGER DEFAULT 0,
   ObjectType INTEGER DEFAULT 0,
   FileIndex INTEGER UNSIGNED DEFAULT 0,
   JobId INTEGER UNSIGNED NOT NULL REFERENCES Job,
   ObjectCompression INTEGER DEFAULT 0,
   PRIMARY KEY(RestoreObjectId),
   INDEX (JobId)
);

CREATE INDEX jobhisto_jobid_idx ON JobHisto (JobId);

ALTER TABLE File ADD COLUMN DeltaSeq smallint default 0;

UPDATE Version SET VersionId = 14;
