
-- This script does the same as dbcheck, but in full SQL in order to be faster
-- To run it, exec it like this : psql -U bacula bacula (YOUR username and database)
-- then \i dbckeck.sql
-- It will tell you what it does. At the end you'll have to commit yourself. 
-- Check the numbers of altered records before ...
--
--  Notes from Marc Cousin, the author of this script: 01Sep08
--  The script version won't work better with mysql without indexes.

--  The reason is that the script works with global queries instead of many small 
--  queries like dbcheck. So PostgreSQL can optimise the query by building hash 
--  joins or merge joins.

--  Mysql can't do that (last time I checked, at least ...), and will do nested 
--  loops between job and file for instance. And without the missing indexes, 
--  mysql will still be as slow as with dbcheck, as you'll more or less have 
----thousands of full scans on the job table (where postgresql will do only a few 
--  to build its hash).

--  So for dbcheck with mysql, there is no other solution than adding the missing 
--  indexes (but adding and dropping them just for the dbcheck is a good option).

--repair_bad_paths():
--  -    SELECT PathId,Path from Path "
--      "WHERE Path NOT LIKE '%/'
--    - ask for confirmation
--    - add a slash, doing one update for each record to be updated ...
--
--Proposal :
--    UPDATE Path Set Path=Path || '/' WHERE Path NOT LIKE '%/'; # Should work everywhere
--
--repair_bad_filenames():
--    -   SELECT FileNameId,Name from FileName
--        WHERE Name LIKE '%/'
--    - ask for confirmation
--    - remove the slash, one update per row ...
--
--Proposal : 
--    UPDATE FileName Set Name=substr(Name,1,char_length(Name)-1) WHERE Name LIKE '%/'; # Works at least with Pg and Mysql
--
--eliminate_duplicate_filenames():
--    - Lists all min filenameids from filename where there are duplicates on name
--    - Updates filetable to this entry instead of one of its duplicates
--    - Deletes filenameids from filename which are duplicate and not the min filenameids
--
--Proposal :
--    CREATE TEMP TABLE t1 AS SELECT Name,min(FileNameId) AS minfilenameid FROM FileName GROUP BY Name HAVING count(*) > 1;
--    CREATE TEMP TABLE t2 AS SELECT FileName.Name, FileName.FileNameId, t1.minfilenameid from FileName join t1 ON (FileName.Name=t1.Name) WHERE FileNameId <> minfilenameid;
--    UPDATE File SET FileNameId=(SELECT t2.minfilenameid FROM t2 WHERE t2.FileNameId=File.FileNameId) WHERE FileNameId IN (SELECT FileNameId FROM t2);
--    DELETE FROM FileName WHERE FileNameId IN (SELECT FileNameId FROM t2);
--    DROP TABLE t1;
--    DROP TABLE t2;
--
--eliminate_duplicate_paths():
--    Does exactly the same as above ...
--    
--Proposal : 
--    CREATE TEMP TABLE t1 AS SELECT Path,min(PathId) AS minpathid FROM Path GROUP BY Path HAVING count(*) > 1;
--    CREATE TEMP TABLE t2 AS SELECT Path.Path, Path.PathId, t1.minpathid from Path join t1 ON (Path.Path=t1.Path) WHERE PathId <> minpathid;
--    UPDATE Path SET PathId=(SELECT t2.minpathid FROM t2 WHERE t2.PathId=Path.PathId) WHERE PathId IN (SELECT PathId FROM t2);
--    DELETE FROM Path WHERE PathId IN (SELECT PathId FROM t2);
--    DROP TABLE t1;
--    DROP TABLE t2;
--
--
--All the orphaned below delete records from a table when they are not referenced anymore in the others...
--
--eliminate_orphaned_jobmedia_records():
--Proposal :
--    DELETE FROM JobMedia WHERE JobId NOT IN (SELECT JobId FROM Job) OR MediaID NOT IN (SELECT MediaID FROM Media);
--
--eliminate_orphaned_file_records():
--Proposal :
--    DELETE FROM File WHERE JobId NOT IN (SELECT JobId FROM JOB);
--
--eliminate_orphaned_path_records():
--Here, the problem is that File is a big table ... we'd better avoid NOT IN on it ...
--Proposal :
--    CREATE TEMP TABLE t1 AS 
--        SELECT Path.PathId
--        FROM Path LEFT OUTER JOIN File ON (Path.PathId=File.PathId)
--        WHERE File.PathId IS NULL;
--    DELETE FROM Path WHERE PathId IN (SELECT PathID FROM t1);
--    DROP TABLE t1;
--
--eliminate_orphaned_filename_records():
--Here, again, the problem is that File is a big table ... we'd better avoid NOT IN on it ...
--Proposal :
--    CREATE TEMP TABLE t1 AS 
--        SELECT FileName.FileNameId
--        FROM FileName LEFT OUTER JOIN File ON (FileName.FileNameId=File.FileNameId)
--        WHERE File.FileNameId IS NULL;
--    DELETE FROM FileName WHERE FileNameId IN (SELECT FileNameId FROM t1);
--    DROP TABLE t1;
--
--eliminate_orphaned_fileset_records():
--
--Proposal :
--    DELETE FROM FileSet WHERE FileSetId NOT IN (SELECT DISTINCT FileSetId FROM Job);
--
--eliminate_orphaned_client_records():
--Proposal :
--    DELETE FROM Client WHERE ClientId NOT IN (SELECT DISTINCT ClientId FROM Job);
--
--eliminate_orphaned_job_records():
--Proposal :
--    DELETE FROM Job WHERE ClientId NOT IN (SELECT ClientId FROM Client);
--
--eliminate_admin_records():
--Proposal :
--    DELETE FROM Job WHERE Job.Type='D';
--
--eliminate_restore_records():
--Proposal :
--    DELETE FROM Job WHERE Job.Type='R';
--    
--    
--    
--One script that does it all :
--
\t
\a
BEGIN;
-- Uncomment to raise to '1GB' or more to get better results
-- SET work_mem TO '1GB';

SELECT('eliminate_admin_records()');
DELETE FROM Job WHERE Job.Type='D';

SELECT('eliminate_restore_records()');
DELETE FROM Job WHERE Job.Type='R';

SELECT('repair_bad_paths()');
UPDATE Path Set Path=Path||'/'  WHERE Path NOT LIKE '%/' AND Path <> '';

SELECT('repair_bad_filenames()');
UPDATE FileName Set Name=substr(Name,1,char_length(Name)-1) WHERE Name LIKE '%/';

SELECT('eliminate_duplicate_filenames()');
CREATE TEMP TABLE t1 AS SELECT Name,min(FileNameId) AS minfilenameid FROM FileName GROUP BY Name HAVING count(*) > 1;
ANALYSE t1;
CREATE TEMP TABLE t2 AS SELECT FileName.Name, FileName.FileNameId, t1.minfilenameid from FileName join t1 ON (FileName.Name=t1.Name) WHERE FileNameId <> minfilenameid;
ANALYSE t2;
UPDATE File SET FileNameId=(SELECT t2.minfilenameid FROM t2 WHERE t2.FileNameId=File.FileNameId) WHERE FileNameId IN (SELECT FileNameId FROM t2);
DELETE FROM FileName WHERE FileNameId IN (SELECT FileNameId FROM t2);
DROP TABLE t1;
DROP TABLE t2;

SELECT('eliminate_duplicate_paths()');
CREATE TEMP TABLE t1 AS SELECT Path,min(PathId) AS minpathid FROM Path GROUP BY Path HAVING count(*) > 1;
ANALYSE t1;
CREATE TEMP TABLE t2 AS SELECT Path.Path, Path.PathId, t1.minpathid from Path join t1 ON (Path.Path=t1.Path) WHERE PathId <> minpathid;
ANALYSE t2;
UPDATE Path SET PathId=(SELECT t2.minpathid FROM t2 WHERE t2.PathId=Path.PathId) WHERE PathId IN (SELECT PathId FROM t2);
DELETE FROM Path WHERE PathId IN (SELECT PathId FROM t2);
DROP TABLE t1;
DROP TABLE t2;

SELECT('eliminate_orphaned_job_records()');
DELETE FROM Job WHERE ClientId NOT IN (SELECT ClientId FROM Client);

SELECT('eliminate_orphaned_jobmedia_records()');
DELETE FROM JobMedia WHERE JobId NOT IN (SELECT JobId FROM Job) OR MediaID NOT IN (SELECT MediaID FROM Media);

SELECT('eliminate_orphaned_file_records()');
DELETE FROM File WHERE JobId NOT IN (SELECT JobId FROM JOB);

SELECT('eliminate_orphaned_path_records()');
CREATE TEMP TABLE t1 AS 
    SELECT Path.PathId
    FROM Path LEFT OUTER JOIN File ON (Path.PathId=File.PathId)
    WHERE File.PathId IS NULL;
ANALYSE t1;
DELETE FROM Path WHERE PathId IN (SELECT PathID FROM t1);
DROP TABLE t1;

SELECT('eliminate_orphaned_filename_records()');
CREATE TEMP TABLE t1 AS 
    SELECT FileName.FileNameId
    FROM FileName LEFT OUTER JOIN File ON (FileName.FileNameId=File.FileNameId)
    WHERE File.FileNameId IS NULL;
ANALYSE t1;
DELETE FROM FileName WHERE FileNameId IN (SELECT FileNameId FROM t1);
DROP TABLE t1;

SELECT('eliminate_orphaned_fileset_records()');
DELETE FROM FileSet WHERE FileSetId NOT IN (SELECT DISTINCT FileSetId FROM Job);

SELECT('eliminate_orphaned_client_records()');
DELETE FROM Client WHERE ClientId NOT IN (SELECT DISTINCT ClientId FROM Job);


SELECT('Now you should commit,');
SELECT('but check that the amount of deleted or updated data is sane...');
SELECT('If you''re sure, type ''COMMIT;''');
SELECT('THIS SCRIPT IS STILL BETA !');
