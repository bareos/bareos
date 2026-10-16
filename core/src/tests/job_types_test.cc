/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "gtest/gtest.h"

#include "include/job_types.h"

// The "rerun" command can only reschedule jobs whose catalog record holds
// everything needed to build a "run" command again.
static_assert(IsRerunableJobType(JT_BACKUP));
static_assert(IsRerunableJobType(JT_COPY));
static_assert(IsRerunableJobType(JT_MIGRATE));

// Every other job type is rejected by reRunJob().
static_assert(!IsRerunableJobType(JT_MIGRATED_JOB));
static_assert(!IsRerunableJobType(JT_VERIFY));
static_assert(!IsRerunableJobType(JT_RESTORE));
static_assert(!IsRerunableJobType(JT_CONSOLE));
static_assert(!IsRerunableJobType(JT_SYSTEM));
static_assert(!IsRerunableJobType(JT_ADMIN));
static_assert(!IsRerunableJobType(JT_ARCHIVE));
static_assert(!IsRerunableJobType(JT_JOB_COPY));
static_assert(!IsRerunableJobType(JT_SCAN));
static_assert(!IsRerunableJobType(JT_CONSOLIDATE));

// A job type that is not part of the enumeration must not be rerunable
// either, so an unknown catalog value cannot slip through.
static_assert(!IsRerunableJobType(static_cast<JobTypes>('\0')));
static_assert(!IsRerunableJobType(static_cast<JobTypes>('X')));

TEST(JobTypes, only_backup_copy_and_migrate_are_rerunable)
{
  for (int type = 0; type < 128; ++type) {
    const bool expected
        = type == JT_BACKUP || type == JT_COPY || type == JT_MIGRATE;
    EXPECT_EQ(IsRerunableJobType(static_cast<JobTypes>(type)), expected)
        << "unexpected rerun support for job type " << type;
  }
}
