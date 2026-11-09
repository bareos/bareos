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

check_subscription_accounting()
{
  local phase=$1 expected_chain=$2 previous_phase=${3:-}
  local mode report ready attempt windows=no
  if on_windows_host; then
    windows=yes
  fi

  for mode in updated recalculated; do
    if [[ "$mode" == recalculated ]]; then
      run_bconsole - <<END_OF_DATA || return 1
status subscriptions recalculate
END_OF_DATA
    fi
    report="${tmp}/accounting-${phase}-${mode}.json"
    ready=no
    for attempt in {1..60}; do
      run_bconsole - <<END_OF_DATA || return 1
.api 2
@$out ${report}
status subscriptions client=bareos-fd
END_OF_DATA
      if grep -q '"refresh_thread_state": "idle"' "$report"; then
        ready=yes
        break
      fi
      sleep 1
    done
    if [[ "$ready" != yes ]]; then
      echo "Accounting worker did not become idle (${phase}, ${mode})" >&2
      cat "$report" >&2
      return 1
    fi
  done

  "${PYTHON_EXECUTABLE}" - "$tmp" "$phase" "$expected_chain" \
    "$previous_phase" "$BackupDirectory" "$windows" <<'PY'
import json
import os
import pathlib
import stat
import sys

tmp, phase, expected_chain, previous, source, windows = sys.argv[1:]


def load_row(phase, mode):
    text = (pathlib.Path(tmp) / f"accounting-{phase}-{mode}.json").read_text()
    result = json.JSONDecoder().raw_decode(text[text.index("{"):])[0]["result"]
    snapshot = result["accounting_snapshot"]
    assert snapshot["available"], snapshot
    assert not snapshot.get("last_refresh_error"), snapshot
    rows = [row for row in result["accounting"] if row["fileset"] == "SelfTest"]
    assert len(rows) == 1, rows
    row = rows[0]
    assert not row["excluded"] and not row["estimated"], row
    assert row["rule"] == ("st_size" if windows == "yes" else "st_blocks*512"), row
    return row


files = logical_bytes = 0
for directory, _, names in os.walk(source):
    for name in names:
        attributes = os.lstat(os.path.join(directory, name))
        if stat.S_ISREG(attributes.st_mode):
            files += 1
            logical_bytes += attributes.st_size

updated = load_row(phase, "updated")
recalculated = load_row(phase, "recalculated")
keys = ("files", "bytes", "logical_bytes", "rule", "jobs_in_chain")
assert all(updated[key] == recalculated[key] for key in keys), (
    updated,
    recalculated,
)
assert updated["files"] == files and files > 0, (updated, files)
assert updated["logical_bytes"] == logical_bytes, (updated, logical_bytes)
assert updated["bytes"] > 0, updated
if windows == "yes":
    assert updated["bytes"] == logical_bytes, updated
if int(expected_chain):
    assert updated["jobs_in_chain"] == int(expected_chain), updated
if previous:
    before = load_row(previous, "recalculated")
    keys = ("files", "bytes", "logical_bytes", "rule")
    assert all(before[key] == updated[key] for key in keys), (before, updated)
    assert updated["jobs_in_chain"] < before["jobs_in_chain"], (before, updated)
PY
}
