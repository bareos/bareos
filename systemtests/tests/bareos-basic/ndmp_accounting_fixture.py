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

import json
import pathlib
import sys


def sql_literal(value):
    if isinstance(value, int):
        return str(value)
    return " || chr(10) || ".join(
        "'" + line.replace("'", "''") + "'" for line in value.split("\n")
    )


def sql_command(query):
    print('.sql query="' + query.replace('"', '\\"') + '"')


def import_job(mode, fixture, phase):
    client = f"acct-ndmp-{mode}"
    fileset = f"AcctNdmp{mode.title()}"
    if phase == 0:
        sql_command(
            "INSERT INTO Client (Name, Uname) VALUES "
            f"({sql_literal(client)}, {sql_literal(fixture['client']['uname'])})"
        )
        source = fixture["fileset"]
        sql_command(
            "INSERT INTO FileSet (FileSet, FileSetText, Md5, CreateTime) VALUES "
            f"({sql_literal(fileset)}, {sql_literal(source['filesettext'])}, "
            f"{sql_literal(source['md5'])}, {sql_literal(source['createtime'])})"
        )
    job = fixture["jobs"][phase]
    name = f"{client}-{job['level']}"
    columns = [
        "type",
        "level",
        "jobstatus",
        "starttime",
        "endtime",
        "jobtdate",
        "jobfiles",
        "jobbytes",
        "readbytes",
    ]
    sql_command(
        f"INSERT INTO Job (Job, Name, ClientId, FileSetId, {','.join(columns)}) "
        f"SELECT {sql_literal(name)}, {sql_literal(name)}, ClientId, FileSetId, "
        f"{','.join(sql_literal(job[column]) for column in columns)} "
        f"FROM Client CROSS JOIN FileSet WHERE Client.Name={sql_literal(client)} "
        f"AND FileSet.FileSet={sql_literal(fileset)}"
    )
    for path in sorted({row["path"] for row in job["files"]}):
        sql_command(
            f"INSERT INTO Path (Path) SELECT {sql_literal(path)} WHERE NOT EXISTS "
            f"(SELECT 1 FROM Path WHERE Path={sql_literal(path)})"
        )
    columns = ["fileindex", "name", "lstat", "md5", "deltaseq", "fhinfo", "fhnode"]
    for row in job["files"]:
        sql_command(
            f"INSERT INTO File (JobId, PathId, {','.join(columns)}) "
            f"SELECT JobId, PathId, "
            f"{','.join(sql_literal(row[column]) for column in columns)} "
            f"FROM Job CROSS JOIN Path WHERE Job.Job={sql_literal(name)} "
            f"AND Path.Path={sql_literal(row['path'])}"
        )


def check_report(mode, fixture, phase, report):
    text = report.read_text()
    result = json.JSONDecoder().raw_decode(text[text.index("{") :])[0]["result"]
    rows = [
        row
        for row in result["accounting"]
        if row["client"] == f"acct-ndmp-{mode}"
        and row["fileset"] == f"AcctNdmp{mode.title()}"
    ]
    assert len(rows) == 1, (mode, phase, rows)
    row = rows[0]
    assert not row["excluded"], row
    assert row["rule"] == "st_blocks*512", row
    assert row["jobs_in_chain"] == phase + 1, row
    for key in ("files", "bytes", "logical_bytes"):
        assert row[key] == fixture["expected"][phase][key], (
            mode,
            phase,
            key,
            row,
            fixture["expected"][phase],
        )


if __name__ == "__main__":
    action, phase_name = sys.argv[1:3]
    phase = {"full": 0, "incremental": 1}[phase_name]
    for mode in ("bareos", "native"):
        fixture = json.loads(
            (
                pathlib.Path(__file__).parent / "fixtures" / f"ndmp-{mode}.json"
            ).read_text()
        )
        if action == "import":
            import_job(mode, fixture, phase)
        elif action == "check":
            check_report(mode, fixture, phase, pathlib.Path(sys.argv[3]))
        else:
            raise ValueError(f"unknown action: {action}")
