#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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

include(CheckStructHasMember)

check_struct_has_member(
  "struct stat" st_blksize sys/stat.h HAVE_STRUCT_STAT_ST_BLKSIZE
)
set(HAVE_ST_BLKSIZE ${HAVE_STRUCT_STAT_ST_BLKSIZE})

check_struct_has_member(
  "struct stat" st_blocks sys/stat.h HAVE_STRUCT_STAT_ST_BLOCKS
)
set(HAVE_ST_BLOCKS ${HAVE_STRUCT_STAT_ST_BLOCKS})

check_struct_has_member(
  "struct stat" st_rdev sys/stat.h HAVE_STRUCT_STAT_ST_RDEV
)
set(HAVE_ST_RDEV ${HAVE_STRUCT_STAT_ST_RDEV})

check_struct_has_member("struct tm" tm_zone time.h HAVE_STRUCT_TM_TM_ZONE)
set(HAVE_TM_ZONE ${HAVE_STRUCT_TM_TM_ZONE})
