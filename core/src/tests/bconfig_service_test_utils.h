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

#ifndef BAREOS_CORE_SRC_TESTS_BCONFIG_SERVICE_TEST_UTILS_H_
#define BAREOS_CORE_SRC_TESTS_BCONFIG_SERVICE_TEST_UTILS_H_

#include "tools/bconfig_service.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

#if HAVE_WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

namespace bconfig::service {
namespace {

[[maybe_unused]] std::filesystem::path MakeTempPath()
{
  static std::atomic<uint64_t> counter{0};
  const auto unique_suffix = std::to_string(static_cast<unsigned long long>(
      std::chrono::steady_clock::now().time_since_epoch().count()));
#if HAVE_WIN32
  const auto process_id = std::to_string(static_cast<unsigned>(_getpid()));
#else
  const auto process_id = std::to_string(static_cast<unsigned>(getpid()));
#endif
  return std::filesystem::temp_directory_path()
         / ("bconfig-service-test-" + process_id + "-"
            + std::to_string(++counter) + "-" + unique_suffix);
}

class [[maybe_unused]] ScopedDirectory {
 public:
  explicit ScopedDirectory(std::filesystem::path path) : path_{std::move(path)}
  {
  }

  ~ScopedDirectory() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

class [[maybe_unused]] ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, const std::string& value)
      : name_{name}
  {
    if (const char* current = std::getenv(name_)) { previous_value_ = current; }
    had_previous_value_ = previous_value_.has_value();
    Set(value);
  }

  ~ScopedEnvironmentVariable()
  {
    if (had_previous_value_) {
      Set(*previous_value_);
    } else {
#if HAVE_WIN32
      _putenv_s(name_, "");
#else
      unsetenv(name_);
#endif
    }
  }

 private:
  void Set(const std::string& value)
  {
#if HAVE_WIN32
    _putenv_s(name_, value.c_str());
#else
    setenv(name_, value.c_str(), 1);
#endif
  }

  const char* name_;
  std::optional<std::string> previous_value_{};
  bool had_previous_value_{false};
};

[[maybe_unused]] std::filesystem::path FindFixtureRoot()
{
  auto cursor = std::filesystem::current_path();
  for (;;) {
    const auto build_tree_path = cursor / "configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(build_tree_path)) {
      return build_tree_path;
    }

    const auto source_tree_path
        = cursor / "core/src/tests/configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(source_tree_path)) {
      return source_tree_path;
    }

    if (cursor == cursor.root_path()) { break; }
    cursor = cursor.parent_path();
  }

  return {};
}

[[maybe_unused]] std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream file{path};
  EXPECT_TRUE(file.good());
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

[[maybe_unused]] void WriteTextFile(const std::filesystem::path& path,
                                    std::string_view content)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file{path};
  ASSERT_TRUE(file.good());
  file << content;
  ASSERT_TRUE(file.good());
}

[[maybe_unused]] void AddCounterFixtures(
    const std::filesystem::path& source_root)
{
  WriteTextFile(source_root / "bareos-dir.d/counter/WrapSeed.conf",
                "Counter {\n"
                "  Name = \"WrapSeed\"\n"
                "  Minimum = 1\n"
                "  Maximum = 5\n"
                "}\n");
  WriteTextFile(source_root / "bareos-dir.d/counter/ExistingCounter.conf",
                "Counter {\n"
                "  Name = \"ExistingCounter\"\n"
                "  Description = \"Existing counter\"\n"
                "  Minimum = 7\n"
                "  Maximum = 99\n"
                "  WrapCounter = WrapSeed\n"
                "  Catalog = MyCatalog\n"
                "}\n");
}

[[maybe_unused]] void AddConsoleImportFixture(
    const std::filesystem::path& source_root)
{
  WriteTextFile(source_root / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");
}

[[maybe_unused]] std::optional<JobRecord> WaitForJobTerminal(
    ServiceState& state,
    std::string_view id)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto job = state.GetJob(id);
    if (job
        && (job->status == JobStatus::kSucceeded
            || job->status == JobStatus::kFailed)) {
      return job;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return state.GetJob(id);
}

}  // namespace
}  // namespace bconfig::service

#endif  // BAREOS_CORE_SRC_TESTS_BCONFIG_SERVICE_TEST_UTILS_H_
