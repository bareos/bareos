-- update db schema from 2004 to 2171

-- table UnsavedFiles is not used, therefore dropped.
DROP TABLE IF EXISTS UnsavedFiles;

--
-- merge Filenames (from Filename.Name) info File table
--

-- To use lesser disk space,
-- drops indices, that needs to regenerated anyway.
DROP INDEX IF EXISTS file_jobid_idx;
DROP INDEX IF EXISTS file_jpfid_idx;

-- this temporary table, used for the migration,  should not exist, but drop it just in case.
DROP TABLE IF EXISTS TmpMergeFilenameIntoFileTable;



-- start transaction
BEGIN;

-- create temporary table to be used as File table later
CREATE TABLE TmpMergeFilenameIntoFileTable (
   FileId           BIGSERIAL   NOT NULL,
   FileIndex        INTEGER     NOT NULL  DEFAULT 0,
   JobId            INTEGER     NOT NULL,
   PathId           INTEGER     NOT NULL,
   DeltaSeq         SMALLINT    NOT NULL  DEFAULT 0,
   MarkId           INTEGER     NOT NULL  DEFAULT 0,
   Fhinfo           NUMERIC(20) NOT NULL  DEFAULT 0,
   Fhnode           NUMERIC(20) NOT NULL  DEFAULT 0,
   LStat            TEXT        NOT NULL,
   Md5              TEXT        NOT NULL,
   Name             TEXT        NOT NULL,
   PRIMARY KEY (FileId)
);

-- merge Filename in File table
INSERT INTO TmpMergeFilenameIntoFileTable
  (FileId, FileIndex, JobId, PathId, Name, DeltaSeq, MarkId, LStat, Md5)
  SELECT File.FileId, File.FileIndex, File.JobId, File. PathId, Filename.Name, File.DeltaSeq,
         File.MarkId, File.LStat, File.Md5 FROM File, Filename
  WHERE  File.FilenameId = Filename.FilenameId;

DROP TABLE Filename;
DROP TABLE File;
ALTER TABLE TmpMergeFilenameIntoFileTable RENAME TO File;
ALTER INDEX TmpMergeFilenameIntoFileTable_pkey RENAME TO file_pkey;

-- adapt index
CREATE INDEX file_jobid_idx ON File (JobId);
ALTER SEQUENCE TmpMergeFilenameIntoFileTable_fileid_seq RENAME TO file_fileid_seq;
SELECT setval('file_fileid_seq', (SELECT max(fileid) from file));
CREATE INDEX file_jpfid_idx ON File (JobId, PathId, Name);
CREATE INDEX file_pjidpart_idx ON File(PathId,JobId) WHERE FileIndex = 0 AND Name = '';

-- Add JobMedia JobBytes info, also used by NDMP DAR
ALTER TABLE JobMedia ADD COLUMN JobBytes NUMERIC(20) DEFAULT 0;

UPDATE Version SET VersionId = 2171;

COMMIT;

set client_min_messages = fatal;

ANALYSE;
