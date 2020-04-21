
pg_ctl -D data stop


rm --recursive --force tmp data log
mkdir tmp data log
LANG= pg_ctl -D data -l log/postgres.log initdb

sed -i.bak "s@#listen_addresses.*@listen_addresses = ''@g" data/postgresql.conf
sed -i.bak "s@#unix_socket_directories.*@unix_socket_directories = \'$(pwd)/tmp\'@g" data/postgresql.conf

pg_ctl -D data -l  log/logfile start


echo stop server with "pg_ctl -D data stop"
