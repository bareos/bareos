USE mysql

-- mysql >= 5.7 / mariadb >= 10.1  do not autocreate
-- users on GRANT like it was before.
CREATE USER '@DB_USER@'@'localhost';
CREATE USER '@DB_USER@'@'127.0.0.1';
CREATE USER '@DB_USER@'@'::1';

GRANT ALL PRIVILEGES ON TABLE @DB_NAME@.* TO @DB_USER@@localhost @DB_PASS@;
GRANT ALL PRIVILEGES ON TABLE @DB_NAME@.* TO @DB_USER@@'127.0.0.1' @DB_PASS@;
GRANT ALL PRIVILEGES ON TABLE @DB_NAME@.* TO @DB_USER@@'::1' @DB_PASS@;
FLUSH PRIVILEGES;
