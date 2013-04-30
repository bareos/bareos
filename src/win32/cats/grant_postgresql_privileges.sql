create user bacula;

-- for tables
grant all on unsavedfiles to bacula;
grant all on basefiles	  to bacula;
grant all on jobmedia	  to bacula;
grant all on file	  to bacula;
grant all on job	  to bacula;
grant all on media	  to bacula;
grant all on client	  to bacula;
grant all on pool	  to bacula;
grant all on fileset	  to bacula;
grant all on path	  to bacula;
grant all on filename	  to bacula;
grant all on counters	  to bacula;
grant all on version	  to bacula;
grant all on cdimages	  to bacula;
grant all on mediatype	  to bacula;
grant all on storage	  to bacula;
grant all on device	  to bacula;
grant all on status	  to bacula;

-- for sequences on those tables

grant select, update on filename_filenameid_seq    to bacula;
grant select, update on path_pathid_seq 	   to bacula;
grant select, update on fileset_filesetid_seq	   to bacula;
grant select, update on pool_poolid_seq 	   to bacula;
grant select, update on client_clientid_seq	   to bacula;
grant select, update on media_mediaid_seq	   to bacula;
grant select, update on job_jobid_seq		   to bacula;
grant select, update on file_fileid_seq 	   to bacula;
grant select, update on jobmedia_jobmediaid_seq    to bacula;
grant select, update on basefiles_baseid_seq	   to bacula;
grant select, update on storage_storageid_seq	   to bacula;
grant select, update on mediatype_mediatypeid_seq  to bacula;
grant select, update on device_deviceid_seq	   to bacula;
