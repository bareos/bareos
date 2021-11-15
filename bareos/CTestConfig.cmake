#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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

# This file should be placed in the root directory of your project. Then modify
# the CMakeLists.txt file in the root directory of your project to incorporate
# the testing dashboard.
#
# # The following are required to submit to the CDash dashboard:
# ENABLE_TESTING() INCLUDE(CTest)

set(CTEST_PROJECT_NAME "Bareos")
set(CTEST_NIGHTLY_START_TIME "23:00:00 CET")

set(CTEST_DROP_METHOD "https")
set(CTEST_DROP_SITE "cdash.bareos.org")
set(CTEST_DROP_LOCATION "/submit.php?project=Bareos")
set(CTEST_DROP_SITE_CDASH TRUE)
