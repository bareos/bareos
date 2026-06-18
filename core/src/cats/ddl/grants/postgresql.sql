DO $$
BEGIN
  IF NOT EXISTS (SELECT FROM pg_catalog.pg_roles WHERE rolname = '@DB_USER@') THEN
    CREATE USER @DB_USER@ @DB_PASS@;
  END IF;
END $$;

-- For schema
GRANT ALL ON SCHEMA public TO @DB_USER@;

-- For tables & views
GRANT ALL ON ALL TABLES IN SCHEMA public TO @DB_USER@;

-- For sequences ON for tables
GRANT SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA public TO @DB_USER@;

-- For functions 
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO @DB_USER@;
