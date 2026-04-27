#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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
#
# Bootstrap a default-like Bareos deployment in bconfig_service by calling the
# public REST API. This creates a deployment, component daemons, the main
# director/storage/client resources, validates the generated repository, and
# commits it.

set -euo pipefail

BASE_URL=${BASE_URL:-http://127.0.0.1:8080}

DEPLOYMENT_ID=${DEPLOYMENT_ID:-prod}
DEPLOYMENT_NAME=${DEPLOYMENT_NAME:-Production}
REPOSITORY_PATH=${REPOSITORY_PATH:-$PWD/${DEPLOYMENT_ID}-repo}
COMMIT_MESSAGE=${COMMIT_MESSAGE:-Create default Bareos deployment}

DIRECTOR_NAME=${DIRECTOR_NAME:-bareos-dir}
STORAGE_DAEMON_NAME=${STORAGE_DAEMON_NAME:-bareos-sd}
CLIENT_NAME=${CLIENT_NAME:-bareos-fd}
DIRECTOR_STORAGE_NAME=${DIRECTOR_STORAGE_NAME:-File}
DEVICE_NAME=${DEVICE_NAME:-FileStorage}
CATALOG_NAME=${CATALOG_NAME:-MyCatalog}
POOL_NAME=${POOL_NAME:-File}
SCHEDULE_NAME=${SCHEDULE_NAME:-WeeklyCycle}
FILESET_NAME=${FILESET_NAME:-SelfTest}
JOBDEFS_NAME=${JOBDEFS_NAME:-DefaultJob}
JOB_NAME=${JOB_NAME:-backup-bareos-fd}

CONSOLE_CONFIG_NAME=${CONSOLE_CONFIG_NAME:-admin}
CONSOLE_RESOURCE_NAME=${CONSOLE_RESOURCE_NAME:-admin}
INCLUDE_CONSOLE=${INCLUDE_CONSOLE:-1}

DIRECTOR_ADDRESS=${DIRECTOR_ADDRESS:-127.0.0.1}
DIRECTOR_PORT=${DIRECTOR_PORT:-9101}
STORAGE_ADDRESS=${STORAGE_ADDRESS:-127.0.0.1}
STORAGE_PORT=${STORAGE_PORT:-9103}
CLIENT_ADDRESS=${CLIENT_ADDRESS:-127.0.0.1}
CLIENT_PORT=${CLIENT_PORT:-9102}

DIRECTOR_PASSWORD=${DIRECTOR_PASSWORD:-director-secret}
STORAGE_DIRECTOR_PASSWORD=${STORAGE_DIRECTOR_PASSWORD:-storage-dir-secret}
CLIENT_DIRECTOR_PASSWORD=${CLIENT_DIRECTOR_PASSWORD:-client-dir-secret}
CONSOLE_PASSWORD=${CONSOLE_PASSWORD:-console-secret}
CATALOG_PASSWORD=${CATALOG_PASSWORD:-catalog-secret}

DIRECTOR_WORKING_DIRECTORY=${DIRECTOR_WORKING_DIRECTORY:-/var/lib/bareos}
STORAGE_WORKING_DIRECTORY=${STORAGE_WORKING_DIRECTORY:-/var/lib/bareos/storage}
CLIENT_WORKING_DIRECTORY=${CLIENT_WORKING_DIRECTORY:-/var/lib/bareos}
DEVICE_ARCHIVE_DIRECTORY=${DEVICE_ARCHIVE_DIRECTORY:-/var/lib/bareos/storage}
DIRECTOR_QUERY_FILE=${DIRECTOR_QUERY_FILE:-/usr/lib/bareos/scripts/query.sql}

CATALOG_DB_NAME=${CATALOG_DB_NAME:-bareos}
CATALOG_DB_USER=${CATALOG_DB_USER:-bareos}

POLL_INTERVAL_SECONDS=${POLL_INTERVAL_SECONDS:-1}

require_command()
{
  command -v "$1" >/dev/null 2>&1 || {
    printf 'missing required command: %s\n' "$1" >&2
    exit 1
  }
}

json_get()
{
  jq --raw-output ".$1"
}

job_logs()
{
  jq --raw-output '.job.logs[]?'
}

get_json()
{
  curl --silent --show-error --fail-with-body "$1"
}

post_json()
{
  local url="$1"
  curl --silent --show-error --fail-with-body --json @- "$url"
}

put_json()
{
  local url="$1"
  curl --silent --show-error --fail-with-body -X PUT \
    -H 'Content-Type: application/json' \
    --data-binary @- \
    "$url"
}

create_job()
{
  local payload="$1"
  local response
  response=$(printf '%s' "$payload" | post_json "$BASE_URL/v1/jobs")
  printf '%s' "$response" | json_get "job.id"
}

wait_for_job()
{
  local job_id="$1"
  local response status

  while true; do
    response=$(get_json "$BASE_URL/v1/jobs/$job_id")
    status=$(printf '%s' "$response" | json_get "job.status")
    case "$status" in
      queued|running)
        sleep "$POLL_INTERVAL_SECONDS"
        ;;
      succeeded)
        printf '%s\n' "job $job_id succeeded"
        printf '%s' "$response" | job_logs
        return 0
        ;;
      failed)
        printf '%s\n' "job $job_id failed" >&2
        printf '%s\n' "last_error: $(printf '%s' "$response" | json_get "job.last_error")" >&2
        printf '%s' "$response" | job_logs >&2
        return 1
        ;;
      *)
        printf '%s\n' "job $job_id returned unexpected status: $status" >&2
        printf '%s\n' "$response" >&2
        return 1
        ;;
    esac
  done
}

ensure_deployment()
{
  local status_code

  status_code=$(curl --silent --output /dev/null --write-out '%{http_code}' \
    "$BASE_URL/v1/deployments/$DEPLOYMENT_ID")
  if [ "$status_code" = "200" ]; then
    printf 'deployment %s already exists, reusing it\n' "$DEPLOYMENT_ID"
    return 0
  fi
  if [ "$status_code" != "404" ]; then
    printf 'failed to probe deployment %s: HTTP %s\n' \
      "$DEPLOYMENT_ID" "$status_code" >&2
    return 1
  fi

  printf 'creating deployment %s\n' "$DEPLOYMENT_ID"
  mkdir -p "$(dirname "$REPOSITORY_PATH")"
  post_json "$BASE_URL/v1/deployments" <<EOF >/dev/null
{
  "id": "$DEPLOYMENT_ID",
  "name": "$DEPLOYMENT_NAME",
  "repository_path": "$REPOSITORY_PATH"
}
EOF
}

put_resource()
{
  local title="$1"
  local url="$2"
  local payload="$3"

  printf 'upserting %s\n' "$title"
  printf '%s' "$payload" | put_json "$url" >/dev/null
}

require_command curl
require_command jq

ensure_deployment

put_resource "director daemon messages" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/messages/Daemon" \
  "$(cat <<'EOF'
{
  "description": "Message delivery for daemon messages.",
  "mail_command": "/usr/sbin/bsmtp -h smtp_host -f \"(Bareos) <%r>\" -s \"Bareos daemon message\" %r",
  "mail_entries": ["root@localhost = all, !skipped, !audit"],
  "console_entries": ["all, !skipped, !saved, !audit"],
  "append_entries": [
    "/var/log/bareos/bareos.log = all, !skipped, !audit",
    "/var/log/bareos/bareos-audit.log = audit"
  ]
}
EOF
)"

put_resource "director standard messages" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/messages/Standard" \
  "$(cat <<'EOF'
{
  "description": "Reasonable message delivery.",
  "mail_command": "/usr/sbin/bsmtp -h smtp_host -f \"(Bareos) <%r>\" -s \"Bareos: %t %e of %c %l\" %r",
  "operator_command": "/usr/sbin/bsmtp -h smtp_host -f \"(Bareos) <%r>\" -s \"Bareos: Intervention needed for %j\" %r",
  "operator_entries": ["root@localhost = mount"],
  "mail_entries": ["root@localhost = all, !skipped, !saved, !audit"],
  "console_entries": ["all, !skipped, !saved, !audit"],
  "append_entries": ["/var/log/bareos/bareos.log = all, !skipped, !saved, !audit"],
  "catalog_entries": ["all, !skipped, !saved, !audit"]
}
EOF
)"

put_resource "storage messages" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/storages/$STORAGE_DAEMON_NAME/messages/Standard" \
  "$(cat <<EOF
{
  "description": "Send all messages to the Director.",
  "director_entries": ["$DIRECTOR_NAME = all"]
}
EOF
)"

put_resource "client messages" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/clients/$CLIENT_NAME/messages/Standard" \
  "$(cat <<EOF
{
  "description": "Send relevant messages to the Director.",
  "director_entries": ["$DIRECTOR_NAME = all, !skipped, !restored"]
}
EOF
)"

put_resource "director daemon" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME" \
  "$(cat <<EOF
{
  "address": "$DIRECTOR_ADDRESS",
  "port": $DIRECTOR_PORT,
  "password": "$DIRECTOR_PASSWORD",
  "working_directory": "$DIRECTOR_WORKING_DIRECTORY",
  "messages": "Daemon",
  "query_file": "$DIRECTOR_QUERY_FILE",
  "maximum_concurrent_jobs": 20
}
EOF
)"

put_resource "storage daemon" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/storages/$STORAGE_DAEMON_NAME" \
  "$(cat <<EOF
{
  "address": "$STORAGE_ADDRESS",
  "port": $STORAGE_PORT,
  "working_directory": "$STORAGE_WORKING_DIRECTORY",
  "messages": "Standard",
  "maximum_concurrent_jobs": 20
}
EOF
)"

put_resource "client daemon" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/clients/$CLIENT_NAME" \
  "$(cat <<EOF
{
  "address": "$CLIENT_ADDRESS",
  "port": $CLIENT_PORT,
  "working_directory": "$CLIENT_WORKING_DIRECTORY",
  "messages": "Standard",
  "maximum_concurrent_jobs": 20
}
EOF
)"

put_resource "storage device" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/storages/$STORAGE_DAEMON_NAME/devices/$DEVICE_NAME" \
  "$(cat <<EOF
{
  "media_type": "File",
  "archive_device": "$DEVICE_ARCHIVE_DIRECTORY",
  "device_type": "File",
  "description": "File device"
}
EOF
)"

put_resource "director storage" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/storages/$DIRECTOR_STORAGE_NAME" \
  "$(cat <<EOF
{
  "address": "$STORAGE_ADDRESS",
  "port": $STORAGE_PORT,
  "password": "$STORAGE_DIRECTOR_PASSWORD",
  "device": "$DEVICE_NAME",
  "media_type": "File"
}
EOF
)"

put_resource "stored director" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/storages/$STORAGE_DAEMON_NAME/directors/$DIRECTOR_NAME" \
  "$(cat <<EOF
{
  "password": "$STORAGE_DIRECTOR_PASSWORD"
}
EOF
)"

put_resource "catalog" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/catalogs/$CATALOG_NAME" \
  "$(cat <<EOF
{
  "db_name": "$CATALOG_DB_NAME",
  "db_user": "$CATALOG_DB_USER",
  "db_password": "$CATALOG_PASSWORD",
  "description": "Bareos catalog"
}
EOF
)"

put_resource "director client" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/clients/$CLIENT_NAME" \
  "$(cat <<EOF
{
  "address": "$CLIENT_ADDRESS",
  "port": $CLIENT_PORT,
  "password": "$CLIENT_DIRECTOR_PASSWORD",
  "catalog": "$CATALOG_NAME"
}
EOF
)"

put_resource "filed director" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/clients/$CLIENT_NAME/directors/$DIRECTOR_NAME" \
  "$(cat <<EOF
{
  "address": "$DIRECTOR_ADDRESS",
  "port": $DIRECTOR_PORT,
  "password": "$CLIENT_DIRECTOR_PASSWORD"
}
EOF
)"

put_resource "pool" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/pools/$POOL_NAME" \
  "$(cat <<EOF
{
  "pool_type": "Backup",
  "label_format": "File-",
  "recycle": true,
  "auto_prune": true,
  "catalog": "$CATALOG_NAME",
  "storages": ["$DIRECTOR_STORAGE_NAME"]
}
EOF
)"

put_resource "schedule" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/schedules/$SCHEDULE_NAME" \
  "$(cat <<'EOF'
{
  "run_entries": [
    "Full 1st sat at 21:00",
    "Differential 2nd-5th sat at 21:00",
    "Incremental mon-fri at 21:00"
  ]
}
EOF
)"

put_resource "fileset" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/filesets/$FILESET_NAME" \
  "$(cat <<'EOF'
{
  "description": "Default client fileset",
  "include_blocks": [
    "Include {\n  Options {\n    Signature = XXH128\n  }\n  File = /\n}"
  ]
}
EOF
)"

put_resource "jobdefs" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/jobdefs/$JOBDEFS_NAME" \
  "$(cat <<EOF
{
  "type": "Backup",
  "level": "Incremental",
  "client": "$CLIENT_NAME",
  "fileset": "$FILESET_NAME",
  "schedule": "$SCHEDULE_NAME",
  "messages": "Standard",
  "storages": ["$DIRECTOR_STORAGE_NAME"],
  "pool": "$POOL_NAME"
}
EOF
)"

put_resource "job" \
  "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/directors/$DIRECTOR_NAME/jobs/$JOB_NAME" \
  "$(cat <<EOF
{
  "jobdefs": "$JOBDEFS_NAME",
  "description": "Backup $CLIENT_NAME"
}
EOF
)"

if [ "$INCLUDE_CONSOLE" = "1" ]; then
  put_resource "console director" \
    "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/consoles/$CONSOLE_CONFIG_NAME/directors/$DIRECTOR_NAME" \
    "$(cat <<EOF
{
  "address": "$DIRECTOR_ADDRESS",
  "port": $DIRECTOR_PORT,
  "password": "$CONSOLE_PASSWORD"
}
EOF
)"

  put_resource "console resource" \
    "$BASE_URL/v1/deployments/$DEPLOYMENT_ID/consoles/$CONSOLE_CONFIG_NAME/consoles/$CONSOLE_RESOURCE_NAME" \
    "$(cat <<EOF
{
  "director": "$DIRECTOR_NAME",
  "password": "$CONSOLE_PASSWORD"
}
EOF
)"
fi

printf 'validating deployment repository\n'
validate_job_id=$(create_job "$(cat <<EOF
{
  "type": "validate_deployment_repo",
  "deployment_id": "$DEPLOYMENT_ID"
}
EOF
)")
wait_for_job "$validate_job_id"

printf 'committing deployment repository\n'
commit_job_id=$(create_job "$(cat <<EOF
{
  "type": "commit_deployment_repo",
  "deployment_id": "$DEPLOYMENT_ID",
  "commit_message": "$COMMIT_MESSAGE"
}
EOF
)")
wait_for_job "$commit_job_id"

printf 'bootstrap complete for deployment %s\n' "$DEPLOYMENT_ID"
