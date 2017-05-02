CREATE USER @DB_USER@ @DB_PASS@;

-- For tables
GRANT ALL ON BaseFiles TO @DB_USER@;
GRANT ALL ON JobMedia TO @DB_USER@;
GRANT ALL ON File TO @DB_USER@;
GRANT ALL ON Job TO @DB_USER@;
GRANT ALL ON Media TO @DB_USER@;
GRANT ALL ON Client TO @DB_USER@;
GRANT ALL ON Pool TO @DB_USER@;
GRANT ALL ON Fileset TO @DB_USER@;
GRANT ALL ON Path TO @DB_USER@;
GRANT ALL ON Counters TO @DB_USER@;
GRANT ALL ON Version TO @DB_USER@;
GRANT ALL ON MediaType TO @DB_USER@;
GRANT ALL ON Storage TO @DB_USER@;
GRANT ALL ON Device TO @DB_USER@;
GRANT ALL ON Status TO @DB_USER@;
GRANT ALL ON Location TO @DB_USER@;
GRANT ALL ON LocationLog TO @DB_USER@;
GRANT ALL ON Log TO @DB_USER@;
GRANT ALL ON JobHisto TO @DB_USER@;
GRANT ALL ON PathHierarchy TO @DB_USER@;
GRANT ALL ON PathVisibility TO @DB_USER@;
GRANT ALL ON RestoreObject TO @DB_USER@;
GRANT ALL ON Quota TO @DB_USER@;
GRANT ALL ON NDMPLevelMap TO @DB_USER@;
GRANT ALL ON NDMPJobEnvironment TO @DB_USER@;
GRANT ALL ON DeviceStats TO @DB_USER@;
GRANT ALL ON JobStats TO @DB_USER@;
GRANT ALL ON TapeAlerts TO @DB_USER@;

-- For sequences ON those tables
GRANT SELECT, UPDATE ON path_pathid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON fileset_filesetid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON pool_poolid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON client_clientid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON media_mediaid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON job_jobid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON file_fileid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON jobmedia_jobmediaid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON basefiles_baseid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON storage_storageid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON mediatype_mediatypeid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON device_deviceid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON location_locationid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON locationlog_loclogid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON log_logid_seq TO @DB_USER@;
GRANT SELECT, UPDATE ON restoreobject_restoreobjectid_seq TO @DB_USER@;
