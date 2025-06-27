/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2025 Bareos GmbH & Co. KG

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
#include <algorithm>
#include "dird/reload.h"
#include "lib/runscript.h"
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#  include "include/bareos.h"
#endif

#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "lib/output_formatter_resource.h"
#include "testing_dir_common.h"

#include <thread>
#include <condition_variable>
#include <chrono>
#include <memory>
using namespace std::chrono_literals;

namespace directordaemon {

static std::string sprintoutput{};

bool sprintit(void*, const char* fmt, ...)
{
  va_list arg_ptr;
  PoolMem msg;

  va_start(arg_ptr, fmt);
  msg.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  sprintoutput += msg.c_str();
  return true;
}

class ConfigParser_Dir : public ::testing::Test {
  void SetUp() override { InitDirGlobals(); }
};

TEST_F(ConfigParser_Dir, ParseSchedulerOddEvenDaysCorrectly)
{
  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests");
  std::unique_ptr<ConfigurationParser> dir_conf{
      InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM)};
  my_config = dir_conf.get();
  my_config->ParseConfig();

  ScheduleResource* even_schedule
      = (directordaemon::ScheduleResource*)my_config->GetResWithName(
          R_SCHEDULE, "Even Weeks");

  ScheduleResource* odd_schedule
      = (directordaemon::ScheduleResource*)my_config->GetResWithName(
          R_SCHEDULE, "Odd Weeks");

  OutputFormatter output_formatter
      = OutputFormatter(sprintit, nullptr, nullptr, nullptr);
  OutputFormatterResource send = OutputFormatterResource(&output_formatter);

  even_schedule->PrintConfig(send, *my_config);
  odd_schedule->PrintConfig(send, *my_config);


  std::string expected_output{
      "Schedule {\n"
      "  Name = \"Even Weeks\"\n "
      " Run = w00/w02 Fri at 23:10\n"
      "}\n\n"
      "Schedule {\n"
      "  Name = \"Odd Weeks\"\n"
      "  Run = w01/w02 Sun at 23:10\n"
      "}\n\n"};


  EXPECT_EQ(sprintoutput, expected_output);
}

TEST_F(ConfigParser_Dir, bareos_configparser_tests)
{
  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests");
  std::unique_ptr<ConfigurationParser> dir_conf{
      InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM)};
  my_config = dir_conf.get();
  my_config->ParseConfig();
  my_config->DumpResources(PrintMessage, NULL);

  {
    auto backup = my_config->BackupResourcesContainer();
    my_config->ParseConfig();

    me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
    my_config->own_resource_ = me;
    ASSERT_NE(nullptr, me);
    my_config->RestoreResourcesContainer(std::move(backup));
    ASSERT_NE(nullptr, me);
  }
  // If a config already exists, BackupResourcesContainer() needs to be called
  // before ParseConfig(), otherwise memory of the existing config is not freed
  // completely
  {
    auto backup = my_config->BackupResourcesContainer();
    my_config->ParseConfig();

    me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
    my_config->own_resource_ = me;
    assert(me);

    ASSERT_NE(nullptr, me);
    my_config->DumpResources(PrintMessage, NULL);
    ASSERT_NE(nullptr, me);
  }
}

TEST_F(ConfigParser_Dir, foreach_res_and_reload)
{
  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests");
  std::unique_ptr<ConfigurationParser> dir_conf{
      InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM)};
  my_config = dir_conf.get();
  my_config->ParseConfig();

  [[maybe_unused]] auto do_reload = [](bool keep_config) {
    auto backup = my_config->BackupResourcesContainer();
    my_config->ParseConfig();

    me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
    my_config->own_resource_ = me;
    ASSERT_NE(nullptr, me);
    if (!keep_config) {
      my_config->RestoreResourcesContainer(std::move(backup));
      ASSERT_NE(nullptr, me);
    }
  };

  std::mutex cv_m;
  std::condition_variable cv;
  bool done = false;

  auto do_foreach_res = [&]() {
    std::unique_lock lk(cv_m);
    BareosResource* client;
    client = (BareosResource*)0xfefe;
    // auto handle = my_config->GetResourcesContainer();
    foreach_res (client, R_CLIENT) {
      cv.wait(lk);
      printf("%p %s\n", client->resource_name_, client->resource_name_);
    }
    done = true;
    return;
  };

  auto reload_loop = [&]() {
    std::unique_lock<std::mutex> lk(cv_m);
    while (!done) {
      lk.unlock();
      printf("Config reload\n");
      do_reload(true);
      cv.notify_one();
      std::this_thread::sleep_for(10ms);
      lk.lock();
    }
  };

  std::thread t1{do_foreach_res}, t2{reload_loop};
  t1.join();
  t2.join();
}  // namespace directordaemon

TEST_F(ConfigParser_Dir, runscript_test)
{
  std::string path_to_config_file
      = std::string("configs/runscript-tests/bareos-dir.conf");
  std::unique_ptr<ConfigurationParser> dir_conf{
      InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM)};
  my_config = dir_conf.get();
  my_config->ParseConfig();

  my_config->DumpResources(PrintMessage, NULL);
}

void test_config_directive_type(
    std::function<void(DirectorResource*)> test_func)
{
  std::string test_name = std::string(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());

  OSDependentInit();

#if HAVE_WIN32
  WSA_Init();
#endif

  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests/bareos-dir-") + test_name
        + std::string(".conf");
  std::unique_ptr<ConfigurationParser> dir_conf{
      InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM)};
  my_config = dir_conf.get();
  my_config->ParseConfig();

  my_config->DumpResources(PrintMessage, NULL);

  const char* dir_resource_name = "bareos-dir";

  DirectorResource* my_res
      = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, NULL);
  EXPECT_STREQ(dir_resource_name, my_res->resource_name_);

  test_func(my_res);
}


void test_CFG_TYPE_AUDIT(DirectorResource* res)
{
  ASSERT_TRUE(res->audit_events);
  for (auto* val : res->audit_events) { printf("AuditEvents = %s\n", val); }
  EXPECT_EQ(res->audit_events->size(), 8);
}

TEST_F(ConfigParser_Dir, CFG_TYPE_AUDIT)
{
  test_config_directive_type(test_CFG_TYPE_AUDIT);
}


void test_CFG_TYPE_PLUGIN_NAMES(DirectorResource* res)
{
  ASSERT_TRUE(res->plugin_names);
  for (auto* val : res->plugin_names) { printf("PluginNames = %s\n", val); }
  EXPECT_EQ(res->plugin_names->size(), 16);
}

TEST_F(ConfigParser_Dir, CFG_TYPE_PLUGIN_NAMES)
{
  test_config_directive_type(test_CFG_TYPE_PLUGIN_NAMES);
}


void test_CFG_TYPE_STR_VECTOR(DirectorResource* res)
{
  EXPECT_EQ(res->tls_cert_.allowed_certificate_common_names_.size(), 8);
}

TEST_F(ConfigParser_Dir, CFG_TYPE_STR_VECTOR)
{
  test_config_directive_type(test_CFG_TYPE_STR_VECTOR);
}

void test_CFG_TYPE_ALIST_STR(DirectorResource*)
{
  JobResource* job1 = (JobResource*)my_config->GetResWithName(R_JOB, "job1");
  EXPECT_STREQ("job1", job1->resource_name_);
  EXPECT_EQ(job1->run_cmds->size(), 8);

  JobResource* jobdefs2
      = (JobResource*)my_config->GetResWithName(R_JOBDEFS, "jobdefs2");
  EXPECT_STREQ("jobdefs2", jobdefs2->resource_name_);
  EXPECT_EQ(jobdefs2->run_cmds->size(), 8);

  JobResource* job2 = (JobResource*)my_config->GetResWithName(R_JOB, "job2");
  EXPECT_STREQ("job2", job2->resource_name_);
  EXPECT_EQ(job2->run_cmds->size(), 8);
}

TEST_F(ConfigParser_Dir, CFG_TYPE_ALIST_STR)
{
  test_config_directive_type(test_CFG_TYPE_ALIST_STR);
}


void test_CFG_TYPE_ALIST_RES(DirectorResource*)
{
  JobResource* job1
      = (JobResource*)my_config->GetResWithName(R_JOB, "resultjob");
  EXPECT_TRUE(job1 != nullptr);
  EXPECT_TRUE(job1->resource_name_ != nullptr);
  EXPECT_STREQ("resultjob", job1->resource_name_);
  EXPECT_EQ(job1->storage->size(), 8);
}

TEST_F(ConfigParser_Dir, CFG_TYPE_ALIST_RES)
{
  test_config_directive_type(test_CFG_TYPE_ALIST_RES);
}


void test_CFG_TYPE_STR(DirectorResource* res)
{
  /* Only the first entry from the last "Description" config directive is taken.
   * This can be considered as a bug. */
  EXPECT_STREQ(res->description_, "item31");
}

TEST_F(ConfigParser_Dir, CFG_TYPE_STR)
{
  test_config_directive_type(test_CFG_TYPE_STR);
}


void test_CFG_TYPE_FNAME(DirectorResource*)
{
  FilesetResource* fileset1
      = (FilesetResource*)my_config->GetResWithName(R_FILESET, "fileset1");
  EXPECT_STREQ("fileset1", fileset1->resource_name_);
  EXPECT_EQ(fileset1->exclude_items.size(), 0);
  EXPECT_EQ(fileset1->include_items.size(), 1);

  alist<const char*>* files
      = std::addressof(fileset1->include_items.at(0)->name_list);
  ASSERT_TRUE(files);
  for (auto* val : files) { printf("Files = %s\n", val); }
}

TEST_F(ConfigParser_Dir, CFG_TYPE_FNAME)
{
  test_config_directive_type(test_CFG_TYPE_FNAME);
}

void test_CFG_TYPE_TIME(DirectorResource* res)
{
  /* clang-format off */
  // Heartbeat Interval = 1 years 2 months 3 weeks 4 days 5 hours 6 minutes 7 seconds
  /* clang-format on */
  EXPECT_EQ(res->heartbeat_interval, 38898367);
}

TEST_F(ConfigParser_Dir, CFG_TYPE_TIME)
{
  test_config_directive_type(test_CFG_TYPE_TIME);
}

static std::vector<std::string> GetCommands(const alist<RunScript*>* scripts)
{
  std::vector<std::string> commands;
  for (auto* script : scripts) { commands.push_back(script->command); }

  std::sort(std::begin(commands), std::end(commands));

  return commands;
}

TEST_F(ConfigParser_Dir, RunScriptInheritance)
{
  std::string path_to_config_file
      = std::string("configs/runscript-inheritance.conf");
  std::unique_ptr<ConfigurationParser> dir_conf{
      InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM)};
  my_config = dir_conf.get();
  ASSERT_TRUE(my_config->ParseConfig());
  ASSERT_TRUE(PopulateDefs());

  auto jobdef1 = dynamic_cast<JobResource*>(
      my_config->GetResWithName(R_JOBDEFS, "Level1"));
  ASSERT_NE(jobdef1, nullptr);
  ASSERT_NE(jobdef1->RunScripts, nullptr);
  ASSERT_EQ(jobdef1->RunScripts->size(), 1);
  std::string command1 = jobdef1->RunScripts->first()->command;
  EXPECT_EQ(command1, "echo 1");

  auto jobdef2 = dynamic_cast<JobResource*>(
      my_config->GetResWithName(R_JOBDEFS, "Level2"));
  ASSERT_NE(jobdef2, nullptr);
  ASSERT_NE(jobdef2->RunScripts, nullptr);
  ASSERT_EQ(jobdef2->RunScripts->size(), 2);
  std::string command2;
  {
    auto* r1 = jobdef2->RunScripts->first();
    auto* r2 = jobdef2->RunScripts->last();

    if (r1->command == command1) {
      ASSERT_TRUE(r1->from_jobdef);
      command2 = r2->command;
    } else {
      ASSERT_TRUE(r2->from_jobdef);
      ASSERT_EQ(r2->command, command1);
      command2 = r1->command;
    }
  }
  EXPECT_EQ(command2, "echo 2");

  using namespace testing;
  auto commands1 = GetCommands(jobdef1->RunScripts);
  ASSERT_THAT(commands1, ElementsAre(command1));
  auto commands2 = GetCommands(jobdef2->RunScripts);
  ASSERT_THAT(commands2, ElementsAre(command1, command2));

  std::string command3 = "echo 3";

  {
    auto job = dynamic_cast<JobResource*>(
        my_config->GetResWithName(R_JOB, "CopyJob"));
    ASSERT_NE(job, nullptr);
    auto commands = GetCommands(job->RunScripts);
    EXPECT_EQ(commands.size(), 0);
  }
  {
    auto job = dynamic_cast<JobResource*>(
        my_config->GetResWithName(R_JOB, "JobWithRunscript"));
    ASSERT_NE(job, nullptr);
    auto commands = GetCommands(job->RunScripts);
    EXPECT_THAT(commands, ElementsAre(command3));
  }
  {
    auto job = dynamic_cast<JobResource*>(
        my_config->GetResWithName(R_JOB, "JobWithLevel1"));
    ASSERT_NE(job, nullptr);
    auto commands = GetCommands(job->RunScripts);
    EXPECT_EQ(commands, commands1);
  }
  {
    auto job = dynamic_cast<JobResource*>(
        my_config->GetResWithName(R_JOB, "JobWithLevel2"));
    ASSERT_NE(job, nullptr);
    auto commands = GetCommands(job->RunScripts);
    EXPECT_EQ(commands, commands2);
  }
  {
    auto job = dynamic_cast<JobResource*>(
        my_config->GetResWithName(R_JOB, "JobWithRunscriptAndLevel2"));
    ASSERT_NE(job, nullptr);
    auto commands = GetCommands(job->RunScripts);
    EXPECT_THAT(commands, ElementsAre(command1, command2, command3));
  }
}
}  // namespace directordaemon
