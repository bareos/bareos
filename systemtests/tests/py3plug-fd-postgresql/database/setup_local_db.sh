#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.


local_db_stop_server() {
  echo "stop db server"
  if [ $UID -eq 0 ]; then
    su postgres -c "${POSTGRES_BIN_PATH}/pg_ctl  --pgdata=data stop" || :
  else
    ${POSTGRES_BIN_PATH}/pg_ctl  --silent --pgdata=data stop || :
  fi
  tries=10
  while ${POSTGRES_BIN_PATH}/psql --host="$1" --list > /dev/null 2>&1; do
    echo " -- $tries -- "
    [ $((tries-=1)) -eq 0 ] && {
      echo "Could not stop postgres server"
      return 1
    }
    sleep 1
  done
}

local_db_prepare_files() {
  echo "Prepare files"
  rm --recursive --force tmp data log wal_archive table_space index_space wal_archive_symlink
  mkdir tmp data log wal_archive table_space index_space
  ln -s wal_archive wal_archive_symlink
  if [ $UID -eq 0 ]; then
    chown postgres  tmp data log wal_archive table_space index_space
    LANG= su postgres -c "${POSTGRES_BIN_PATH}/pg_ctl  --pgdata=data --log=log/postgres.log initdb"
  else
    LANG= ${POSTGRES_BIN_PATH}/pg_ctl --silent --pgdata=data --log=log/postgres.log initdb
  fi

# In case we saw too much timeout issue because maybe not checkpoint occurs
# we can add the following parameters to the cluster configuration
# echo "checkpoint_timeout = 45"
  {
    echo "listen_addresses = ''"
    echo "unix_socket_directories = '$1'"
    echo "wal_level = archive"
    echo "archive_mode = on"
    echo "archive_command = 'cp %p ../wal_archive'"
    echo "max_wal_senders = 10"
    echo "log_connections = on"
    echo "log_disconnections = on"
    echo "log_min_messages = info"
    echo "log_min_error_statement = info"
    echo "log_error_verbosity = verbose"
    echo "log_statement = 'all'"
    echo "log_temp_files = 0"
  } >> data/postgresql.conf
}

local_db_start_server() {
  echo "start db server"
  if [ $UID -eq 0 ]; then
    su postgres -c "${POSTGRES_BIN_PATH}/pg_ctl --timeout=15 --wait --pgdata=data --log=log/logfile start"
  else
    ${POSTGRES_BIN_PATH}/pg_ctl --timeout=15 --wait --pgdata=data --log=log/logfile start
  fi
}

local_db_create_superuser_role() {
  if [ $UID -eq 0 ]; then
    su postgres -c "${POSTGRES_BIN_PATH}/psql -h \"$1\" -d postgres -c 'CREATE ROLE root WITH SUPERUSER CREATEDB CREATEROLE REPLICATION LOGIN'"
    su postgres -c "${POSTGRES_BIN_PATH}/psql -h \"$1\" -d postgres -c 'CREATE DATABASE root WITH OWNER root'"
  else
    # create role and db root for podman test.
    ${POSTGRES_BIN_PATH}/psql -h "$1" -d postgres -c 'CREATE ROLE root WITH SUPERUSER CREATEDB CREATEROLE REPLICATION LOGIN'
    ${POSTGRES_BIN_PATH}/psql -h "$1" -d postgres -c 'CREATE DATABASE root WITH OWNER root'
  fi
}

local_detect_pg_version(){
  # PG_VERSION will be pick from backuped PG data dir
  export PG_VERSION="$(cut -d '.' -f1 data/PG_VERSION)"
  if [ ${PG_VERSION} -lt 10 ]; then
    local_db_stop_server "$1" || true
    # skip test
    echo "${TestName} test skipped: not compatible with PG <= 10"
    exit 77;
  fi
}

setup_local_db() {
  local_db_stop_server "$1" || true
  local_db_prepare_files "$1"
  if ! local_db_start_server "$1"; then return 1; fi
  local_db_create_superuser_role "$1"
  if [ $UID -eq 0 ]; then
    echo stop server with "su postgres -c '${POSTGRES_BIN_PATH}/pg_ctl --pgdata=data stop'"
  else
    echo stop server with "${POSTGRES_BIN_PATH}/pg_ctl --pgdata=data stop"
  fi
  local_detect_pg_version
  return 0
}

setup_local_tablespace() {
  if [ $UID -eq 0 ]; then
    su postgres -c "${POSTGRES_BIN_PATH}/psql -h \"$1\" -d postgres -c \"CREATE TABLESPACE table_space LOCATION '$(pwd)/table_space';\""
    su postgres -c "${POSTGRES_BIN_PATH}/psql -h \"$1\" -d postgres -c \"CREATE TABLESPACE index_space LOCATION '$(pwd)/index_space';\""
  else
    ${POSTGRES_BIN_PATH}/psql -h "$1" -d postgres -c "CREATE TABLESPACE table_space LOCATION '$(pwd)/table_space';"
    ${POSTGRES_BIN_PATH}/psql -h "$1" -d postgres -c "CREATE TABLESPACE index_space LOCATION '$(pwd)/index_space';"
  fi
}
