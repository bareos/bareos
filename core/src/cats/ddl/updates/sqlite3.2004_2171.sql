-- Stop on error. Prevents, that tables get droped, when merging data into new table has failed.
.bail on

-- table UnsavedFiles is not used, therefore dropped.
DROP TABLE IF EXISTS UnsavedFiles;

--
-- merge Filenames (from Filename.Name) info File table
--

-- this temporary table, used for the migration,  should not exist, but drop it just in case.
DROP TABLE IF EXISTS TmpMergeFilenameIntoFileTable;



BEGIN;

-- create temporary table to be used as File table later
CREATE TABLE           TmpMergeFilenameIntoFileTable (
   FileId           INTEGER             NOT NULL,
   FileIndex        INTEGER   UNSIGNED            DEFAULT 0,
   JobId            INTEGER   UNSIGNED  NOT NULL  REFERENCES Job,
   PathId           INTEGER   UNSIGNED  NOT NULL  REFERENCES Path,
   DeltaSeq         SMALLINT  UNSIGNED            DEFAULT 0,
   MarkId           INTEGER   UNSIGNED            DEFAULT 0,
   Fhinfo           TEXT                          DEFAULT 0,
   Fhnode           TEXT                          DEFAULT 0,
   LStat            TINYBLOB            NOT NULL,
   MD5              TINYBLOB            NOT NULL,
   Name             BLOB                NOT NULL,
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

-- adapt index
CREATE INDEX File_JobId_PathId_Name ON File (JobId, PathId, Name);
CREATE INDEX PathId_JobId_FileIndex ON File (PathId, JobId, FileIndex);

-- Add JobMedia JobBytes info, also used by NDMP DAR
ALTER TABLE JobMedia ADD COLUMN JobBytes TEXT DEFAULT 0;

UPDATE Version SET VersionId = 2171;

COMMIT;
