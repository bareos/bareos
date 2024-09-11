/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <iostream>
#include <map>
#include <cstdlib>
#include <cassert>
#include <optional>
#include <filesystem>
#include <string_view>
#include <chrono>

enum class find_strategy
{
  FindFileEx,
  FindFileEx_Large,
  GetFileInformationByHandleEx,
  FindFile,
};

const std::map<std::string, find_strategy> strategy_names{
    {"findfileex", find_strategy::FindFileEx},
    {"findfileexlarge", find_strategy::FindFileEx_Large},
    {"getfileinformationbyhandleex",
     find_strategy::GetFileInformationByHandleEx},
    {"findfile", find_strategy::FindFile},
};

using String = std::wstring;

void Error(const wchar_t* msg)
{
  std::wcout << L"[ERROR] " << msg << L": <>" << std::endl;
}

void dir_append(String& base, std::wstring_view part)
{
  assert(base.size() > 0);
  if (base.back() != '\\') { base += '\\'; }
  base += part;
}

bool Ignore(std::wstring_view name)
{
  return (name == L"") || (name == L".") || (name == L"..");
}

void Handle(size_t depth, std::wstring_view name)
{
  std::wcout << std::setfill(L'.') << std::setw(depth) << "";
  std::wcout << name << "\n";
}

template <typename StrategyBuilder>
void FindRecursively(const StrategyBuilder& builder,
                     const String& root,
                     size_t depth = 0)
{
  auto strategy = builder.start(root);

  if (!strategy) { return; }

  String child;
  do {
    auto [name, dir] = strategy->get();
    if (Ignore(name)) { continue; }

    Handle(depth, name);

    if (dir) {
      child.assign(root);
      dir_append(child, name);
      FindRecursively(builder, child, depth + 1);
    }
  } while (strategy->next());
}

struct find_file {
  struct strategy {
    HANDLE hnd{INVALID_HANDLE_VALUE};
    WIN32_FIND_DATAW data{};

    strategy(const std::wstring& root)
    {
      std::wstring search = root;
      dir_append(search, L"*");
      hnd = FindFirstFileW(search.c_str(), &data);

      if (hnd == INVALID_HANDLE_VALUE) { Error(L"FindFirstFile"); }
    }

    strategy(const strategy& other) = delete;
    strategy& operator=(const strategy& other) = delete;
    strategy(strategy&& other) { *this = std::move(other); }
    strategy& operator=(strategy&& other)
    {
      std::swap(hnd, other.hnd);
      std::swap(data, other.data);
      return *this;
    }


    std::pair<std::wstring_view, bool> get()
    {
      return {std::wstring_view{data.cFileName},
              (FILE_ATTRIBUTE_DIRECTORY & data.dwFileAttributes) != 0};
    }

    bool next() { return FindNextFileW(hnd, &data); }

    ~strategy()
    {
      if (hnd != INVALID_HANDLE_VALUE) { FindClose(hnd); }
    }
  };

  std::optional<strategy> start(const String& root) const noexcept
  {
    auto s = strategy{root};
    if (s.hnd == INVALID_HANDLE_VALUE) { return std::nullopt; }
    return s;
  }
};

struct find_file_ex {
  struct strategy {
    HANDLE hnd{INVALID_HANDLE_VALUE};
    WIN32_FIND_DATAW data{};

    strategy(const std::wstring& root, bool large)
    {
      std::wstring search = root;
      dir_append(search, L"*");
      hnd = FindFirstFileExW(search.c_str(), FindExInfoBasic, &data,
                             FindExSearchNameMatch, NULL,
                             large ? FIND_FIRST_EX_LARGE_FETCH : 0);

      if (hnd == INVALID_HANDLE_VALUE) { Error(L"FindFirstFile"); }
    }

    strategy(const strategy& other) = delete;
    strategy& operator=(const strategy& other) = delete;
    strategy(strategy&& other) { *this = std::move(other); }
    strategy& operator=(strategy&& other)
    {
      std::swap(hnd, other.hnd);
      std::swap(data, other.data);
      return *this;
    }


    std::pair<std::wstring_view, bool> get()
    {
      return {std::wstring_view{data.cFileName},
              (FILE_ATTRIBUTE_DIRECTORY & data.dwFileAttributes) != 0};
    }

    bool next() { return FindNextFileW(hnd, &data); }

    ~strategy()
    {
      if (hnd != INVALID_HANDLE_VALUE) { FindClose(hnd); }
    }
  };

  std::optional<strategy> start(const String& root) const noexcept
  {
    auto s = strategy{root, large};
    if (s.hnd == INVALID_HANDLE_VALUE) { return std::nullopt; }
    return s;
  }

  bool large;
};

struct get_information_ex {
  struct strategy {
    HANDLE hnd{INVALID_HANDLE_VALUE};
    size_t buffer_size{0};
    uint8_t* buffer{nullptr};
    FILE_FULL_DIR_INFO* fileInfo{nullptr};

    strategy() = default;
    strategy(const String& root, size_t bs) : buffer_size{bs}
    {
      hnd = CreateFileW(root.c_str(), FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

      if (hnd == INVALID_HANDLE_VALUE) {
        String error_string = L"CreateFile ";
        error_string += root;
        Error(error_string.c_str());
        return;
      }

      buffer = new uint8_t[buffer_size];

      if (!GetFileInformationByHandleEx(hnd, FileFullDirectoryInfo, buffer,
                                        (DWORD)buffer_size)) {
        // We should always get at least "." and ".." entries for the directory.
        // If we go here, you've got a weird error to work through.
        CloseHandle(hnd);
        hnd = INVALID_HANDLE_VALUE;
        Error(L"Failed to list initial files");
        return;
      }
      fileInfo = (decltype(fileInfo))buffer;
    }

    strategy(const strategy& other) = delete;
    strategy& operator=(const strategy& other) = delete;
    strategy(strategy&& other) { *this = std::move(other); }
    strategy& operator=(strategy&& other)
    {
      std::swap(hnd, other.hnd);
      std::swap(buffer, other.buffer);
      std::swap(buffer_size, other.buffer_size);
      std::swap(fileInfo, other.fileInfo);
      return *this;
    }


    std::pair<std::wstring_view, bool> get()
    {
      return {std::wstring_view{&*fileInfo->FileName,
                                fileInfo->FileNameLength / sizeof(wchar_t)},
              (FILE_ATTRIBUTE_DIRECTORY & fileInfo->FileAttributes) != 0};
    }

    bool next()
    {
      if (fileInfo->NextEntryOffset != 0) {
        // Go to the next file.
        fileInfo = (decltype(fileInfo))((uint8_t*)fileInfo
                                        + fileInfo->NextEntryOffset);
      } else {
        // Check whether there are more files to fetch.
        if (!GetFileInformationByHandleEx(hnd, FileFullDirectoryInfo, buffer,
                                          (DWORD)buffer_size)) {
          const DWORD error = GetLastError();
          if (error != ERROR_NO_MORE_FILES) { Error(L"NextInfo"); }
          return false;
        }
        fileInfo = (decltype(fileInfo))buffer;
      }

      return true;
    }

    ~strategy()
    {
      if (hnd != INVALID_HANDLE_VALUE) { CloseHandle(hnd); }
      if (buffer) { delete buffer; }
    }
  };

  std::optional<strategy> start(const String& root) const noexcept
  {
    auto s = strategy{root, buffer_size};
    if (s.hnd == INVALID_HANDLE_VALUE) { return std::nullopt; }
    return s;
  }

  size_t buffer_size{64 * 1024};
};

// void Find_GetInformationByHandleEx(const std::string& root,
//                                    size_t depth = 0)
// {
//   HANDLE dirHandle = CreateFileW((wchar_t*)fullPath.Data,
//                                  FILE_LIST_DIRECTORY,
//                                  FILE_SHARE_READ | FILE_SHARE_WRITE |
//                                  FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
//                                  FILE_FLAG_BACKUP_SEMANTICS,
//                                  0
//                                 );
//   constexpr DWORD BufferSize = 64 * 1024;
//   uint8_t buffer[BufferSize];
//   if (!GetFileInformationByHandleEx(dirHandle,
//   FileIdExtdDirectoryRestartInfo, buffer, BufferSize)) {
//       // We should always get at least "." and ".." entries for the
//       directory.
//       // If we go here, you've got a weird error to work through.
//       Error("Failed to list initial files");
//       return;
//   }
//   FILE_ID_EXTD_DIR_INFO* fileInfo = (FILE_ID_EXTD_DIR_INFO*)buffer;
//   while (true) {
//       // do something with your file here!

//     if (fileInfo->NextEntryOffset != 0) {
//       // Go to the next file.
//       fileInfo = (FILE_ID_EXTD_DIR_INFO*)((uint8_t*)fileInfo
//       +fileInfo->NextEntryOffset);
//     } else {
//       // Check whether there are more files to fetch.
//       if (!GetFileInformationByHandleEx(dirHandle, FileIdExtdDirectoryInfo,
//       buffer, BufferSize)) {
//         const DWORD error = GetLastError();
//         if (error == ERROR_NO_MORE_FILES)
//           break;
//         ASSERT(false, "Failed to list files, error %d", GetLastError());
//       }
//       fileInfo = (FILE_ID_EXTD_DIR_INFO*)buffer;
//     }
//   }

//   std::string search = root;
//   dir_append(search, "*");
//   WIN32_FIND_DATA data{};
//   HANDLE hnd = FindFirstFileExA(search.c_str(),
//                                 FindExInfoBasic,
//                                 &data,
//                                 FindExSearchNameMatch,
//                                 NULL,
//                                 large ? FIND_FIRST_EX_LARGE_FETCH : 0);

//   if (hnd == INVALID_HANDLE_VALUE) {
//     Error("FindFirstFile");
//     return;
//   }

//   std::string child;
//   do {
//     if (Ignore(data)) { continue; }

//     Handle(depth, data);
//     if (FILE_ATTRIBUTE_DIRECTORY & data.dwFileAttributes) {
//       child.assign(root);
//       dir_append(child, data.cFileName);

//       Find_FindFileEx(child, large, depth + 1);
//     }
//   } while (FindNextFileA(hnd, &data));

//   FindClose(hnd);
// }

void FindAll(const std::string& root, find_strategy strategy)
{
  String s{std::begin(root), std::end(root)};

  switch (strategy) {
    case find_strategy::FindFileEx: {
      auto builder = find_file_ex{false};
      FindRecursively(builder, s);
    } break;
    case find_strategy::FindFileEx_Large: {
      auto builder = find_file_ex{true};
      FindRecursively(builder, s);
    } break;
    case find_strategy::GetFileInformationByHandleEx: {
      auto builder = get_information_ex{};
      FindRecursively(builder, s);
    } break;
    case find_strategy::FindFile: {
      auto builder = find_file{};
      FindRecursively(builder, s);
    } break;
  }
}

int main(int argc, const char* argv[])
{
  CLI::App app;

  std::string root{"."};
  app.add_option("root", root)
      ->check(CLI::ExistingDirectory)
      ->type_name("<path>");

  find_strategy strategy{find_strategy::FindFileEx};
  app.add_option("-s, --strategy", strategy, "strategy used to enumerate files")
      ->transform(CLI::CheckedTransformer(strategy_names, CLI::ignore_case));

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    exit(app.exit(e));
  }

  std::cout << "Scanning '" << root << "' using strategy <>" << std::endl;

  auto start = std::chrono::steady_clock::now();
  FindAll(root, strategy);
  auto end = std::chrono::steady_clock::now();

  std::cout << "took " << (end - start) << std::endl;

  return 0;
}
