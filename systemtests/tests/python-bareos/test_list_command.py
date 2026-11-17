#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2026 Bareos GmbH & Co. KG
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

# -*- coding: utf-8 -*-

from __future__ import print_function
from concurrent.futures import ThreadPoolExecutor
import json
import logging
import os
import re
import subprocess
from time import monotonic, sleep
import unittest
from unittest.mock import Mock
import uuid
import warnings

import bareos.bsock
from bareos.bsock.constants import Constants
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.bsock.lowlevel import LowLevel
import bareos.exceptions

import bareos_unittest


class PythonBareosListCommandTest(bareos_unittest.Json):
    @staticmethod
    def delete_temporary_volume(admin, volume):
        admin.call(f"delete volume={volume} yes")
        admin.call(
            '.sql query="UPDATE Pool SET NumVols=(SELECT count(*) FROM Media '
            "WHERE Media.PoolId=Pool.PoolId) WHERE Name IN ('Full','Incremental')\""
        )
        os.remove(f"storage/{volume}")

    def cleanup_cleaning_race(self, admin, volume, mediaid, jobid):
        if mediaid is not None:
            admin.call(
                f'.sql query="DELETE FROM JobMedia WHERE MediaId={mediaid} '
                f'AND JobId={jobid}"'
            )
        self.delete_temporary_volume(admin, volume)

    def test_cleaning_race_cleanup_preserves_volume_if_unlink_fails(self):
        admin = Mock()
        admin.call.side_effect = RuntimeError("unlink failed")

        with self.assertRaisesRegex(RuntimeError, "unlink failed"):
            self.cleanup_cleaning_race(admin, "test-volume", 42, 43)
        admin.call.assert_called_once_with(
            '.sql query="DELETE FROM JobMedia WHERE MediaId=42 AND JobId=43"'
        )

    def test_frompool_reports_missing_pool_as_failure(self):
        username = self.get_operator_username()
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        volume = f"MissingPoolDefaults-{uuid.uuid4().hex}"
        director.call(f"label volume={volume} storage=File pool=Full")
        try:
            poolid = director.call(f"llist volume={volume}")["volume"]["poolid"]
            director.call(
                f'.sql query="UPDATE Media SET PoolId=2147483647 '
                f"WHERE VolumeName='{volume}'\""
            )
            try:
                signals = []
                original_is_end = director.is_end_of_message

                def capture_signal(signal):
                    signals.append(signal)
                    return original_is_end(signal)

                director.is_end_of_message = capture_signal
                with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
                    director.call(f"update volume={volume} frompool=yes")
                self.assertIn(Constants.BNET_CMD_FAILED, signals)
            finally:
                director.is_end_of_message = original_is_end
                director.call(
                    f'.sql query="UPDATE Media SET PoolId={poolid} '
                    f"WHERE VolumeName='{volume}'\""
                )
        finally:
            self.delete_temporary_volume(director, volume)

    def test_pool_defaults_rejects_combined_comment_without_changes(self):
        username = self.get_operator_username()
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        volume = f"PoolComment-{uuid.uuid4().hex}"
        director.call(f"label volume={volume} storage=File pool=Full")
        try:
            director.call(f'update volume={volume} volretention=1 comment="original"')
            for pool_option in ("frompool=yes", "allfrompool=Full"):
                with self.subTest(pool_option=pool_option):
                    with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
                        director.call(
                            f'update volume={volume} volretention=2 '
                            f'{pool_option} comment="changed"'
                        )
                    media = director.call(f"llist volume={volume}")["volume"]
                    self.assertEqual(media["comment"], "original")
                    self.assertEqual(media["volretention"], "1")
            director.call(f"update volume={volume} frompool=yes")
            media = director.call(f"llist volume={volume}")["volume"]
            self.assertEqual(media["comment"], "original")
            self.assertEqual(
                media["volretention"],
                director.call("llist pool=Full")["pools"][0]["volretention"],
            )
        finally:
            self.delete_temporary_volume(director, volume)

    def test_move_volume_with_pool_defaults(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        director = bareos.bsock.DirectorConsoleJson(**options)
        console = bareos.bsock.DirectorConsole(**options)
        volume = f"AtomicPoolMove-{uuid.uuid4().hex}"
        director.call(f"label volume={volume} storage=File pool=Full")
        try:
            source = director.call("llist pool=Full")["pools"][0]
            target = director.call("llist pool=Incremental")["pools"][0]
            director.call(f"update volume={volume} volretention=1")
            prompt = console.call(
                f"update volume={volume} poolwithdefaultsunsupported=Incremental"
            )
            self.assertIn(b"Parameters to modify", prompt)
            console.call(".")
            self.assertEqual(
                director.call(f"llist volume={volume}")["volume"]["poolid"],
                source["poolid"],
            )
            with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
                director.call(
                    f"update volume={volume} pool=Incremental "
                    "poolwithdefaults=Incremental"
                )
            self.assertEqual(
                director.call(f"llist volume={volume}")["volume"]["poolid"],
                source["poolid"],
            )
            response = director.call(
                f"update volume={volume} poolwithdefaults=Incremental"
            )
            self.assertEqual(
                response["poolmove"], {"volumename": volume, "pool": "Incremental"}
            )
            media = director.call(f"llist volume={volume}")["volume"]
            self.assertEqual(media["poolid"], target["poolid"])
            for field in (
                "volretention",
                "voluseduration",
                "maxvoljobs",
                "maxvolfiles",
                "maxvolbytes",
                "recycle",
                "recyclepoolid",
                "actiononpurge",
                "minblocksize",
                "maxblocksize",
            ):
                with self.subTest(field=field):
                    self.assertEqual(media[field], target[field])
            self.assertEqual(
                int(director.call("llist pool=Full")["pools"][0]["numvols"]),
                int(source["numvols"]) - 1,
            )
            self.assertEqual(
                int(director.call("llist pool=Incremental")["pools"][0]["numvols"]),
                int(target["numvols"]) + 1,
            )

            director.call(f"update volume={volume} volretention=1")
            director.call(f"update volume={volume} poolwithdefaults=Incremental")
            self.assertEqual(
                director.call(f"llist volume={volume}")["volume"]["volretention"],
                target["volretention"],
            )
            self.assertEqual(
                int(director.call("llist pool=Incremental")["pools"][0]["numvols"]),
                int(target["numvols"]) + 1,
            )
        finally:
            if console.socket is not None:
                console.close()
            self.delete_temporary_volume(director, volume)

    def test_pool_move_defaults_survives_stale_enabled_prompt(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        admin = bareos.bsock.DirectorConsoleJson(**options)
        console = bareos.bsock.DirectorConsole(**options)
        volume = f"PoolMoveEnabled-{uuid.uuid4().hex}"
        admin.call(f"label volume={volume} storage=File pool=Full")
        try:
            admin.call(f"update volume={volume} volretention=1")
            source = admin.call("llist pool=Full")["pools"][0]
            target = admin.call("llist pool=Incremental")["pools"][0]
            console.call(f"update volume={volume}")
            self.assertIn(b"Enter new Enabled", console.call("15"))
            admin.call(f"update volume={volume} poolwithdefaults=Incremental")
            result = console.call("no").decode("utf-8")
            self.assertIn("pool changed", result.lower())
            media = admin.call(f"llist volume={volume}")["volume"]
            self.assertEqual(media["poolid"], target["poolid"])
            self.assertEqual(media["volretention"], target["volretention"])
            self.assertEqual(media["enabled"], "1")
            self.assertEqual(
                int(admin.call("llist pool=Full")["pools"][0]["numvols"]),
                int(source["numvols"]) - 1,
            )
            self.assertEqual(
                int(admin.call("llist pool=Incremental")["pools"][0]["numvols"]),
                int(target["numvols"]) + 1,
            )
            admin.call(f"update volume={volume} enabled=no")
            self.assertEqual(
                admin.call(f"llist volume={volume}")["volume"]["enabled"], "0"
            )
        finally:
            console.call(".")
            self.delete_temporary_volume(admin, volume)

    def test_pool_move_defaults_rolls_back_on_counter_failure(self):
        username = self.get_operator_username()
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        volume = f"AtomicPoolFailure-{uuid.uuid4().hex}"
        director.call(f"label volume={volume} storage=File pool=Full")
        try:
            director.call(f"update volume={volume} volretention=1")
            before = director.call(f"llist volume={volume}")["volume"]
            director.call(
                '.sql query="UPDATE Pool SET NumVols=(SELECT count(*) FROM Media '
                "WHERE Media.PoolId=Pool.PoolId) WHERE Name IN ('Full','Incremental')\""
            )
            source = director.call("llist pool=Full")["pools"][0]
            target = director.call("llist pool=Incremental")["pools"][0]
            options = dict(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=self.get_operator_password(username),
                **self.director_extra_options,
            )
            mover = bareos.bsock.DirectorConsoleJson(**options)
            observer = bareos.bsock.DirectorConsoleJson(**options)
            director.call('.sql query="BEGIN"')
            transaction_open = True
            try:
                director.call(
                    f'.sql query="UPDATE Pool SET NumVols=2147483647 '
                    f'WHERE PoolId={target["poolid"]}"'
                )
                with ThreadPoolExecutor(max_workers=1) as executor:
                    pending = executor.submit(
                        mover.call,
                        f"update volume={volume} poolwithdefaults=Incremental",
                    )
                    try:
                        deadline = monotonic() + 10
                        while monotonic() < deadline:
                            waiting = observer.call(
                                '.sql query="SELECT count(*) AS waiting FROM '
                                "pg_stat_activity WHERE wait_event_type='Lock' "
                                "AND query LIKE '%WITH pools AS%'\""
                            )["query"][0]["waiting"]
                            if int(waiting):
                                break
                            sleep(0.1)
                        else:
                            mover.close()
                            self.fail("Pool move did not wait for the locked pool")
                    finally:
                        director.call('.sql query="COMMIT"')
                        transaction_open = False
                    with self.assertRaises(
                        bareos.exceptions.JsonRpcErrorReceivedException
                    ):
                        self.wait_for_console_call(pending, mover)
                after = director.call(f"llist volume={volume}")["volume"]
                self.assertEqual(after["poolid"], before["poolid"])
                self.assertEqual(after["volretention"], before["volretention"])
                self.assertEqual(
                    director.call("llist pool=Full")["pools"][0]["numvols"],
                    source["numvols"],
                )
                self.assertEqual(
                    director.call(
                        f'.sql query="SELECT NumVols FROM Pool '
                        f'WHERE PoolId={target["poolid"]}"'
                    )["query"][0]["numvols"],
                    "2147483647",
                )
            finally:
                if transaction_open:
                    director.call('.sql query="ROLLBACK"')
                director.call(
                    f'.sql query="UPDATE Pool SET NumVols={target["numvols"]} '
                    f'WHERE PoolId={target["poolid"]}"'
                )
        finally:
            self.delete_temporary_volume(director, volume)

    def test_pool_move_defaults_requires_both_pool_acls(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        restricted = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name="poolfull",
            password=bareos.bsock.Password("secret"),
            **self.director_extra_options,
        )
        volume = f"AtomicPoolAcl-{uuid.uuid4().hex}"
        admin.call(f"label volume={volume} storage=File pool=Full")
        try:
            before = admin.call(f"llist volume={volume}")["volume"]
            result = restricted.call(
                f"update volume={volume} poolwithdefaults=Incremental"
            )
            self.assertIn(b"Could not access target Pool", result)
            self.assertEqual(
                admin.call(f"llist volume={volume}")["volume"]["poolid"],
                before["poolid"],
            )
            admin.call(f"update volume={volume} pool=Incremental")
            before = admin.call(f"llist volume={volume}")["volume"]
            result = restricted.call(f"update volume={volume} poolwithdefaults=Full")
            self.assertIn(b"Could not access current Pool", result)
            self.assertEqual(
                admin.call(f"llist volume={volume}")["volume"]["poolid"],
                before["poolid"],
            )
        finally:
            self.delete_temporary_volume(admin, volume)

    def test_pool_move_defaults_rejects_stale_source(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        admin = bareos.bsock.DirectorConsoleJson(**options)
        mover = bareos.bsock.DirectorConsoleJson(**options)
        observer = bareos.bsock.DirectorConsoleJson(**options)
        volume = f"AtomicPoolStale-{uuid.uuid4().hex}"
        admin.call(f"label volume={volume} storage=File pool=Full")
        try:
            admin.call(f"update volume={volume} volretention=1")
            source = admin.call(f"llist volume={volume}")["volume"]
            target = admin.call("llist pool=Incremental")["pools"][0]
            admin.call(
                '.sql query="UPDATE Pool SET NumVols=(SELECT count(*) FROM Media '
                "WHERE Media.PoolId=Pool.PoolId) WHERE Name IN ('Full','Incremental')\""
            )
            admin.call('.sql query="BEGIN"')
            transaction_open = True
            try:
                admin.call(
                    f'.sql query="UPDATE Media SET PoolId={target["poolid"]} '
                    f"WHERE VolumeName='{volume}'\""
                )
                with ThreadPoolExecutor(max_workers=1) as executor:
                    pending = executor.submit(
                        mover.call,
                        f"update volume={volume} poolwithdefaults=Incremental",
                    )
                    try:
                        deadline = monotonic() + 10
                        while monotonic() < deadline:
                            waiting = observer.call(
                                '.sql query="SELECT count(*) AS waiting FROM '
                                "pg_stat_activity WHERE wait_event_type='Lock' "
                                "AND query LIKE '%WITH pools AS%'\""
                            )["query"][0]["waiting"]
                            if int(waiting):
                                break
                            sleep(0.1)
                        else:
                            mover.close()
                            self.fail("Pool move did not wait for the locked volume")
                    finally:
                        admin.call('.sql query="COMMIT"')
                        transaction_open = False
                    with self.assertRaises(
                        bareos.exceptions.JsonRpcErrorReceivedException
                    ):
                        self.wait_for_console_call(pending, mover)
                current = admin.call(f"llist volume={volume}")["volume"]
                self.assertEqual(current["poolid"], target["poolid"])
                self.assertEqual(current["volretention"], "1")
            finally:
                if transaction_open:
                    admin.call('.sql query="ROLLBACK"')
                admin.call(
                    f'.sql query="UPDATE Media SET PoolId={source["poolid"]} '
                    f"WHERE VolumeName='{volume}'\""
                )
        finally:
            self.delete_temporary_volume(admin, volume)

    def test_pool_move_defaults_concurrent_counters(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        admin = bareos.bsock.DirectorConsoleJson(**options)
        volumes = [f"AtomicConcurrent-{uuid.uuid4().hex}" for _ in range(2)]
        created = []
        try:
            for volume in volumes:
                admin.call(f"label volume={volume} storage=File pool=Full")
                created.append(volume)
            mediaids = [
                int(admin.call(f"llist volume={volume}")["volume"]["mediaid"])
                for volume in volumes
            ]
            admin.call(
                '.sql query="UPDATE Pool SET NumVols=(SELECT count(*) FROM Media '
                "WHERE Media.PoolId=Pool.PoolId) WHERE Name IN ('Full','Incremental')\""
            )
            consoles = [bareos.bsock.DirectorConsoleJson(**options) for _ in volumes]
            observer = bareos.bsock.DirectorConsoleJson(**options)
            admin.call('.sql query="BEGIN"')
            transaction_open = True
            try:
                admin.call(
                    '.sql query="SELECT PoolId FROM Pool '
                    "WHERE Name='Incremental' FOR UPDATE\""
                )
                with ThreadPoolExecutor(max_workers=2) as executor:
                    pending = [
                        executor.submit(
                            console.call,
                            f"update volume={volume} poolwithdefaults=Incremental",
                        )
                        for console, volume in zip(consoles, volumes)
                    ]
                    try:
                        deadline = monotonic() + 10
                        while monotonic() < deadline:
                            waiting = observer.call(
                                '.sql query="SELECT count(DISTINCT CASE '
                                f"WHEN query LIKE '%WHERE Media.MediaId={mediaids[0]} AND%' THEN 1 "
                                f"WHEN query LIKE '%WHERE Media.MediaId={mediaids[1]} AND%' THEN 2 "
                                "END) AS waiting FROM pg_stat_activity "
                                "WHERE wait_event_type='Lock' "
                                "AND query LIKE '%WITH pools AS%'\""
                            )["query"][0]["waiting"]
                            if int(waiting) == len(volumes):
                                break
                            sleep(0.1)
                        else:
                            for console in consoles:
                                console.close()
                            self.fail(
                                "Pool moves did not overlap while the pool was locked"
                            )
                    finally:
                        admin.call('.sql query="COMMIT"')
                        transaction_open = False
                    results = self.wait_for_console_calls(pending, consoles)
                    for result, volume in zip(results, volumes):
                        self.assertEqual(
                            result["poolmove"]["volumename"], volume
                        )
            finally:
                if transaction_open:
                    admin.call('.sql query="ROLLBACK"')
            counts = admin.call(
                '.sql query="SELECT Name,NumVols,'
                "(SELECT count(*) FROM Media WHERE Media.PoolId=Pool.PoolId) "
                "AS actual FROM Pool WHERE Name IN ('Full','Incremental')\""
            )["query"]
            self.assertEqual(len(counts), 2)
            for pool in counts:
                self.assertEqual(pool["numvols"], pool["actual"], pool["name"])
        finally:
            for volume in created:
                self.delete_temporary_volume(admin, volume)
            counts = admin.call(
                '.sql query="SELECT Name,NumVols,'
                "(SELECT count(*) FROM Media WHERE Media.PoolId=Pool.PoolId) "
                "AS actual FROM Pool WHERE Name IN ('Full','Incremental')\""
            )["query"]
            for pool in counts:
                self.assertEqual(pool["numvols"], pool["actual"], pool["name"])

    def test_volume_status_rejects_stale_interactive_selection(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        admin = bareos.bsock.DirectorConsoleJson(**options)
        console = bareos.bsock.DirectorConsole(**options)
        volume = f"StatusPromptRace-{uuid.uuid4().hex}"
        admin.call(f"label volume={volume} storage=File pool=Full")
        try:
            console.call(f"update volume={volume}")
            choices = console.call("1").decode("utf-8")
            full = re.search(r"^\s*(\d+): Full\s*$", choices, re.MULTILINE)
            self.assertIsNotNone(full, choices)
            admin.call(f"update volume={volume} volstatus=Used")
            result = console.call(full.group(1)).decode("utf-8")
            self.assertIn("status changed", result.lower())
            record = admin.call(f"llist volume={volume}")["volume"]
            self.assertEqual(record["volstatus"], "Used")
        finally:
            console.call(".")
            self.delete_temporary_volume(admin, volume)

    def test_volume_status_rejects_pool_move_after_interactive_selection(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        admin = bareos.bsock.DirectorConsoleJson(**options)
        console = bareos.bsock.DirectorConsole(**options)
        volume = f"StatusPoolRace-{uuid.uuid4().hex}"
        admin.call(f"label volume={volume} storage=File pool=Full")
        try:
            console.call(f"update volume={volume}")
            choices = console.call("1").decode("utf-8")
            full = re.search(r"^\s*(\d+): Full\s*$", choices, re.MULTILINE)
            self.assertIsNotNone(full, choices)
            admin.call(f"update volume={volume} pool=Incremental")
            result = console.call(full.group(1)).decode("utf-8")
            self.assertIn("status changed", result.lower())
            record = admin.call(f"llist volume={volume}")["volume"]
            self.assertEqual(record["pool"], "Incremental")
            self.assertEqual(record["volstatus"], "Append")
        finally:
            console.call(".")
            self.delete_temporary_volume(admin, volume)

    def test_cleaning_rejects_concurrent_jobmedia(self):
        username = self.get_operator_username()
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        admin = bareos.bsock.DirectorConsoleJson(**options)
        changer = bareos.bsock.DirectorConsoleJson(**options)
        observer = bareos.bsock.DirectorConsoleJson(**options)
        jobid = self.get_backup_jobid(admin, "backup-bareos-fd", level="Full")
        volume = f"CleaningRace-{uuid.uuid4().hex}"
        admin.call(f"label volume={volume} storage=File pool=Full")
        mediaid = None
        try:
            record = admin.call(f"llist volume={volume}")["volume"]
            mediaid = record["mediaid"]
            admin.call('.sql query="BEGIN"')
            transaction_open = True
            try:
                admin.call(
                    f'.sql query="SELECT MediaId FROM Media '
                    f'WHERE MediaId={mediaid} FOR UPDATE"'
                )
                with ThreadPoolExecutor(max_workers=1) as executor:
                    pending = executor.submit(
                        changer.call, f"update volume={volume} volstatus=Cleaning"
                    )
                    try:
                        deadline = monotonic() + 10
                        while monotonic() < deadline:
                            waiting = observer.call(
                                '.sql query="SELECT count(*) AS waiting FROM '
                                "pg_stat_activity WHERE wait_event_type='Lock' "
                                "AND query LIKE '%UPDATE Media SET VolStatus=%'\""
                            )["query"][0]["waiting"]
                            if int(waiting):
                                break
                            sleep(0.1)
                        else:
                            changer.close()
                            self.fail(
                                "Status change did not wait for the locked volume"
                            )
                        admin.call(
                            f'.sql query="INSERT INTO JobMedia (JobId,MediaId) '
                            f'VALUES ({jobid},{mediaid})"'
                        )
                    finally:
                        admin.call('.sql query="COMMIT"')
                        transaction_open = False
                    with self.assertRaises(
                        bareos.exceptions.JsonRpcErrorReceivedException
                    ):
                        self.wait_for_console_call(pending, changer)
                self.assertEqual(
                    admin.call(f"llist volume={volume}")["volume"]["volstatus"],
                    record["volstatus"],
                )
                changer.call(f"update volume={volume} volstatus=Cleaning force")
                self.assertEqual(
                    admin.call(f"llist volume={volume}")["volume"]["volstatus"],
                    "Cleaning",
                )
            finally:
                if transaction_open:
                    admin.call('.sql query="ROLLBACK"')
        finally:
            self.cleanup_cleaning_race(admin, volume, mediaid, jobid)

    def test_job_comment_update_is_atomic(self):
        username = self.get_operator_username()
        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        self.addCleanup(director.close)
        jobid = self.run_job(director, "backup-bareos-fd", "Full")
        self.addCleanup(director.call, f"delete jobid={jobid} yes")
        self.wait_job(director, jobid)
        director.call(f'update jobid={jobid} comment="original"')

        for invalid in (
            "jobtype=5",
            "starttime=invalid",
            "client=missing-client",
            "filesetid=invalid",
            "filesetid=12x",
            "filesetid=4294967296",
        ):
            with self.subTest(invalid=invalid):
                with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
                    director.call(f'update jobid={jobid} comment="changed" {invalid}')
                job = director.call(f"llist jobid={jobid}")["jobs"][0]
                self.assertEqual(job["comment"], "original")

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            director.call(
                f'update jobid={jobid} comment="changed" filesetid=2147483648'
            )
        job = director.call(f"llist jobid={jobid}")["jobs"][0]
        self.assertEqual(job["comment"], "original")
        self.assertEqual(job["name"], "backup-bareos-fd")

        director.call(
            f'update jobid={jobid} comment="updated" jobname=atomic-good-name'
        )
        job = director.call(f"llist jobid={jobid}")["jobs"][0]
        self.assertEqual(job["comment"], "updated")
        self.assertEqual(job["name"], "atomic-good-name")

    def test_job_counts_respect_list_acls(self):
        username = self.get_operator_username()
        admin = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=self.get_operator_password(username),
            **self.director_extra_options,
        )
        jobid = self.run_job(admin, "backup-bareos-fd", "Full", wait=True)
        other_jobid = self.run_job(admin, "backup-test2-fd", "Full", wait=True)
        self.configure_add(
            admin,
            "consoles",
            "count-regex-deny",
            "console=count-regex-deny password=secret profile=operator "
            'jobacl="!backup-test2-fd,backup-.*,historical-.*" '
            "tlsenable=no tlsrequire=no",
        )
        self.configure_add(
            admin,
            "consoles",
            "count-no-fileset",
            "console=count-no-fileset password=secret profile=operator "
            "filesetacl=!SelfTest tlsenable=no tlsrequire=no",
        )

        for username in (
            "client-bareos-fd",
            "job-backup-bareos-fd",
            "poolnotfull",
            "count-regex-deny",
            "count-no-fileset",
        ):
            console = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name=username,
                password=bareos.bsock.Password("secret"),
                **self.director_extra_options,
            )
            if username == "count-regex-deny":
                names = {row["name"] for row in console.call("list jobs")["jobs"]}
                self.assertIn("backup-bareos-fd", names)
                self.assertNotIn("backup-test2-fd", names)
            if username == "count-no-fileset":
                short_ids = {row["jobid"] for row in console.call("list jobs")["jobs"]}
                long_ids = {row["jobid"] for row in console.call("llist jobs")["jobs"]}
                self.assertIn(str(jobid), short_ids)
                self.assertNotIn(str(jobid), long_ids)
            for command in ("list", "llist"):
                for suffix in (
                    "",
                    " last",
                    " current",
                    " last current",
                    " search=backup",
                ):
                    with self.subTest(console=username, command=command, suffix=suffix):
                        rows = console.call(f"{command} jobs{suffix}")["jobs"]
                        count = console.call(f"{command} jobs count{suffix}")["jobs"][0]
                        self.assertEqual(int(count["count"]), len(rows))
                        paged_count = console.call(
                            f"{command} jobs count{suffix} limit=0 offset=1"
                        )["jobs"][0]
                        self.assertEqual(int(paged_count["count"]), len(rows))
                        for offset in range(0, min(len(rows), 4)):
                            page = console.call(
                                f"{command} jobs{suffix} limit=2 offset={offset}"
                            )["jobs"]
                            self.assertEqual(
                                [row["jobid"] for row in page],
                                [row["jobid"] for row in rows[offset : offset + 2]],
                            )

        admin.call(
            f".sql query=\"UPDATE Job SET Name='historical-count' WHERE JobId={jobid}\""
        )
        try:
            console = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name="count-regex-deny",
                password=bareos.bsock.Password("secret"),
                **self.director_extra_options,
            )
            for command in ("list", "llist"):
                for suffix in ("", " last", " search=historical"):
                    rows = console.call(f"{command} jobs{suffix}")["jobs"]
                    self.assertIn("historical-count", {row["name"] for row in rows})
                    count = console.call(f"{command} jobs count{suffix}")["jobs"][0]
                    self.assertEqual(int(count["count"]), len(rows))
        finally:
            admin.call(
                f".sql query=\"UPDATE Job SET Name='backup-bareos-fd' WHERE JobId={jobid}\""
            )

        try:
            for id_ in (jobid, other_jobid):
                admin.call(
                    f".sql query=\"UPDATE Job SET Name='count-latest-selection' WHERE JobId={id_}\""
                )
            console = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name="client-bareos-fd",
                password=bareos.bsock.Password("secret"),
                **self.director_extra_options,
            )
            for command in ("list", "llist"):
                self.assertEqual(
                    int(
                        console.call(
                            f"{command} jobs count search=count-latest-selection"
                        )["jobs"][0]["count"]
                    ),
                    1,
                )
                self.assertEqual(
                    int(
                        console.call(
                            f"{command} jobs count last search=count-latest-selection"
                        )["jobs"][0]["count"]
                    ),
                    0,
                )
        finally:
            try:
                admin.call(
                    f".sql query=\"UPDATE Job SET Name='backup-bareos-fd' WHERE JobId={jobid}\""
                )
            finally:
                admin.call(
                    f".sql query=\"UPDATE Job SET Name='backup-test2-fd' WHERE JobId={other_jobid}\""
                )

        try:
            admin.call(
                '.sql query="INSERT INTO Job (Job, Name, Type, Level, JobStatus, ClientId) '
                "SELECT 'acl-count-cursor-' || n, 'acl-count-cursor', 'B', 'F', 'T', "
                f"CASE WHEN n <= 120 THEN (SELECT ClientId FROM Job WHERE JobId={jobid}) "
                f"ELSE (SELECT ClientId FROM Job WHERE JobId={other_jobid}) END "
                'FROM generate_series(1, 340) n"'
            )
            console = bareos.bsock.DirectorConsoleJson(
                address=self.director_address,
                port=self.director_port,
                name="client-bareos-fd",
                password=bareos.bsock.Password("secret"),
                **self.director_extra_options,
            )
            for command in ("list", "llist"):
                count = console.call(f"{command} jobs count search=acl-count-cursor")
                self.assertEqual(int(count["jobs"][0]["count"]), 120)
                rows = console.call(
                    f"{command} jobs search=acl-count-cursor sortby=jobid reverse"
                )["jobs"]
                for offset in (0, 1, 99, 100, 119, 120):
                    page = console.call(
                        f"{command} jobs search=acl-count-cursor sortby=jobid "
                        f"reverse limit=2 offset={offset}"
                    )["jobs"]
                    self.assertEqual(
                        [row["jobid"] for row in page],
                        [row["jobid"] for row in rows[offset : offset + 2]],
                    )
        finally:
            admin.call(".sql query=\"DELETE FROM Job WHERE Name='acl-count-cursor'\"")

    def test_list_jobs_during_reload(self):
        username = self.get_operator_username()
        password = self.get_operator_password(username)
        options = dict(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )
        director = bareos.bsock.DirectorConsoleJson(**options)
        reloader = bareos.bsock.DirectorConsoleJson(**options)
        self.get_backup_jobid(director, "backup-bareos-fd", level="Full")
        commands = (
            "list jobs count current",
            "list jobs count enabled",
            "list jobs count disabled",
        )
        expected = [director.call(command)["jobs"][0]["count"] for command in commands]

        with ThreadPoolExecutor(max_workers=1) as executor:
            future = executor.submit(
                lambda: [reloader.call("reload") for _ in range(20)]
            )
            for _ in range(20):
                counts = [
                    director.call(command)["jobs"][0]["count"] for command in commands
                ]
                self.assertEqual(counts, expected)
            future.result()

    def test_list_jobs(self):
        """
        verifying `list jobs` and `llist jobs ...` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        backup_jobname = "backup-bareos-fd"
        backup_jobid = self.get_backup_jobid(director, backup_jobname, level="Full")
        self.run_restore(director, client="bareos-fd", jobid=backup_jobid, wait=True)
        backup_jobid_incremental = self.get_backup_jobid(
            director, backup_jobname, level="Incremental"
        )

        # Regular list jobs
        result = director.call("list jobs")

        expected_list_keys = [
            "jobid",
            "name",
            "client",
            "starttime",
            "duration",
            "type",
            "level",
            "jobfiles",
            "jobbytes",
            "jobstatus",
        ]
        resultkeys = list(result["jobs"][0].keys())
        resultkeys.sort()
        expected_list_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_list_keys,
        )
        with self.subTest(format="json", command="list backups"):
            backups = director.call("list backups client=bareos-fd")["backups"]
            self.assertTrue(backups)
            for backup in backups:
                self.assertEqual(set(backup), set(expected_list_keys))
        plain_director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )
        for command in (
            "list jobs sortby=joberrors",
            "list jobs last sortby=joberrors",
            "list backups client=bareos-fd",
        ):
            with self.subTest(format="text", command=command):
                output = plain_director.call(command).decode("utf-8")
                lines = output.splitlines()
                rows = [line for line in lines if line.startswith("|")]
                borders = [line for line in lines if line.startswith("+")]
                self.assertGreaterEqual(len(rows), 2)
                self.assertEqual(len(borders), 3)
                self.assertTrue(all(row.count("|") == 11 for row in rows))
                self.assertTrue(all(len(line) == len(rows[0]) for line in borders))
        api_command = "list jobs limit=2 sortby=jobid"
        with self.subTest(format="api", command=api_command):
            expected_ids = [job["jobid"] for job in director.call(api_command)["jobs"]]
            self.assertEqual(len(expected_ids), 2)
            plain_director.call(".api 1")
            try:
                output = plain_director.call(api_command).decode("utf-8")
                rows = output.splitlines()
                self.assertEqual(len(rows), 2)
                self.assertEqual(
                    [row.split("|", 1)[0].strip() for row in rows], expected_ids
                )
                self.assertTrue(output.endswith("\n"))
            finally:
                plain_director.call(".api 0")
        jobmedia = director.call(f"list jobmedia jobid={backup_jobid}")["jobmedia"]
        volume = jobmedia[0]["volumename"]
        usage_command = f'list volumeusage volume="{volume}"'
        segments = director.call(usage_command)["volumeusage"]["segments"]
        self.assertIn(str(backup_jobid), {str(row["jobid"]) for row in segments})
        text_output = plain_director.call(usage_command).decode("utf-8")
        self.assertIn(str(backup_jobid), text_output)
        plain_director.call(".api 1")
        try:
            output = plain_director.call(usage_command).decode("utf-8")
            self.assertIn(str(backup_jobid), output)
            self.assertIn(backup_jobname, output)
            self.assertTrue(output.endswith("\n"))
        finally:
            plain_director.call(".api 0")
        reg = re.compile("..:..:..")
        self.assertTrue(re.match(reg, result["jobs"][0]["duration"]))

        # Long list jobs
        result = director.call("llist jobs")

        expected_long_list_keys = [
            "jobid",
            "job",
            "name",
            "purgedfiles",
            "type",
            "level",
            "clientid",
            "client",
            "jobstatus",
            "schedtime",
            "starttime",
            "endtime",
            "realendtime",
            "duration",
            "jobtdate",
            "volsessionid",
            "volsessiontime",
            "jobfiles",
            "jobbytes",
            "joberrors",
            "jobmissingfiles",
            "poolid",
            "poolname",
            "priorjobid",
            "filesetid",
            "fileset",
            "comment",
        ]
        resultkeys = list(result["jobs"][0].keys())

        resultkeys.sort()
        expected_long_list_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_long_list_keys,
        )
        with self.subTest(format="json", command="llist backups"):
            backups = director.call("llist backups client=bareos-fd")["backups"]
            self.assertTrue(backups)
            for backup in backups:
                self.assertEqual(set(backup), set(expected_long_list_keys))

        original_errors = director.call(
            f'.sql query="SELECT JobErrors FROM Job WHERE JobId={backup_jobid_incremental}"'
        )["query"][0]["joberrors"]
        original_starttime = director.call(
            f'.sql query="SELECT StartTime FROM Job WHERE JobId={backup_jobid}"'
        )["query"][0]["starttime"]
        try:
            director.call(
                f'.sql query="UPDATE Job SET JobErrors=123 WHERE JobId={backup_jobid_incremental}"'
            )
            director.call(
                f""".sql query="UPDATE Job SET StartTime='2000-01-01 00:00:00' WHERE JobId={backup_jobid}" """
            )
            for last in ("", " last"):
                for reverse in ("", " reverse"):
                    with self.subTest(last=last, reverse=reverse):
                        long_jobs = director.call(f"llist jobs{last}")["jobs"]
                        errors = {
                            job["jobid"]: int(job["joberrors"]) for job in long_jobs
                        }
                        for command in ("list", "llist"):
                            query = f"{command} jobs{last} sortby=joberrors{reverse}"
                            jobs = director.call(query)["jobs"]
                            if command == "list":
                                self.assertEqual(set(jobs[0]), set(expected_list_keys))
                            self.assertEqual(
                                {job["jobid"] for job in jobs}, set(errors)
                            )
                            sort_keys = [
                                (
                                    errors[job["jobid"]],
                                    job["starttime"],
                                    int(job["jobid"]),
                                )
                                for job in jobs
                            ]
                            self.assertEqual(
                                sort_keys, sorted(sort_keys, reverse=bool(reverse))
                            )
                            for offset in range(min(3, len(jobs))):
                                page = director.call(
                                    f"{query} limit=1 offset={offset}"
                                )["jobs"]
                                self.assertEqual(page, jobs[offset : offset + 1])
        finally:
            try:
                director.call(
                    f""".sql query="UPDATE Job SET StartTime='{original_starttime}' WHERE JobId={backup_jobid}" """
                )
            finally:
                director.call(
                    f'.sql query="UPDATE Job SET JobErrors={int(original_errors)} WHERE JobId={backup_jobid_incremental}"'
                )

        # Long list with options

        result = director.call("llist jobs current")
        self.assertTrue(result["jobs"])

        result = director.call("llist jobs enable")
        self.assertTrue(result["jobs"])

        result = director.call("llist jobs last")
        self.assertTrue(result["jobs"])

        expected_long_list_last_keys = [
            "jobid",
            "job",
            "name",
            "purgedfiles",
            "type",
            "level",
            "clientid",
            "client",
            "jobstatus",
            "schedtime",
            "starttime",
            "endtime",
            "realendtime",
            "duration",
            "jobtdate",
            "volsessionid",
            "volsessiontime",
            "jobfiles",
            "jobbytes",
            "joberrors",
            "jobmissingfiles",
            "poolid",
            "poolname",
            "priorjobid",
            "filesetid",
            "fileset",
            "comment",
        ]
        resultkeys = list(result["jobs"][0].keys())

        resultkeys.sort()
        expected_long_list_last_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_long_list_last_keys,
        )

        result = director.call("llist jobs last current")
        self.assertTrue(result["jobs"])

        result = director.call("llist jobs last current enable")
        self.assertTrue(result["jobs"])

        # Counting jobs
        result = director.call("list jobs count")

        expected_list_count_keys = ["count"]
        self.assertEqual(
            list(result["jobs"][0].keys()),
            expected_list_count_keys,
        )

        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("list jobs count last")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("list jobs count current")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        # Long list counting

        result = director.call("llist jobs count")
        expected_long_list_count_keys = ["count"]
        self.assertEqual(
            list(result["jobs"][0].keys()),
            expected_long_list_count_keys,
        )
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("llist jobs count last")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        result = director.call("llist jobs count current")
        self.assertNotEqual(
            result["jobs"][0]["count"],
            "",
        )

        for command in ("list", "llist"):
            for last in ("", " last"):
                base = f"{command} jobs count{last} job=backup-bareos-fd"
                expected_count = director.call(base)["jobs"][0]["count"]
                for pagination in ("limit=0", "limit=1 offset=1"):
                    with self.subTest(command=command, last=last, range=pagination):
                        result = director.call(f"{base} {pagination}")
                        self.assertEqual(result["jobs"][0]["count"], expected_count)

        result = director.call(
            "llist jobs limit=99999999999999999999999999999999999999 offset=99999999999999999999999999999999999999999999999"
        )
        self.assertEqual(len(result["jobs"]), 0)

        # list jobs jobstatus=X
        result = director.call("list jobs jobstatus=T")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertTrue(job["jobstatus"], "T")

        # running a job a canceling
        jobid = self.run_job(director, "backup-bareos-fd")
        director.call(f"cancel jobid={jobid} yes")
        director.call(f"wait jobid={jobid}")

        # list jobs jobstatus=X,Y,z
        result = director.call("list jobs jobstatus=T,A")
        self.assertTrue(result["jobs"])

        terminated_jobs = 0
        canceled_jobs = 0
        for job in result["jobs"]:
            if job["jobstatus"] == "T":
                terminated_jobs += 1
            if job["jobstatus"] == "A":
                canceled_jobs += 1

        self.assertTrue(terminated_jobs >= 1)
        self.assertTrue(canceled_jobs >= 1)

        result = director.call("list jobs jobstatus=R")
        self.assertFalse(result["jobs"])

        # list jobs jobtype=X
        result = director.call("list jobs jobtype=B")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["type"], "B")

        result = director.call("list jobs jobtype=Restore")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["type"], "R")

        result = director.call("list jobs jobtype=B,R")
        self.assertTrue(result["jobs"])

        backup_jobs = 0
        restore_jobs = 0
        for job in result["jobs"]:
            if job["type"] == "B":
                backup_jobs += 1
            if job["type"] == "R":
                restore_jobs += 1

        self.assertTrue(backup_jobs >= 1)
        self.assertTrue(restore_jobs >= 1)

        # list jobs joblevel/level=X
        result = director.call("list jobs joblevel=F")
        result2 = director.call("list jobs level=F")
        self.assertTrue(result == result2)
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["level"], "F")

        result = director.call("list jobs level=I")
        self.assertTrue(result["jobs"])
        for job in result["jobs"]:
            self.assertEqual(job["level"], "I")

        result = director.call("list jobs joblevel=F,I")
        self.assertTrue(result["jobs"])
        full_jobs = 0
        incremental_jobs = 0
        for job in result["jobs"]:
            if job["level"] == "F":
                full_jobs += 1
            if job["level"] == "I":
                incremental_jobs += 1

        self.assertTrue(full_jobs >= 1)
        self.assertTrue(incremental_jobs >= 1)

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = director.call("list jobs=x")

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = director.call("list job")

        result = director.call("list job=invalid_jobname")
        self.assertEqual(len(result["jobs"]), 0)

        result = director.call(f"list job={backup_jobname}")
        self.assertGreater(len(result["jobs"]), 1)

        # this is not required to work.
        result = director.call(f"list jobs job={backup_jobname}")
        self.assertGreater(len(result["jobs"]), 1)

        result = director.call(f"list jobname={backup_jobname}")
        self.assertGreater(len(result["jobs"]), 1)

        # this is not required to work.
        result = director.call(f"list jobs jobname={backup_jobname}")
        self.assertGreater(len(result["jobs"]), 1)

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = director.call("list jobid")

        result = director.call(f"llist jobid={backup_jobid}")
        self.assertEqual(result["jobs"][0]["jobid"], backup_jobid)
        backup_job_ujobid = result["jobs"][0]["job"]

        # this is not required to work.
        result = director.call(f"llist jobs jobid={backup_jobid}")
        self.assertEqual(result["jobs"][0]["jobid"], backup_jobid)

        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            result = director.call("list ujobid")

        result = director.call(f"llist ujobid={backup_job_ujobid}")
        self.assertEqual(result["jobs"][0]["jobid"], backup_jobid)

        # this is not required to work.
        result = director.call(f"llist jobs ujobid={backup_job_ujobid}")
        self.assertEqual(result["jobs"][0]["jobid"], backup_jobid)

        result = director.call(f"llist ujobid=invalid_ujobid")
        self.assertEqual(len(result["jobs"]), 0)

    def test_list_media(self):
        """
        verifying `list media` and `llist media ...` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        self.get_backup_jobid(director, "backup-bareos-fd", level="Full")

        # check for expected keys
        result = director.call("list media")
        expected_list_media_keys = [
            "mediaid",
            "volumename",
            "volstatus",
            "enabled",
            "volbytes",
            "volfiles",
            "volretention",
            "recycle",
            "slot",
            "inchanger",
            "mediatype",
            "lastwritten",
            "storage",
            "hasencryptionkey",
        ]
        resultkeys = list(result["volumes"]["full"][0].keys())

        resultkeys.sort()
        expected_list_media_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_list_media_keys,
        )

        # check expected behavior when asking for specific volume by name
        test_volume = "test_volume0001"

        result = director.call("list volume={}".format(test_volume))
        if len(result["volume"]) >= 1:
            director.call("delete volume={} yes".format(test_volume))
        try:
            os.remove("storage/{}".format(test_volume))
        except FileNotFoundError:
            pass

        director.call("label volume={} pool=Full".format(test_volume))

        result = director.call("list media={}".format(test_volume))
        self.assertEqual(
            result["volume"]["volumename"],
            test_volume,
        )
        result = director.call("list volume={}".format(test_volume))
        self.assertEqual(
            result["volume"]["volumename"],
            test_volume,
        )

        mediaid = result["volume"]["mediaid"]
        # raises an exception if not an integer
        int(mediaid)

        # check expected behavior when asking for specific volume by mediaid
        result = director.call(f"list mediaid={mediaid}")
        self.assertEqual(
            result["volume"]["mediaid"],
            mediaid,
        )
        result = director.call(f"list volumeid={mediaid}")
        self.assertEqual(
            result["volume"]["mediaid"],
            mediaid,
        )

        result = director.call(f"llist mediaid={mediaid}")
        self.assertEqual(
            result["volume"]["mediaid"],
            mediaid,
        )
        result = director.call(f"llist volumeid={mediaid}")
        self.assertEqual(
            result["volume"]["mediaid"],
            mediaid,
        )

    def test_list_pool(self):
        """
        verifying `list pool` and `llist pool ...` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        self.get_backup_jobid(director, "backup-bareos-fd")

        result = director.call("list pool")
        expected_list_pool_keys = [
            "poolid",
            "name",
            "numvols",
            "maxvols",
            "pooltype",
            "labelformat",
        ]

        resultkeys = list(result["pools"][0].keys())

        resultkeys.sort()
        expected_list_pool_keys.sort()
        self.assertEqual(
            resultkeys,
            expected_list_pool_keys,
        )

        # check expected behavior when asking for specific volume by name
        result = director.call("list pool=Incremental")
        self.assertEqual(
            result["pools"][0]["name"],
            "Incremental",
        )

        result = director.call("list pool=Full")
        self.assertEqual(
            result["pools"][0]["name"],
            "Full",
        )
        # raises an exception, if pool_id_full is not an integer.
        pool_id_full = int(result["pools"][0]["poolid"])

        result = director.call("llist pool")
        expected_long_list_pool_keys = [
            "poolid",
            "name",
            "numvols",
            "maxvols",
            "useonce",
            "usecatalog",
            "acceptanyvolume",
            "volretention",
            "voluseduration",
            "maxvoljobs",
            "maxvolbytes",
            "autoprune",
            "recycle",
            "pooltype",
            "labelformat",
            "enabled",
            "scratchpoolid",
            "recyclepoolid",
            "labeltype",
            "maxvolfiles",
            "actiononpurge",
            "minblocksize",
            "maxblocksize",
        ]
        optional_long_list_pool_keys = {
            "prunablevolumes",
            "prunablejobs",
            "prunablebytes",
        }
        resultkeys = list(result["pools"][0].keys())
        self.assertEqual(
            set(resultkeys) - optional_long_list_pool_keys,
            set(expected_long_list_pool_keys),
        )
        self.assertTrue(
            set(resultkeys).issubset(
                set(expected_long_list_pool_keys) | optional_long_list_pool_keys
            ),
        )

        # check expected behavior when asking for specific volume by name
        result = director.call("llist pool=Incremental")
        self.assertEqual(
            result["pools"][0]["name"],
            "Incremental",
        )

        result = director.call(f"list poolid={pool_id_full}")
        self.assertEqual(
            result["pools"][0]["name"],
            "Full",
        )

        self.assertEqual(
            int(result["pools"][0]["poolid"]),
            pool_id_full,
        )

        result = director.call(f"llist poolid={pool_id_full}")
        self.assertEqual(
            result["pools"][0]["name"],
            "Full",
        )

        self.assertEqual(
            int(result["pools"][0]["poolid"]),
            pool_id_full,
        )

    def test_list_files(self):
        """
        verifying `list files` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director_json = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.run_job(
            director_json,
            jobname="backup-bareos-fd",
            level="Full",
            fileset="NumberedFiles",
            wait=True,
        )
        result = director_json.call(f"list files jobid={jobid}")
        # Before fixing https://bugs.bareos.org/view.php?id=1007 the keys could be scrambled after 100 entries.
        filenames = result["filenames"]
        min_files = 100
        self.assertGreater(
            len(filenames),
            min_files,
            f"More than {min_files} files are required for this test, only {len(filenames)} given.",
        )

        bad_headers = [f for f in filenames if "filename" not in f]
        self.assertEqual(
            bad_headers,
            [],
            f"Some files were not listed with the correct 'filename' header.",
        )

        filenames_json = set(f["filename"] for f in filenames)

        director = bareos.bsock.DirectorConsole(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )
        result = (
            director.call(f"list files jobid={jobid}")
            .decode("utf-8")
            .replace("\n ", "\n")
        )
        # start at the first path,
        # skipping lines like 'Automatically selected Catalog: MyCatalog\nUsing Catalog "MyCatalog"\n'
        start = result.find("\n/") + 1
        filenames_plain = set(result[start:].splitlines())
        self.assertEqual(
            filenames_json,
            filenames_plain,
            "JSON result is not identical to plain (api 0) result.",
        )

    def verify_joblog(self, result):
        self.assertIn("joblog", result)

        for i in result["joblog"]:
            self.assertIn("time", i)
            self.assertIn("logtext", i)

    def test_list_joblog(self):
        """
        verifying `list joblog` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.get_backup_jobid(director=director, jobname="backup-bareos-fd")

        result = director.call(f"list joblog jobid={jobid}")
        self.verify_joblog(result)

        expected = director.call(f"list joblog jobids={jobid}")
        self.assertGreater(len(expected["joblog"]), 2)
        for token in (f"{jobid}x", f"{jobid}.0", "4294967296", "-1"):
            with self.subTest(invalid_jobid=token):
                self.assertEqual(
                    director.call(f"list joblog jobids={token} count")["joblog"],
                    [{"count": "0"}],
                )
                self.assertEqual(
                    director.call(f"list joblog jobids={token},{jobid}"), expected
                )
        for command in ("list", "llist"):
            with self.subTest(command=command):
                logs = director.call(f"{command} joblog jobids={jobid}")["joblog"]
                page = director.call(
                    f"{command} joblog jobids={jobid} limit=1 offset=1"
                )["joblog"]
                self.assertEqual(page, logs[1:2])
                self.assertEqual(
                    director.call(f"{command} joblog jobids={jobid} limit=0")["joblog"],
                    [],
                )
                for options in ("count", "count limit=0 offset=1"):
                    self.assertEqual(
                        director.call(
                            f"{command} joblog jobids={jobid},{jobid} {options}"
                        )["joblog"],
                        [{"count": str(len(logs))}],
                    )
                self.assertEqual(
                    director.call(f"{command} joblog jobids=0 count")["joblog"],
                    [{"count": "0"}],
                )
        at_limit = ",".join([str(jobid)] * 1000)
        self.assertEqual(director.call(f"list joblog jobids={at_limit}"), expected)

        for token in (str(jobid), "0"):
            with self.subTest(token=token):
                over_limit = ",".join([token] * 1001)
                with self.assertRaises(
                    bareos.exceptions.JsonRpcErrorReceivedException
                ) as error:
                    director.call(f"list joblog jobids={over_limit}")
                self.assertIn("1000", str(error.exception))

    def test_list_joblog_non_utf8(self):
        """
        verifying `list joblog` outputs correct data
        """
        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options,
        )

        jobid = self.get_backup_jobid(director=director, jobname="backup-bareos-fd")
        logger.info(f"jobid={jobid}")
        print(f"jobid={jobid}")

        result = director.call(f"list joblog jobid={jobid}")
        self.verify_joblog(result)

        result = director.call(
            f".sql query=\"insert into log (jobid, time, logtext) values ({jobid}, '2099-01-01 00:00:00', E'\\\\xAB\\\\xCD\\\\nINVALID\\\\n');\""
        )
        print(result)

        result = director.call(f"list joblog jobid={jobid}")
        self.verify_joblog(result)
