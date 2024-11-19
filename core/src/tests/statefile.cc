/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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

#include "lib/bsys.h"
#include "lib/recent_job_results_list.h"

#include <iostream>
#include <fstream>

static inline bool Is32BitAligned()
{
  return offsetof(StateFileHeader, last_jobs_addr) == 20;
}

static bool StateFileExists(const char* dir, const char* name, int port)
{
  struct stat buf;
  std::string path{dir};
  path += "/";
  path += name;
  path += "." + std::to_string(port) + ".state";
  std::cout << path << "\n";
  return stat(path.c_str(), &buf) == 0;
}

static bool CopyTestStateFileFromOriginal(std::string orig_path,
                                          std::string test_path,
                                          std::string filename)
{
  OSDependentInit();
  std::string filename_orig(orig_path + "/" + filename + ".original");
  std::string filename_test(test_path + "/" + filename);

  std::ifstream file_orig;
  std::ofstream file_test;

  file_orig.exceptions(std::ios::badbit | std::ios::failbit);
  file_test.exceptions(std::ios::badbit | std::ios::failbit);

  try {
    file_orig.open(filename_orig, std::ios::binary);
    file_test.open(filename_test, std::ios::binary);
    file_test << file_orig.rdbuf();
  } catch (const std::system_error& e) {
    std::cout << e.code().message().c_str() << std::endl;
    return false;
  } catch (const std::exception& e) {
    std::cout << "Unspecific error occurred: " << e.what() << std::endl;
    return false;
  }
  return true;
}

TEST(statefile, read)
{
  OSDependentInit();
  RecentJobResultsList::Cleanup();

  char orig_path[]{TEST_ORIGINAL_FILE_DIR};
  char test_path[]{TEST_TEMP_DIR};

  const char* fname
      = Is32BitAligned() ? "bareos-dir-32bit-read" : "bareos-dir-read";

  ASSERT_TRUE(CopyTestStateFileFromOriginal(
      orig_path, test_path, std::string(fname) + ".42001.state"));
  ASSERT_TRUE(StateFileExists(test_path, fname, 42001))
      << "path = " << test_path;
  ReadStateFile(test_path, fname, 42001);

  auto recent_jobs{RecentJobResultsList::Get()};

  ASSERT_EQ(recent_jobs.size(), 2);

  EXPECT_EQ(recent_jobs[0].JobId, 1);
  EXPECT_STREQ(recent_jobs[0].Job, "backup-bareos-fd.2019-08-08_22.38.13_04");

  EXPECT_EQ(recent_jobs[1].JobId, 2);
  EXPECT_STREQ(recent_jobs[1].Job, "RestoreFiles.2019-08-08_22.38.17_05");
}

TEST(statefile, write)
{
  OSDependentInit();
  RecentJobResultsList::Cleanup();

  std::string orig_path{TEST_ORIGINAL_FILE_DIR};
  std::string test_path{TEST_TEMP_DIR};

  const char* fname = Is32BitAligned() ? "bareos-dir-32bit" : "bareos-dir";
  std::string statefile_name(std::string(fname) + ".42001.state");

  ASSERT_TRUE(CopyTestStateFileFromOriginal(orig_path.c_str(),
                                            test_path.c_str(), statefile_name));
  ASSERT_TRUE(StateFileExists(test_path.c_str(), fname, 42001))
      << "path = " << test_path;
  ReadStateFile(test_path.c_str(), fname, 42001);

  std::string write_path{TEST_TEMP_DIR "/write-test"};

  std::remove((write_path + "/" + statefile_name).c_str());
  ASSERT_FALSE(StateFileExists(write_path.c_str(), fname, 42001));
  WriteStateFile(write_path.c_str(), fname, 42001);

  RecentJobResultsList::Cleanup();

  ASSERT_TRUE(StateFileExists(write_path.c_str(), fname, 42001));
  ReadStateFile(write_path.c_str(), fname, 42001);

  auto recent_jobs{RecentJobResultsList::Get()};

  ASSERT_EQ(recent_jobs.size(), 2);

  EXPECT_EQ(recent_jobs[0].JobId, 1);
  EXPECT_STREQ(recent_jobs[0].Job, "backup-bareos-fd.2019-08-08_22.38.13_04");

  EXPECT_EQ(recent_jobs[1].JobId, 2);
  EXPECT_STREQ(recent_jobs[1].Job, "RestoreFiles.2019-08-08_22.38.17_05");
}

TEST(statefile, handle_truncated_jobs)
{
  OSDependentInit();
  RecentJobResultsList::Cleanup();

  char orig_path[]{TEST_ORIGINAL_FILE_DIR};
  char test_path[]{TEST_TEMP_DIR};

  const char* fname = Is32BitAligned() ? "bareos-dir-truncated-jobs-32bit"
                                       : "bareos-dir-truncated-jobs";

  ASSERT_TRUE(CopyTestStateFileFromOriginal(
      orig_path, test_path, std::string(fname) + ".42001.state"));
  ReadStateFile(test_path, fname, 42001);

  auto recent_jobs{RecentJobResultsList::Get()};

  ASSERT_EQ(recent_jobs.size(), 1);

  EXPECT_EQ(recent_jobs[0].JobId, 1);
  EXPECT_STREQ(recent_jobs[0].Job, "backup-bareos-fd.2019-08-08_22.38.13_04");
}

TEST(statefile, handle_truncated_headers)
{
  OSDependentInit();
  RecentJobResultsList::Cleanup();

  char orig_path[]{TEST_ORIGINAL_FILE_DIR};
  char test_path[]{TEST_TEMP_DIR};

  ASSERT_TRUE(CopyTestStateFileFromOriginal(
      orig_path, test_path, "bareos-dir-truncated-header.42001.state"));
  ReadStateFile(test_path, "bareos-dir-truncated-header", 42001);

  auto recent_jobs{RecentJobResultsList::Get()};

  ASSERT_EQ(recent_jobs.size(), 0);
}

TEST(statefile, handle_nonexisting_file)
{
  OSDependentInit();
  RecentJobResultsList::Cleanup();

  char test_path[]{TEST_TEMP_DIR};

  ASSERT_FALSE(StateFileExists(test_path, "file-does-not-exist", 42001));
  ReadStateFile(test_path, "file-does-not-exist", 42001);

  auto recent_jobs{RecentJobResultsList::Get()};

  ASSERT_EQ(recent_jobs.size(), 0);
}
