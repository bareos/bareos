BEGIN; -- Necessary for Bacula core

CREATE TABLE RestoreObject (
   RestoreObjectId SERIAL NOT NULL,
   ObjectName TEXT NOT NULL,
   RestoreObject BYTEA NOT NULL,
   PluginName TEXT NOT NULL,
   ObjectLength INTEGER DEFAULT 0,
   ObjectFullLength INTEGER DEFAULT 0,
   ObjectIndex INTEGER DEFAULT 0,
   ObjectType INTEGER DEFAULT 0,
   FileIndex INTEGER DEFAULT 0,
   JobId INTEGER,
   ObjectCompression INTEGER DEFAULT 0,
   PRIMARY KEY(RestoreObjectId)
);
CREATE INDEX restore_jobid_idx on RestoreObject(JobId);

ALTER TABLE File ADD COLUMN DeltaSeq smallint default 0;

UPDATE Version SET VersionId=14;
COMMIT;

set client_min_messages = fatal;
CREATE INDEX media_poolid_idx on Media (PoolId);

ANALYSE;
