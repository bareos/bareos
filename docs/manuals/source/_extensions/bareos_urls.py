#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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

import os


class BareosUrls(object):
    def __init__(self):
        release_variant = os.environ.get("RELEASE_VARIANT", "prerelease")
        branch_name = os.environ.get("BRANCH_NAME", "")
        publish_repo_path = os.environ.get("PUBLISH_REPO_PATH", "experimental/nightly")

        self.download_bareos_org_url = "https://download.bareos.org/bareos/{}/".format(
            publish_repo_path
        )
        self.download_bareos_com_url = "https://download.bareos.com/bareos/{}/".format(
            publish_repo_path
        )

        # if release_variant == 'subscription':
        # # org + com
        # elif release_variant == 'community':
        # # org
        # else:
        # # env.RELEASE_VARIANT == 'prerelease'
        # if branch_name.startswith("bareos-"):
        # # org + com
        # else:
        # # org

        self.download_bareos_com = False
        if release_variant == "subscription" or branch_name.startswith("bareos-"):
            self.download_bareos_com = True

    def get_download_bareos_org_url(self, tail=""):
        return self.download_bareos_org_url + tail

    def get_download_bareos_com_url(self, tail="", force=False):
        if force or self.download_bareos_com:
            return self.download_bareos_com_url + tail
        else:
            return None
