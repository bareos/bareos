#
# This file contains sample queries that you can possibly use in
# your query.sql file.  However, please note that these examples
# are not supported. They may or may not work -- in fact, they may
# work with one SQL engine and not another.  As a consequence,
# please do not submit bug reports on either query.sql or 
# sample-query.sql
#
# If you find that they work for MySQL and not for PostgreSQL,
#  then please send us a whole new file corrected for PostgreSQL
#  and we will post it in the examples directory for everyone
#  to use.
#

# 1 
:List up to 20 places where a File is saved regardless of the directory
*Enter Filename (no path):
SELECT DISTINCT Job.JobId as JobId, Client.Name as Client,
  Path.Path,Filename.Name,StartTime,Level,JobFiles,JobBytes
 FROM Client,Job,File,Filename,Path WHERE Client.ClientId=Job.ClientId
 AND JobStatus IN ('T','W') AND Job.JobId=File.JobId
 AND Path.PathId=File.PathId AND Filename.FilenameId=File.FilenameId
 AND Filename.Name='%1' 
 ORDER BY Job.StartTime LIMIT 20;
# 2
:List where the most recent copies of a file are saved
*Enter path with trailing slash:
*Enter filename:
*Enter Client name:
SELECT DISTINCT Job.JobId,StartTime AS JobStartTime,VolumeName,Client.Name AS ClientName
 FROM Job,File,Path,Filename,Media,JobMedia,Client
 WHERE File.JobId=Job.JobId
 AND Path.Path='%1'
 AND Filename.Name='%2'
 AND Client.Name='%3'
 AND Path.PathId=File.PathId
 AND Filename.FilenameId=File.FilenameId
 AND JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 AND Client.ClientId=Job.ClientId
 ORDER BY Job.StartTime DESC LIMIT 5;
# 3
:List last 20 Full Backups for a Client
*Enter Client name:
SELECT DISTINCT Job.JobId,Client.Name AS Client,StartTime,JobFiles,JobBytes,
  JobMedia.StartFile as VolFile,VolumeName
 FROM Client,Job,JobMedia,Media
 WHERE Client.Name='%1'
 AND Client.ClientId=Job.ClientId
 AND Level='F' AND JobStatus IN ('T', 'W')
 AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId
 ORDER BY Job.StartTime DESC LIMIT 20;
# 4
:List all backups for a Client after a specified time
*Enter Client Name:
*Enter time in YYYY-MM-DD HH:MM:SS format:
SELECT DISTINCT Job.JobId,Client.Name as Client,Level,StartTime,JobFiles,JobBytes,VolumeName
 FROM Client,Job,JobMedia,Media
 WHERE Client.Name='%1'
 AND Client.ClientId=Job.ClientId
 AND JobStatus IN ('T', 'W')
 AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId
 AND Job.StartTime >= '%2'
 ORDER BY Job.StartTime;
# 5
:List all backups for a Client
*Enter Client Name:
SELECT DISTINCT Job.JobId as JobId,Client.Name as Client,
   FileSet.FileSet AS FileSet,Level,StartTime,
   JobFiles,JobBytes,VolumeName
 FROM Client,Job,JobMedia,Media,FileSet
 WHERE Client.Name='%1'
 AND Client.ClientId=Job.ClientId AND Job.Type='B'
 AND Job.JobStatus IN ('T','W') AND Job.FileSetId=FileSet.FileSetId
 AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId
 ORDER BY Job.StartTime;
# 6
:List Volume Attributes for a selected Volume
*Enter Volume name:
SELECT Slot,MaxVolBytes,VolCapacityBytes,VolStatus,Recycle,VolRetention,
  VolUseDuration,MaxVolJobs,MaxVolFiles
 FROM Media   
 WHERE VolumeName='%1';
# 7
:List Volumes used by selected JobId
*Enter JobId:
SELECT DISTINCT Job.JobId,VolumeName 
 FROM Job,JobMedia,Media 
 WHERE Job.JobId=%1 
 AND Job.JobId=JobMedia.JobId 
 AND JobMedia.MediaId=Media.MediaId;
# 8
:List Volumes to Restore All Files
*Enter Client Name:
!DROP TABLE temp;
!DROP TABLE temp2;
CREATE TABLE temp (JobId BIGINT NOT NULL,
 JobTDate BIGINT,
 ClientId BIGINT,
 Level CHAR,
 StartTime TEXT,
 VolumeName TEXT,
 StartFile BIGINT, 
 VolSessionId BIGINT,
 VolSessionTime BIGINT );
CREATE TABLE temp2 (JobId BIGINT NOT NULL,
 StartTime TEXT,
 VolumeName TEXT,
 Level CHAR,
 StartFile BIGINT, 
 VolSessionId BIGINT,
 VolSessionTime BIGINT);
# Select last Full save
INSERT INTO temp SELECT Job.JobId,JobTDate,Job.ClientId,Job.Level,
  StartTime,VolumeName,JobMedia.StartFile,VolSessionId,VolSessionTime
 FROM Client,Job,JobMedia,Media WHERE Client.Name='%1'
 AND Client.ClientId=Job.ClientId
 AND Level='F' AND JobStatus IN ('T', 'W')
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId
 ORDER BY Job.JobTDate DESC LIMIT 1;
# Copy into temp 2 getting all volumes of Full save
INSERT INTO temp2 SELECT Job.JobId,Job.StartTime,Media.VolumeName,Job.Level,
  JobMedia.StartFile,Job.VolSessionId,Job.VolSessionTime
 FROM temp,Job,JobMedia,Media WHERE temp.JobId=Job.JobId
 AND Job.Level='F' AND Job.JobStatus IN ('T', 'W')
 AND JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId;
# Now add subsequent incrementals
INSERT INTO temp2 SELECT DISTINCT Job.JobId,Job.StartTime,Media.VolumeName,
  Job.Level,JobMedia.StartFile,Job.VolSessionId,Job.VolSessionTime
 FROM Job,temp,JobMedia,Media
 WHERE Job.JobTDate>temp.JobTDate 
 AND Job.ClientId=temp.ClientId
 AND Job.Level IN ('I','D') AND JobStatus IN ('T', 'W')
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId;
# list results
SELECT DISTINCT VolumeName from temp2;
!DROP TABLE temp;
!DROP TABLE temp2;
# 9
:List Pool Attributes for a selected Pool
*Enter Pool name:
SELECT Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes
 FROM Pool
 WHERE Name='%1';
# 10
:List total files/bytes by Job
SELECT count(*) AS Jobs,sum(JobFiles) AS Files,sum(JobBytes) AS Bytes,Name AS Job
 FROM Job GROUP by Name;
# 11
:List total files/bytes by Volume
SELECT count(*) AS Jobs,sum(JobFiles) AS Files,sum(JobBytes) AS Bytes,VolumeName
 FROM Job,JobMedia,Media
 WHERE JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 GROUP by VolumeName;  
# 12
:List Files for a selected JobId
*Enter JobId:
SELECT Path.Path,Filename.Name FROM File,Filename,Path WHERE File.JobId=%1 
 AND Filename.FilenameId=File.FilenameId 
 AND Path.PathId=File.PathId ORDER BY
 Path.Path,Filename.Name;
# 13
:List Jobs stored on a selected MediaId
*Enter MediaId:
SELECT DISTINCT Job.JobId,Job.Name,Job.StartTime,Job.Type,
  Job.Level,Job.JobFiles,Job.JobBytes,Job.JobStatus
 FROM JobMedia,Job
 WHERE JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=%1 
 ORDER by Job.StartTime;
# 14  
:List Jobs stored for a given Volume name
*Enter Volume name:
SELECT DISTINCT Job.JobId as JobId,Job.Name as Name,Job.StartTime as StartTime,
  Job.Type as Type,Job.Level as Level,Job.JobFiles as Files,
  Job.JobBytes as Bytes,Job.JobStatus as Status
 FROM Media,JobMedia,Job
 WHERE Media.VolumeName='%1'
 AND Media.MediaId=JobMedia.MediaId              
 AND JobMedia.JobId=Job.JobId
 ORDER by Job.StartTime;
# 15
:List Volumes Bacula thinks are in changer
SELECT MediaId,VolumeName,VolBytes/(1024*1024*1024) AS GB,Storage.Name 
  AS Storage,Slot,Pool.Name AS Pool,MediaType,VolStatus
  FROM Media,Pool,Storage
  WHERE Media.PoolId=Pool.PoolId
  AND Slot>0 AND InChanger=1
  AND Media.StorageId=Storage.StorageId
  ORDER BY MediaType ASC, Slot ASC;
# 16
:List Volumes likely to need replacement from age or errors
SELECT VolumeName AS Volume,VolMounts AS Mounts,VolErrors AS Errors,
         VolWrites AS Writes,VolStatus AS Status
  FROM Media
  WHERE (VolErrors>0) OR (VolStatus='Error') OR (VolMounts>50) OR
         (VolStatus='Disabled') OR (VolWrites>3999999)
  ORDER BY VolStatus ASC, VolErrors,VolMounts,VolumeName DESC;
#  17
:List Volumes Bacula thinks are eligible for the changer
SELECT VolumeName,VolStatus,Storage.Name AS Location,
   VolBytes/(1024*1024*1024) AS GB,MediaId,MediaType,Pool.Name AS Pool
   FROM Media,Pool,Storage
   WHERE Media.PoolId=Pool.PoolId
   AND Media.StorageId=Storage.StorageId
   AND InChanger=0
   AND ((VolStatus='Purged') OR (VolStatus='Append') OR (VolStatus='Recycle'))
   ORDER BY VolMounts ASC, Pool.Name ASC, VolumeName ASC
# 18
:List Volumes by Volume:
SELECT VolumeName, Job.JobId as JobID, Job.Name as JobName, Job.StartTime as
Start, sum(JobFiles) AS Files,sum(JobBytes) AS Bytes
 FROM Job,JobMedia,Media
 WHERE JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 GROUP by VolumeName, Job.JobID, Job.Name, Job.StartTime
 ORDER by VolumeName;
# 19
:List Volumes by Jobs:
SELECT Job.Name as JobName, Job.JobId as JobID, VolumeName, Job.StartTime as
Start, sum(JobFiles) AS Files,sum(JobBytes) AS Bytes
 FROM Job,JobMedia,Media
 WHERE JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 GROUP by VolumeName, Job.JobID, Job.Name, Job.StartTime
 ORDER by JobName, Start;
# 20
:List Volumes for a jobname:
*Enter Job name:
SELECT Job.Name as JobName, Job.JobId as JobID, VolumeName, Job.StartTime as
Start, sum(JobFiles) AS Files,sum(JobBytes) AS Bytes
 FROM Job,JobMedia,Media
 WHERE Job.Name='%1'
 AND JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 GROUP by VolumeName, Job.JobID, Job.Name, Job.StartTime
 ORDER by JobName, Start;

