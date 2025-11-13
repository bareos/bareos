#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2025 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# Original Author: Alexandre Bruyelles <git@jack.fr.eu.org>

import traceback
import configparser
import datetime
import io
import os
import time
import ast
import operator
from sys import version_info
from collections import defaultdict

from BareosFdWrapper import *  # noqa
import bareosfd
from BareosFdPluginBaseclass import BareosFdPluginBaseclass

from bareos_libcloud_api.bucket_explorer import TaskType
from bareos_libcloud_api.debug import debugmessage
from bareos_libcloud_api.debug import jobmessage
from bareos_libcloud_api.get_libcloud_driver import get_driver

from BareosLibcloudApi import WorkerState
from BareosLibcloudApi import BareosLibcloudApi

from libcloud.storage.types import ObjectDoesNotExistError


def strtobool(query: str) -> bool:
    """
    reimplement strtobool per PEP 632 and python 3.12 deprecation
    True values are y, yes, t, true, on and 1;
    false values are n, no, f, false, off and 0.
    Raises ValueError if val is anything else.
    """
    if query.lower() in ["y", "yes", "t", "true", "on", "1"]:
        return True
    if query.lower() in ["n", "no", "f", "false", "off", "0"]:
        return False

    raise ValueError


class StringCodec:
    """
    Utility class for handling string encoding and decoding
    for backup and restore operations, based on the Python version.
    """

    @staticmethod
    def encode_for_backup(var):
        """
        Encodes a string to UTF-8 for backup purposes on Python 2.

        On Python 2, this method returns the input string encoded as UTF-8.
        On Python 3, it returns the string unchanged.
        """
        if version_info.major < 3:
            return var.encode("utf-8")

        return var

    @staticmethod
    def encode_for_restore(var):
        """
        Decodes a string for restore purposes.

        On Python 2, this method returns the string unchanged.
        On Python 3, it returns the input string encoded as UTF-8.
        """
        if version_info.major < 3:
            return var

        return var.encode("utf-8")


class FilenameConverter:
    """
    Utility class for converting filenames between backup and bucket formats.
    In the Bareos Catalog filenames are stored with the prefix 'PYLIBCLOUD:/',
    for restores this prefix must be removed.

    Example bucket format: 'testbucket/testfolder/testfile.txt'
    Example backup format: 'PYLIBCLOUD:/testbucket/testfolder/testfile.txt'
    """

    __pathprefix = "PYLIBCLOUD:/"

    @staticmethod
    def bucket_to_backup(filename):
        """
        Convert a bucket filename to backup format by adding the path prefix.
        """
        return FilenameConverter.__pathprefix + filename

    @staticmethod
    def backup_to_bucket(filename):
        """
        Convert a backup filename to bucket format by removing the path prefix.
        """
        return filename.replace(FilenameConverter.__pathprefix, "")


class SafeEval:
    """
    A safer alternative to eval() for limited numeric expressions.
    This class evaluates expressions represented in an Abstract Syntax Tree (AST)
    to prevent the execution of malicious or unsupported code.
    """

    # Allowed operators for binary operations (here only * and ** allowed)
    ALLOWED_BIN_OPS = {
        ast.Mult: operator.mul,
        ast.Pow: operator.pow,
    }

    # Allowed operators for unary operations (here only + allowed)
    ALLOWED_UNARY_OPS = {
        ast.UAdd: operator.pos,
    }

    @staticmethod
    def size(numeric_expression):
        """
        Safely evaluates a numeric expression string.

        Args:
            numeric_expression (str): The string expression to evaluate.

        Returns:
            The numeric result of the expression.

        Raises:
            ValueError: If the expression contains unsupported operators,
                        unsupported constant types, or other invalid syntax.
        """

        def _eval(node):
            # Handles numeric constants (e.g., 5, 3.14)
            if isinstance(node, ast.Constant):
                if isinstance(node.value, (int, float)):
                    return node.value
                raise ValueError(
                    f"Unsupported constant type: {type(node.value).__name__}"
                )
            # Handles older Python versions' number representation (ast.Num)
            if hasattr(ast, "Num") and isinstance(node, ast.Num):
                return node.n
            # Handles binary operations (e.g., 5 * 2, 10 + 5)
            if isinstance(node, ast.BinOp):
                op_type = type(node.op)
                if op_type not in SafeEval.ALLOWED_BIN_OPS:
                    raise ValueError(f"Binary operator {op_type.__name__} not allowed")
                left_val = _eval(node.left)
                right_val = _eval(node.right)
                return SafeEval.ALLOWED_BIN_OPS[op_type](left_val, right_val)
            # Handles unary operations (e.g., -5)
            if isinstance(node, ast.UnaryOp):
                op_type = type(node.op)
                if op_type not in SafeEval.ALLOWED_UNARY_OPS:
                    raise ValueError(f"Unary operator {op_type.__name__} not allowed")
                operand_val = _eval(node.operand)
                return SafeEval.ALLOWED_UNARY_OPS[op_type](operand_val)

            # Catches any other unsupported AST node types
            raise ValueError(f"Unsupported expression type: {ast.dump(node)}")

        # Parse the expression string into an AST
        try:
            parsed_node = ast.parse(numeric_expression, mode="eval").body
        except SyntaxError as exc:
            raise ValueError(
                f"Invalid syntax in expression: {numeric_expression}"
            ) from exc

        # Evaluate the AST recursively
        return _eval(parsed_node)


@BareosPlugin
class BareosFdPluginLibcloud(BareosFdPluginBaseclass):
    """
    Bareos File Daemon Plugin for backup of stored on S3 or compatible cloud storage
    using Apache Libcloud library.
    """

    def __init__(self, plugindef):
        """
        Initialize the plugin with the given plugin definition.
        """
        debugmessage(100, f"BareosFdPluginLibcloud called with plugindef: {plugindef}")

        super().__init__(plugindef)
        # defaults for optional plugin options
        self.default_options = {}
        self.default_options["fail_on_download_error"] = "yes"
        self.default_options["job_message_after_each_number_of_objects"] = "100"
        self.default_options["prefetch_inmemory_size"] = "10240"
        self.default_options["global_timeout"] = "600"
        self.last_run = datetime.datetime.fromtimestamp(self.since)
        self.last_run = self.last_run.replace(tzinfo=None)

        # The backup task in process
        # Set to None when the whole backup is completed
        # Restore's path will not touch this
        self.current_backup_task = {}
        self.backed_up_objects_count = 0
        self.file = None
        self.active = True
        self.api = None
        self.config = None
        self.job_message_after_each_number_of_objects = None
        # global timeout in seconds
        self.global_timeout = 600
        self.io_in_core = True
        self.file_count = defaultdict(int)
        self.mandatory_options = [
            "hostname",
            "port",
            "provider",
            "tls",
            "username",
            "password",
            "nb_worker",
            "queue_size",
            "prefetch_size",
            "temporary_download_directory",
        ]
        self.optional_options = [
            "fail_on_download_error",
            "job_message_after_each_number_of_objects",
            "buckets_include",
            "prefetch_inmemory_size",
            "global_timeout",
        ]

        # this maps config file options to libcloud options
        self.option_map = {
            "hostname": "host",
            "port": "port",
            "provider": "provider",
            "username": "key",
            "password": "secret",
        }

        self.effective_options = {}

    def _get_accurate_mode(self):
        """
        Parse plugin options and set defaults.
        """

        bvar_accurate = bareosfd.GetValue(bareosfd.bVarAccurate)
        if bvar_accurate is None or bvar_accurate == 0:
            self.effective_options["accurate"] = False
        else:
            self.effective_options["accurate"] = True

    def _parse_config_file(self, config_filename):
        """
        Parse the config file given in the config_file plugin option.
        """
        debugmessage(100, f"parse_config_file(): parse {config_filename}")

        self.config = configparser.ConfigParser()

        try:
            with open(config_filename, "r", encoding="utf-8") as config_file:
                self.config.read_file(config_file)
        except (IOError, OSError) as err:
            debugmessage(
                100,
                (
                    f"Error reading config file {self.options['config_file']}: "
                    f"{err.strerror}"
                ),
            )
            return False

        return self._check_config(config_filename)

    def _check_config(self, config_filename):
        """
        This implements backwards compatibility with the config_file option.
        As now defaults_file and overrides_file (implemented in BareosFdPluginBaseclass)
        can be used, this mimics the behavior of overrides_file by just setting the values
        in self.options, special option handling afterwards is done in check_options().
        """
        mandatory_sections = ["credentials", "host", "misc"]
        mandatory_options = {}
        mandatory_options["credentials"] = ["username", "password"]
        mandatory_options["host"] = ["hostname", "port", "provider", "tls"]
        mandatory_options["misc"] = [
            "nb_worker",
            "queue_size",
            "prefetch_size",
            "temporary_download_directory",
        ]
        optional_options = {}
        optional_options["misc"] = [
            "fail_on_download_error",
            "job_message_after_each_number_of_objects",
            "buckets_include",
            "prefetch_inmemory_size",
        ]

        for section in mandatory_sections:
            if not self.config.has_section(section):
                debugmessage(
                    100,
                    (
                        f"BareosFdPluginLibcloud: Section [{section}] missing in config file "
                        f"{config_filename}"
                    ),
                )
                return False

            for option in mandatory_options[section]:

                if not self.config.has_option(section, option):
                    debugmessage(
                        100,
                        (
                            f"BareosFdPluginLibcloud: Option [{option}] missing in Section "
                            f"{section}"
                        ),
                    )
                    return False

                value = self.config.get(section, option)
                self.options[option] = value

        for option in optional_options["misc"]:
            if self.config.has_option(section, option):
                self.options[option] = self.config.get(section, option)
            elif option in self.default_options:
                self.options[option] = self.default_options[option]

        return True

    def _check_options(self, mandatory_options=None):

        for option in mandatory_options:

            value = self.options.get(option)
            if value is None:
                error_message = f"Mandatory option '{option}' not defined."
                debugmessage(100, error_message)
                jobmessage(bareosfd.M_FATAL, error_message)
                return bareosfd.bRC_Error

            try:
                if option == "tls":
                    self.effective_options["secure"] = strtobool(value)
                elif option == "nb_worker":
                    self.effective_options["nb_worker"] = int(value)
                elif option == "queue_size":
                    self.effective_options["queue_size"] = int(value)
                elif option == "prefetch_size":
                    self.effective_options["prefetch_size"] = int(SafeEval.size(value))
                elif option == "temporary_download_directory":
                    self.effective_options["temporary_download_directory"] = value
                else:
                    self.effective_options[option] = value
            except ValueError:
                debugmessage(
                    100,
                    (f"Could not evaluate option {option} = {value}"),
                )
                jobmessage(
                    bareosfd.M_FATAL,
                    (f"Could not evaluate option '{option} = {value}'"),
                )
                return bareosfd.bRC_Error

        for option in self.optional_options:
            # use default value if not present, buckets_include has no default.
            if option != "buckets_include" and option not in self.options:
                self.options[option] = self.default_options[option]

            if option in self.options:
                try:
                    value = self.options[option]
                    if option == "fail_on_download_error":
                        self.effective_options["fail_on_download_error"] = strtobool(
                            value
                        )
                    elif option == "job_message_after_each_number_of_objects":
                        self.job_message_after_each_number_of_objects = int(value)
                    elif option == "prefetch_inmemory_size":
                        self.effective_options["prefetch_inmemory_size"] = int(value)
                    elif option == "global_timeout":
                        self.global_timeout = int(value)
                    else:
                        self.effective_options[option] = value
                except ValueError:
                    debugmessage(
                        100,
                        f"Could not evaluate: {option} = {value}",
                    )
                    return False

        for plugin_option, libcloud_option in self.option_map.items():
            if plugin_option in self.effective_options:
                self.effective_options[libcloud_option] = self.effective_options[
                    plugin_option
                ]

        for k, v in self.options.items():
            debugmessage(100, f"_check_options(): options {k}: {v} ({type(v)})")

        for k, v in self.effective_options.items():
            debugmessage(
                100, f"_check_options(): effective_options {k}: {v} ({type(v)})"
            )

        return bareosfd.bRC_OK

    def check_options(self, mandatory_options=None):
        self._get_accurate_mode()
        config_filename = self.options.get("config_file")
        if config_filename:
            if not self._parse_config_file(config_filename):
                debugmessage(100, f"Could not load configfile {config_filename}")
                jobmessage(
                    bareosfd.M_FATAL, f"Could not load configfile {config_filename}"
                )
                return bareosfd.bRC_Error

        return self._check_options(mandatory_options)

    def start_backup_job(self):
        """
        Start the backup job and initialize the Libcloud API connection.
        """
        jobmessage(
            bareosfd.M_INFO,
            f"Start backup, try to connect to {self.effective_options['host']}:{self.effective_options['port']}",
        )

        try:
            # probe the libcloud driver
            get_driver(self.effective_options)

            jobmessage(
                bareosfd.M_INFO,
                f"Connected, last backup: {self.last_run} (ts: {self.since})",
            )

            self.api = BareosLibcloudApi(
                self.effective_options,
                self.last_run,
                self.effective_options["temporary_download_directory"],
            )
            debugmessage(100, "BareosLibcloudApi started")

        except Exception as e:
            debugmessage(100, f"Error: {e} \n{traceback.format_exc()}")
            jobmessage(bareosfd.M_FATAL, f"Starting BareosLibcloudApi failed: {e}")
            return bareosfd.bRC_Cancel

        return bareosfd.bRC_OK

    def end_backup_job(self):
        """
        Called at end of the backup job, calls shutdown of Libcloud API.
        """
        debugmessage(100, "end_backup_job() was called")
        if self.active:
            self._shutdown()

        jobmessage(
            bareosfd.M_INFO,
            f"File count in-memory: {self.file_count[TaskType.DOWNLOADED]}",
        )
        jobmessage(
            bareosfd.M_INFO,
            f"File count preloaded: {self.file_count[TaskType.TEMP_FILE]}",
        )
        jobmessage(
            bareosfd.M_INFO, f"File count streamed:  {self.file_count[TaskType.STREAM]}"
        )
        if self.file_count[TaskType.ACCURATE] > 0:
            jobmessage(
                bareosfd.M_INFO, f"File count : {self.file_count[TaskType.ACCURATE]}"
            )

        return bareosfd.bRC_OK

    def check_file(self, fname):
        """
        Bareos will call this after backing up all file data during an accurate mode backup
        to check if a file will be marked as deleted.
        """
        # All existing files/objects are passed to bareos
        # If bareos has not seen one, it does not exist anymore
        debugmessage(
            100,
            f"check_file(): accurate mode, marking {fname} as deleted",
        )
        return bareosfd.bRC_Error

    def _shutdown(self):
        """
        Calls the shutdown of Libcloud API if it was initialized.
        If so, wait for all worker threads to finish processing the task queues.
        """
        self.active = False
        jobmessage(
            bareosfd.M_INFO,
            f"BareosFdPluginLibcloud finished with {self.backed_up_objects_count} "
            f"files",
        )

        if self.api is None:
            jobmessage(bareosfd.M_INFO, "BareosLibcloudApi did not initialize properly")
        else:
            jobmessage(bareosfd.M_INFO, "Shut down BareosLibcloudApi")
            self.api.shutdown()
            jobmessage(bareosfd.M_INFO, "BareosLibcloudApi is shut down")

    def start_backup_file(self, savepkt):
        """
        Start backing up a single file/object.
        """
        debugmessage(110, "start_backup_file() was called")
        error = False
        error_message = "Shutdown after worker error"
        while self.active:
            debugmessage(120, "start_backup_file(): active, checking worker messages")
            worker_result = self.api.check_worker_messages()
            debugmessage(
                120, f"start_backup_file(): worker message result: {worker_result}"
            )
            # self.api.check_worker_messages() returns either WorkerState.ABORT
            # or WorkerState.SUCCESS
            if worker_result is WorkerState.ABORT:
                self.active = False
                error = True
            else:
                self.current_backup_task = self.api.get_next_task()
                if self.current_backup_task is not None:
                    self.current_backup_task["skip_file"] = False
                    self.current_backup_task["stream_length"] = 0
                    break
                if self.api.worker_ready():
                    # all workers finished
                    self.active = False
                else:
                    # we only get here when there was no task, check for global timeout,
                    # if there were no worker messages within the defined timeout, assuming
                    # that all workers are unresponsive, so we terminate to prevent from
                    # an infinite loop here.
                    if (
                        int(time.time()) - self.api.last_worker_message_time
                        > self.global_timeout
                    ):
                        self.active = False
                        error = True
                        error_message = (
                            f"No worker message received since {self.global_timeout} seconds, "
                            f"terminating"
                        )
                    else:
                        time.sleep(0.1)

        if not self.active:
            debugmessage(110, "start_backup_file(): not active, calling _shutdown()")
            self._shutdown()
            savepkt.fname = ""  # dummy value
            if error:
                jobmessage(bareosfd.M_FATAL, error_message)
                return bareosfd.bRC_Cancel

            return bareosfd.bRC_Stop

        filename = FilenameConverter.bucket_to_backup(
            f"{self.current_backup_task['bucket']}/{self.current_backup_task['name']}"
        )
        debugmessage(100, f"Backup file: {filename}")

        statp = bareosfd.StatPacket()

        statp.st_size = self.current_backup_task["size"]
        statp.st_mtime = self.current_backup_task["mtime"]
        statp.st_atime = 0
        statp.st_ctime = 0

        savepkt.statp = statp
        savepkt.fname = StringCodec.encode_for_backup(filename)
        savepkt.type = bareosfd.FT_REG

        if self.current_backup_task["type"] == TaskType.DOWNLOADED:
            self.file_count[TaskType.DOWNLOADED] += 1
            self.file = self.current_backup_task["data"]
        elif self.current_backup_task["type"] == TaskType.TEMP_FILE:
            self.file_count[TaskType.TEMP_FILE] += 1
            try:
                self.file = io.open(self.current_backup_task["tmpfile"], "rb")
            except Exception as err:
                jobmessage(
                    bareosfd.M_FATAL,
                    f"Could not open temporary file "
                    f"{self.current_backup_task['tmpfile']} for reading: {err}",
                )
                self._shutdown()
                return bareosfd.bRC_Error
        elif self.current_backup_task["type"] == TaskType.STREAM:
            self.file_count[TaskType.STREAM] += 1
            try:
                self.file = self.api.start_stream_pipe(self.current_backup_task["data"])

            except ObjectDoesNotExistError as e:
                if self.effective_options["fail_on_download_error"]:
                    jobmessage(
                        bareosfd.M_FATAL,
                        f"File {self.current_backup_task['name']} does not exist anymore "
                        f"({str(e)})",
                    )
                    return bareosfd.bRC_Error

            except Exception as e:
                if self.effective_options["fail_on_download_error"]:
                    jobmessage(
                        bareosfd.M_FATAL,
                        f"Unexpected error for file {self.current_backup_task['name']}: {e}",
                    )
                    return bareosfd.bRC_Error

                jobmessage(
                    bareosfd.M_ERROR,
                    f"Skipped file {self.current_backup_task['name']} because it does not "
                    f"exist anymore \n({str(e)})",
                )
                self.current_backup_task["skip_file"] = True
                return bareosfd.bRC_OK
        elif self.current_backup_task["type"] == TaskType.ACCURATE:
            self.file_count[TaskType.ACCURATE] += 1
            self.file = None
        else:
            raise Exception(value='Wrong argument for current_backup_task["type"]')

        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """
        Create a file during restore operation.
        """
        debugmessage(
            100, f"create_file() entry point in Python called with {restorepkt}\n"
        )
        file_name = FilenameConverter.backup_to_bucket(restorepkt.ofname)
        dirname = StringCodec.encode_for_restore(os.path.dirname(file_name))
        if not os.path.exists(dirname):
            jobmessage(
                bareosfd.M_INFO, f"Directory {dirname} does not exist, creating it\n"
            )
            os.makedirs(dirname)
        if restorepkt.type == bareosfd.FT_REG:
            restorepkt.create_status = bareosfd.CF_EXTRACT
        return bareosfd.bRC_OK

    def plugin_io(self, IOP):
        """
        Handle IO operations for backup and restore.
        """
        if self.current_backup_task is None:
            return bareosfd.bRC_Error

        if IOP.func == bareosfd.IO_OPEN:
            # Only used by the 'restore' path
            if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                self.file = open(
                    StringCodec.encode_for_restore(
                        FilenameConverter.backup_to_bucket(IOP.fname)
                    ),
                    "wb",
                )

            # always use io in core if possible
            if self.io_in_core:
                try:
                    file_descriptor = self.file.fileno()
                    IOP.filedes = file_descriptor
                    IOP.status = bareosfd.iostat_do_in_core
                    debugmessage(100, "plugin_io() IO_OPEN: doing IO in core\n")
                except Exception:
                    IOP.status = bareosfd.iostat_do_in_plugin
                    debugmessage(100, "plugin_io() IO_OPEN: doing IO in plugin\n")

            return bareosfd.bRC_OK

        if IOP.func == bareosfd.IO_READ:
            IOP.buf = bytearray(IOP.count)
            IOP.io_errno = 0
            if self.file is None:
                return bareosfd.bRC_Error
            try:
                if self.current_backup_task["type"] == TaskType.STREAM:
                    if self.current_backup_task["skip_file"]:
                        return bareosfd.bRC_Skip

                IOP.status = self.file.readinto(IOP.buf)
                if self.current_backup_task["type"] == TaskType.STREAM:
                    # self.current_backup_task["stream_length"] += len(buf)
                    self.current_backup_task["stream_length"] += IOP.status
                return bareosfd.bRC_OK
            except IOError as e:
                jobmessage(
                    bareosfd.M_ERROR,
                    f"Cannot read from {self.current_backup_task['bucket']}/"
                    f"{self.current_backup_task['name']}: {e}",
                )
                IOP.status = 0
                if self.effective_options["fail_on_download_error"]:
                    return bareosfd.bRC_Error

                return bareosfd.bRC_Skip

        elif IOP.func == bareosfd.IO_WRITE:
            # This is the restore case
            try:
                self.file.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as msg:
                IOP.io_errno = -1
                jobmessage(bareosfd.M_ERROR, f"Failed to write data: {msg}")
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.IO_CLOSE:
            if self.file:
                self.file.close()
            if "type" in self.current_backup_task:
                if self.current_backup_task["type"] == TaskType.TEMP_FILE:
                    debugmessage(
                        110,
                        f"Removing temporary file: {self.current_backup_task['tmpfile']}",
                    )
                    try:
                        os.remove(self.current_backup_task["tmpfile"])
                    except Exception as exc:
                        debugmessage(
                            110,
                            f"Could not remove temporary file: "
                            f"{self.current_backup_task['tmpfile']} ({exc})",
                        )
                elif self.current_backup_task["type"] == TaskType.STREAM:
                    self.file.close()
                    self.api.end_stream_pipe()
                    self.file = None

            return bareosfd.bRC_OK

        return bareosfd.bRC_OK

    def end_backup_file(self):
        """
        Finalize backup of a single file/object.
        """
        debugmessage(110, "end_backup_file() was called")
        if self.current_backup_task is not None:
            self.backed_up_objects_count += 1
            if self.job_message_after_each_number_of_objects != 0:
                if (
                    self.backed_up_objects_count
                    % self.job_message_after_each_number_of_objects
                    == 0
                ):
                    jobmessage(
                        bareosfd.M_INFO,
                        f"Number of backed-up objects: {self.backed_up_objects_count}",
                    )
            debugmessage(110, "end_backup_file() returing bareosfd.bRC_More")
            return bareosfd.bRC_More

        debugmessage(110, "end_backup_file() returing bareosfd.bRC_OK")
        return bareosfd.bRC_OK
