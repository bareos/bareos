-- update db schema from 2210 to 2230
-- start transaction
BEGIN;
-- Removing statistics tables

DROP TABLE IF EXISTS DeviceStats;
DROP TABLE IF EXISTS JobStats;
DROP TABLE IF EXISTS TapeAlerts;

COMMIT;
set client_min_messages = warning;
ANALYSE;
