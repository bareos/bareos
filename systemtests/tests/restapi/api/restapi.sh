#!/bin/bash

#
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2021-2023 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

set -e
set -u

cd $(dirname "$BASH_SOURCE")
. ../environment
. ../environment-local

if [ "${PYTHONPATH:-}" ]; then
    export PYTHONPATH=${CMAKE_SOURCE_DIR}/restapi/:$PYTHONPATH
else
    export PYTHONPATH=${CMAKE_SOURCE_DIR}/restapi/
fi

LSOF_CMD="lsof -n -iTCP:$REST_API_PORT -sTCP:LISTEN"

start()
{
    printf "Starting bareos-restapi: "
    if LSOF=$(${LSOF_CMD}); then
        printf " FAILED: port $REST_API_PORT already in use\n"
        printf " %s\n" "${LSOF}"
        exit 1
    fi

    $PYTHON_EXECUTABLE -m uvicorn bareos_restapi:app --port ${REST_API_PORT} > ../log/bareos-restapi.log 2>&1 &
    PID=$!
    
    WAIT=10
    while ! curl --silent ${REST_API_URL}/token >/dev/null; do
        if ! ps -p $PID >/dev/null; then
            printf " FAILED\n"
            exit 1
        fi

        WAIT=$[$WAIT-1]
        if [ "$WAIT" -le 0 ]; then
            printf " FAILED\n"
            exit 2
        fi

        printf "."
        sleep 1
    done
    echo $PID > api.pid
    printf " OK (PORT=${REST_API_PORT}, PID=$PID)\n"
}

stop()
{
  printf "Stopping bareos-restapi: "
  if [ -e api.pid ]; then
    PID=$(cat api.pid)
    kill $PID
    rm api.pid
    printf "OK\n"
    return
  fi
  printf "OK (already stopped)\n"
}

status()
{
  printf "bareos-restapi: "
  if ! lsof -i:$REST_API_PORT >/dev/null; then
    printf "not running\n"
    return 1
  fi
  PORTPID=$(${LSOF_CMD} -t)
  PID=$(cat api.pid)
  if [ "$PORTPID" != "$PID" ]; then
    printf "running with unexpected PID (expected PID=$PID, running PID=$PORTPID)\n"
    return 1
  fi
  printf "running (PORT=${REST_API_PORT}, PID=$PID)\n"
  return 0
}

case "$1" in
   start)
      start
      ;;

   stop)
      stop
      ;;

  restart)
      stop
      sleep 1
      start
      ;;

  forcestart)
      status || true
      stop >/dev/null 2>&1 || true
      if PID=$(${LSOF_CMD} -t); then
        kill $PID
        sleep 1
      fi
      start
      ;;

   status)
      status
      exit $?
      ;;

   *)
      echo "Usage: $0 {start|stop|restart|status}"
      exit 1
      ;;
esac

exit 0
