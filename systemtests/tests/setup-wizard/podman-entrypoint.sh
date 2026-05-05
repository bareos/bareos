#!/bin/bash

set -e
set -o pipefail
set -u

: "${BAREOS_SOURCE_DIR:?}"
: "${BAREOS_BUILD_DIR:?}"
: "${BAREOS_BCONFIG_PORT:?}"
: "${BAREOS_WEBUI_PORT:?}"
: "${BAREOS_WEBUI_PROXY_PORT:?}"
: "${BAREOS_SETUP_REPOSITORY_PATH:?}"
: "${BAREOS_SETUP_RUNTIME_ROOT:?}"
: "${BAREOS_SETUP_DIRECTOR_PORT:?}"
: "${BAREOS_SETUP_CLIENT_PORT:?}"
: "${BAREOS_SETUP_STORAGE_PORT:?}"

STACK_LOG_DIR=/var/log/setup-wizard
PROXY_CONFIG=/tmp/setup-wizard-webui-proxy.ini
POSTGRES_DATA_DIR=/var/lib/pgsql/data
POSTGRES_LOG=/var/log/postgresql/postgresql.log
POSTGRES_SOCKET_DIR=/run/postgresql
DEFAULT_DAEMON_ADDRESS=$(hostname -f 2>/dev/null || hostname)
LD_DIRS=(
  "${BAREOS_BUILD_DIR}/core/src/lib"
  "${BAREOS_BUILD_DIR}/core/src/findlib"
  "${BAREOS_BUILD_DIR}/core/src/cats"
  "${BAREOS_BUILD_DIR}/core/src/lmdb"
  "${BAREOS_BUILD_DIR}/core/src/fastlz"
  "${BAREOS_BUILD_DIR}/core/src/ndmp"
  "${BAREOS_BUILD_DIR}/core/src/stored"
  "${BAREOS_BUILD_DIR}/core/src/stored/backends"
)
LD_LIBRARY_PATH_VALUE=$(IFS=:; echo "${LD_DIRS[*]}")

mkdir -p \
  /etc/bareos \
  /usr/local/etc \
  /usr/local/lib/bareos/scripts \
  /usr/local/var/lib \
  /usr/local/sbin \
  /usr/lib/bareos/scripts \
  /var/lib/bconfig \
  /var/lib/bareos \
  /var/log/bareos \
  /var/log/postgresql \
  "${STACK_LOG_DIR}" \
  "${POSTGRES_SOCKET_DIR}"

ln -sf "${BAREOS_BUILD_DIR}/core/src/cats/make_catalog_backup" \
  /usr/lib/bareos/scripts/make_catalog_backup
ln -sf "${BAREOS_BUILD_DIR}/core/src/cats/delete_catalog_backup" \
  /usr/lib/bareos/scripts/delete_catalog_backup
ln -sf "${BAREOS_BUILD_DIR}/core/scripts/bareos-config-lib.sh" \
  /usr/lib/bareos/scripts/bareos-config-lib.sh
ln -sf "${BAREOS_BUILD_DIR}/core/scripts/bareos-config-lib.sh" \
  /usr/local/lib/bareos/scripts/bareos-config-lib.sh
ln -sfn "${BAREOS_SETUP_REPOSITORY_PATH}/directors/bareos-dir" /usr/local/etc/bareos
ln -sfn /var/lib/bareos /usr/local/var/lib/bareos
ln -sf "${BAREOS_BUILD_DIR}/core/src/dird/bareos-dir" /usr/local/sbin/bareos-dir
ln -sf "${BAREOS_BUILD_DIR}/core/src/filed/bareos-fd" /usr/local/sbin/bareos-fd
ln -sf "${BAREOS_BUILD_DIR}/core/src/stored/bareos-sd" /usr/local/sbin/bareos-sd
ln -sf "${BAREOS_BUILD_DIR}/core/src/tools/bsmtp" /usr/sbin/bsmtp

chown postgres:postgres /var/lib/pgsql "${POSTGRES_SOCKET_DIR}" /var/log/postgresql
chmod 2775 "${POSTGRES_SOCKET_DIR}"

if [ ! -f "${POSTGRES_DATA_DIR}/PG_VERSION" ]; then
  runuser -u postgres -- initdb -D "${POSTGRES_DATA_DIR}" >/tmp/initdb.log
  {
    echo "listen_addresses = '127.0.0.1'"
    echo "unix_socket_directories = '${POSTGRES_SOCKET_DIR}'"
  } >>"${POSTGRES_DATA_DIR}/postgresql.conf"
  cat >"${POSTGRES_DATA_DIR}/pg_hba.conf" <<'EOF'
local   all             all                                     trust
host    all             all             127.0.0.1/32            trust
host    all             all             ::1/128                 trust
EOF
fi

cleanup() {
  if [ -n "${frontend_pid-}" ] && kill -0 "${frontend_pid}" 2>/dev/null; then
    kill "${frontend_pid}" || true
  fi
  if [ -n "${proxy_pid-}" ] && kill -0 "${proxy_pid}" 2>/dev/null; then
    kill "${proxy_pid}" || true
  fi
  if [ -n "${bconfig_pid-}" ] && kill -0 "${bconfig_pid}" 2>/dev/null; then
    kill "${bconfig_pid}" || true
  fi
  if [ -n "${postgres_pid-}" ] && kill -0 "${postgres_pid}" 2>/dev/null; then
    kill "${postgres_pid}" || true
    wait "${postgres_pid}" || true
  fi
}
trap cleanup EXIT INT TERM

runuser -u postgres -- /usr/bin/postgres \
  -D "${POSTGRES_DATA_DIR}" \
  -k "${POSTGRES_SOCKET_DIR}" \
  >/var/log/postgresql/postgresql.log 2>&1 &
postgres_pid=$!

for _ in $(seq 1 60); do
  if pg_isready -h 127.0.0.1 -p 5432 >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
pg_isready -h 127.0.0.1 -p 5432 >/dev/null

cat >"${PROXY_CONFIG}" <<EOF
[listen]
ws_host = 0.0.0.0
ws_port = ${BAREOS_WEBUI_PROXY_PORT}

[director:bareos-dir]
host = ${DEFAULT_DAEMON_ADDRESS}
port = ${BAREOS_SETUP_DIRECTOR_PORT}
director_name = bareos-dir
EOF

export LD_LIBRARY_PATH="${LD_LIBRARY_PATH_VALUE}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export BAREOS_CONFIG_DIR="${BAREOS_SETUP_REPOSITORY_PATH}/directors/bareos-dir"
export BAREOS_CONFIG_TEMPLATE_DIR="${BAREOS_CONFIG_DIR}"
export PGHOST=127.0.0.1
export PGPORT=5432
export PGUSER=postgres
export BCONFIG_BAREOS_DIR_BINARY="${BAREOS_BUILD_DIR}/core/src/dird/bareos-dir"
export BCONFIG_BAREOS_FD_BINARY="${BAREOS_BUILD_DIR}/core/src/filed/bareos-fd"
export BCONFIG_BAREOS_SD_BINARY="${BAREOS_BUILD_DIR}/core/src/stored/bareos-sd"
export BCONFIG_BAREOS_SD_BACKEND_DIR="${BAREOS_BUILD_DIR}/core/src/stored/backends"
export BCONFIG_BAREOS_CONFIG_LIB="${BAREOS_BUILD_DIR}/core/scripts/bareos-config-lib.sh"
export BCONFIG_BAREOS_SQL_DDL_DIR="${BAREOS_SOURCE_DIR}/core/src/cats/ddl"
export BCONFIG_PSQL_BINARY=/usr/bin/psql

"${BAREOS_BUILD_DIR}/core/src/tools/bconfig-service" \
  --address 127.0.0.1 \
  --port "${BAREOS_BCONFIG_PORT}" \
  --state-dir /var/lib/bconfig/service-state \
  >"${STACK_LOG_DIR}/bconfig-service.log" 2>&1 &
bconfig_pid=$!

for _ in $(seq 1 60); do
  if curl --silent --fail "http://127.0.0.1:${BAREOS_BCONFIG_PORT}/api/bconfig/v1/deployments" >/dev/null; then
    break
  fi
  sleep 1
done
curl --silent --fail "http://127.0.0.1:${BAREOS_BCONFIG_PORT}/api/bconfig/v1/deployments" >/dev/null

BAREOS_BCONFIG_HOST=127.0.0.1 \
  BAREOS_BCONFIG_PORT="${BAREOS_BCONFIG_PORT}" \
  "${BAREOS_BUILD_DIR}/core/src/webui-proxy/bareos-webui-proxy" \
    --config "${PROXY_CONFIG}" \
    >"${STACK_LOG_DIR}/webui-proxy.log" 2>&1 &
proxy_pid=$!

BAREOS_SETUP_DIST_DIR="${BAREOS_SOURCE_DIR}/webui-vue/dist" \
  BAREOS_BCONFIG_HOST=127.0.0.1 \
  BAREOS_BCONFIG_PORT="${BAREOS_BCONFIG_PORT}" \
  BAREOS_SETUP_WS_PROXY_HOST=127.0.0.1 \
  BAREOS_SETUP_WS_PROXY_PORT="${BAREOS_WEBUI_PROXY_PORT}" \
  BAREOS_SETUP_LISTEN_HOST=0.0.0.0 \
  BAREOS_SETUP_LISTEN_PORT="${BAREOS_WEBUI_PORT}" \
  BAREOS_SETUP_DEFAULT_REPOSITORY_PATH="${BAREOS_SETUP_REPOSITORY_PATH}" \
  BAREOS_SETUP_DEFAULT_RUNTIME_ROOT="${BAREOS_SETUP_RUNTIME_ROOT}" \
  BAREOS_SETUP_DEFAULT_DAEMON_ADDRESS="${DEFAULT_DAEMON_ADDRESS}" \
  BAREOS_SETUP_DEFAULT_DIRECTOR_PORT="${BAREOS_SETUP_DIRECTOR_PORT}" \
  BAREOS_SETUP_DEFAULT_CLIENT_PORT="${BAREOS_SETUP_CLIENT_PORT}" \
  BAREOS_SETUP_DEFAULT_STORAGE_PORT="${BAREOS_SETUP_STORAGE_PORT}" \
  PYTHONUNBUFFERED=1 \
  python3 "${BAREOS_SOURCE_DIR}/systemtests/tests/setup-wizard/setup_wizard_http_server.py" \
  >"${STACK_LOG_DIR}/frontend.log" 2>&1 &
frontend_pid=$!

for _ in $(seq 1 60); do
  if curl --silent --fail "http://127.0.0.1:${BAREOS_WEBUI_PORT}/" >/dev/null; then
    break
  fi
  sleep 1
done
curl --silent --fail "http://127.0.0.1:${BAREOS_WEBUI_PORT}/" >/dev/null

echo "podman setup-wizard stack is ready:"
echo "  frontend: http://127.0.0.1:${BAREOS_WEBUI_PORT}/"
echo "  bconfig-service: http://127.0.0.1:${BAREOS_BCONFIG_PORT}/api/bconfig/v1/"
echo "  repository path: ${BAREOS_SETUP_REPOSITORY_PATH}"
echo "  runtime root: ${BAREOS_SETUP_RUNTIME_ROOT}"
echo "  daemon ports: ${BAREOS_SETUP_DIRECTOR_PORT}/${BAREOS_SETUP_CLIENT_PORT}/${BAREOS_SETUP_STORAGE_PORT}"

wait -n "${bconfig_pid}" "${proxy_pid}" "${frontend_pid}" "${postgres_pid}"
