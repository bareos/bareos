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

# TODO: needs rework


class BareosUrls(object):
    def __init__(self):
        self.download_url = os.environ.get(
            "DOWNLOADSERVER_URL", "https://download.bareos.org/current/"
        )

    def get_download_bareos_org_url(self, tail=""):
        return self.download_url + tail

    def get_download_bareos_com_url(self, tail="", force=False):
        if force:
            return self.download_url + tail
        else:
            return None
