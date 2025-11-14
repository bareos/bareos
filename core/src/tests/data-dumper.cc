/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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
#endif

#include "gtest/gtest.h"

#include "dump.h"

#include <iostream>

struct logger : GenericLogger {
  void Begin(std::size_t FileSize) override { (void)FileSize; }
  void Progressed(std::size_t Amount) override { (void)Amount; }
  void End() override {}

  void SetStatus(std::string_view Status) override
  {
    std::cerr << "[STATUS] >>" << Status << "<<\n";
  }
  void Output(Message message) override
  {
    std::cerr << "[LOG] >>" << message.text << "<<\n";
  }

  logger() : GenericLogger(true) {}
};

TEST(Test_Dumping, simple_bytes)
{
  std::vector<char> bytes;

  bytes.resize(100);

  for (size_t i = 0; i < bytes.size(); ++i) { bytes[i] = static_cast<char>(i); }

  insert_plan plan{
      bytes,
  };

  logger log;
  data_dumper* dumper = dumper_setup(&log, std::move(plan));

  std::vector<char> result;
  result.resize(bytes.size());

  std::size_t bytes_written = dumper_write(dumper, result);
  dumper_stop(dumper);

  ASSERT_EQ(bytes_written, result.size());

  for (size_t i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(result[i], bytes[i]) << "at index " << i;
  }
}

std::pair<std::size_t, std::size_t> split_read(data_dumper* dumper,
                                               std::span<char> buffer)
{
  size_t split_offset = buffer.size() / 2;

  std::size_t bytes_written_pre
      = dumper_write(dumper, buffer.subspan(0, split_offset));
  std::size_t bytes_written_post
      = dumper_write(dumper, buffer.subspan(split_offset));

  return {bytes_written_pre, bytes_written_post};
}

TEST(Test_Dumping, split_bytes)
{
  std::vector<char> bytes;

  bytes.resize(100);

  for (size_t i = 0; i < bytes.size(); ++i) { bytes[i] = static_cast<char>(i); }

  insert_plan plan{
      bytes,
  };

  logger log;
  data_dumper* dumper = dumper_setup(&log, std::move(plan));

  std::vector<char> result;
  result.resize(bytes.size());

  auto [bytes_written_pre, bytes_written_post] = split_read(dumper, result);

  dumper_stop(dumper);

  std::size_t bytes_written = bytes_written_pre + bytes_written_post;

  ASSERT_EQ(bytes_written, result.size())
      << "pre=" << bytes_written_pre << ", post=" << bytes_written_post;

  for (size_t i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(result[i], bytes[i]) << "at index " << i;
  }
}

HANDLE CreateTempFile()
{
  char temp_path_buffer[MAX_PATH];

  if (auto dwRetVal = GetTempPath2A(sizeof(temp_path_buffer), temp_path_buffer);
      dwRetVal > sizeof(temp_path_buffer) || (dwRetVal == 0)) {
    // PrintError(TEXT("GetTempPath2 failed"));
    return INVALID_HANDLE_VALUE;
  }

  std::cerr << "found temp path = " << temp_path_buffer << std::endl;

  char temp_file_name[MAX_PATH];
  //  Generates a temporary file name.
  if (auto uRetVal = GetTempFileNameA(temp_path_buffer, "bareos-data-dumper", 0,
                                      temp_file_name);
      uRetVal == 0) {
    // PrintError(TEXT("GetTempFileName failed"));
    return INVALID_HANDLE_VALUE;
  }

  std::cerr << "Opening temp file " << (char*)temp_file_name << std::endl;

  //  Creates the new file to write to for the upper-case version.
  HANDLE hndl = CreateFileA(
      temp_file_name, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

  return hndl;
}

HANDLE CreateTempFile(std::span<char> contents)
{
  HANDLE hndl = CreateTempFile();
  if (hndl == INVALID_HANDLE_VALUE) { return hndl; }

  std::size_t bytes_written = 0;
  for (;;) {
    std::size_t towrite = contents.size() - bytes_written;
    if (towrite == 0) { break; }
    DWORD clamped = std::numeric_limits<DWORD>::max();
    if (towrite < static_cast<std::size_t>(clamped)) { clamped = towrite; }

    DWORD written_bytes = 0;
    if (!WriteFile(hndl, contents.data() + bytes_written, clamped,
                   &written_bytes, NULL)) {
      std::cerr << "bad byte write detected (" << GetLastError() << ")"
                << std::endl;
      break;
    }

    if (written_bytes == 0) {
      std::cerr << "0 byte write detected" << std::endl;
      break;
    }

    bytes_written += written_bytes;
  }

  if (contents.size() != bytes_written) {
    CloseHandle(hndl);
    return INVALID_HANDLE_VALUE;
  }

  return hndl;
}

TEST(Test_Dumping, simple_from)
{
  std::vector<char> bytes;

  bytes.resize(100);

  for (size_t i = 0; i < bytes.size(); ++i) { bytes[i] = static_cast<char>(i); }

  HANDLE hndl = CreateTempFile(bytes);
  ASSERT_NE(hndl, INVALID_HANDLE_VALUE);

  insert_plan plan{insert_from{hndl, 0, bytes.size()}};

  logger log;
  data_dumper* dumper = dumper_setup(&log, std::move(plan));

  std::vector<char> result;
  result.resize(bytes.size());

  std::size_t bytes_written = dumper_write(dumper, result);
  dumper_stop(dumper);

  ASSERT_EQ(bytes_written, result.size());

  for (size_t i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(result[i], bytes[i]) << "at index " << i;
  }

  CloseHandle(hndl);
}

TEST(Test_Dumping, split_from)
{
  std::vector<char> bytes;

  bytes.resize(100);

  for (size_t i = 0; i < bytes.size(); ++i) { bytes[i] = static_cast<char>(i); }

  HANDLE hndl = CreateTempFile(bytes);
  ASSERT_NE(hndl, INVALID_HANDLE_VALUE);

  insert_plan plan{insert_from{hndl, 0, bytes.size()}};

  logger log;
  data_dumper* dumper = dumper_setup(&log, std::move(plan));

  std::vector<char> result;
  result.resize(bytes.size());

  auto [bytes_written_pre, bytes_written_post] = split_read(dumper, result);

  dumper_stop(dumper);

  std::size_t bytes_written = bytes_written_pre + bytes_written_post;

  ASSERT_EQ(bytes_written, result.size())
      << "pre=" << bytes_written_pre << ", post=" << bytes_written_post;

  for (size_t i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(result[i], bytes[i]) << "at index " << i;
  }

  CloseHandle(hndl);
}

TEST(Test_Plan, check_size_computation)
{
  std::vector<char> bytes;


  insert_plan plan;

  std::size_t target_size = 0;
  for (size_t i = 0; i < 100; ++i) {
    bytes.resize(i);
    plan.push_back(bytes);

    target_size += i;

    EXPECT_EQ(compute_plan_size(plan), target_size);
  }

  for (size_t i = 0; i < 100; ++i) {
    plan.push_back(insert_from{INVALID_HANDLE_VALUE, target_size, i});

    target_size += i;

    EXPECT_EQ(compute_plan_size(plan), target_size);
  }
}
