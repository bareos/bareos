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


FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "plugin-backups.json"


def sql_literal(value):
    if isinstance(value, int):
        return str(value)
    return " || chr(10) || ".join(
        "'" + line.replace("'", "''") + "'" for line in value.split("\n")
    )


def sql_command(query):
    print('.sql query="' + query.replace('"', '\\"') + '"')


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
                "JobTDate, JobFiles, JobBytes, FileSetId) "
                f"SELECT {sql_literal(name)}, {sql_literal(name)}, "
                f"{sql_literal(job['type'])}, {sql_literal(job['level'])}, "
                f"ClientId, {sql_literal(job['jobstatus'])}, "
                f"{sql_literal(job['jobtdate'])}, "
                f"{job.get('jobfiles', 0)}, {job.get('jobbytes', 0)}, "
                "FileSetId "
                f"FROM Client CROSS JOIN FileSet "
                f"WHERE Client.Name={sql_literal(client['name'])} "
                f"AND FileSet.FileSet={sql_literal(fileset['name'])}"
            )
            for path in sorted({row["path"] for row in job["files"]}):
                sql_command(
                    "INSERT INTO Path (Path) SELECT "
                    f"{sql_literal(path)} WHERE NOT EXISTS "
                    f"(SELECT 1 FROM Path WHERE Path={sql_literal(path)})"
                )
            for row in job["files"]:
                columns = (
                    "fileindex",
                    "name",
                    "lstat",
                    "md5",
                    "deltaseq",
                    "fhinfo",
                    "fhnode",
                )
                sql_command(
                    "INSERT INTO File (JobId, PathId, FileIndex, Name, LStat, "
                    "Md5, DeltaSeq, Fhinfo, Fhnode) "
                    "SELECT JobId, PathId, "
                    + ", ".join(sql_literal(row[key]) for key in columns)
                    + " FROM Job CROSS JOIN Path "
                    f"WHERE Job.Job={sql_literal(name)} "
                    f"AND Path.Path={sql_literal(row['path'])}"
                )


def result(report):
    text = pathlib.Path(report).read_text()
    return json.JSONDecoder().raw_decode(text[text.index("{") :])[0]["result"]


def check(report, phase):
    fixture = json.loads(FIXTURE.read_text())
    response = result(report)
    barri = next(
        row
        for row in response["accounting"]
        if row["client"] == fixture["barri"]["client"]["name"]
        and row["fileset"] == fixture["barri"]["fileset"]["name"]
    )
    assert barri["excluded"], barri
    assert (
        barri["exclusion_reason"] == fixture["barri"]["expected"]["exclusion_reason"]
    ), barri

    windows_fixture = fixture["windows"]
    windows = next(
        row
        for row in response["accounting"]
        if row["client"] == windows_fixture["client"]["name"]
        and row["fileset"] == windows_fixture["fileset"]["name"]
    )
    expected = windows_fixture["expected"][0 if phase == "full" else 1]
    assert not windows["excluded"], windows
    assert windows["rule"] == "st_size", windows
    assert windows["jobs_in_chain"] == (1 if phase == "full" else 2), windows
    for key, value in expected.items():
        assert windows[key] == value, (key, windows, expected)

    for fileset in fixture["mssql"]["filesets"]:
        mssql = next(
            row
            for row in response["accounting"]
            if row["client"] == fixture["mssql"]["client"]["name"]
            and row["fileset"] == fileset["name"]
        )
        assert mssql["excluded"], mssql
        assert mssql["exclusion_reason"] == "opaque_backup_image", mssql

    for fileset in fixture["vmware"]["filesets"]:
        vmware = next(
            row
            for row in response["accounting"]
            if row["client"] == fixture["vmware"]["client"]["name"]
            and row["fileset"] == fileset["name"]
        )
        assert vmware["excluded"], vmware
        assert vmware["exclusion_reason"] == "opaque_backup_image", vmware

    for fileset in fixture["hyperv"]["filesets"]:
        hyperv = next(
            row
            for row in response["accounting"]
            if row["client"] == fixture["hyperv"]["client"]["name"]
            and row["fileset"] == fileset["name"]
        )
        assert hyperv["excluded"], hyperv
        assert hyperv["exclusion_reason"] == "opaque_backup_image", hyperv


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
