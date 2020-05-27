#!/bin/bash

local_db_stop_server() {
  echo "Stop db server"
  pg_ctl --silent --pgdata=data stop
  tries=10
  while psql --host="$1" --list > /dev/null 2>&1; do
    echo " -- $tries -- "
    [ $((tries-=1)) -eq 0 ] && {
      echo "Could not stop postgres server"
      return 1
    }
    sleep 0.3
  done
}

local_db_prepare_files() {
  echo "Prepare files"
  rm --recursive --force tmp data log wal_archive
  mkdir tmp data log wal_archive
  LANG= pg_ctl --silent --pgdata=data --log=log/postgres.log initdb

  sed -i.bak "s@#listen_addresses.*@listen_addresses = ''@g" data/postgresql.conf
  sed -i.bak "s@#unix_socket_directories.*@unix_socket_directories = \'$1\'@g" data/postgresql.conf

  {
  # for online backups we need wal_archiving
    echo "wal_level = archive"
    echo "archive_mode = on"
    echo "archive_command = 'cp %p ../wal_archive'"
    echo "max_wal_senders = 10"
  } >> data/postgresql.conf
}

local_db_start_server() {
  echo "Start db server"
  pg_ctl --silent --pgdata=data --log=log/logfile start

  tries=10
  while ! psql --host="$1" --list > /dev/null 2>&1; do
    [ $((tries-=1)) -eq 0 ] && {
      echo "Could not start postgres server"
      cat log/logfile
      cat database/log/*.log
      return 1
    }
    sleep 0.1
  done

  return 0
}

local_db_create_superuser_role() {
  echo "CREATE ROLE root WITH SUPERUSER CREATEDB CREATEROLE REPLICATION LOGIN" |psql -h "$1" postgres
}

setup_local_db() {
  local_db_stop_server "$1"
  local_db_prepare_files "$1"
  if ! local_db_start_server "$1"; then return 1; fi
  local_db_create_superuser_role "$1"

  echo stop server with "pg_ctl --pgdata=data stop"

  return 0
}

