#!/usr/bin/env python
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

import libcloud.storage.types
import libcloud.storage.providers
import libcloud.common.types
import threading

options = {
    "secret": "minioadmin",
    "key": "minioadmin",
    "host": "127.0.0.1",
    "port": 9000,
    "secure": False,
    "buckets_include": "core",
}


def get_driver(driver_options):
    """
    Get and return a libcloud storage driver based on the provided options.
    Note that exception handling is the responsibility of the caller.
    """
    # copy options to avoid modifying the original
    driver_opt = driver_options.copy()

    provider_opt = driver_opt.get("provider", "S3")

    # remove unknown options
    for opt in (
        "buckets_exclude",
        "accurate",
        "nb_worker",
        "prefetch_size",
        "queue_size",
        "provider",
        "buckets_include",
        "debug",
    ):
        if opt in driver_opt:
            del driver_opt[opt]

    # driver = None
    # Using thread local variable here because as libcloud docs recommend,
    # unfortunately this doesn't help, when libcloud calls throw exceptions
    # threads start to become unresponsive, because the next libcloud call
    # does not time out and throw and exception as expected, instead the
    # thread gets stuck on that call.
    tl = threading.local()
    tl.driver = None

    # FIXME: remove again or make it configurable
    driver_opt["timeout"] = 30

    tl.provider = getattr(libcloud.storage.types.Provider, provider_opt)
    tl.driver = libcloud.storage.providers.get_driver(tl.provider)(**driver_opt)

    # This could seem useless, but it is not. Some drivers (like S3)
    # do not actually connect until the first operation is performed.
    # This will raise an exception if the connection cannot be made.
    tl.driver.iterate_containers()
    return tl.driver


if __name__ == "__main__":
    try:
        storage_driver = get_driver(options)
    except libcloud.common.types.InvalidCredsError:
        print("Invalid credentials")
        exit(1)
    except Exception as e:
        print(f"Failed to get driver: {e} ({type(e).__name__})")
        exit(1)

    print("Success")
