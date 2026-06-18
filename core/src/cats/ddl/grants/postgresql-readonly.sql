-- read-only access for third party applications
DO $$
BEGIN
  IF NOT EXISTS (SELECT FROM pg_catalog.pg_roles WHERE rolname = '@DB_USER@') THEN
    CREATE USER @DB_USER@ @DB_PASS@;
  END IF;
END $$;

-- Allow access to used schema
GRANT USAGE ON SCHEMA public TO @DB_USER@;

-- Give read access to tables and views
GRANT SELECT ON ALL TABLES IN SCHEMA public;
-- and associated sequences
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public;

-- Allow functions executions
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public;
