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

FILE_COLUMNS = ("fileindex", "name", "lstat", "md5", "deltaseq", "fhinfo", "fhnode")


def sql_literal(value):
    if isinstance(value, int):
        return str(value)
    return " || chr(10) || ".join(
        "'" + line.replace("'", "''") + "'" for line in value.split("\n")
    )


def sql_command(query):
    print('.sql query="' + query.replace('"', '\\"') + '"')


def import_files(job_name, files):
    """Emit Path and File inserts for the rows of an already imported Job."""
    for path in sorted({row["path"] for row in files}):
        sql_command(
            f"INSERT INTO Path (Path) SELECT {sql_literal(path)} WHERE NOT EXISTS "
            f"(SELECT 1 FROM Path WHERE Path={sql_literal(path)})"
        )
    for row in files:
        sql_command(
            f"INSERT INTO File (JobId, PathId, {','.join(FILE_COLUMNS)}) "
            "SELECT JobId, PathId, "
            f"{','.join(sql_literal(row[column]) for column in FILE_COLUMNS)} "
            f"FROM Job CROSS JOIN Path WHERE Job.Job={sql_literal(job_name)} "
            f"AND Path.Path={sql_literal(row['path'])}"
        )


def load_result(report):
    text = pathlib.Path(report).read_text()
    return json.JSONDecoder().raw_decode(text[text.index("{") :])[0]["result"]


def find_row(result, client, fileset):
    return next(
        row
        for row in result["accounting"]
        if row["client"] == client and row["fileset"] == fileset
    )


def find_acct_test_row(result):
    return next(row for row in result["accounting"] if row["fileset"] == "AcctTest")
