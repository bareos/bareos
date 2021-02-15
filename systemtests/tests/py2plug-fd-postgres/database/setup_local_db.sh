#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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
    su postgres -c "pg_ctl --pgdata=data stop"
  else
    pg_ctl --silent --pgdata=data stop
  fi
  tries=10
  while psql --host="$1" --list > /dev/null 2>&1; do
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
  rm --recursive --force tmp data log wal_archive
  mkdir tmp data log wal_archive
  if [ $UID -eq 0 ]; then
    chown postgres  tmp data log wal_archive
    LANG= su postgres -c "pg_ctl --pgdata=data --log=log/postgres.log initdb"
  else
    LANG= pg_ctl --silent --pgdata=data --log=log/postgres.log initdb
  fi
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
  echo "start db server"
  if [ $UID -eq 0 ]; then
    su postgres -c "pg_ctl -w --pgdata=data --log=log/logfile start"
  else
    pg_ctl --silent --pgdata=data --log=log/logfile start
  fi
#  tries=60
#  while ! psql --host="$1" --list > /dev/null 2>&1; do
#    [ $((tries-=1)) -eq 0 ] && {
#      echo "Could not start postgres server"
#      return 1
#    }
#    sleep 1
#  done

#  tries=60
#  while ! echo "select pg_is_in_recovery()" | psql --host="$1" postgres | grep -q -e "^ f$" ; do
#    [ $((tries-=1)) -eq 0 ] && {
#      echo "Could not start postgres server (still recovering)"
#      return 1
#    }
#    sleep 1
#  done

  return 0
}

local_db_create_superuser_role() {
  if [ $UID -eq 0 ]; then
    su postgres -c "createuser -s --host=$1 root"
    su postgres -c "createdb   --host=$1 root"
  else
    echo "CREATE ROLE root WITH SUPERUSER CREATEDB CREATEROLE REPLICATION LOGIN" | psql -h "$1" postgres
  fi
}

setup_local_db() {
  local_db_stop_server "$1"
  local_db_prepare_files "$1"
  if ! local_db_start_server "$1"; then return 1; fi
  local_db_create_superuser_role "$1"
  if [ $UID -eq 0 ]; then
    echo stop server with "su postgres -c 'pg_ctl --pgdata=data stop'"
  else
    echo stop server with "pg_ctl --pgdata=data stop"
  fi
  return 0
}
