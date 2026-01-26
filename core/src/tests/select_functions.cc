/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2026 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include <unordered_map>

#include "dird/ua_select.h"
#include "dird/ua.h"
#include "include/jcr.h"
#include "dird/dird_conf.h"
#include "include/job_types.h"

void FakeCmd(directordaemon::UaContext* ua, std::string cmd)
{
  std::string command = cmd;
  PmStrcpy(ua->cmd, command.c_str());
  ParseArgs(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
}

struct directordaemon::s_jt allowed_jobtypes[]
    = {{"Backup", JT_BACKUP},
       {"Admin", JT_ADMIN},
       {"Archive", JT_ARCHIVE},
       {"Verify", JT_VERIFY},
       {"Restore", JT_RESTORE},
       {"Migrate", JT_MIGRATE},
       {"Copy", JT_COPY},
       {"Consolidate", JT_CONSOLIDATE},
       {NULL, 0}};


std::vector<char> notpermitted_jobtypes{JT_SCAN, JT_JOB_COPY, JT_MIGRATED_JOB,
                                        JT_CONSOLE, JT_SYSTEM};

class JobTypeSelection : public testing::Test {
 protected:
  void SetUp() override { ua = directordaemon::new_ua_context(&jcr); }

  void TearDown() override { FreeUaContext(ua); }
  void FakeListCommand(directordaemon::UaContext* t_ua, std::string arguments)
  {
    FakeCmd(t_ua, "list jobs " + arguments);
  }
  void FakeListJobTypeCommand(std::string argument_value)
  {
    FakeCmd(ua, "list jobs jobtype=" + argument_value);
  }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua{nullptr};
};

TEST_F(JobTypeSelection, NothingHappensWhenJobtypeNotSpecified)
{
  std::vector<char> jobtypelist{};
  FakeListCommand(ua, "");
  EXPECT_TRUE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_TRUE(jobtypelist.empty());
}

TEST_F(JobTypeSelection, ErrorWhenJobtypeArgumentSpecifiedButNoneGiven)
{
  std::vector<char> jobtypelist{};
  FakeListJobTypeCommand("");
  EXPECT_FALSE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_TRUE(jobtypelist.empty());
}

TEST_F(JobTypeSelection, SinglePermittedJobtypeNameIsCorrectlyParsed)
{
  std::vector<char> jobtypelist{};
  std::string argument{"Backup"};

  FakeListJobTypeCommand(argument);
  EXPECT_TRUE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_EQ(jobtypelist[0], 'B');
}

TEST_F(JobTypeSelection, SinglePermittedJobtypeIsCorrectlyParsed)
{
  std::vector<char> jobtypelist{};
  std::string argument{"B"};

  FakeListJobTypeCommand(argument);
  EXPECT_TRUE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_EQ(jobtypelist[0], 'B');
}

TEST_F(JobTypeSelection, PermittedJobtypesFullNamesAreCorrectlyParsed)
{
  std::vector<char> jobtypelist{};
  std::vector<char> expected_types{};
  std::string argument{};
  for (int i = 0; allowed_jobtypes[i].name; i++) {
    auto type_name = allowed_jobtypes[i].name;
    argument += type_name;
    argument += ',';

    expected_types.push_back(allowed_jobtypes[i].job_type);
  }
  argument.pop_back();

  FakeListJobTypeCommand(argument);
  EXPECT_TRUE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_EQ(jobtypelist, expected_types);
}

TEST_F(JobTypeSelection, PermittedShortJobtypesAreCorrectlyParsed)
{
  std::vector<char> jobtypelist{};
  std::vector<char> expected_types{};
  std::string argument{};
  for (int i = 0; allowed_jobtypes[i].name; i++) {
    auto jobtype = allowed_jobtypes[i].job_type;
    argument += jobtype;
    argument += ',';

    expected_types.push_back(allowed_jobtypes[i].job_type);
  }
  argument.pop_back();

  FakeListJobTypeCommand(argument);
  EXPECT_TRUE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_EQ(jobtypelist, expected_types);
}

TEST_F(JobTypeSelection,
       PermittedMixedShortJobtypesAndFullNamesAreCorrectlyParsed)
{
  std::vector<char> jobtypelist{};
  std::vector<char> expected_types{};
  std::string argument{};
  for (int i = 0; allowed_jobtypes[i].name; i++) {
    auto jobtype = allowed_jobtypes[i];

    if (i % 2 == 0) {
      argument += jobtype.name;
    } else {
      argument += jobtype.job_type;
    }
    argument += ',';

    expected_types.push_back(allowed_jobtypes[i].job_type);
  }
  argument.pop_back();

  FakeListJobTypeCommand(argument);
  EXPECT_TRUE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_EQ(jobtypelist, expected_types);
}

TEST_F(JobTypeSelection, NonPermittedJobtypesAreNotParsed)
{
  std::vector<char> jobtypelist{};
  std::string argument{};
  for (auto type : notpermitted_jobtypes) {
    argument += type;
    argument += ',';
  }
  argument.pop_back();

  FakeListJobTypeCommand(argument);
  EXPECT_FALSE(GetUserJobTypeListSelection(ua, jobtypelist, false));
  EXPECT_TRUE(jobtypelist.empty());
}


// Unit Tests for Job status argument


class JobStatusSelection : public testing::Test {
 protected:
  void SetUp() override { ua = directordaemon::new_ua_context(&jcr); }

  void TearDown() override { FreeUaContext(ua); }
  void FakeListCommand(directordaemon::UaContext* t_ua, std::string arguments)
  {
    FakeCmd(t_ua, "list jobs " + arguments);
  }
  void FakeListJobStatusCommand(std::string argument_value)
  {
    FakeCmd(ua, "list jobs jobstatus=" + argument_value);
  }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua{nullptr};
  std::unordered_map<char, std::string> allowed_jobstatuses{
      {JS_Terminated, "terminated"}, {JS_Warnings, "warnings"},
      {JS_Canceled, "canceled"},     {JS_Running, "running"},
      {JS_ErrorTerminated, "error"}, {JS_FatalError, "fatal"}};
};


TEST_F(JobStatusSelection, NothingHappensWhenJobstatusNotSpecified)
{
  std::vector<char> jobstatuslist{};
  FakeListCommand(ua, "");
  EXPECT_TRUE(GetUserJobStatusSelection(ua, jobstatuslist));
  EXPECT_TRUE(jobstatuslist.empty());
}

TEST_F(JobStatusSelection, ErrorWhenJobtatusArgumentSpecifiedButNoneGiven)
{
  std::vector<char> jobstatuslist{};
  FakeListJobStatusCommand("");
  EXPECT_FALSE(GetUserJobStatusSelection(ua, jobstatuslist));
  EXPECT_TRUE(jobstatuslist.empty());
}

TEST_F(JobStatusSelection, ReturnOnlyOneJobStatusIfOnlyOneIsEntered)
{
  for (const auto& jobstatus : allowed_jobstatuses) {
    std::vector<char> jobstatuslist{};
    std::string argument{jobstatus.first};
    FakeListJobStatusCommand(argument);
    EXPECT_TRUE(GetUserJobStatusSelection(ua, jobstatuslist));
    EXPECT_EQ(jobstatuslist[0], jobstatus.first);
  }
}

TEST_F(JobStatusSelection,
       ReturnOnlyOneShortJobStatusIfOnlyOneLongJobStatusIsEntered)
{
  for (const auto& jobstatus : allowed_jobstatuses) {
    std::vector<char> jobstatuslist{};
    std::string argument{jobstatus.second};
    FakeListJobStatusCommand(argument);
    EXPECT_TRUE(GetUserJobStatusSelection(ua, jobstatuslist));
    EXPECT_EQ(jobstatuslist[0], jobstatus.first);
  }
}

TEST_F(JobStatusSelection,
       ReturnMultipleShortJobStatusIfMultipleLongJobStatusesEntered)
{
  std::vector<char> jobstatuslist{};
  std::vector<char> expectedJobStatusList{};
  std::string argumentForMultipleLongJobstatus;
  for (const auto& jobstatus : allowed_jobstatuses) {
    argumentForMultipleLongJobstatus += jobstatus.second;
    argumentForMultipleLongJobstatus += ',';

    expectedJobStatusList.push_back(jobstatus.first);
  }
  argumentForMultipleLongJobstatus.pop_back();

  FakeListJobStatusCommand(argumentForMultipleLongJobstatus);
  EXPECT_TRUE(GetUserJobStatusSelection(ua, jobstatuslist));
  EXPECT_EQ(jobstatuslist, expectedJobStatusList);
}

TEST_F(JobStatusSelection,
       ReturnMultipleShortJobStatusIfMultipleLongAndShortJobstatusesEntered)
{
  std::vector<char> jobstatuslist{};
  std::vector<char> expectedJobStatusList{};
  std::string argumentForMultipleLongAndShortJobstatus;
  int i = 0;
  for (const auto& jobstatus : allowed_jobstatuses) {
    if (i % 2 == 0) {
      argumentForMultipleLongAndShortJobstatus
          += jobstatus.first;  // short jobstatus
    } else {
      argumentForMultipleLongAndShortJobstatus
          += jobstatus.second;  // long jobstatus
    }
    argumentForMultipleLongAndShortJobstatus += ',';

    expectedJobStatusList.push_back(jobstatus.first);
    i++;
  }
  argumentForMultipleLongAndShortJobstatus.pop_back();

  FakeListJobStatusCommand(argumentForMultipleLongAndShortJobstatus);
  EXPECT_TRUE(GetUserJobStatusSelection(ua, jobstatuslist));
  EXPECT_EQ(jobstatuslist, expectedJobStatusList);
}
