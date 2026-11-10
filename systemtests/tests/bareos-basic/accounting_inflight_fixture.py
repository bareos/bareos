#   BAREOS - Backup Archiving REcovery Open Sourced
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

import contextlib
import io
import json
import os
import subprocess
import time

from accounting_edge_fixture import import_job


def sql(query):
    return subprocess.run(
        [
            "psql",
            "--no-psqlrc",
            "--quiet",
            "--tuples-only",
            "--no-align",
            "--set=ON_ERROR_STOP=1",
            "--dbname",
            os.environ["db_name"],
        ],
        input="SET statement_timeout='10s';\n" + query,
        universal_newlines=True,
        stdout=subprocess.PIPE,
        check=True,
        timeout=15,
    ).stdout.strip()


def console(commands):
    return subprocess.run(
        [
            os.environ["BAREOS_BCONSOLE_BINARY"],
            "--config",
            os.environ["BAREOS_CONFIG_DIR"],
        ],
        input=commands,
        universal_newlines=True,
        stdout=subprocess.PIPE,
        check=True,
        timeout=30,
    ).stdout


def wait_for(predicate, description):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.05)
    raise AssertionError(f"Timed out waiting for {description}")


def check_inflight_requeue():
    wait_for(
        lambda: "worker: idle." in console("status subscriptions state\n"),
        "the accounting worker to become idle",
    )
    sql(
        "INSERT INTO Client (Name, Uname) SELECT 'acct-chain-edges', 'Linux' "
        "WHERE NOT EXISTS (SELECT 1 FROM Client WHERE Name='acct-chain-edges')"
    )
    commands = io.StringIO()
    with contextlib.redirect_stdout(commands):
        import_job(
            "AcctInFlight", "inflight-full", "F", 2000000200, [("old", "B", "B")]
        )
        import_job("AcctInFlight", "inflight-inc", "I", 2000000201, [])
    console(commands.getvalue())
    full_id, inc_id = map(
        int,
        sql(
            "SELECT JobId FROM Job WHERE Job IN "
            "('acct-edge-inflight-full', 'acct-edge-inflight-inc') ORDER BY JobId"
        ).splitlines(),
    )
    # A statement trigger pauses after the scan, before acquiring the Job row
    # lock. Purge can then invalidate the calculation without a timing race.
    sql(f"""
        CREATE FUNCTION acct_pause_save() RETURNS trigger LANGUAGE plpgsql AS $$
        BEGIN
          IF position('AccountingValid=TRUE' in current_query()) > 0
             AND position('WHERE JobId={inc_id} ' in current_query()) > 0 THEN
            PERFORM pg_advisory_xact_lock(2910, 1);
          END IF;
          RETURN NULL;
        END;
        $$;
        CREATE TRIGGER acct_pause_save BEFORE UPDATE OF AccountingValid ON Job
        FOR EACH STATEMENT EXECUTE FUNCTION acct_pause_save();
        """)
    gate = subprocess.Popen(
        [
            "psql",
            "--no-psqlrc",
            "--quiet",
            "--set=ON_ERROR_STOP=1",
            "--dbname",
            os.environ["db_name"],
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        universal_newlines=True,
    )
    try:
        gate.stdin.write("SELECT pg_advisory_lock(2910, 1);\n")
        gate.stdin.flush()
        lock_query = (
            "SELECT COUNT(*) FROM pg_locks WHERE locktype='advisory' "
            "AND database=(SELECT oid FROM pg_database "
            "WHERE datname=current_database()) AND classid=2910 AND objid=1 "
        )
        wait_for(
            lambda: sql(lock_query + "AND granted") == "1",
            "the accounting save gate",
        )
        console("status subscriptions client=acct-chain-edges\n")
        wait_for(
            lambda: sql(lock_query + "AND NOT granted") == "1",
            "the in-flight accounting save to reach the gate",
        )
        console(f"purge files jobid={full_id} yes\n")
        assert sql(f"SELECT PurgedFiles FROM Job WHERE JobId={full_id}") == "1"
        assert (
            sql(f"SELECT AccountingValid FROM Job WHERE JobId={inc_id}") == "f"
        ), "Purge did not invalidate the in-flight calculation"
    finally:
        gate.stdin.close()
        try:
            gate.wait(timeout=10)
        except subprocess.TimeoutExpired:
            gate.kill()
            gate.wait()
            raise
        finally:
            sql("DROP TRIGGER acct_pause_save ON Job; DROP FUNCTION acct_pause_save()")

    wait_for(
        lambda: "worker: idle." in console("status subscriptions state\n"),
        "the accounting worker to finish the invalidated job",
    )
    persisted = json.loads(
        sql(
            "SELECT json_build_object("
            "'valid', AccountingValid, 'excluded', AccountingExcluded, "
            "'reason', AccountingExclusionReason, 'files', AccountingFiles, "
            "'bytes', AccountingBytes, 'logical_bytes', AccountingLogicalBytes) "
            f"FROM Job WHERE JobId={inc_id}"
        )
    )
    assert persisted == {
        "valid": True,
        "excluded": True,
        "reason": "no_usable_chain",
        "files": 0,
        "bytes": 0,
        "logical_bytes": 0,
    }, f"In-flight purge was overwritten by stale accounting: {persisted}"
    print(
        "In-flight purge preserved: persisted accounting excludes the unusable chain."
    )


if __name__ == "__main__":
    check_inflight_requeue()
