CREATE USER @DB_USER@ @DB_PASS@;

-- For schema
GRANT ALL ON SCHEMA public TO @DB_USER@;

-- For tables & views
GRANT ALL ON ALL TABLES IN SCHEMA public TO @DB_USER@;

-- For sequences ON for tables
GRANT SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA public TO @DB_USER@;
