#!/bin/bash

# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
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

set -e
set -o pipefail
set -u

: "${BAREOS_SOURCE_DIR:?}"
: "${BAREOS_INSTALL_STAGE_DIR:?}"
: "${BAREOS_BCONFIG_PORT:=8080}"
: "${BAREOS_WEBUI_PORT:?}"
: "${BAREOS_WEBUI_PROXY_PORT:?}"
: "${BAREOS_SETUP_REPOSITORY_PATH:?}"
: "${BAREOS_SETUP_RUNTIME_ROOT:?}"
: "${BAREOS_SETUP_DIRECTOR_PORT:?}"
: "${BAREOS_SETUP_CLIENT_PORT:?}"
: "${BAREOS_SETUP_STORAGE_PORT:?}"

STACK_LOG_DIR=/var/log/setup-wizard
POSTGRES_DATA_DIR=/var/lib/pgsql/data
POSTGRES_SOCKET_DIR=/run/postgresql
DEFAULT_DAEMON_ADDRESS=$(hostname -f 2>/dev/null || hostname)

mkdir -p \
  /etc/bareos \
  /etc/systemd/system/setup-wizard-frontend.service.d \
  "${POSTGRES_SOCKET_DIR}" \
  "${STACK_LOG_DIR}"

install_runtime_packages()
{
  local attempt
  for attempt in 1 2 3; do
    rm -f /var/cache/dnf/metadata_lock.pid
    if dnf -y install \
      git-core \
      jansson \
      libedit \
      libpq \
      libtirpc \
      lzo \
      openldap \
      postgresql \
      postgresql-server \
      readline; then
      return 0
    fi
    sleep 2
  done

  echo "failed to install runtime packages after retries" >&2
  exit 1
}

install_local_bareos_tree()
{
  if [ ! -d "${BAREOS_INSTALL_STAGE_DIR}/usr" ]; then
    echo "staged local install tree not found: ${BAREOS_INSTALL_STAGE_DIR}" >&2
    exit 1
  fi

  cp -a "${BAREOS_INSTALL_STAGE_DIR}/usr/." /usr/
  if [ -d "${BAREOS_INSTALL_STAGE_DIR}/etc" ]; then
    cp -a "${BAREOS_INSTALL_STAGE_DIR}/etc/." /etc/
  fi

  systemctl daemon-reload
}

ensure_bareos_accounts()
{
  if ! getent group bareos >/dev/null; then
    groupadd --system bareos
  fi

  if ! id bareos >/dev/null 2>&1; then
    useradd --system --gid bareos --home-dir /var/lib/bareos \
      --shell /sbin/nologin bareos
  fi
}

disable_bareos_daemons()
{
  systemctl disable --now \
    bareos-fd.service \
    bareos-sd.service \
    bareos-dir.service
}

configure_postgresql()
{
  chown postgres:postgres /var/lib/pgsql "${POSTGRES_SOCKET_DIR}" \
    /var/log/postgresql
  chmod 2775 "${POSTGRES_SOCKET_DIR}"

  if [ ! -f "${POSTGRES_DATA_DIR}/PG_VERSION" ]; then
    /usr/bin/postgresql-setup --initdb
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

  systemctl enable --now postgresql.service
}

configure_bareos_runtime_paths()
{
  mkdir -p "${BAREOS_SETUP_RUNTIME_ROOT}" /var/log/bareos
  chown -R bareos:bareos "${BAREOS_SETUP_RUNTIME_ROOT}" /var/log/bareos
}

configure_bconfig_service()
{
  cat >/etc/systemd/system/bconfig-service.service <<EOF
[Unit]
Description=Bareos Configuration Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
Environment=PGHOST=127.0.0.1
Environment=PGPORT=5432
Environment=PGUSER=postgres
Environment=BCONFIG_PSQL_BINARY=/usr/bin/psql
Environment=BCONFIG_BAREOS_CREATE_DATABASE_SCRIPT=/usr/lib/bareos/scripts/create_bareos_database
Environment=BCONFIG_BAREOS_MAKE_TABLES_SCRIPT=/usr/lib/bareos/scripts/make_bareos_tables
Environment=BCONFIG_BAREOS_GRANT_PRIVILEGES_SCRIPT=/usr/lib/bareos/scripts/grant_bareos_privileges
Environment=BCONFIG_BAREOS_CONFIG_LIB=/usr/lib/bareos/scripts/bareos-config-lib.sh
Environment=BCONFIG_BAREOS_SQL_DDL_DIR=/usr/lib/bareos/scripts/ddl
ExecStart=/usr/sbin/bconfig-service --address 127.0.0.1 --port ${BAREOS_BCONFIG_PORT} --state-dir /var/lib/bconfig/service-state
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

  mkdir -p /var/lib/bconfig
  systemctl daemon-reload
  systemctl enable --now bconfig-service.service
}

configure_webui_proxy()
{
  cat >/etc/bareos/bareos-webui-proxy.ini <<EOF
[listen]
ws_host = 0.0.0.0
ws_port = ${BAREOS_WEBUI_PROXY_PORT}

[director:bareos-dir]
host = ${DEFAULT_DAEMON_ADDRESS}
port = ${BAREOS_SETUP_DIRECTOR_PORT}
director_name = bareos-dir
EOF

  cat >/etc/systemd/system/bareos-webui-proxy.service <<EOF
[Unit]
Description=Bareos WebUI WebSocket Proxy
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/sbin/bareos-webui-proxy --config /etc/bareos/bareos-webui-proxy.ini
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

  systemctl daemon-reload
  systemctl enable --now bareos-webui-proxy.service
}

configure_frontend_service()
{
  cat >/etc/systemd/system/setup-wizard-frontend.service <<EOF
[Unit]
Description=Setup Wizard Frontend
After=network-online.target bconfig-service.service bareos-webui-proxy.service
Wants=network-online.target

[Service]
Type=simple
Environment=BAREOS_SETUP_DIST_DIR=/usr/share/bareos-webui-vue
Environment=BAREOS_BCONFIG_HOST=127.0.0.1
Environment=BAREOS_BCONFIG_PORT=${BAREOS_BCONFIG_PORT}
Environment=BAREOS_SETUP_WS_PROXY_HOST=127.0.0.1
Environment=BAREOS_SETUP_WS_PROXY_PORT=${BAREOS_WEBUI_PROXY_PORT}
Environment=BAREOS_SETUP_LISTEN_HOST=0.0.0.0
Environment=BAREOS_SETUP_LISTEN_PORT=${BAREOS_WEBUI_PORT}
Environment=BAREOS_SETUP_DEFAULT_REPOSITORY_PATH=${BAREOS_SETUP_REPOSITORY_PATH}
Environment=BAREOS_SETUP_DEFAULT_RUNTIME_ROOT=${BAREOS_SETUP_RUNTIME_ROOT}
Environment=BAREOS_SETUP_DEFAULT_DAEMON_ADDRESS=${DEFAULT_DAEMON_ADDRESS}
Environment=BAREOS_SETUP_DEFAULT_DIRECTOR_PORT=${BAREOS_SETUP_DIRECTOR_PORT}
Environment=BAREOS_SETUP_DEFAULT_CLIENT_PORT=${BAREOS_SETUP_CLIENT_PORT}
Environment=BAREOS_SETUP_DEFAULT_STORAGE_PORT=${BAREOS_SETUP_STORAGE_PORT}
Environment=PYTHONUNBUFFERED=1
ExecStart=/usr/bin/python3 ${BAREOS_SOURCE_DIR}/systemtests/tests/setup-wizard/setup_wizard_http_server.py
Restart=on-failure
RestartSec=2
StandardOutput=append:${STACK_LOG_DIR}/frontend.log
StandardError=append:${STACK_LOG_DIR}/frontend.log

[Install]
WantedBy=multi-user.target
EOF

  systemctl daemon-reload
  systemctl enable --now setup-wizard-frontend.service
}

wait_http_ready()
{
  local url=$1
  local label=$2
  for _ in $(seq 1 90); do
    if curl --silent --fail "${url}" >/dev/null; then
      return 0
    fi
    sleep 1
  done
  echo "${label} did not become ready: ${url}" >&2
  return 1
}

install_runtime_packages
install_local_bareos_tree
ensure_bareos_accounts
disable_bareos_daemons
configure_postgresql
configure_bareos_runtime_paths
configure_bconfig_service
wait_http_ready "http://127.0.0.1:${BAREOS_BCONFIG_PORT}/api/bconfig/v1/deployments" \
  "bconfig-service"
configure_webui_proxy
configure_frontend_service
wait_http_ready "http://127.0.0.1:${BAREOS_WEBUI_PORT}/" "setup-wizard frontend"

echo "podman setup-wizard stack is ready:"
echo "  frontend: http://127.0.0.1:${BAREOS_WEBUI_PORT}/"
echo "  bconfig-service: http://127.0.0.1:${BAREOS_BCONFIG_PORT}/api/bconfig/v1/"
echo "  repository path: ${BAREOS_SETUP_REPOSITORY_PATH}"
echo "  runtime root: ${BAREOS_SETUP_RUNTIME_ROOT}"
echo "  daemon ports: ${BAREOS_SETUP_DIRECTOR_PORT}/${BAREOS_SETUP_CLIENT_PORT}/${BAREOS_SETUP_STORAGE_PORT}"
