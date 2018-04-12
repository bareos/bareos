BEGIN;

CREATE TABLE RestoreObject (
   RestoreObjectId INTEGER,
   ObjectName TEXT DEFAULT '',
   RestoreObject TEXT DEFAULT '',
   PluginName TEXT DEFAULT '',
   ObjectLength INTEGER DEFAULT 0,
   ObjectFullLength INTEGER DEFAULT 0,
   ObjectIndex INTEGER DEFAULT 0,
   ObjectType INTEGER DEFAULT 0,
   FileIndex INTEGER UNSIGNED DEFAULT 0,
   ObjectCompression INTEGER DEFAULT 0,
   JobId INTEGER UNSIGNED REFERENCES Job NOT NULL,
   PRIMARY KEY(RestoreObjectId)
   );
CREATE INDEX restore_jobid_idx ON RestoreObject (JobId);

ALTER TABLE File ADD COLUMN DeltaSeq smallint default 0;

UPDATE Version SET VersionId=14;
COMMIT;
