USE mysql
-- read-only access for third party applications
GRANT SELECT PRIVILEGES ON @DB_NAME@.* TO @DB_USER@@localhost @DB_PASS@;
GRANT SELECT PRIVILEGES ON @DB_NAME@.* TO @DB_USER@@"%" @DB_PASS@;
FLUSH PRIVILEGES;
