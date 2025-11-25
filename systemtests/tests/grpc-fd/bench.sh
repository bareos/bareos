#!/bin/bash

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

set -eou pipefail

#shellcheck source=../../environment.in
source environment
#shellcheck source=../../scripts/functions
source "${BAREOS_SCRIPTS_DIR}"/functions

DIRECTORY="$1"
COUNT="${2:-10}"

DATA_PATH="${HOME}/test_data/${DIRECTORY}"

if ! [ -d "${DATA_PATH}" ]; then
  echo "path '${DATA_PATH}' does not exist"
  exit 1
fi

FILESETS=("PythonNative" "PythonNative_do_io_in_core"
  "PythonCore" "PythonCore_do_io_in_core"
  "PythonTest" "PythonTest_do_io_in_core")

./test-setup
echo "${DATA_PATH}" >"${tmp}/file-list"

echo "@tee ${tmp}/bench.out" >"${tmp}/benchcmds.out"
for fileset in "${FILESETS[@]}"; do
  (for _ in $(seq "${COUNT}"); do
    echo "run job=backup-bareos-fd fileset=${fileset} storage=Null level=Full yes"
  done) >>"${tmp}/benchcmds.out"
done
echo "wait" >>"${tmp}/benchcmds.out"
bin/bconsole <"${tmp}/benchcmds.out"

cat <<EOF | run_query_with_options "-t" "-A" | jq 'map( { (.fileset|tostring) : (. | del(.fileset)) } ) | add' >"${tmp}/bench-query.json"
WITH durations AS (
     SELECT filesetid, realendtime - starttime AS duration FROM job
)
SELECT json_agg(t) FROM
(SELECT
    fileset,
    EXTRACT(EPOCH FROM MIN(duration)) AS min,
    EXTRACT(EPOCH FROM percentile_cont(0.25) WITHIN GROUP (ORDER BY duration)) AS q1,
    EXTRACT(EPOCH FROM percentile_cont(0.5)  WITHIN GROUP (ORDER BY duration)) AS median,
    EXTRACT(EPOCH FROM percentile_cont(0.75) WITHIN GROUP (ORDER BY duration)) AS q3,
    EXTRACT(EPOCH FROM MAX(duration)) AS max
FROM
  durations JOIN fileset USING (filesetid)
GROUP BY fileset) as t
EOF
