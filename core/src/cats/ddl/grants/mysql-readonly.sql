USE mysql
-- read-only access for third party applications
GRANT SELECT ON TABLE @DB_NAME@.* TO @DB_USER@@localhost @DB_PASS@;
GRANT SELECT ON TABLE @DB_NAME@.* TO @DB_USER@@'127.0.0.1' @DB_PASS@;
GRANT SELECT ON TABLE @DB_NAME@.* TO @DB_USER@@'::1' @DB_PASS@;
FLUSH PRIVILEGES;
