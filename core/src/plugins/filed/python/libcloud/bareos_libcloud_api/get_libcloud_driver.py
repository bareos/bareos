#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

import ctypes
import libcloud
from multiprocessing.sharedctypes import RawArray

options = {
    "secret": "minioadmin",
    "key": "minioadmin",
    "host": "127.0.0.1",
    "port": 9000,
    "secure": False,
    "buckets_include": "core",
}


def get_driver(options):
    driver_opt = dict(options)

    try:
        provider_opt = driver_opt.get("provider")
    except KeyError:
        provider_opt = "S3"

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
        if opt in options:
            del driver_opt[opt]

    driver = None

    provider = getattr(libcloud.storage.types.Provider, provider_opt)
    driver = libcloud.storage.providers.get_driver(provider)(**driver_opt)

    try:
        driver.get_container("bareos-libcloud-invalidname-bareos-libcloud")
        return driver  # success if bucket accidentally matches

    # success
    except libcloud.storage.types.ContainerDoesNotExistError:
        return driver

    # error
    except libcloud.common.types.InvalidCredsError:
        print("Wrong credentials")
        pass

    # error
    except:
        print("Error connecting driver")
        pass

    print(
        "Could not connect to libcloud driver: %s:%s"
        % (driver_opt["host"], driver_opt["port"])
    )

    return None


if __name__ == "__main__":
    driver = get_driver(options)

    if driver == None:
        exit(1)

    print("Success")
