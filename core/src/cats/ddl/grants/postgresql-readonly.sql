-- read-only access for third party applications
CREATE USER @DB_USER@ @DB_PASS@;

-- Allow access to used schema
GRANT USAGE ON SCHEMA public TO @DB_USER@;

-- Give read access to tables and views
GRANT SELECT ON ALL TABLES IN SCHEMA public;
-- and associated sequences
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public;

-- Allow functions executions
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public;
