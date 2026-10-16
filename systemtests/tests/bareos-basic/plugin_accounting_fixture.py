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

from accounting_fixture_common import (
    find_row,
    import_files,
    load_result,
    sql_command,
    sql_literal,
)

FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "plugin-backups.json"


def import_case(case_name, phase):
    fixture = json.loads(FIXTURE.read_text())[case_name]
    client = fixture["client"]
    filesets = fixture.get("filesets")
    if filesets is None:
        filesets = [dict(fixture["fileset"], jobs=fixture["jobs"])]
    if phase == "full":
        sql_command(
            "INSERT INTO Client (Name, Uname) VALUES "
            f"({sql_literal(client['name'])}, {sql_literal(client['uname'])})"
        )
        for fileset in filesets:
            sql_command(
                "INSERT INTO FileSet (FileSet, FileSetText, Md5, CreateTime) "
                "VALUES "
                f"({sql_literal(fileset['name'])}, "
                f"{sql_literal(fileset['text'])}, '0', CURRENT_TIMESTAMP)"
            )

    for fileset in filesets:
        jobs = fileset.get("jobs", fixture.get("jobs", []))
        jobs = jobs[:1] if phase == "full" else jobs[1:]
        for job in jobs:
            name = f"{client['name']}-{fileset['name']}-{job['level']}"
            sql_command(
                "INSERT INTO Job (Job, Name, Type, Level, ClientId, JobStatus, "
                "JobTDate, JobFiles, JobBytes, ReadBytes, FileSetId) "
                f"SELECT {sql_literal(name)}, {sql_literal(name)}, "
                f"{sql_literal(job['type'])}, {sql_literal(job['level'])}, "
                f"ClientId, {sql_literal(job['jobstatus'])}, "
                f"{sql_literal(job['jobtdate'])}, "
                f"{job.get('jobfiles', 0)}, {job.get('jobbytes', 0)}, "
                f"{job.get('readbytes', 0)}, "
                "FileSetId "
                f"FROM Client CROSS JOIN FileSet "
                f"WHERE Client.Name={sql_literal(client['name'])} "
                f"AND FileSet.FileSet={sql_literal(fileset['name'])}"
            )
            import_files(name, job["files"])


def check_job_estimate(row, section, fileset):
    """Opaque images are sized from the latest Full job's recorded bytes."""
    jobs = fileset.get("jobs", section.get("jobs", []))
    full = max(
        (job for job in jobs if job["level"] == "F"), key=lambda j: j["jobtdate"]
    )
    assert not row["excluded"], row
    assert row["estimated"] and row["rule"] == "job_bytes", row
    assert (
        row["bytes"]
        == row["logical_bytes"]
        == max(full["jobbytes"], full.get("readbytes", 0))
    ), (row, full)


def check(report, phase):
    fixture = json.loads(FIXTURE.read_text())
    response = load_result(report)
    barri_fixture = fixture["barri"]
    barri = find_row(
        response, barri_fixture["client"]["name"], barri_fixture["fileset"]["name"]
    )
    expected_barri = barri_fixture["expected"]
    assert not barri["excluded"], barri
    assert barri["estimated"] and barri["rule"] == expected_barri["rule"], barri
    assert barri["bytes"] == barri["logical_bytes"] == expected_barri["bytes"], barri

    windows_fixture = fixture["windows"]
    windows = find_row(
        response,
        windows_fixture["client"]["name"],
        windows_fixture["fileset"]["name"],
    )
    expected = windows_fixture["expected"][0 if phase == "full" else 1]
    assert not windows["excluded"], windows
    assert windows["rule"] == "st_size", windows
    assert windows["jobs_in_chain"] == (1 if phase == "full" else 2), windows
    for key, value in expected.items():
        assert windows[key] == value, (key, windows, expected)

    for case in ("mssql", "vmware", "hyperv"):
        section = fixture[case]
        for fileset in section["filesets"]:
            row = find_row(response, section["client"]["name"], fileset["name"])
            check_job_estimate(row, section, fileset)


if __name__ == "__main__":
    action = sys.argv[1]
    if action == "import":
        phase = sys.argv[2]
        import_case("barri", "full") if phase == "full" else None
        import_case("windows", phase)
        import_case("mssql", phase)
        if phase == "full":
            import_case("vmware", phase)
            import_case("hyperv", phase)
    elif action == "check":
        check(sys.argv[3], sys.argv[2])
    else:
        raise ValueError(f"unknown action: {action}")
