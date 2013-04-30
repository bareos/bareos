
CREATE TABLE filename
(
    filenameid        serial      not null,
    name              text        not null,
    primary key (filenameid)
);

ALTER TABLE filename ALTER COLUMN name SET STATISTICS 1000;
CREATE UNIQUE INDEX filename_name_idx on filename (name);

CREATE TABLE path
(
    pathid            serial      not null,
    path              text        not null,
    primary key (pathid)
);

ALTER TABLE path ALTER COLUMN path SET STATISTICS 1000;
CREATE UNIQUE INDEX path_name_idx on path (path);

CREATE TABLE file
(
    fileid            bigserial   not null,
    fileindex         integer     not null  default 0,
    jobid             integer     not null,
    pathid            integer     not null,
    filenameid        integer     not null,
    markid            integer     not null  default 0,
    lstat             text        not null,
    md5               text        not null,
    primary key (fileid)
);

CREATE INDEX file_jobid_idx on file (jobid);
CREATE INDEX file_fp_idx on file (filenameid, pathid);

--
-- Add this if you have a good number of job
-- that run at the same time
-- ALTER SEQUENCE file_fileid_seq CACHE 1000;

--
-- Possibly add one or more of the following indexes
--  if your Verifies are too slow.
--
-- CREATE INDEX file_pathid_idx on file(pathid);
-- CREATE INDEX file_filenameid_idx on file(filenameid);
-- CREATE INDEX file_jpfid_idx on file (jobid, pathid, filenameid);

CREATE TABLE job
(
    jobid             serial      not null,
    job               text        not null,
    name              text        not null,
    type              char(1)     not null,
    level             char(1)     not null,
    clientid          integer     default 0,
    jobstatus         char(1)     not null,
    schedtime         timestamp   without time zone,
    starttime         timestamp   without time zone,
    endtime           timestamp   without time zone,
    realendtime       timestamp   without time zone,
    jobtdate          bigint      default 0,
    volsessionid      integer     default 0,
    volsessiontime    integer     default 0,
    jobfiles          integer     default 0,
    jobbytes          bigint      default 0,
    readbytes        bigint      default 0,
    joberrors         integer     default 0,
    jobmissingfiles   integer     default 0,
    poolid            integer     default 0,
    filesetid         integer     default 0,
    purgedfiles       smallint    default 0,
    hasbase           smallint    default 0,
    priorjobid        integer     default 0,
    primary key (jobid)
);

CREATE INDEX job_name_idx on job (name);

-- Create a table like Job for long term statistics 
CREATE TABLE JobHisto (LIKE Job);
CREATE INDEX jobhisto_idx ON jobhisto ( starttime );


CREATE TABLE Location (
   LocationId         serial      not null,
   Location           text        not null,
   Cost               integer     default 0,
   Enabled            smallint,
   primary key (LocationId)
);


CREATE TABLE fileset
(
    filesetid         serial      not null,
    fileset           text        not null,
    md5               text        not null,
    createtime        timestamp without time zone not null,
    primary key (filesetid)
);

CREATE INDEX fileset_name_idx on fileset (fileset);

CREATE TABLE jobmedia
(
    jobmediaid        serial      not null,
    jobid             integer     not null,
    mediaid           integer     not null,
    firstindex        integer     default 0,
    lastindex         integer     default 0,
    startfile         integer     default 0,
    endfile           integer     default 0,
    startblock        bigint      default 0,
    endblock          bigint      default 0,
    volindex          integer     default 0,
    copy              integer     default 0,
    primary key (jobmediaid)
);

CREATE INDEX job_media_job_id_media_id_idx on jobmedia (jobid, mediaid);

CREATE TABLE media
(
    mediaid           serial      not null,
    volumename        text        not null,
    slot              integer     default 0,
    poolid            integer     default 0,
    mediatype         text        not null,
    mediatypeid       integer     default 0,
    labeltype         integer     default 0,
    firstwritten      timestamp   without time zone,
    lastwritten       timestamp   without time zone,
    labeldate         timestamp   without time zone,
    voljobs           integer     default 0,
    volfiles          integer     default 0,
    volblocks         integer     default 0,
    volmounts         integer     default 0,
    volbytes          bigint      default 0,
    volparts          integer     default 0,
    volerrors         integer     default 0,
    volwrites         integer     default 0,
    volcapacitybytes  bigint      default 0,
    volstatus         text        not null
        check (volstatus in ('Full','Archive','Append',
              'Recycle','Purged','Read-Only','Disabled',
              'Error','Busy','Used','Cleaning','Scratch')),
    enabled           smallint    default 1,
    recycle           smallint    default 0,
    ActionOnPurge     smallint    default 0,
    volretention      bigint      default 0,
    voluseduration    bigint      default 0,
    maxvoljobs        integer     default 0,
    maxvolfiles       integer     default 0,
    maxvolbytes       bigint      default 0,
    inchanger         smallint    default 0,
    StorageId         integer     default 0,
    DeviceId          integer     default 0,
    mediaaddressing   smallint    default 0,
    volreadtime       bigint      default 0,
    volwritetime      bigint      default 0,
    endfile           integer     default 0,
    endblock          bigint      default 0,
    LocationId        integer     default 0,
    recyclecount      integer     default 0,
    initialwrite      timestamp   without time zone,
    scratchpoolid     integer     default 0,
    recyclepoolid     integer     default 0,
    comment           text,
    primary key (mediaid)
);

create unique index media_volumename_id on media (volumename);

 
CREATE TABLE MediaType (
   MediaTypeId SERIAL,
   MediaType TEXT NOT NULL,
   ReadOnly INTEGER DEFAULT 0,
   PRIMARY KEY(MediaTypeId)
   );

CREATE TABLE Storage (
   StorageId SERIAL,
   Name TEXT NOT NULL,
   AutoChanger INTEGER DEFAULT 0,
   PRIMARY KEY(StorageId)
   );

CREATE TABLE Device (
   DeviceId SERIAL,
   Name TEXT NOT NULL,
   MediaTypeId INTEGER NOT NULL,
   StorageId INTEGER NOT NULL,
   DevMounts INTEGER NOT NULL DEFAULT 0,
   DevReadBytes BIGINT NOT NULL DEFAULT 0,
   DevWriteBytes BIGINT NOT NULL DEFAULT 0,
   DevReadBytesSinceCleaning BIGINT NOT NULL DEFAULT 0,
   DevWriteBytesSinceCleaning BIGINT NOT NULL DEFAULT 0,
   DevReadTime BIGINT NOT NULL DEFAULT 0,
   DevWriteTime BIGINT NOT NULL DEFAULT 0,
   DevReadTimeSinceCleaning BIGINT NOT NULL DEFAULT 0,
   DevWriteTimeSinceCleaning BIGINT NOT NULL DEFAULT 0,
   CleaningDate timestamp without time zone,
   CleaningPeriod BIGINT NOT NULL DEFAULT 0,
   PRIMARY KEY(DeviceId)
   );


CREATE TABLE pool
(
    poolid            serial      not null,
    name              text        not null,
    numvols           integer     default 0,
    maxvols           integer     default 0,
    useonce           smallint    default 0,
    usecatalog        smallint    default 0,
    acceptanyvolume   smallint    default 0,
    volretention      bigint      default 0,
    voluseduration    bigint      default 0,
    maxvoljobs        integer     default 0,
    maxvolfiles       integer     default 0,
    maxvolbytes       bigint      default 0,
    autoprune         smallint    default 0,
    recycle           smallint    default 0,
    ActionOnPurge     smallint    default 0,
    pooltype          text                          
      check (pooltype in ('Backup','Copy','Cloned','Archive','Migration','Scratch')),
    labeltype         integer     default 0,
    labelformat       text        not null,
    enabled           smallint    default 1,
    scratchpoolid     integer     default 0,
    recyclepoolid     integer     default 0,
    NextPoolId        integer     default 0,
    MigrationHighBytes BIGINT     DEFAULT 0,
    MigrationLowBytes  BIGINT     DEFAULT 0,
    MigrationTime      BIGINT     DEFAULT 0,
    primary key (poolid)
);

CREATE INDEX pool_name_idx on pool (name);

CREATE TABLE client
(
    clientid          serial      not null,
    name              text        not null,
    uname             text        not null,
    autoprune         smallint    default 0,
    fileretention     bigint      default 0,
    jobretention      bigint      default 0,
    primary key (clientid)
);

create unique index client_name_idx on client (name);

CREATE TABLE Log
(
    LogId             serial      not null,
    JobId             integer     not null,
    Time              timestamp   without time zone,
    LogText           text        not null,
    primary key (LogId)
);
create index log_name_idx on Log (JobId);

CREATE TABLE LocationLog (
   LocLogId SERIAL NOT NULL,
   Date timestamp   without time zone,
   Comment TEXT NOT NULL,
   MediaId INTEGER DEFAULT 0,
   LocationId INTEGER DEFAULT 0,
   newvolstatus text not null
        check (newvolstatus in ('Full','Archive','Append',
              'Recycle','Purged','Read-Only','Disabled',
              'Error','Busy','Used','Cleaning','Scratch')),
   newenabled smallint,
   PRIMARY KEY(LocLogId)
);



CREATE TABLE counters
(
    counter           text        not null,
    minvalue          integer     default 0,
    maxvalue          integer     default 0,
    currentvalue      integer     default 0,
    wrapcounter       text        not null,
    primary key (counter)
);



CREATE TABLE basefiles
(
    baseid            serial                not null,
    jobid             integer               not null,
    fileid            bigint                not null,
    fileindex         integer                       ,
    basejobid         integer                       ,
    primary key (baseid)
);

CREATE TABLE unsavedfiles
(
    UnsavedId         integer               not null,
    jobid             integer               not null,
    pathid            integer               not null,
    filenameid        integer               not null,
    primary key (UnsavedId)
);

CREATE TABLE CDImages 
(
   MediaId integer not null,
   LastBurn timestamp without time zone not null,
   primary key (MediaId)
);


CREATE TABLE version
(
    versionid         integer               not null
);

CREATE TABLE Status (
   JobStatus CHAR(1) NOT NULL,
   JobStatusLong TEXT, 
   PRIMARY KEY (JobStatus)
   );

INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('C', 'Created, not yet running');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('R', 'Running');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('B', 'Blocked');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('T', 'Completed successfully');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('E', 'Terminated with errors');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('e', 'Non-fatal error');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('f', 'Fatal error');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('D', 'Verify found differences');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('A', 'Canceled by user');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('F', 'Waiting for Client');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('S', 'Waiting for Storage daemon');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('m', 'Waiting for new media');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('M', 'Waiting for media mount');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('s', 'Waiting for storage resource');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('j', 'Waiting for job resource');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('c', 'Waiting for client resource');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('d', 'Waiting on maximum jobs');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('t', 'Waiting on start time');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('p', 'Waiting on higher priority jobs');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('a', 'SD despooling attributes');
INSERT INTO Status (JobStatus,JobStatusLong) VALUES
   ('i', 'Doing batch insert file records');

INSERT INTO Version (VersionId) VALUES (11);

-- Make sure we have appropriate permissions

