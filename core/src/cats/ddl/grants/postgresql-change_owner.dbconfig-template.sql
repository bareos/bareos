--
-- change table owner
--

CREATE OR REPLACE FUNCTION execute(text)
    returns void as $BODY$BEGIN raise notice 'exec: %',$1; execute $1; END;$BODY$ language plpgsql;

SELECT execute('ALTER TABLE '|| tablename ||' OWNER TO _DBC_DBUSER_;')
    FROM pg_tables WHERE schemaname='public' AND tableowner!='_DBC_DBUSER_';

SELECT execute('ALTER VIEW '|| viewname ||' OWNER TO _DBC_DBUSER_;')
    FROM pg_views WHERE schemaname='public' AND viewowner!='_DBC_DBUSER_';

DROP FUNCTION execute(text);
