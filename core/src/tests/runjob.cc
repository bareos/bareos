/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#include "dird/ua.h"
#include "include/jcr.h"
#include "dird/ua_run.h"

void FakeCmd(directordaemon::UaContext* ua, std::string cmd)
{
  std::string command = cmd;
  PmStrcpy(ua->cmd, command.c_str());
  ParseArgs(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
}

class RerunArgumentsParsing : public testing::Test {
 protected:
  void SetUp() override { ua = directordaemon::new_ua_context(&jcr); }

  void TearDown() override { FreeUaContext(ua); }

  void FakeRerunCommand(std::string arguments)
  {
    FakeCmd(ua, "rerun " + arguments);
  }

  JobControlRecord jcr{};
  directordaemon::UaContext* ua{nullptr};
};

TEST_F(RerunArgumentsParsing, NothingHappensWhenNoArgumentSpecified)
{
  FakeRerunCommand("");
  EXPECT_TRUE(GetRerunCmdlineArguments(ua).parsingerror);
}

TEST_F(RerunArgumentsParsing, SingleJobId)
{
  FakeRerunCommand("jobid=1");
  std::vector<JobId_t> expected_jobids{1};
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.JobIds, expected_jobids);
}

TEST_F(RerunArgumentsParsing, WrongJobId)
{
  FakeRerunCommand("jobid=jsahd");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_TRUE(rereunargs.parsingerror);
}

TEST_F(RerunArgumentsParsing, MultipleJobids)
{
  FakeRerunCommand("jobid=1,2,3,5,1342134");
  std::vector<JobId_t> expected_jobids{1, 2, 3, 5, 1342134};
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.JobIds, expected_jobids);
}

TEST_F(RerunArgumentsParsing, MultipleJobidsWithOneWrongId)
{
  FakeRerunCommand("jobid=1,2,3,5,aaaa");
  std::vector<JobId_t> expected_jobids{};
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_TRUE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.JobIds, expected_jobids);
}

TEST_F(RerunArgumentsParsing, Days)
{
  FakeRerunCommand("Days=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.days, 12);
}

TEST_F(RerunArgumentsParsing, Hours)
{
  FakeRerunCommand("hours=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.hours, 12);
}

TEST_F(RerunArgumentsParsing, Since_jobid)
{
  FakeRerunCommand("since_jobid=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.since_jobid, 12);
}

TEST_F(RerunArgumentsParsing, Until_jobid)
{
  FakeRerunCommand("until_jobid=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_TRUE(rereunargs.parsingerror);
}

TEST_F(RerunArgumentsParsing, Since_jobid_Until_jobid)
{
  FakeRerunCommand("since_jobid=1 until_jobid=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.since_jobid, 1);
  EXPECT_EQ(rereunargs.until_jobid, 12);
}

TEST_F(RerunArgumentsParsing, Days_hours)
{
  FakeRerunCommand("days=1 hours=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_FALSE(rereunargs.parsingerror);
  EXPECT_EQ(rereunargs.days, 1);
  EXPECT_EQ(rereunargs.hours, 12);
}

TEST_F(RerunArgumentsParsing, Jobid_Days_hours)
{
  FakeRerunCommand("jobid=1 days=1 hours=12");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_TRUE(rereunargs.parsingerror);
}

TEST_F(RerunArgumentsParsing, Jobid_Since)
{
  FakeRerunCommand("jobid=1 since_jobid=1");
  directordaemon::RerunArguments rereunargs = GetRerunCmdlineArguments(ua);

  EXPECT_TRUE(rereunargs.parsingerror);
}
