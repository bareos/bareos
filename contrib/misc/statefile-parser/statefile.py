#!/usr/bin/env python3

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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

"""This module parses a Bareos filedaemon statefile.

Passing the raw data of the statefile to parse_statefile() will return a list
of JobResult objects. A code sample can be found is at the end of this file.

This module will probably not work for files generated with the windows version
of Bareos, as structs are packed differently.
"""

from glob import glob
from struct import unpack, calcsize
from dataclasses import dataclass
from datetime import datetime

HEADER_FMT = "".join(
    (
        "=",  # native byte-order with standard sizes
        "14s2x",  # magic
        "i4x",  # version
        "Q",  # last job offset
        "Q",  # end of list
    )
)
HEADER_LEN = calcsize(HEADER_FMT)

NUM_FMT = "=I"  # number of result records
NUM_FMT_LEN = calcsize(NUM_FMT)

RESULT_FMT = "".join(
    (
        "=",  # native byte-order with standard sizes
        "16x",  # initial padding
        "i",  # errors
        "i",  # job type
        "i",  # job status
        "i",  # job level
        "I",  # job id
        "I",  # volume session id
        "I",  # volume session time
        "I",  # job files
        "Q",  # job bytes
        "q",  # job start time
        "q",  # job end time
        "128s",  # job name
    )
)
RESULT_LEN = calcsize(RESULT_FMT)


class StatefileParseException(Exception):
    pass


@dataclass
class JobResult:  # pylint: disable=too-many-instance-attributes
    errors: int
    jobtype: str
    status: str
    level: str
    id: int
    vol_session_id: int
    vol_session_time: int
    files: int
    bytes: int
    start_time: int
    end_time: int
    name: str

    def __init__(self, data):
        (
            self.errors,
            jobtype,
            status,
            level,
            self.id,
            self.vol_session_id,
            self.vol_session_time,
            self.files,
            self.bytes,
            start_ts,
            end_ts,
            name,
        ) = data
        self.jobtype = chr(jobtype)
        self.status = chr(status)
        self.level = chr(level)
        self.start_time = datetime.fromtimestamp(start_ts).strftime(
            "%Y-%m-%d %H:%M:%S"
        )
        self.end_time = datetime.fromtimestamp(end_ts).strftime("%Y-%m-%d %H:%M:%S")
        self.name = name.rstrip(b"\x00").decode("ascii")


def parse_statefile(buffer):
    (magic, version, last_job_offset, _) = unpack(HEADER_FMT, buffer[:HEADER_LEN])
    if magic != b"Bareos State\n\x00":
        raise StatefileParseException("magic value does not match")
    if version != 4:
        raise StatefileParseException("only version 4 is supported")

    (num_recs,) = unpack(
        NUM_FMT, buffer[last_job_offset : last_job_offset + NUM_FMT_LEN]
    )

    results = []
    offset = last_job_offset + NUM_FMT_LEN
    for _ in range(0, num_recs):
        results.append(
            JobResult(unpack(RESULT_FMT, buffer[offset : offset + RESULT_LEN]))
        )
        offset += RESULT_LEN

    return results


def find_statefile():
    files = glob("/var/lib/bareos/bareos-fd.*.state")
    if len(files) == 1:
        return files[0]
    if len(files) == 0:
        return None
    raise LookupError("multiple statefiles found")


if __name__ == "__main__":
    path = find_statefile()
    if path is not None:
        with open(path, "rb") as fp:
            print(
                "   JobId  Level       Files     Bytes    Status  Finished             Name"
            )
            print("=" * 120)
            for r in parse_statefile(fp.read()):
                print(
                    "  ".join(
                        (
                            f"{r.id:8d}",
                            f"{r.level:5s}",
                            f"{r.files:10d}",
                            f"{r.bytes/1000000000:8.3f} G",
                            f"{r.status:6s}",
                            f"{r.end_time:19s}",
                            f"{r.name}",
                        )
                    )
                )
