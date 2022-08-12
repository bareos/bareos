/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2022 Bareos GmbH & Co. KG

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

#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "lib/output_formatter_resource.h"

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

namespace directordaemon {

static std::string sprintoutput{};

bool sprintit(void* ctx, const char* fmt, ...)
{
  va_list arg_ptr;
  PoolMem msg;

  va_start(arg_ptr, fmt);
  msg.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  sprintoutput += msg.c_str();
  return true;
}

TEST(ConfigParser_Dir, ParseSchedulerOddEvenDaysCorrectly)
{
  OSDependentInit();

  std::string path_to_config_file = std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/bareos-configparser-tests");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
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
      " Run = Fri "
      "w00,w02,w04,w06,w08,w10,w12,w14,w16,w18,w20,w22,w24,w26,w28,w30,w32,w34,"
      "w36,w38,w40,w42,w44,w46,w48,w50,w52 at 23:10\n"
      "}\n\n"
      "Schedule {\n"
      "  Name = \"Odd Weeks\"\n"
      "  Run = Sun "
      "w01,w03,w05,w07,w09,w11,w13,w15,w17,w19,w21,w23,w25,w27,w29,w31,w33,w35,"
      "w37,w39,w41,w43,w45,w47,w49,w51,w53 at 23:10\n"
      "}\n\n"};


  EXPECT_EQ(sprintoutput, expected_output);
}

TEST(ConfigParser_Dir, bareos_configparser_tests)
{
  OSDependentInit();
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file = std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/bareos-configparser-tests");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();
  my_config->DumpResources(PrintMessage, NULL);

  auto backup = my_config->BackupResourceTable();
  my_config->ParseConfig();

  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;
  ASSERT_NE(nullptr, me);
  my_config->RestoreResourceTable(std::move(backup));
  ASSERT_NE(nullptr, me);

  // If a config already exists, BackupResourceTable() needs to be called before
  // ParseConfig(), otherwise memory of the existing config is not freed
  // completely
  auto backup2 = my_config->BackupResourceTable();
  my_config->ParseConfig();

  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;
  assert(me);

  ASSERT_NE(nullptr, me);
  my_config->DumpResources(PrintMessage, NULL);
  ASSERT_NE(nullptr, me);
  delete my_config;

  TermMsg(); /* Terminate message handler */
}

TEST(ConfigParser_Dir, runscript_test)
{
  OSDependentInit();
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file = std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/runscript-tests/bareos-dir.conf");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();

  my_config->DumpResources(PrintMessage, NULL);

  delete my_config;

  TermMsg(); /* Terminate message handler */
}

void test_config_directive_type(
    std::function<void(DirectorResource*)> test_func)
{
  std::string test_name = std::string(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());

  OSDependentInit();
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/bareos-configparser-tests/bareos-dir-")
        + test_name + std::string(".conf");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();

  my_config->DumpResources(PrintMessage, NULL);

  const char* dir_resource_name = "bareos-dir";

  DirectorResource* me
      = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, NULL);
  EXPECT_STREQ(dir_resource_name, me->resource_name_);

  test_func(me);

  delete my_config;

  TermMsg(); /* Terminate message handler */
}


void test_CFG_TYPE_AUDIT(DirectorResource* me)
{
  char* val = nullptr;
  foreach_alist (val, me->audit_events) {
    printf("AuditEvents = %s\n", val);
  }
  EXPECT_EQ(me->audit_events->size(), 8);
}

TEST(ConfigParser_Dir, CFG_TYPE_AUDIT)
{
  test_config_directive_type(test_CFG_TYPE_AUDIT);
}


void test_CFG_TYPE_PLUGIN_NAMES(DirectorResource* me)
{
  char* val = nullptr;
  foreach_alist (val, me->plugin_names) {
    printf("PluginNames = %s\n", val);
  }
  EXPECT_EQ(me->plugin_names->size(), 16);
}

TEST(ConfigParser_Dir, CFG_TYPE_PLUGIN_NAMES)
{
  test_config_directive_type(test_CFG_TYPE_PLUGIN_NAMES);
}


void test_CFG_TYPE_STR_VECTOR(DirectorResource* me)
{
  EXPECT_EQ(me->tls_cert_.allowed_certificate_common_names_.size(), 8);
}

TEST(ConfigParser_Dir, CFG_TYPE_STR_VECTOR)
{
  test_config_directive_type(test_CFG_TYPE_STR_VECTOR);
}

void test_CFG_TYPE_STR_VECTOR_OF_DIRS(DirectorResource* me)
{
  EXPECT_EQ(me->backend_directories.size(), 9);
  /*
   *  WIN32:
   *  cmake uses some value for PATH_BAREOS_BACKENDDIR,
   *  which ends up in the configuration files
   *  but this is later overwritten in the Director Daemon with ".".
   *  Therefore we skip this test.
   */
#if !defined(HAVE_WIN32)
  EXPECT_EQ(me->backend_directories.at(0), PATH_BAREOS_BACKENDDIR);
#endif
}

TEST(ConfigParser_Dir, CFG_TYPE_STR_VECTOR_OF_DIRS)
{
  test_config_directive_type(test_CFG_TYPE_STR_VECTOR_OF_DIRS);
}

void test_CFG_TYPE_ALIST_STR(DirectorResource* me)
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

TEST(ConfigParser_Dir, CFG_TYPE_ALIST_STR)
{
  test_config_directive_type(test_CFG_TYPE_ALIST_STR);
}


void test_CFG_TYPE_ALIST_RES(DirectorResource* me)
{
  JobResource* job1
      = (JobResource*)my_config->GetResWithName(R_JOB, "resultjob");
  EXPECT_STREQ("resultjob", job1->resource_name_);
  EXPECT_EQ(job1->base->size(), 8);
}

TEST(ConfigParser_Dir, CFG_TYPE_ALIST_RES)
{
  test_config_directive_type(test_CFG_TYPE_ALIST_RES);
}


void test_CFG_TYPE_STR(DirectorResource* me)
{
  /*
   * Only the first entry from the last "Description" config directive is taken.
   * This can be considered as a bug.
   */
  EXPECT_STREQ(me->description_, "item31");
}

TEST(ConfigParser_Dir, CFG_TYPE_STR)
{
  test_config_directive_type(test_CFG_TYPE_STR);
}


void test_CFG_TYPE_FNAME(DirectorResource* me)
{
  FilesetResource* fileset1
      = (FilesetResource*)my_config->GetResWithName(R_FILESET, "fileset1");
  EXPECT_STREQ("fileset1", fileset1->resource_name_);
  EXPECT_EQ(fileset1->exclude_items.size(), 0);
  EXPECT_EQ(fileset1->include_items.size(), 1);

  alist<const char*>* files
      = std::addressof(fileset1->include_items.at(0)->name_list);
  const char* val = nullptr;
  foreach_alist (val, files) {
    printf("Files = %s\n", val);
  }
}

TEST(ConfigParser_Dir, CFG_TYPE_FNAME)
{
  test_config_directive_type(test_CFG_TYPE_FNAME);
}

void test_CFG_TYPE_TIME(DirectorResource* me)
{
  /* clang-format off */
  // Heartbeat Interval = 1 years 2 months 3 weeks 4 days 5 hours 6 minutes 7 seconds
  /* clang-format on */
  EXPECT_EQ(me->heartbeat_interval, 38898367);
}

TEST(ConfigParser_Dir, CFG_TYPE_TIME)
{
  test_config_directive_type(test_CFG_TYPE_TIME);
}
}  // namespace directordaemon
