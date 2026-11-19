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

import sys

from accounting_fixture_common import (
    find_row,
    import_files,
    load_result,
    sql_command,
    sql_literal,
)

CLIENT = "acct-chain-edges"

# Literal LStat sizes and block counts below use A=0, B=1, C=2, etc.


def import_job(fileset, name, level, timestamp, files, kind="B", status="T", purged=0):
    name = f"acct-edge-{name}"
    sql_command(
        "INSERT INTO FileSet (FileSet, Md5, CreateTime) "
        f"SELECT {sql_literal(fileset)}, '0', CURRENT_TIMESTAMP WHERE NOT EXISTS "
        f"(SELECT 1 FROM FileSet WHERE FileSet={sql_literal(fileset)})"
    )
    sql_command(
        "INSERT INTO Job (Job, Name, ClientId, FileSetId, Type, Level, "
        "JobStatus, JobTDate, JobFiles, JobBytes, ReadBytes, PurgedFiles) "
        f"SELECT {sql_literal(name)}, {sql_literal(name)}, ClientId, FileSetId, "
        f"{sql_literal(kind)}, {sql_literal(level)}, {sql_literal(status)}, "
        f"{timestamp}, {len(files)}, 999, 1000, {purged} "
        f"FROM Client CROSS JOIN FileSet WHERE Client.Name={sql_literal(CLIENT)} "
        f"AND FileSet.FileSet={sql_literal(fileset)}"
    )
    import_files(
        name,
        [
            {
                "path": "/accounting-edges/",
                "name": filename,
                "fileindex": index,
                "lstat": f"A A IGk B A A A {size} BAA {blocks} A A A A A A",
                "md5": "",
                "deltaseq": 0,
                "fhinfo": 0,
                "fhnode": 0,
            }
            for index, (filename, size, blocks) in enumerate(files, 1)
        ],
    )


def import_fileset_version_job(version, name, level, timestamp, valid, size, blocks):
    fileset = "AcctVersioned"
    md5 = f"acct-version-{version}"
    lstat_digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    if name.endswith("full"):
        sql_command(
            "INSERT INTO FileSet (FileSet, Md5, CreateTime) VALUES "
            f"({sql_literal(fileset)}, {sql_literal(md5)}, CURRENT_TIMESTAMP)"
        )
    job_name = f"acct-version-{name}"
    sql_command(
        "INSERT INTO Job (Job, Name, ClientId, FileSetId, Type, Level, "
        "JobStatus, JobTDate, JobFiles, AccountingValid, AccountingFiles, "
        "AccountingBytes, AccountingLogicalBytes, AccountingRule, "
        "AccountingJobsInChain) "
        f"SELECT {sql_literal(job_name)}, {sql_literal(job_name)}, ClientId, "
        f"FileSetId, 'B', {sql_literal(level)}, 'T', {timestamp}, "
        f"{1 if level == 'F' else 0}, "
        f"{'TRUE' if valid else 'FALSE'}, {1 if level == 'F' else 0}, "
        f"{blocks * 512}, {size}, "
        "'st_blocks*512', 1 FROM Client CROSS JOIN FileSet "
        f"WHERE Client.Name={sql_literal(CLIENT)} "
        f"AND FileSet.Md5={sql_literal(md5)}"
    )
    if level == "F":
        import_files(
            job_name,
            [
                {
                    "path": "/accounting-versions/",
                    "name": "same-file",
                    "fileindex": 1,
                    "lstat": (
                        f"A A IGk B A A A {lstat_digits[size]} BAA "
                        f"{lstat_digits[blocks]} A A A A A A"
                    ),
                    "md5": "",
                    "deltaseq": 0,
                    "fhinfo": 0,
                    "fhnode": 0,
                }
            ],
        )


def import_fixture():
    sql_command(f"INSERT INTO Client (Name, Uname) VALUES ('{CLIENT}', 'Linux')")
    import_job("AcctEmpty", "empty-full", "F", 2000000000, [])
    import_job(
        "AcctDifferential",
        "full",
        "F",
        2000000000,
        [("changed", "B", "B"), ("retained", "F", "B")],
    )
    import_job(
        "AcctDifferential",
        "old-inc",
        "I",
        2000000001,
        [("changed", "C", "C"), ("before-diff-only", "H", "H")],
    )
    import_job("AcctDifferential", "diff", "D", 2000000002, [("changed", "D", "D")])
    import_job(
        "AcctDifferential",
        "new-inc",
        "I",
        2000000002,
        [("changed", "E", "E")],
        status="W",
    )
    import_job(
        "AcctDifferential",
        "failed-full",
        "F",
        2000000003,
        [("wrong", "H", "H")],
        status="E",
    )
    import_job(
        "AcctDifferential",
        "migrated-full",
        "F",
        2000000004,
        [("wrong", "H", "H")],
        kind="M",
    )
    for kind in ("B", "C", "g", "O"):
        import_job(
            f"AcctType{kind}",
            f"type-{kind}",
            "F",
            2000000000,
            [("regular", "B", "B")],
            kind=kind,
        )
    for level in ("F", "D", "I"):
        fileset = f"AcctPurged{level}"
        import_job(
            fileset,
            f"purged-{level}-full",
            "F",
            2000000000,
            [("regular", "B", "B")],
            purged=int(level == "F"),
        )
        if level != "F":
            import_job(fileset, f"purged-{level}", level, 2000000001, [], purged=1)
    import_job(
        "AcctWithoutFull", "without-full", "I", 2000000000, [("regular", "B", "B")]
    )
    import_job("AcctFullTie", "tie-first", "F", 2000000000, [("regular", "B", "B")])
    import_job("AcctFullTie", "tie-second", "F", 2000000000, [("regular", "C", "C")])
    import_fileset_version_job("old", "old-full", "F", 2000000100, True, 1, 1)
    import_fileset_version_job("old", "old-inc", "I", 2000000101, False, 0, 0)
    import_fileset_version_job("new", "new-full", "F", 2000000102, True, 2, 3)


def check_fixture(report):
    result = load_result(report)
    row = find_row(result, CLIENT, "AcctEmpty")
    assert not row["excluded"] and row["rule_code"] == "A", row
    assert (row["files"], row["bytes"], row["logical_bytes"]) == (0, 0, 0), row
    row = find_row(result, CLIENT, "AcctDifferential")
    assert not row["excluded"] and row["rule_code"] == "A", row
    assert (row["files"], row["bytes"], row["logical_bytes"], row["jobs_in_chain"]) == (
        2,
        2560,
        9,
        3,
    ), row
    for kind in ("B", "C"):
        row = find_row(result, CLIENT, f"AcctType{kind}")
        assert not row["excluded"], row
        assert (row["files"], row["bytes"], row["jobs_in_chain"]) == (1, 512, 1), row
    assert not any(
        row["fileset"] in ("AcctTypeg", "AcctTypeO") for row in result["accounting"]
    ), result
    for level in ("F", "D", "I"):
        row = find_row(result, CLIENT, f"AcctPurged{level}")
        assert row["excluded"] and row["exclusion_reason"] == "no_usable_chain", row
        assert "no usable backup chain" in row["accounting_issue"], row
    assert not any(
        row["fileset"] == "AcctWithoutFull" for row in result["accounting"]
    ), result
    row = find_row(result, CLIENT, "AcctFullTie")
    assert (row["files"], row["bytes"], row["logical_bytes"]) == (1, 1024, 2), row
    versioned_rows = [
        row
        for row in result["accounting"]
        if row["client"] == CLIENT and row["fileset"] == "AcctVersioned"
    ]
    assert len(versioned_rows) == 1, versioned_rows
    assert (
        versioned_rows[0]["bytes"],
        versioned_rows[0]["logical_bytes"],
    ) == (1536, 2), versioned_rows
    assert result["summary"]["incomplete"], result["summary"]


def check_versioned_queue(report):
    result = load_result(report)
    assert result["accounting_snapshot"]["pending_jobs"] == 0, result


if __name__ == "__main__":
    if sys.argv[1] == "import":
        import_fixture()
    elif sys.argv[1] == "check":
        check_fixture(sys.argv[2])
    elif sys.argv[1] == "check-versioned-queue":
        check_versioned_queue(sys.argv[2])
    else:
        raise ValueError(f"unknown action: {sys.argv[1]}")
