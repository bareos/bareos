#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2024 Bareos GmbH & Co. KG
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
import json
import logging
import os
import re
import subprocess
from time import sleep
import unittest
import warnings

import bareos.bsock
from bareos.bsock.constants import Constants
from bareos.bsock.protocolmessages import ProtocolMessages
from bareos.bsock.protocolversions import ProtocolVersions
from bareos.bsock.lowlevel import LowLevel
import bareos.exceptions

import bareos_unittest


class PythonBareosBvfsTest(bareos_unittest.Json):
    def get_sql_table_count(self, director, sql_from, minimum=None, maximum=None):
        # expect:
        # .sql query="SELECT count(*) FROM Job WHERE HasCache!=0;"
        # .api 0
        # +----------+
        # | count    |
        # +----------+
        # | 12       |
        # +----------+
        # .api 2
        # "query": [
        #   {
        #     "count": "12"
        #   }
        # ]
        logger = logging.getLogger()
        cmd = '.sql query="SELECT count(*) FROM {};"'.format(sql_from)
        count = int(director.call(cmd)["query"][0]["count"])
        if minimum is not None:
            self.assertGreaterEqual(
                count,
                minimum,
                "{}: expecting at least {} entries, got {}".format(
                    sql_from, minimum, count
                ),
            )
        if maximum is not None:
            self.assertLessEqual(
                count,
                maximum,
                "{}: expecting at most {} entries, got {}".format(
                    sql_from, minimum, count
                ),
            )
        logger.debug(
            "{}: {} entries ({}, {})\n".format(sql_from, count, minimum, maximum)
        )
        return count

    def prepare_bvfs_jobs(self, director, jobname, format_params):
        self.append_to_file(format_params["BackupFileExtra"], "Test Content: initial\n")

        jobid1 = self.get_backup_jobid(director, jobname, level="Full")
        files1 = director.call("list files jobid={}".format(jobid1))["filenames"]

        # modify file and rerun backup
        self.append_to_file(
            format_params["BackupFileExtra"],
            "Additional content, after job {}\n".format(jobid1),
        )

        jobid2 = self.run_job(director, jobname, wait=True)

        bvfs_jobids = director.call(".bvfs_get_jobids jobid={}".format(jobid2))[
            "jobids"
        ]
        backup_job_ids = [jobid["id"] for jobid in bvfs_jobids]
        self.assertGreaterEqual(
            len(backup_job_ids),
            2,
            "Expecting at least 2 jobs, got {}.".format(backup_job_ids),
        )
        format_params["BackupJobIds"] = ",".join(backup_job_ids)

        result = director.call(
            ".bvfs_update jobid={BackupJobIds}".format(**format_params)
        )

        self.get_sql_table_count(
            director, "Job WHERE HasCache!=0", minimum=len(backup_job_ids)
        )
        self.get_sql_table_count(director, "PathHierarchy", minimum=1)
        self.get_sql_table_count(director, "PathVisibility", minimum=1)

        return bvfs_jobids

    def verify_bvfs_dirs(self, bvfs_lsdirs_result, expected_dirs):
        # bvfs_lsdirs_result contains the result of ".bvfs_lsdir ...".
        # Example:
        # .api 0
        # 10    0   0   A A A A A A A A A A A A A A .
        # 9   0   0   A A A A A A A A A A A A A A /
        # 8   0   0   A A A A A A A A A A A A A A BPIPE_ROOT/
        # .api 2
        # "directories": [
        #   {
        #      "type": "D",
        #      "pathid": 10,
        #      "fileid": 0,
        #      "jobid": 0,
        #      "lstat": "A A A A A A A A A A A A A A",
        #      "name": ".",
        #      "fullpath": ".",
        #      "stat": {
        #          ...
        #      },
        #      "linkfileindex": 0
        #   },
        #   {
        #     "type": "D",
        #     "pathid": 15,
        #     "fileid": 0,
        #     "jobid": 0,
        #     "lstat": "A A A A A A A A A A A A A A",
        #     "name": "/",
        #     "fullpath": "/",
        #     "stat": {
        #        ...
        #     },
        #     "linkfileindex": 0
        #   },
        #   {
        #     "type": "D",
        #     "pathid": 14,
        #     "fileid": 0,
        #     "jobid": 0,
        #     "lstat": "A A A A A A A A A A A A A A",
        #     "name": "BPIPE-ROOT/",
        #     "fullpath": "BPIPE-ROOT/",
        #     "stat": {
        #       ...
        #     },
        #     "linkfileindex": 0
        #   }
        # ]

        directories = [item["name"] for item in bvfs_lsdirs_result]

        for directory in expected_dirs:
            self.assertIn(
                directory,
                directories,
                'Expecting "{}" in list of directories, instead received: {}'.format(
                    directory, bvfs_lsdirs_result
                ),
            )

    def test_bvfs(self):
        """
        Test BVFS functionality:
        Run 1 full and 1 incremental backup job.
        These jobs have the normal "/" directory and an additional BPIPE-ROOT/ directory.
        Also do a restore using bvfs.
        """

        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-bpipe"

        format_params = {
            "Client": self.client,
            "BvfsPathId": "b201",
            "BackupDirectory": self.backup_directory,
            "BackupFileNameExtra": "bvfstest.txt",
            "BackupFileExtra": self.backup_directory + "/bvfstest.txt",
        }

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        bvfs_jobids = self.prepare_bvfs_jobs(director, jobname, format_params)

        bvfs_get_root_path = director.call(
            ".bvfs_lsdir jobid={BackupJobIds} path=".format(**format_params)
        )["directories"]

        self.verify_bvfs_dirs(bvfs_get_root_path, ["/", "BPIPE-ROOT/"])

        # Test offset
        cmd = ".bvfs_lsdir jobid={BackupJobIds} path= offset=1000 limit=1000".format(
            **format_params
        )
        bvfs_get_root_path_offset = director.call(cmd)["directories"]
        self.assertEqual(
            len(bvfs_get_root_path_offset),
            0,
            'Expecting "{}" to return empty list, instead received: {}'.format(
                cmd, bvfs_get_root_path_offset
            ),
        )

        cmd = ".bvfs_lsdir jobid={BackupJobIds} path=/".format(**format_params)
        bvfs_lsdir_root = director.call(cmd)["directories"]
        # expect:
        # 9   0   0   0   A A A A A A A A A A A A A A .
        # 10  0   0   0   A A A A A A A A A A A A A A ..
        # 8   0   0   0   A A A A A A A A A A A A A A TOP_DIRECTORY
        self.assertGreaterEqual(
            len(bvfs_lsdir_root),
            3,
            'Expecting at least 3 directories from "{}", instead received: {}'.format(
                cmd, bvfs_lsdir_root
            ),
        )

        cmd = ".bvfs_lsdir jobid={BackupJobIds} path={BackupDirectory}/".format(
            **format_params
        )
        bvfs_lsdir_BackupDirectory = director.call(cmd)["directories"]
        # expect:
        # 1 0   19  1   x GoHK EHt C GHH GHH A BAA BAA I BWDNOj BZwlgI BZwlgI A A C .
        # 2   0   0   0   A A A A A A A A A A A A A A ..
        # need to get dirid of "."
        backup_directory_pathid = next(
            directory["pathid"]
            for directory in bvfs_lsdir_BackupDirectory
            if directory["name"] == "."
        )
        self.assertIsInstance(
            backup_directory_pathid,
            int,
            'Failed to determine "pathid" of {} from {}. Received: {}'.format(
                cmd, format_params["BackupDirectory"], bvfs_lsdir_BackupDirectory
            ),
        )
        format_params["BackupDirectoryPathId"] = backup_directory_pathid

        cmd = ".bvfs_lsfiles jobid={BackupJobIds} path={BackupDirectory}/".format(
            **format_params
        )
        bvfs_lsfiles_BackupDirectory = director.call(cmd)["files"]
        backup_extrafile_id = next(
            directory["fileid"]
            for directory in bvfs_lsfiles_BackupDirectory
            if directory["name"] == "{BackupFileNameExtra}".format(**format_params)
        )
        self.assertIsInstance(
            backup_extrafile_id,
            int,
            'Failed to "{}" from "{}". Received: {}'.format(
                format_params["BackupFileNameExtra"], cmd, bvfs_lsdir_BackupDirectory
            ),
        )

        # test limit.
        cmd = ".bvfs_lsfiles jobid={BackupJobIds} path={BackupDirectory}/ offset 0 limit 1".format(
            **format_params
        )
        bvfs_lsfiles_BackupDirectory_limit1 = director.call(cmd)["files"]
        # TODO: bug, this returns multiple entires
        # self.assertEqual(len(bvfs_lsfiles_BackupDirectory_limit1), 1, 'Expecting 1 file from "{}", instead received: {}'.format(cmd, bvfs_lsfiles_BackupDirectory_limit1))

        cmd = ".bvfs_versions jobid=0 client={Client} path={BackupDirectory}/ fname={BackupFileNameExtra}".format(
            **format_params
        )
        bvfs_versions_extrafile = director.call(cmd)["versions"]
        self.assertGreaterEqual(
            len(bvfs_versions_extrafile),
            2,
            'Expecting at least 2 versions of file "{}" from "{}". Received: {}'.format(
                format_params["BackupFileNameExtra"], cmd, bvfs_versions_extrafile
            ),
        )

        cmd = ".bvfs_lsfiles jobid={BackupJobIds} path=BPIPE-ROOT/".format(
            **format_params
        )
        bvfs_lsfiles_bpipe_directory = director.call(cmd)["files"]
        files = [item["name"] for item in bvfs_lsfiles_bpipe_directory]
        self.assertIn(
            "bpipe-dir-as-root-test.txt",
            files,
            'Failed to get "bpipe-dir-as-root-test.txt" from "{}". Received: {}'.format(
                cmd, bvfs_lsfiles_bpipe_directory
            ),
        )

        #
        # now do a restore
        #

        # syntax:
        #   .bvfs_restore path=b201 fileid=numlist dirid=numlist hardlink=numlist path=b201
        cmd = ".bvfs_restore path={BvfsPathId} jobid={BackupJobIds} dirid={BackupDirectoryPathId}".format(
            **format_params
        )
        bvfs_restore_prepare = director.call(cmd)
        self.get_sql_table_count(director, format_params["BvfsPathId"], minimum=1)

        cmd = "restore client={Client} file=?{BvfsPathId} yes".format(**format_params)
        bvfs_restore = director.call(cmd)
        jobId = bvfs_restore["run"]["jobid"]
        self.wait_job(director, jobId)

        bvfs_cleanup = director.call(
            ".bvfs_cleanup path={BvfsPathId}".format(**format_params)
        )

        # table does not exist any more, so query will fail.
        with self.assertRaises(bareos.exceptions.JsonRpcErrorReceivedException):
            self.get_sql_table_count(director, format_params["BvfsPathId"])

        bvfs_clear_cache = director.call(".bvfs_clear_cache yes")

        self.get_sql_table_count(director, "Job WHERE HasCache!=0", maximum=0)
        self.get_sql_table_count(director, "PathHierarchy", maximum=0)
        self.get_sql_table_count(director, "PathVisibility", maximum=0)

        ## check for differences between original files and restored files
        # check_restore_diff ${BackupDirectory}

    def test_bvfs_and_dbcheck(self):
        """
        Test BVFS together with dbcheck:
        previous versions (<= 17.2.4) had a problem with bvfs after running dbcheck.
        Therefore this test checks
          * check bvfs root directory
          * run all dbcheck fixes
          * check bvfs root directory again
        """

        logger = logging.getLogger()

        username = self.get_operator_username()
        password = self.get_operator_password(username)

        jobname = "backup-bareos-fd-bpipe"

        format_params = {
            "Client": self.client,
            "BvfsPathId": "b201",
            "BackupDirectory": self.backup_directory,
            "BackupFileNameExtra": "bvfstest.txt",
            "BackupFileExtra": self.backup_directory + "/bvfstest.txt",
        }

        director = bareos.bsock.DirectorConsoleJson(
            address=self.director_address,
            port=self.director_port,
            name=username,
            password=password,
            **self.director_extra_options
        )

        bvfs_jobids = self.prepare_bvfs_jobs(director, jobname, format_params)

        bvfs_get_root_path = director.call(
            ".bvfs_lsdir jobid={BackupJobIds} path=".format(**format_params)
        )["directories"]

        self.verify_bvfs_dirs(bvfs_get_root_path, ["/", "BPIPE-ROOT/"])

        self.run_dbcheck()

        bvfs_get_root_path = director.call(
            ".bvfs_lsdir jobid={BackupJobIds} path=".format(**format_params)
        )["directories"]

        self.verify_bvfs_dirs(bvfs_get_root_path, ["/", "BPIPE-ROOT/"])

        self.run_dbcheck()

        bvfs_clear_cache = director.call(".bvfs_clear_cache yes")
        result = director.call(
            ".bvfs_update jobid={BackupJobIds}".format(**format_params)
        )

        self.run_dbcheck()
