#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2024 Bareos GmbH & Co. KG
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

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
if(TARGET droplet
   AND S3CMD
   AND MINIO
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()

# set_tests_properties( system:bareos-basic:simple-backup-and-restore PROPERTIES
# FIXTURES_SETUP "system:bareos-basic:backup-job-fixture" )
set_tests_properties(
  system:droplet-s3:backup
  PROPERTIES FIXTURES_REQUIRED
             "system:droplet-s3-fixture;system:droplet-s3:minio-fixture"
)
set_tests_properties(
  system:droplet-s3:restore
  PROPERTIES
    FIXTURES_REQUIRED
    "system:droplet-s3-fixture;system:droplet-s3:minio-fixture;system:droplet-s3/backup-fixture"
)
set_tests_properties(
  system:droplet-s3:start-minio PROPERTIES FIXTURES_SETUP
                                           system:droplet-s3:minio-fixture
)
set_tests_properties(
  system:droplet-s3:stop-minio PROPERTIES FIXTURES_CLEANUP
                                          system:droplet-s3:minio-fixture
)
