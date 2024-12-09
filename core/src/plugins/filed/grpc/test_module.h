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

#ifndef BAREOS_PLUGINS_FILED_GRPC_TEST_MODULE_H_
#define BAREOS_PLUGINS_FILED_GRPC_TEST_MODULE_H_

#include <optional>
#include <sys/stat.h>

#include "bareos.grpc.pb.h"
#include "bareos.pb.h"

#include <fmt/format.h>

namespace bc = bareos::core;
namespace bco = bareos::common;

bool Register(std::basic_string_view<bc::EventType> types);
bool Unregister(std::basic_string_view<bc::EventType> types);

// TODO: implement these

void AddExclude();
void AddInclude();
void AddOptions();
void AddRegex();
void AddWild();
void NewOptions();
void NewInclude();
void NewPreInclude();

//

std::optional<size_t> getInstanceCount();

std::optional<bool> checkChanges(bco::FileType ft,
                                 std::string_view name,
                                 time_t timestamp,
                                 const struct stat& statp);

std::optional<bool> AcceptFile(std::string_view name, const struct stat& statp);

bool SetSeen(std::optional<std::string_view> name = std::nullopt);
bool ClearSeen(std::optional<std::string_view> name = std::nullopt);
void JobMessage(bc::JMsgType type,
                int line,
                const char* file,
                const char* fun,
                std::string_view msg);

void DebugMessage(int level,
                  std::string_view msg,
                  int line,
                  const char* file,
                  const char* fun);

struct Severity {
  int severity{};
  const char* file{};
  const char* function{};
  int line{};

  Severity(int severity_,
           const char* file_ = __builtin_FILE(),
           const char* function_ = __builtin_FUNCTION(),
           int line_ = __builtin_LINE())
      : severity{severity_}, file{file_}, function{function_}, line{line_}
  {
  }
};

struct Type {
  bc::JMsgType type{};
  const char* file{};
  const char* function{};
  int line{};

  Type(bc::JMsgType type_,
       const char* file_ = __builtin_FILE(),
       const char* function_ = __builtin_FUNCTION(),
       int line_ = __builtin_LINE())
      : type{type_}, file{file_}, function{function_}, line{line_}
  {
  }
};

template <typename... Args>
void DebugLog(Severity severity,
              fmt::format_string<Args...> fmt,
              Args&&... args)
{
  auto formatted = fmt::vformat(fmt, fmt::make_format_args(args...));

  DebugMessage(severity.severity, formatted, severity.line, severity.file,
               severity.function);
}

template <typename... Args>
void JobLog(Type type, fmt::format_string<Args...> fmt, Args&&... args)
{
  auto formatted = fmt::format(fmt, args...);

  JobMessage(type.type, type.line, type.file, type.function, formatted);
}

bool Bareos_SetString(bc::BareosStringVariable var, std::string_view val);
std::optional<std::string> Bareos_GetString(bc::BareosStringVariable var);

bool Bareos_SetInt(bc::BareosIntVariable var, int val);
std::optional<int> Bareos_GetInt(bc::BareosIntVariable var);

bool Bareos_SetFlag(bc::BareosFlagVariable var, bool val);
std::optional<bool> Bareos_GetFlag(bc::BareosFlagVariable var);

#endif  // BAREOS_PLUGINS_FILED_GRPC_TEST_MODULE_H_
