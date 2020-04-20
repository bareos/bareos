
pg_ctl -D data stop


rm --recursive --force tmp data log
mkdir tmp data log wal_archive
LANG= pg_ctl -D data -l log/postgres.log initdb

sed -i.bak "s@#listen_addresses.*@listen_addresses = ''@g" data/postgresql.conf
sed -i.bak "s@#unix_socket_directories.*@unix_socket_directories = \'$(pwd)/tmp\'@g" data/postgresql.conf

# for online backups we need wal_archiving
echo "wal_level = archive" >> data/postgresql.conf
echo "archive_mode = on" >> data/postgresql.conf
echo "archive_command = 'cp %p @current_test_directory@/database/wal_archive'" >> data/postgresql.conf
echo "max_wal_senders = 10" >> data/postgresql.conf

pg_ctl -D data -l  log/logfile start
sleep 10 

echo "CREATE ROLE root WITH SUPERUSER CREATEDB CREATEROLE REPLICATION LOGIN" |psql -h @current_test_directory@/database/tmp postgres

echo stop server with "pg_ctl -D data stop"
