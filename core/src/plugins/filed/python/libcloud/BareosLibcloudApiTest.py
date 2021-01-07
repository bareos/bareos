#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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

from BareosLibcloudApi import BareosLibcloudApi
from BareosLibcloudApi import SUCCESS
from BareosLibcloudApi import ERROR
from BareosLibcloudApi import ABORT
from bareos_libcloud_api.get_libcloud_driver import options as driver_options
import datetime
import dateutil.parser
import sys
from time import sleep
from bareos_libcloud_api.bucket_explorer import TASK_TYPE


def create_options():
    options = driver_options
    options["nb_worker"] = 10
    options["queue_size"] = 100
    options["keep_stdout"] = True
    options["fail_on_download_error"] = False
    options["job_message_after_each_number_of_objects"] = 100
    options["buckets_include"] = "bareos-test"
    options["prefetch_size"] = 1024 * 1024
    return options


def run(api: BareosLibcloudApi):
    counter = 0
    active = True
    while active:
        worker_result = api.check_worker_messages()
        if worker_result == ERROR:
            if options["fail_on_download_error"]:
                active = False
                error = True
        elif worker_result == ABORT:
            active = False
            error = True
        else:
            current_backup_task = api.get_next_task()
            if current_backup_task != None:
                current_backup_task["skip_file"] = False
                current_backup_task["stream_length"] = 0
                counter += 1

                print(
                    "Current File: %s/%s (%s)"
                    % (
                        current_backup_task["bucket"],
                        current_backup_task["name"],
                        TASK_TYPE.to_str(current_backup_task["type"]),
                    )
                )

            elif api.worker_ready():
                active = False
            else:
                sleep(0.01)
    return counter


if __name__ == "__main__":
    options = create_options()
    tmp_dir_path = "/tmp"

    last_run = datetime.datetime.fromtimestamp(0)
    last_run = last_run.replace(tzinfo=None)

    if BareosLibcloudApi.probe_driver(options) == "failed":
        exit(1)

    api = BareosLibcloudApi(options, last_run, tmp_dir_path)
    number_of_files = run(api)

    api.shutdown()
    print("Handled: %d files" % number_of_files)

    exit(0)
