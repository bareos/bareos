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

#ifndef BAREOS_PLUGINS_FILED_GRPC_BAREOS_API_H_
#define BAREOS_PLUGINS_FILED_GRPC_BAREOS_API_H_

#include "include/bareos.h"
#include "filed/fd_plugins.h"

#include <fmt/format.h>

namespace internal {
void DebugMessage(/* optional */ PluginContext* ctx,
                  const char* file,
                  int line,
                  int level,
                  const char* string);
void JobMessage(PluginContext* ctx,
                const char* file,
                int line,
                int type,
                const char* string);
};  // namespace internal

void SetupBareosApi(const filedaemon::CoreFunctions* core);
void RegisterBareosEvent(PluginContext* ctx, filedaemon::bEventType event);
void UnregisterBareosEvent(PluginContext* ctx, filedaemon::bEventType event);
bool SetBareosValue(PluginContext* ctx, filedaemon::bVariable var, void* value);
bool GetBareosValue(PluginContext* ctx, filedaemon::bVariable var, void* value);

// bRC AddExclude(PluginContext* ctx, const char* file);
// bRC AddInclude(PluginContext* ctx, const char* file);
// bRC AddOptions(PluginContext* ctx, const char* opts);
// bRC AddRegex(PluginContext* ctx, const char* item, int type);
// bRC AddWild(PluginContext* ctx, const char* item, int type);
// bRC NewOptions(PluginContext* ctx);
// bRC NewInclude(PluginContext* ctx);
// bRC NewPreInclude(PluginContext* ctx);
bool checkChanges(PluginContext* ctx,
                  const std::string& file,
                  int type,
                  const struct stat& statp,
                  time_t since);
bool AcceptFile(PluginContext* ctx,
                const std::string& file,
                const struct stat& statp);

bRC SetSeenBitmap(PluginContext* ctx, bool all, const char* fname);
bRC ClearSeenBitmap(PluginContext* ctx, bool all, const char* fname);

struct Severity {
  int severity{};
  const char* file{};
  int line{};

  Severity(int severity_,
           const char* file_ = __builtin_FILE(),
           int line_ = __builtin_LINE())
      : severity{severity_}, file{file_}, line{line_}
  {
  }
};

struct Type {
  int type{};
  const char* file{};
  int line{};

  Type(int type_,
       const char* file_ = __builtin_FILE(),
       int line_ = __builtin_LINE())
      : type{type_}, file{file_}, line{line_}
  {
  }
};

void setup_globals(const filedaemon::CoreFunctions* core);

template <typename... Args>
void DebugLog(Severity severity,
              fmt::format_string<Args...> fmt,
              Args&&... args)
{
  auto formatted = fmt::vformat(fmt, fmt::make_format_args(args...));

  internal::DebugMessage(nullptr, severity.file, severity.line,
                         severity.severity, formatted.c_str());
}

template <typename... Args>
void DebugLog(PluginContext* ctx,
              Severity severity,
              fmt::format_string<Args...> fmt,
              Args&&... args)
{
  auto formatted = fmt::vformat(fmt, fmt::make_format_args(args...));

  internal::DebugMessage(ctx, severity.file, severity.line, severity.severity,
                         formatted.c_str());
}

template <typename... Args>
void JobLog(PluginContext* ctx,
            Type type,
            fmt::format_string<Args...> fmt,
            Args&&... args)
{
  auto formatted = fmt::format(fmt, args...);

  internal::JobMessage(ctx, type.file, type.line, type.type, formatted.c_str());
}

namespace bVar {
struct JobId {
  using type = int;
  constexpr static auto value = filedaemon::bVarJobId;
};
struct FDName {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarFDName;
};
struct Level {
  using type = int;
  constexpr static auto value = filedaemon::bVarLevel;
};
struct Type {
  using type = int;
  constexpr static auto value = filedaemon::bVarType;
};
struct Client {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarClient;
};
struct JobName {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarJobName;
};
struct JobStatus {
  using type = int;
  constexpr static auto value = filedaemon::bVarJobStatus;
};
struct SinceTime {
  using type = int;
  constexpr static auto value = filedaemon::bVarSinceTime;
};
struct Accurate {
  using type = int;
  constexpr static auto value = filedaemon::bVarAccurate;
};
struct FileSeen {
  using type = bool;
  constexpr static auto value = filedaemon::bVarFileSeen;
};
struct VssClient {
  using type = void*;
  constexpr static auto value = filedaemon::bVarVssClient;
};
struct WorkingDir {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarWorkingDir;
};
struct Where {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarWhere;
};
struct RegexWhere {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarRegexWhere;
};
struct ExePath {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarExePath;
};
struct Version {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarVersion;
};
struct PrevJobName {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarPrevJobName;
};
struct PrefixLinks {
  using type = int;
  constexpr static auto value = filedaemon::bVarPrefixLinks;
};
struct CheckChanges {
  using type = bool;
  constexpr static auto value = filedaemon::bVarCheckChanges;
};
struct UsedConfig {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarUsedConfig;
};
struct PluginPath {
  using type = const char*;
  constexpr static auto value = filedaemon::bVarPluginPath;
};

template <typename T>
auto Get(PluginContext* ctx) -> std::optional<typename T::type>
{
  typename T::type value;

  if (!GetBareosValue(ctx, T::value, (void*)&value)) { return std::nullopt; }

  return value;
}

template <typename T> bool Set(PluginContext* ctx, typename T::type value)
{
  return SetBareosValue(ctx, T::value, (void*)&value);
}
};  // namespace bVar


#endif  // BAREOS_PLUGINS_FILED_GRPC_BAREOS_API_H_
