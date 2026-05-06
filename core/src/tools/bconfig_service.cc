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

#include "tools/bconfig_service.h"
#include "tools/bconfig_lib.h"

#ifdef BCONFIG_HAVE_CONSOLE
#  include "console/console_conf.h"
#endif
#include "dird/dird_conf.h"
#include "filed/filed_conf.h"
#include "include/bareos.h"
#include "include/auth_protocol_types.h"
#include "include/auth_types.h"
#include "include/ch.h"
#include "include/job_level.h"
#include "include/job_types.h"
#include "include/migration_selection_types.h"
#include "include/protocol_types.h"
#include "lib/address_conf.h"
#include "lib/alist.h"
#include "lib/bpipe.h"
#include "lib/message.h"
#include "lib/output_formatter.h"
#include "lib/output_formatter_resource.h"
#include "lib/thread_specific_data.h"
#include "stored/stored_conf.h"

#include <jansson.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <variant>

#if !HAVE_WIN32
#  include <csignal>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace bconfig::service {
namespace {

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;
constexpr int kServiceDebugLevel = 10;

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

std::string MakeDebugTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_seconds = std::chrono::system_clock::to_time_t(now);
  const auto subseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                              now.time_since_epoch())
                          % std::chrono::seconds(1);
  constexpr int kSubsecondDigits = 6;
  std::tm local{};
#if HAVE_WIN32
  localtime_s(&local, &now_seconds);
#else
  localtime_r(&now_seconds, &local);
#endif

  std::ostringstream stream;
  stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
  if constexpr (kSubsecondDigits > 0) {
    stream << '.' << std::setw(kSubsecondDigits) << std::setfill('0')
           << subseconds.count();
  }
  char offset_buffer[8]{};
  if (std::strftime(offset_buffer, sizeof(offset_buffer), "%z", &local) == 5) {
    stream << std::string_view{offset_buffer, 3} << ':'
           << std::string_view{offset_buffer + 3, 2};
  } else {
    stream << "+00:00";
  }
  return stream.str();
}

void DebugLog(std::string_view message,
              const std::source_location& location
              = std::source_location::current())
{
  if (kServiceDebugLevel > debug_level) { return; }

  const auto line = MakeDebugTimestamp() + " " + std::string{my_name} + " ("
                    + std::to_string(kServiceDebugLevel)
                    + "): " + get_basename(location.file_name()) + ":"
                    + std::to_string(location.line()) + "-"
                    + std::to_string(GetJobIdFromThreadSpecificData()) + " "
                    + std::string{message};
  p_msg(__FILE__, __LINE__, -1, "%s\n", line.c_str());
}

std::filesystem::path HomeDirectory()
{
  if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
    return home;
  }

  return {};
}

std::string QuoteCommandArgument(const std::string& value)
{
  std::string quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back('"');
  for (const char ch : value) {
    if (ch == '\\' || ch == '"') { quoted.push_back('\\'); }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

std::string BuildCommand(const std::vector<std::string>& arguments)
{
  std::ostringstream stream;
  bool first = true;
  for (const auto& argument : arguments) {
    if (!first) { stream << ' '; }
    first = false;
    stream << QuoteCommandArgument(argument);
  }
  return stream.str();
}

OperationResult<std::string> RunCommand(std::string_view command)
{
  auto* pipe = OpenBpipe(std::string{command}.c_str(), 30, "r");
  if (!pipe) {
    return {.error = "failed to execute command: " + std::string{command}};
  }

  std::string output;
  std::array<char, 4096> buffer{};
  while (pipe->rfd && fgets(buffer.data(), buffer.size(), pipe->rfd)) {
    output.append(buffer.data());
  }

  const auto status = CloseBpipe(pipe);
  if (status != 0) {
    std::ostringstream error;
    error << "command failed with status " << status << ": " << command;
    if (!output.empty()) { error << "\n" << output; }
    return {.error = error.str()};
  }

  return {.value = std::move(output)};
}

OperationResult<std::string> RunGitCommand(
    const std::filesystem::path& repository_root,
    const std::vector<std::string>& arguments)
{
  std::vector<std::string> command_arguments{"git", "-C",
                                             repository_root.string()};
  command_arguments.insert(command_arguments.end(), arguments.begin(),
                           arguments.end());
  return RunCommand(BuildCommand(command_arguments));
}

std::vector<std::string> SplitLines(const std::string& text)
{
  std::vector<std::string> lines;
  std::istringstream stream{text};
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) { lines.emplace_back(std::move(line)); }
  }
  return lines;
}

std::optional<std::string> GetEnvironmentVariable(std::string_view name)
{
  if (const char* value = std::getenv(std::string{name}.c_str());
      value && value[0] != '\0') {
    return std::string{value};
  }
  return std::nullopt;
}

bool PathExists(const std::filesystem::path& path)
{
  std::error_code error_code;
  return std::filesystem::exists(path, error_code) && !error_code;
}

bool IsExecutableFile(const std::filesystem::path& path)
{
  if (!PathExists(path)) { return false; }
#if HAVE_WIN32
  return true;
#else
  return access(path.c_str(), X_OK) == 0;
#endif
}

std::vector<std::filesystem::path> FindPathsInEnvironment(
    std::string_view executable_name)
{
  std::vector<std::filesystem::path> results;
  const auto path_environment = GetEnvironmentVariable("PATH");
  if (!path_environment) { return results; }

  std::stringstream stream{*path_environment};
  std::string segment;
  while (std::getline(stream, segment, ':')) {
    if (segment.empty()) { continue; }
    const auto candidate = std::filesystem::path{segment} / executable_name;
    if (IsExecutableFile(candidate)) { results.emplace_back(candidate); }
  }
  return results;
}

template <typename Container>
void AppendUniqueExistingDirectories(std::vector<std::filesystem::path>& target,
                                     const Container& candidates)
{
  for (const auto& candidate : candidates) {
    if (candidate.empty() || !PathExists(candidate)
        || !std::filesystem::is_directory(candidate)) {
      continue;
    }
    if (std::find(target.begin(), target.end(), candidate) == target.end()) {
      target.emplace_back(candidate);
    }
  }
}

std::vector<std::filesystem::path> DefaultStorageBackendDirectories()
{
  std::vector<std::filesystem::path> candidates;
  if (const auto override_path
      = GetEnvironmentVariable("BCONFIG_BAREOS_SD_BACKEND_DIR")) {
    AppendUniqueExistingDirectories(candidates,
                                    std::array<std::filesystem::path, 1>{
                                        std::filesystem::path{*override_path}});
  }
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  if (std::string_view{PATH_BAREOS_BACKENDDIR}.size() > 0) {
    AppendUniqueExistingDirectories(
        candidates, std::array<std::filesystem::path, 1>{
                        std::filesystem::path{PATH_BAREOS_BACKENDDIR}});
  }
#endif
#if !HAVE_WIN32
  std::error_code error_code;
  const auto self = std::filesystem::canonical("/proc/self/exe", error_code);
  if (!error_code) {
    AppendUniqueExistingDirectories(
        candidates,
        std::array<std::filesystem::path, 1>{self.parent_path().parent_path()
                                             / "stored" / "backends"});
  }
#endif
  return candidates;
}

OperationResult<std::filesystem::path> ResolveBareosExecutable(
    std::string_view environment_name,
    std::string_view executable_name,
    const std::filesystem::path& relative_build_path)
{
  if (const auto override_path = GetEnvironmentVariable(environment_name)) {
    const auto candidate = std::filesystem::path{*override_path};
    if (IsExecutableFile(candidate)) { return {.value = candidate}; }
    return {.error = "configured executable '" + candidate.string() + "' for "
                     + std::string{environment_name} + " is not executable."};
  }

#if !HAVE_WIN32
  std::error_code error_code;
  const auto self = std::filesystem::canonical("/proc/self/exe", error_code);
  if (!error_code) {
    const auto candidate
        = self.parent_path().parent_path() / relative_build_path;
    if (IsExecutableFile(candidate)) { return {.value = candidate}; }
  }
#endif

  for (const auto& candidate : FindPathsInEnvironment(executable_name)) {
    return {.value = candidate};
  }

  for (const auto& candidate :
       {std::filesystem::path{"/usr/sbin"} / executable_name,
        std::filesystem::path{"/usr/bin"} / executable_name}) {
    if (IsExecutableFile(candidate)) { return {.value = candidate}; }
  }

  return {.error = "could not locate executable '"
                   + std::string{executable_name} + "'."};
}

#if !HAVE_WIN32
struct StartedSmokeTestProcess {
  std::string label{};
  std::filesystem::path executable{};
  std::filesystem::path config_root{};
  std::filesystem::path log_path{};
  pid_t pid{-1};
};

struct ScopedPathCleanup {
  std::filesystem::path path{};

  ~ScopedPathCleanup()
  {
    if (path.empty()) { return; }
    std::error_code error_code;
    std::filesystem::remove_all(path, error_code);
  }
};

struct CatalogProvisioningTarget {
  DeploymentRecord deployment{};
  std::string director_name{};
  std::string catalog_name{};
  DirectorCatalogResourceSpec catalog_spec{};
};

bool WriteFile(const std::filesystem::path& path, const std::string& content);
OperationResult<std::string> ReadFile(const std::filesystem::path& path);
std::vector<DeploymentConfigRecord> DiscoverDeploymentConfigRoots(
    const std::filesystem::path& repository_root);

std::string TrimTrailingNewlines(std::string text)
{
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.pop_back();
  }
  return text;
}

std::string TrimAsciiWhitespace(std::string text)
{
  const auto is_whitespace = [](unsigned char ch) { return std::isspace(ch); };
  auto begin = std::find_if_not(text.begin(), text.end(), is_whitespace);
  auto end = std::find_if_not(text.rbegin(), text.rend(), is_whitespace).base();
  if (begin >= end) { return {}; }
  return std::string(begin, end);
}

std::string EscapeSqlLiteral(std::string_view value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    escaped.push_back(ch);
    if (ch == '\'') { escaped.push_back('\''); }
  }
  return escaped;
}

void ReplaceAll(std::string& text,
                std::string_view needle,
                std::string_view replacement)
{
  size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
}

OperationResult<std::filesystem::path> ResolveCatalogHelperScript(
    std::string_view environment_name,
    std::string_view script_name,
    const std::filesystem::path& relative_build_path,
    bool& used_override)
{
  used_override = false;
  if (const auto override_path = GetEnvironmentVariable(environment_name)) {
    used_override = true;
    const auto candidate = std::filesystem::path{*override_path};
    if (IsExecutableFile(candidate)) { return {.value = candidate}; }
    return {.error = "configured script '" + candidate.string() + "' for "
                     + std::string{environment_name} + " is not executable."};
  }

  return ResolveBareosExecutable(environment_name, script_name,
                                 relative_build_path);
}

OperationResult<std::filesystem::path> ResolveCatalogConfigLibPath()
{
  if (const auto override_path
      = GetEnvironmentVariable("BCONFIG_BAREOS_CONFIG_LIB")) {
    const auto candidate = std::filesystem::path{*override_path};
    if (PathExists(candidate)) { return {.value = candidate}; }
    return {.error = "configured config library '" + candidate.string()
                     + "' for BCONFIG_BAREOS_CONFIG_LIB does not exist."};
  }

  std::error_code error_code;
  const auto self = std::filesystem::canonical("/proc/self/exe", error_code);
  if (!error_code) {
    const auto base = self.parent_path().parent_path();
    for (const auto& candidate :
         {base / "scripts" / "bareos-config-lib.sh",
          base.parent_path() / "scripts" / "bareos-config-lib.sh"}) {
      if (PathExists(candidate)) { return {.value = candidate}; }
    }
  }

  return {.error = "could not locate bareos-config-lib.sh."};
}

OperationResult<std::filesystem::path> ResolveCatalogSqlDdlDirectory()
{
  if (const auto override_path
      = GetEnvironmentVariable("BCONFIG_BAREOS_SQL_DDL_DIR")) {
    const auto candidate = std::filesystem::path{*override_path};
    if (PathExists(candidate) && std::filesystem::is_directory(candidate)) {
      return {.value = candidate};
    }
    return {.error = "configured SQL DDL directory '" + candidate.string()
                     + "' for BCONFIG_BAREOS_SQL_DDL_DIR is not a directory."};
  }

  std::error_code error_code;
  const auto self = std::filesystem::canonical("/proc/self/exe", error_code);
  if (!error_code) {
    const auto base = self.parent_path().parent_path();
    const auto repo_root = base.parent_path().parent_path().parent_path();
    for (const auto& candidate :
         {repo_root / "core" / "src" / "cats" / "ddl", base / "cats" / "ddl",
          base / "src" / "cats" / "ddl",
          base.parent_path() / "src" / "cats" / "ddl"}) {
      if (PathExists(candidate) && std::filesystem::is_directory(candidate)) {
        return {.value = candidate};
      }
    }
  }

  return {.error = "could not locate Bareos SQL DDL directory."};
}

OperationResult<std::filesystem::path> PrepareCatalogHelperScript(
    const std::filesystem::path& script_path,
    const std::filesystem::path& config_lib_path,
    const std::filesystem::path& staging_directory)
{
  auto script_content = ReadFile(script_path);
  if (!script_content) { return {.error = script_content.error}; }

  const std::string original_source
      = ". \"/usr/local/lib/bareos/scripts\"/bareos-config-lib.sh";
  const std::string replacement_source
      = ". " + QuoteCommandArgument(config_lib_path.string());
  ReplaceAll(*script_content.value, original_source, replacement_source);

  const auto staged_path = staging_directory / script_path.filename();
  if (!WriteFile(staged_path, *script_content.value)) {
    return {.error = "failed to write temporary helper script '"
                     + staged_path.string() + "'."};
  }

  std::error_code error_code;
  std::filesystem::permissions(
      staged_path,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
          | std::filesystem::perms::owner_exec,
      std::filesystem::perm_options::replace, error_code);
  if (error_code) {
    return {.error = "failed to make temporary helper script executable '"
                     + staged_path.string() + "': " + error_code.message()};
  }

  return {.value = staged_path};
}

OperationResult<CatalogProvisioningTarget> ResolveCatalogProvisioningTarget(
    const ServiceState& state,
    const JobRecord& job_snapshot)
{
  if (!job_snapshot.deployment_id) {
    return {.error = "deployment_id is required"};
  }

  auto deployment = state.GetDeployment(*job_snapshot.deployment_id);
  if (!deployment) { return {.error = "deployment not found"}; }

  std::vector<DeploymentConfigRecord> director_configs;
  for (const auto& config :
       DiscoverDeploymentConfigRoots(deployment->repository_path)) {
    if (config.component == bconfig::Component::kDirector) {
      director_configs.emplace_back(config);
    }
  }

  if (director_configs.empty()) {
    return {.error = "no director config found in deployment repo"};
  }
  if (director_configs.size() != 1) {
    return {.error
            = "catalog initialization requires exactly one director "
              "config root"};
  }

  const auto& director_config = director_configs.front();
  const auto catalog_directory
      = director_config.path / "bareos-dir.d" / "catalog";
  if (!std::filesystem::is_directory(catalog_directory)) {
    return {.error = "director '" + director_config.name
                     + "' does not define a catalog directory."};
  }

  std::vector<std::string> catalog_names;
  for (const auto& entry :
       std::filesystem::directory_iterator(catalog_directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".conf") {
      continue;
    }
    catalog_names.emplace_back(entry.path().stem().string());
  }

  if (catalog_names.empty()) {
    return {.error = "director '" + director_config.name
                     + "' does not define any catalog resource files."};
  }
  std::sort(catalog_names.begin(), catalog_names.end());
  if (catalog_names.size() != 1) {
    return {.error
            = "catalog initialization requires exactly one catalog "
              "resource file"};
  }

  auto catalog_spec = state.GetDirectorCatalogResourceSpec(
      *job_snapshot.deployment_id, director_config.name, catalog_names.front());
  if (!catalog_spec) { return {.error = catalog_spec.error}; }
  if (!catalog_spec.value->db_name || catalog_spec.value->db_name->empty()) {
    return {.error = "director catalog '" + catalog_names.front()
                     + "' does not define DbName."};
  }
  if (!catalog_spec.value->db_user || catalog_spec.value->db_user->empty()) {
    return {.error = "director catalog '" + catalog_names.front()
                     + "' does not define DbUser."};
  }
  if (!catalog_spec.value->db_password) {
    return {.error = "director catalog '" + catalog_names.front()
                     + "' does not define DbPassword."};
  }

  return {.value = CatalogProvisioningTarget{
              .deployment = *deployment,
              .director_name = director_config.name,
              .catalog_name = catalog_names.front(),
              .catalog_spec = *catalog_spec.value,
          }};
}

std::vector<std::string> BuildCatalogCommandPrefix(
    const DirectorCatalogResourceSpec& catalog_spec,
    const std::filesystem::path& sql_ddl_directory)
{
  std::vector<std::string> command{
      "env", "SQL_DDL_DIR=" + sql_ddl_directory.string(),
      "db_name=" + *catalog_spec.db_name, "db_user=" + *catalog_spec.db_user,
      "db_password=" + *catalog_spec.db_password};
  if (catalog_spec.db_socket && !catalog_spec.db_socket->empty()) {
    command.emplace_back("PGHOST=" + *catalog_spec.db_socket);
  } else if (catalog_spec.db_address && !catalog_spec.db_address->empty()) {
    command.emplace_back("PGHOST=" + *catalog_spec.db_address);
  }
  if (catalog_spec.db_port) {
    command.emplace_back("PGPORT=" + std::to_string(*catalog_spec.db_port));
  }
  return command;
}

OperationResult<std::string> RunCatalogPsqlQuery(
    const std::filesystem::path& psql_path,
    const DirectorCatalogResourceSpec& catalog_spec,
    std::string_view database_name,
    std::string_view query)
{
  auto command
      = BuildCatalogCommandPrefix(catalog_spec, std::filesystem::path{});
  command.erase(std::remove_if(command.begin(), command.end(),
                               [](const std::string& value) {
                                 return value.rfind("SQL_DDL_DIR=", 0) == 0;
                               }),
                command.end());
  command.emplace_back(psql_path.string());
  command.emplace_back("--no-psqlrc");
  command.emplace_back("--tuples-only");
  command.emplace_back("--no-align");
  command.emplace_back("-d");
  command.emplace_back(std::string{database_name});
  command.emplace_back("--command");
  command.emplace_back(std::string{query});
  return RunCommand(BuildCommand(command));
}

OperationResult<bool> CatalogQueryHasRow(
    const std::filesystem::path& psql_path,
    const DirectorCatalogResourceSpec& spec,
    std::string_view database_name,
    std::string_view query)
{
  auto result = RunCatalogPsqlQuery(psql_path, spec, database_name, query);
  if (!result) { return {.error = result.error}; }
  for (const auto& line : SplitLines(*result.value)) {
    if (TrimAsciiWhitespace(line) == "1") { return {.value = true}; }
  }
  return {.value = false};
}

OperationResult<std::monostate> RunCatalogHelperScript(
    const std::filesystem::path& script_path,
    const DirectorCatalogResourceSpec& catalog_spec,
    const std::filesystem::path& sql_ddl_directory)
{
  auto command = BuildCatalogCommandPrefix(catalog_spec, sql_ddl_directory);
  command.emplace_back(script_path.string());
  auto result = RunCommand(BuildCommand(command));
  if (!result) { return {.error = result.error}; }
  return {.value = std::monostate{}};
}

OperationResult<std::monostate> ApplyCatalogPrivilegesForExistingUser(
    const std::filesystem::path& psql_path,
    const DirectorCatalogResourceSpec& catalog_spec,
    const std::filesystem::path& sql_ddl_directory,
    const std::filesystem::path& staging_directory)
{
  auto grant_template
      = ReadFile(sql_ddl_directory / "grants" / "postgresql.sql");
  if (!grant_template) { return {.error = grant_template.error}; }

  auto translated = *grant_template.value;
  ReplaceAll(translated, "@DB_NAME@", *catalog_spec.db_name);
  ReplaceAll(translated, "@DB_USER@", *catalog_spec.db_user);
  ReplaceAll(
      translated, "@DB_PASS@",
      catalog_spec.db_password->empty()
          ? std::string{}
          : "PASSWORD '" + EscapeSqlLiteral(*catalog_spec.db_password) + "'");
  ReplaceAll(translated, "@DB_VERSION@", "2250");

  std::ostringstream filtered;
  std::istringstream input(translated);
  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind("CREATE USER ", 0) == 0) { continue; }
    filtered << line << "\n";
  }

  const auto grants_path = staging_directory / "grant-existing-user.sql";
  if (!WriteFile(grants_path, filtered.str())) {
    return {.error = "failed to write temporary grants file '"
                     + grants_path.string() + "'."};
  }

  auto command = BuildCatalogCommandPrefix(catalog_spec, sql_ddl_directory);
  command.erase(std::remove_if(command.begin(), command.end(),
                               [](const std::string& value) {
                                 return value.rfind("db_password=", 0) == 0
                                        || value.rfind("db_user=", 0) == 0
                                        || value.rfind("db_name=", 0) == 0
                                        || value.rfind("SQL_DDL_DIR=", 0) == 0;
                               }),
                command.end());
  command.emplace_back(psql_path.string());
  command.emplace_back("--no-psqlrc");
  command.emplace_back("-d");
  command.emplace_back(*catalog_spec.db_name);
  command.emplace_back("-f");
  command.emplace_back(grants_path.string());
  auto result = RunCommand(BuildCommand(command));
  if (!result) { return {.error = result.error}; }
  return {.value = std::monostate{}};
}

std::pair<JobStatus, std::vector<std::string>> ExecuteInitializeCatalogDatabase(
    const ServiceState& state,
    const JobRecord& job_snapshot)
{
  std::vector<std::string> logs;

  auto target = ResolveCatalogProvisioningTarget(state, job_snapshot);
  if (!target) {
    logs.emplace_back(target.error);
    return {JobStatus::kFailed, logs};
  }

  logs.emplace_back("initializing catalog '" + target.value->catalog_name
                    + "' for director '" + target.value->director_name + "'");

  auto psql_path = ResolveBareosExecutable(
      "BCONFIG_PSQL_BINARY", "psql", std::filesystem::path{"bin"} / "psql");
  if (!psql_path) {
    logs.emplace_back(psql_path.error);
    return {JobStatus::kFailed, logs};
  }

  bool create_script_override = false;
  auto create_database_script = ResolveCatalogHelperScript(
      "BCONFIG_BAREOS_CREATE_DATABASE_SCRIPT", "create_bareos_database",
      std::filesystem::path{"cats"} / "create_bareos_database",
      create_script_override);
  if (!create_database_script) {
    logs.emplace_back(create_database_script.error);
    return {JobStatus::kFailed, logs};
  }

  bool make_tables_script_override = false;
  auto make_tables_script = ResolveCatalogHelperScript(
      "BCONFIG_BAREOS_MAKE_TABLES_SCRIPT", "make_bareos_tables",
      std::filesystem::path{"cats"} / "make_bareos_tables",
      make_tables_script_override);
  if (!make_tables_script) {
    logs.emplace_back(make_tables_script.error);
    return {JobStatus::kFailed, logs};
  }

  bool grant_script_override = false;
  auto grant_privileges_script = ResolveCatalogHelperScript(
      "BCONFIG_BAREOS_GRANT_PRIVILEGES_SCRIPT", "grant_bareos_privileges",
      std::filesystem::path{"cats"} / "grant_bareos_privileges",
      grant_script_override);
  if (!grant_privileges_script) {
    logs.emplace_back(grant_privileges_script.error);
    return {JobStatus::kFailed, logs};
  }

  auto sql_ddl_directory = ResolveCatalogSqlDdlDirectory();
  if (!sql_ddl_directory) {
    logs.emplace_back(sql_ddl_directory.error);
    return {JobStatus::kFailed, logs};
  }

  const auto staging_directory
      = std::filesystem::temp_directory_path()
        / ("bconfig-catalog-" + job_snapshot.id + "-"
           + std::to_string(
               static_cast<unsigned long long>(std::time(nullptr))));
  std::error_code error_code;
  std::filesystem::create_directories(staging_directory, error_code);
  if (error_code) {
    logs.emplace_back("failed to create temporary staging directory '"
                      + staging_directory.string()
                      + "': " + error_code.message());
    return {JobStatus::kFailed, logs};
  }
  ScopedPathCleanup cleanup{staging_directory};

  if (!create_script_override || !make_tables_script_override
      || !grant_script_override) {
    auto config_lib_path = ResolveCatalogConfigLibPath();
    if (!config_lib_path) {
      logs.emplace_back(config_lib_path.error);
      return {JobStatus::kFailed, logs};
    }

    if (!create_script_override) {
      auto staged = PrepareCatalogHelperScript(*create_database_script.value,
                                               *config_lib_path.value,
                                               staging_directory);
      if (!staged) {
        logs.emplace_back(staged.error);
        return {JobStatus::kFailed, logs};
      }
      create_database_script = std::move(staged);
    }
    if (!make_tables_script_override) {
      auto staged = PrepareCatalogHelperScript(
          *make_tables_script.value, *config_lib_path.value, staging_directory);
      if (!staged) {
        logs.emplace_back(staged.error);
        return {JobStatus::kFailed, logs};
      }
      make_tables_script = std::move(staged);
    }
    if (!grant_script_override) {
      auto staged = PrepareCatalogHelperScript(*grant_privileges_script.value,
                                               *config_lib_path.value,
                                               staging_directory);
      if (!staged) {
        logs.emplace_back(staged.error);
        return {JobStatus::kFailed, logs};
      }
      grant_privileges_script = std::move(staged);
    }
  }

  const auto quoted_db_user
      = EscapeSqlLiteral(*target.value->catalog_spec.db_user);
  const auto quoted_db_name
      = EscapeSqlLiteral(*target.value->catalog_spec.db_name);
  auto role_exists = CatalogQueryHasRow(
      *psql_path.value, target.value->catalog_spec, "template1",
      "SELECT 1 FROM pg_catalog.pg_roles WHERE rolname = '" + quoted_db_user
          + "';");
  if (!role_exists) {
    logs.emplace_back(role_exists.error);
    return {JobStatus::kFailed, logs};
  }

  auto database_exists = CatalogQueryHasRow(
      *psql_path.value, target.value->catalog_spec, "template1",
      "SELECT 1 FROM pg_catalog.pg_database WHERE datname = '" + quoted_db_name
          + "';");
  if (!database_exists) {
    logs.emplace_back(database_exists.error);
    return {JobStatus::kFailed, logs};
  }

  if (!*database_exists.value) {
    auto create_result = RunCatalogHelperScript(*create_database_script.value,
                                                target.value->catalog_spec,
                                                *sql_ddl_directory.value);
    if (!create_result) {
      logs.emplace_back(create_result.error);
      return {JobStatus::kFailed, logs};
    }
    logs.emplace_back("created database '" + *target.value->catalog_spec.db_name
                      + "'");
  } else {
    logs.emplace_back("database '" + *target.value->catalog_spec.db_name
                      + "' already exists");
  }

  auto schema_exists = CatalogQueryHasRow(
      *psql_path.value, target.value->catalog_spec,
      *target.value->catalog_spec.db_name,
      "SELECT 1 FROM information_schema.tables WHERE table_schema = 'public' "
      "AND table_name = 'version';");
  if (!schema_exists) {
    logs.emplace_back(schema_exists.error);
    return {JobStatus::kFailed, logs};
  }

  if (!*schema_exists.value) {
    auto make_result = RunCatalogHelperScript(*make_tables_script.value,
                                              target.value->catalog_spec,
                                              *sql_ddl_directory.value);
    if (!make_result) {
      logs.emplace_back(make_result.error);
      return {JobStatus::kFailed, logs};
    }
    logs.emplace_back("created catalog tables in '"
                      + *target.value->catalog_spec.db_name + "'");
  } else {
    logs.emplace_back("catalog tables already exist in '"
                      + *target.value->catalog_spec.db_name + "'");
  }

  if (!*role_exists.value) {
    auto grant_result = RunCatalogHelperScript(*grant_privileges_script.value,
                                               target.value->catalog_spec,
                                               *sql_ddl_directory.value);
    if (!grant_result) {
      logs.emplace_back(grant_result.error);
      return {JobStatus::kFailed, logs};
    }
    logs.emplace_back("created database user '"
                      + *target.value->catalog_spec.db_user
                      + "' and granted catalog privileges");
  } else {
    auto grant_result = ApplyCatalogPrivilegesForExistingUser(
        *psql_path.value, target.value->catalog_spec, *sql_ddl_directory.value,
        staging_directory);
    if (!grant_result) {
      logs.emplace_back(grant_result.error);
      return {JobStatus::kFailed, logs};
    }
    logs.emplace_back("granted catalog privileges to existing database user '"
                      + *target.value->catalog_spec.db_user + "'");
  }

  return {JobStatus::kSucceeded, logs};
}

void AppendSmokeTestLogFile(std::vector<std::string>& logs,
                            const StartedSmokeTestProcess& process)
{
  std::ifstream stream{process.log_path};
  if (!stream) { return; }
  const auto output
      = TrimTrailingNewlines(std::string{std::istreambuf_iterator<char>(stream),
                                         std::istreambuf_iterator<char>()});
  if (output.empty()) { return; }
  for (const auto& line : SplitLines(output)) {
    logs.emplace_back(process.label + ": " + line);
  }
}

OperationResult<StartedSmokeTestProcess> StartSmokeTestProcess(
    std::string_view label,
    const std::filesystem::path& executable,
    const std::filesystem::path& config_root)
{
  StartedSmokeTestProcess process{};
  process.label = std::string{label};
  process.executable = executable;
  process.config_root = config_root;
  process.log_path
      = std::filesystem::temp_directory_path()
        / ("bconfig-smoketest-" + process.label + "-"
           + std::to_string(static_cast<unsigned long long>(std::time(nullptr)))
           + ".log");

  const auto log_fd = open(process.log_path.c_str(),
                           O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0600);
  if (log_fd < 0) {
    return {.error = "failed to open smoke-test log file '"
                     + process.log_path.string() + "'."};
  }

  process.pid = fork();
  if (process.pid < 0) {
    close(log_fd);
    return {.error
            = "failed to fork smoke-test process for " + process.label + "."};
  }
  if (process.pid == 0) {
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    close(log_fd);
    execl(process.executable.c_str(), process.executable.c_str(), "-f", "-c",
          process.config_root.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  close(log_fd);
  return {.value = std::move(process)};
}

OperationResult<std::monostate> WaitForSmokeTestStartup(
    const StartedSmokeTestProcess& process,
    std::vector<std::string>& logs)
{
  for (int attempt = 0; attempt < 20; ++attempt) {
    int status = 0;
    const auto waited = waitpid(process.pid, &status, WNOHANG);
    if (waited == process.pid) {
      AppendSmokeTestLogFile(logs, process);
      return {.error = process.label + " exited before startup completed."};
    }
    if (waited < 0) {
      return {.error = "failed to wait for " + process.label + " process."};
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  logs.emplace_back("started " + process.label + " using "
                    + process.executable.string());
  return {.value = std::monostate{}};
}

std::optional<std::string> StopSmokeTestProcess(
    const StartedSmokeTestProcess& process,
    std::vector<std::string>& logs)
{
  if (process.pid <= 0) { return std::nullopt; }

  kill(process.pid, SIGTERM);
  int status = 0;
  for (int attempt = 0; attempt < 40; ++attempt) {
    const auto waited = waitpid(process.pid, &status, WNOHANG);
    if (waited == process.pid) {
      AppendSmokeTestLogFile(logs, process);
      logs.emplace_back("stopped " + process.label);
      return std::nullopt;
    }
    if (waited < 0) {
      return "failed to wait for " + process.label + " during shutdown.";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  kill(process.pid, SIGKILL);
  if (waitpid(process.pid, &status, 0) < 0) {
    return "failed to reap " + process.label + " after SIGKILL.";
  }
  AppendSmokeTestLogFile(logs, process);
  logs.emplace_back("force-stopped " + process.label);
  return std::nullopt;
}
#endif

OperationResult<DeploymentDiffPreviewRecord> LoadDeploymentDiffPreview(
    const std::filesystem::path& repository_root)
{
  const auto git_directory = repository_root / ".git";
  if (!std::filesystem::exists(git_directory)) {
    return {.value = DeploymentDiffPreviewRecord{}};
  }

  auto diff = RunGitCommand(repository_root,
                            {"diff", "--no-ext-diff", "--binary", "HEAD"});
  if (!diff) { return {.error = diff.error}; }

  auto untracked = RunGitCommand(
      repository_root, {"ls-files", "--others", "--exclude-standard"});
  if (!untracked) { return {.error = untracked.error}; }

  DeploymentDiffPreviewRecord preview{
      .initialized = true,
      .has_changes = !diff.value->empty() || !untracked.value->empty(),
      .diff = std::move(*diff.value),
      .untracked_files = SplitLines(*untracked.value),
  };
  return {.value = std::move(preview)};
}

std::optional<DeploymentRecord> FindDeploymentLocked(
    const std::unordered_map<std::string, DeploymentRecord>& deployments,
    std::string_view deployment_id)
{
  auto it = deployments.find(std::string{deployment_id});
  if (it == deployments.end()) { return std::nullopt; }
  return it->second;
}

OperationResult<std::string> RunGitCommand(
    const std::filesystem::path& repository_root,
    std::initializer_list<std::string_view> arguments)
{
  std::vector<std::string> owned_arguments;
  owned_arguments.reserve(arguments.size());
  for (const auto argument : arguments) {
    owned_arguments.emplace_back(argument);
  }
  return RunGitCommand(repository_root, owned_arguments);
}

bool WriteFile(const std::filesystem::path& path, const std::string& content)
{
  std::ofstream stream(path, std::ios::out | std::ios::trunc);
  if (!stream) { return false; }
  stream << content;
  return static_cast<bool>(stream);
}

OperationResult<std::string> ReadFile(const std::filesystem::path& path)
{
  std::ifstream stream(path);
  if (!stream) {
    return {.error = "failed to read file '" + path.string() + "'."};
  }

  return {.value = std::string{std::istreambuf_iterator<char>(stream),
                               std::istreambuf_iterator<char>()}};
}

std::filesystem::path RepositoryRootFromConfigPath(
    const std::filesystem::path& config_root)
{
  return config_root.parent_path().parent_path();
}

OperationResult<std::set<std::string>> LoadManagedPaths(
    const std::filesystem::path& repository_root)
{
  const auto ownership_path = RepositoryLayout::OwnershipPath(repository_root);
  if (!std::filesystem::exists(ownership_path)) {
    return {.value = std::set<std::string>{}};
  }

  json_error_t json_error{};
  auto root = MakeJson(json_load_file(ownership_path.c_str(), 0, &json_error));
  if (!root) {
    return {.error = "failed to parse ownership file '"
                     + ownership_path.string() + "': " + json_error.text};
  }
  if (!json_is_object(root.get())) {
    return {.error = "ownership file '" + ownership_path.string()
                     + "' must contain a JSON object."};
  }

  std::set<std::string> managed_paths;
  auto* managed_files = json_object_get(root.get(), "managed_files");
  if (!managed_files) { return {.value = std::move(managed_paths)}; }
  if (!json_is_array(managed_files)) {
    return {.error = "ownership file '" + ownership_path.string()
                     + "' contains a non-array 'managed_files' field."};
  }

  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(managed_files, index, entry)
  {
    if (!json_is_string(entry)) {
      return {.error = "ownership file '" + ownership_path.string()
                       + "' contains a non-string managed file entry."};
    }
    managed_paths.emplace(json_string_value(entry));
  }

  return {.value = std::move(managed_paths)};
}

std::optional<std::string> WriteManagedPaths(
    const std::filesystem::path& repository_root,
    const std::set<std::string>& managed_paths)
{
  const auto ownership_path = RepositoryLayout::OwnershipPath(repository_root);
  auto root = MakeJson(json_object());
  auto managed_files = MakeJson(json_array());
  for (const auto& path : managed_paths) {
    json_array_append_new(managed_files.get(), json_string(path.c_str()));
  }
  json_object_set_new(root.get(), "managed_files", managed_files.release());

  char* dump = json_dumps(root.get(), JSON_INDENT(2));
  if (!dump) {
    return "failed to serialize ownership file '" + ownership_path.string()
           + "'.";
  }

  std::string content{dump};
  std::free(dump);
  content.push_back('\n');
  if (!WriteFile(ownership_path, content)) {
    return "failed to write ownership file '" + ownership_path.string() + "'.";
  }
  return std::nullopt;
}

bool IsManagedPath(const std::set<std::string>& managed_paths,
                   const std::filesystem::path& repository_root,
                   const std::filesystem::path& path)
{
  std::error_code error_code;
  const auto relative
      = std::filesystem::relative(path, repository_root, error_code);
  if (error_code) { return false; }
  return managed_paths.contains(relative.generic_string());
}

void SetManagedPath(std::set<std::string>& managed_paths,
                    const std::filesystem::path& repository_root,
                    const std::filesystem::path& path)
{
  std::error_code error_code;
  const auto relative
      = std::filesystem::relative(path, repository_root, error_code);
  if (!error_code) { managed_paths.emplace(relative.generic_string()); }
}

void RemoveManagedPath(std::set<std::string>& managed_paths,
                       const std::filesystem::path& repository_root,
                       const std::filesystem::path& path)
{
  std::error_code error_code;
  const auto relative
      = std::filesystem::relative(path, repository_root, error_code);
  if (!error_code) { managed_paths.erase(relative.generic_string()); }
}

template <typename T>
std::vector<T> SortedValues(const std::unordered_map<std::string, T>& values)
{
  std::vector<T> result;
  result.reserve(values.size());
  for (const auto& [_, value] : values) { result.emplace_back(value); }

  std::sort(result.begin(), result.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
  return result;
}

struct ImportedComponent {
  bconfig::Component component;
  std::string resource_name{};
  std::filesystem::path destination_root{};
};

std::filesystem::path ComponentConfigDirectoryName(bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return "bareos-dir.d";
    case bconfig::Component::kStorage:
      return "bareos-sd.d";
    case bconfig::Component::kClient:
      return "bareos-fd.d";
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return {};
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case bconfig::Component::kTrayMonitor:
      return {};
#endif
  }

  return {};
}

std::filesystem::path ComponentDefaultConfigFilename(
    bconfig::Component component)
{
  switch (component) {
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return console::default_config_filename;
#endif
    default:
      break;
  }

  return {};
}

const char* PrimaryResourceType(bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return "Director";
    case bconfig::Component::kStorage:
      return "Storage";
    case bconfig::Component::kClient:
      return "Client";
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return "Console";
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case bconfig::Component::kTrayMonitor:
      return "TrayMonitor";
#endif
  }

  return "unknown";
}

std::filesystem::path ComponentBucketDirectory(
    const std::filesystem::path& repository_root,
    bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return RepositoryLayout::DirectorsDirectory(repository_root);
    case bconfig::Component::kStorage:
      return RepositoryLayout::StoragesDirectory(repository_root);
    case bconfig::Component::kClient:
      return RepositoryLayout::ClientsDirectory(repository_root);
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return RepositoryLayout::ConsolesDirectory(repository_root);
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case bconfig::Component::kTrayMonitor:
      return {};
#endif
  }

  return {};
}

void AppendParseMessages(std::vector<std::string>& logs,
                         const bconfig::ParseMessages& messages,
                         std::string_view prefix)
{
  for (const auto& warning : messages.warnings) {
    logs.emplace_back(std::string{prefix} + "warning: " + warning);
  }
  for (const auto& error : messages.errors) {
    logs.emplace_back(std::string{prefix} + "error: " + error);
  }
}

std::vector<bconfig::Component> SupportedImportComponents()
{
  std::vector<bconfig::Component> components{bconfig::Component::kDirector,
                                             bconfig::Component::kStorage,
                                             bconfig::Component::kClient};
#ifdef BCONFIG_HAVE_CONSOLE
  components.push_back(bconfig::Component::kConsole);
#endif
  return components;
}

std::optional<std::filesystem::path> ResolveImportRoot(
    const std::filesystem::path& source_path,
    std::string& error)
{
  std::error_code error_code;
  const auto normalized
      = std::filesystem::weakly_canonical(source_path, error_code);
  if (error_code || !std::filesystem::exists(normalized)) {
    error = "source path does not exist: " + source_path.string();
    return std::nullopt;
  }

  if (!std::filesystem::is_directory(normalized)) {
    error = "source path must be a directory.";
    return std::nullopt;
  }

  for (const auto component : SupportedImportComponents()) {
    const auto config_directory_name = ComponentConfigDirectoryName(component);
    if (!config_directory_name.empty()
        && normalized.filename() == config_directory_name) {
      return normalized.parent_path();
    }

    const auto config_filename = ComponentDefaultConfigFilename(component);
    if (!config_filename.empty() && normalized.filename() == config_filename) {
      return normalized.parent_path();
    }
  }

  return normalized;
}

std::optional<std::filesystem::path> FindComponentConfigDirectory(
    bconfig::Component component,
    const std::filesystem::path& source_root)
{
#ifdef BCONFIG_HAVE_CONSOLE
  if (component == bconfig::Component::kConsole) {
    const auto root_config
        = source_root / ComponentDefaultConfigFilename(component);
    if (std::filesystem::is_regular_file(root_config)) { return source_root; }
    return std::nullopt;
  }
#endif

  const auto config_directory_name = ComponentConfigDirectoryName(component);
  if (config_directory_name.empty()) { return std::nullopt; }

  const auto config_directory = source_root / config_directory_name;
  if (std::filesystem::is_directory(config_directory)) {
    return config_directory;
  }

  return std::nullopt;
}

std::vector<DeploymentConfigRecord> DiscoverDeploymentConfigRoots(
    const std::filesystem::path& repository_root)
{
  std::vector<DeploymentConfigRecord> configs;

  for (const auto component : SupportedImportComponents()) {
    const auto bucket = ComponentBucketDirectory(repository_root, component);
    const auto config_directory_name = ComponentConfigDirectoryName(component);
    if (bucket.empty() || !std::filesystem::is_directory(bucket)) { continue; }

    for (const auto& entry : std::filesystem::directory_iterator(bucket)) {
      if (!entry.is_directory()) { continue; }

#ifdef BCONFIG_HAVE_CONSOLE
      if (component == bconfig::Component::kConsole) {
        const auto root_config
            = entry.path() / ComponentDefaultConfigFilename(component);
        if (!std::filesystem::is_regular_file(root_config)) { continue; }
      } else
#endif
      {
        if (config_directory_name.empty()
            || !std::filesystem::is_directory(entry.path()
                                              / config_directory_name)) {
          continue;
        }
      }

      configs.push_back(DeploymentConfigRecord{
          .component = component,
          .name = entry.path().filename().string(),
          .path = entry.path(),
      });
    }
  }

  std::sort(configs.begin(), configs.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.component != rhs.component) {
                return std::string{bconfig::ComponentToString(lhs.component)}
                       < std::string{bconfig::ComponentToString(rhs.component)};
              }
              return lhs.name < rhs.name;
            });
  return configs;
}

std::filesystem::path DeploymentConfigRootPath(
    const std::filesystem::path& repository_root,
    bconfig::Component component,
    std::string_view name)
{
  return ComponentBucketDirectory(repository_root, component)
         / std::string{name};
}

bool HasManagedConfigFiles(const DeploymentConfigRecord& config)
{
#ifdef BCONFIG_HAVE_CONSOLE
  if (config.component == bconfig::Component::kConsole) {
    return std::filesystem::is_regular_file(
        config.path / ComponentDefaultConfigFilename(config.component));
  }
#endif

  const auto config_directory_name
      = ComponentConfigDirectoryName(config.component);
  if (config_directory_name.empty()) { return false; }

  const auto config_directory = config.path / config_directory_name;
  if (!std::filesystem::is_directory(config_directory)) { return false; }

  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(config_directory)) {
    if (!entry.is_regular_file()) { continue; }
    if (entry.path().extension() == ".conf") { return true; }
  }

  return false;
}

OperationResult<DeploymentConfigRecord> EnsureDeploymentConfigRoot(
    const ServiceState& state,
    std::string_view deployment_id,
    bconfig::Component component,
    std::string_view name)
{
  auto existing = state.GetDeploymentConfig(deployment_id, component, name);
  if (existing) { return existing; }
  if (existing.error != "deployment config not found.") {
    return {.error = existing.error};
  }

  const auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) { return {.error = "deployment not found."}; }

  const auto config_root
      = DeploymentConfigRootPath(deployment->repository_path, component, name);
  std::error_code error_code;
  std::filesystem::create_directories(config_root, error_code);
  if (error_code) {
    return {.error = "failed to create deployment config root '"
                     + config_root.string() + "': " + error_code.message()};
  }

#ifdef BCONFIG_HAVE_CONSOLE
  if (component == bconfig::Component::kConsole) {
    const auto config_file
        = config_root / ComponentDefaultConfigFilename(component);
    if (!std::filesystem::exists(config_file) && !WriteFile(config_file, "")) {
      return {.error = "failed to write console config root '"
                       + config_file.string() + "'."};
    }
  } else
#endif
  {
    const auto config_directory_name = ComponentConfigDirectoryName(component);
    if (config_directory_name.empty()) {
      return {.error = "unsupported component for deployment config root."};
    }
    std::filesystem::create_directories(config_root / config_directory_name,
                                        error_code);
    if (error_code) {
      return {.error = "failed to create deployment config directory '"
                       + (config_root / config_directory_name).string()
                       + "': " + error_code.message()};
    }
  }

  return {.value = DeploymentConfigRecord{.component = component,
                                          .name = std::string{name},
                                          .path = config_root}};
}

std::optional<std::string> DetectPrimaryResourceName(
    bconfig::Component component,
    bconfig::LoadedConfig& loaded)
{
  const auto primary_type = std::string{PrimaryResourceType(component)};
  for (const auto& resource : bconfig::CollectResources(*loaded.parser)) {
    if (resource.type == primary_type && !resource.name.empty()) {
      return resource.name;
    }
  }

  return std::nullopt;
}

std::optional<std::string> CopyDirectoryTree(
    const std::filesystem::path& source,
    const std::filesystem::path& target)
{
  std::error_code error_code;
  std::filesystem::create_directories(target.parent_path(), error_code);
  if (error_code) {
    return "failed to create directory '" + target.parent_path().string()
           + "': " + error_code.message();
  }

  std::filesystem::copy(source, target,
                        std::filesystem::copy_options::recursive, error_code);
  if (error_code) {
    return "failed to copy '" + source.string() + "' to '" + target.string()
           + "': " + error_code.message();
  }

  return std::nullopt;
}

std::optional<std::string> CopyFileTree(const std::filesystem::path& source,
                                        const std::filesystem::path& target)
{
  std::error_code error_code;
  std::filesystem::create_directories(target.parent_path(), error_code);
  if (error_code) {
    return "failed to create directory '" + target.parent_path().string()
           + "': " + error_code.message();
  }

  std::filesystem::copy_file(source, target,
                             std::filesystem::copy_options::overwrite_existing,
                             error_code);
  if (error_code) {
    return "failed to copy file '" + source.string() + "' to '"
           + target.string() + "': " + error_code.message();
  }

  return std::nullopt;
}

std::optional<std::string> ReplacePathWithCopy(
    const std::filesystem::path& source,
    const std::filesystem::path& target)
{
  std::error_code error_code;
  std::filesystem::create_directories(target.parent_path(), error_code);
  if (error_code) {
    return "failed to create directory '" + target.parent_path().string()
           + "': " + error_code.message();
  }

  const auto temp_target
      = target.parent_path()
        / (target.filename().string() + ".tmp-bconfig-"
           + std::to_string(static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  ScopedPathCleanup cleanup{temp_target};
  std::filesystem::remove_all(temp_target, error_code);

  std::optional<std::string> copy_error;
  if (std::filesystem::is_directory(source)) {
    copy_error = CopyDirectoryTree(source, temp_target);
  } else if (std::filesystem::is_regular_file(source)) {
    copy_error = CopyFileTree(source, temp_target);
  } else {
    return "sync source does not exist: " + source.string();
  }
  if (copy_error) { return copy_error; }

  if (std::filesystem::exists(target)) {
    std::filesystem::remove_all(target, error_code);
    if (error_code) {
      return "failed to remove existing path '" + target.string()
             + "': " + error_code.message();
    }
  }

  std::filesystem::rename(temp_target, target, error_code);
  if (error_code) {
    return "failed to activate synced path '" + target.string()
           + "': " + error_code.message();
  }

  cleanup.path.clear();
  return std::nullopt;
}

OperationResult<std::filesystem::path> ResolveInstalledConfigRoot()
{
  if (const auto override_path
      = GetEnvironmentVariable("BCONFIG_BAREOS_CONFIG_ROOT")) {
    return {.value = std::filesystem::path{*override_path}};
  }

  return {.value = std::filesystem::path{"/etc/bareos"}};
}

struct StringOutputBuffer {
  std::string content{};
};

bool AppendOutputBuffer(void* context, const char* fmt, ...)
{
  auto* buffer = static_cast<StringOutputBuffer*>(context);
  if (!buffer) { return false; }

  PoolMem rendered;
  va_list arguments;
  va_start(arguments, fmt);
  rendered.Bvsprintf(fmt, arguments);
  va_end(arguments);
  buffer->content.append(rendered.c_str());
  return true;
}

std::string RenderParsedResource(BareosResource& resource,
                                 ConfigurationParser& parser)
{
  StringOutputBuffer output;
  OutputFormatter formatter(&AppendOutputBuffer, &output, nullptr, nullptr);
  OutputFormatterResource formatter_resource(&formatter);
  resource.PrintConfig(formatter_resource, parser, false, false);
  formatter.FinalizeResult(true);
  return output.content;
}

std::string QuoteBareosString(std::string_view value);
std::string NormalizeDirectiveLookupName(std::string_view name);
bool HasMemberSource(const BareosResource& resource,
                     std::initializer_list<const char*> member_names);
OperationResult<std::string> LoadSecretMemberSourceValue(
    const BareosResource& resource,
    std::initializer_list<const char*> member_names,
    const s_password& password,
    std::string_view context);

std::string ReplaceRenderedQuotedDirective(std::string rendered,
                                           std::string_view directive_name,
                                           std::string_view value)
{
  std::istringstream stream{rendered};
  std::ostringstream updated;
  std::string line;
  const auto normalized_name = NormalizeDirectiveLookupName(directive_name);
  bool replaced = false;
  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const auto equals = trimmed.find('=');
    if (!replaced && equals != std::string::npos) {
      const auto rendered_name = NormalizeDirectiveLookupName(
          TrimAsciiWhitespace(trimmed.substr(0, equals)));
      if (rendered_name == normalized_name) {
        updated << "  " << directive_name << " = " << QuoteBareosString(value)
                << "\n";
        replaced = true;
        continue;
      }
    }
    updated << line << "\n";
  }

  return updated.str();
}

std::string RemoveRenderedDirective(std::string rendered,
                                    std::string_view directive_name)
{
  std::istringstream stream{rendered};
  std::ostringstream updated;
  std::string line;
  const auto normalized_name = NormalizeDirectiveLookupName(directive_name);
  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const auto equals = trimmed.find('=');
    if (equals != std::string::npos) {
      const auto rendered_name = NormalizeDirectiveLookupName(
          TrimAsciiWhitespace(trimmed.substr(0, equals)));
      if (rendered_name == normalized_name) { continue; }
    }
    updated << line << "\n";
  }

  return updated.str();
}

OperationResult<std::string> RenderParsedResourceWithPreservedSecret(
    BareosResource& resource,
    ConfigurationParser& parser,
    std::string_view directive_name,
    std::initializer_list<const char*> member_names,
    const s_password& password,
    std::string_view context)
{
  if (!HasMemberSource(resource, member_names)
      && (!password.value || password.value[0] == '\0')) {
    return {.value = RenderParsedResource(resource, parser)};
  }

  auto rendered_password
      = LoadSecretMemberSourceValue(resource, member_names, password, context);
  if (!rendered_password) { return {.error = rendered_password.error}; }

  auto rendered = RenderParsedResource(resource, parser);
  if (rendered_password.value->empty()) {
    return {.value = std::move(rendered)};
  }

  return {.value = ReplaceRenderedQuotedDirective(
              std::move(rendered), directive_name, *rendered_password.value)};
}

OperationResult<std::string> RenderParsedConsoleResource(
    console::ConsoleResource& resource,
    ConfigurationParser& parser)
{
  return RenderParsedResourceWithPreservedSecret(
      resource, parser, "Password", {"Password"}, resource.password_,
      "console password for '" + std::string{resource.resource_name_} + "'");
}

OperationResult<std::string> RenderParsedConsoleDirectorResource(
    console::DirectorResource& resource,
    ConfigurationParser& parser,
    bool include_password = true)
{
  auto rendered = RenderParsedResourceWithPreservedSecret(
      resource, parser, "Password", {"Password"}, resource.password_,
      "console director password for '" + std::string{resource.resource_name_}
          + "'");
  if (!rendered || include_password) { return rendered; }
  return {.value
          = RemoveRenderedDirective(std::move(*rendered.value), "Password")};
}

#ifdef BCONFIG_HAVE_CONSOLE
std::string BuildConsoleConfigFileContent(
    const std::vector<std::string>& director_resources,
    const std::vector<std::string>& console_resources)
{
  std::ostringstream content;
  content << "#\n"
          << "# Bareos User Agent (or Console) Configuration File\n"
          << "#\n\n";
  for (const auto& director : director_resources) { content << director; }
  for (const auto& configured_console : console_resources) {
    content << configured_console;
  }
  return content.str();
}

std::optional<std::string> WriteImportedConsoleConfig(
    const std::filesystem::path& target_root,
    bconfig::LoadedConfig& loaded)
{
  std::error_code error_code;
  std::filesystem::create_directories(target_root, error_code);
  if (error_code) {
    return "failed to create directory '" + target_root.string()
           + "': " + error_code.message();
  }

  std::vector<std::string> director_resources;
  std::vector<std::string> console_resources;

  for (auto* resource = loaded.parser->GetNextRes(console::R_CONSOLE, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_CONSOLE, resource)) {
    if (!resource->resource_name_) {
      return "console import encountered a Console resource without a name.";
    }
    auto* configured_console
        = dynamic_cast<console::ConsoleResource*>(resource);
    if (!configured_console) {
      return "console import encountered an unexpected Console resource type.";
    }
    auto rendered
        = RenderParsedConsoleResource(*configured_console, *loaded.parser);
    if (!rendered) { return rendered.error; }
    console_resources.emplace_back(std::move(*rendered.value));
  }

  for (auto* resource = loaded.parser->GetNextRes(console::R_DIRECTOR, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_DIRECTOR, resource)) {
    if (!resource->resource_name_) {
      return "console import encountered a Director resource without a name.";
    }
    auto* configured_director
        = dynamic_cast<console::DirectorResource*>(resource);
    if (!configured_director) {
      return "console import encountered an unexpected Director resource type.";
    }
    auto rendered = RenderParsedConsoleDirectorResource(*configured_director,
                                                        *loaded.parser);
    if (!rendered) { return rendered.error; }
    director_resources.emplace_back(std::move(*rendered.value));
  }

  const auto root_config_path
      = target_root
        / ComponentDefaultConfigFilename(bconfig::Component::kConsole);
  if (!WriteFile(root_config_path,
                 BuildConsoleConfigFileContent(director_resources,
                                               console_resources))) {
    return "failed to write file '" + root_config_path.string() + "'.";
  }

  return std::nullopt;
}
#endif

void RemoveTreeQuietly(const std::filesystem::path& path)
{
  std::error_code error_code;
  std::filesystem::remove_all(path, error_code);
}

std::string QuoteBareosString(std::string_view value)
{
  std::string quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back('"');
  for (const char ch : value) {
    if (ch == '\\' || ch == '"') { quoted.push_back('\\'); }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

bool IsSafePathSegment(std::string_view value);
bool IsSafeBareosToken(std::string_view value);
void AppendBoolDirective(std::ostringstream& content,
                         std::string_view name,
                         const std::optional<bool>& value);
template <typename Integer>
void AppendIntegerDirective(std::ostringstream& content,
                            std::string_view name,
                            const std::optional<Integer>& value);
void AppendBareosDirective(std::ostringstream& content,
                           std::string_view name,
                           const std::optional<std::string>& value);
void AppendQuotedDirective(std::ostringstream& content,
                           std::string_view name,
                           const std::optional<std::string>& value);
void AppendRepeatedBareosDirective(std::ostringstream& content,
                                   std::string_view name,
                                   const std::vector<std::string>& values);
void AppendRepeatedQuotedDirective(std::ostringstream& content,
                                   std::string_view name,
                                   const std::vector<std::string>& values);
void AppendRepeatedRawDirective(std::ostringstream& content,
                                std::string_view name,
                                const std::vector<std::string>& values);
OperationResult<std::vector<std::string>> ExtractNamedTopLevelDirectiveValues(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    std::string_view resource_name,
    const std::set<std::string>& directive_names);

template <typename Resource>
std::optional<std::string> CopyResourceName(const Resource* resource);

std::string BuildClientDirectorStubContent(
    std::string_view director_name,
    std::string_view password,
    std::string_view description,
    const std::optional<std::string>& address = std::nullopt,
    const std::optional<uint32_t>& port = std::nullopt,
    const std::optional<std::vector<std::string>>& allowed_script_dirs
    = std::nullopt,
    const std::optional<std::vector<std::string>>& allowed_job_commands
    = std::nullopt,
    const std::optional<bool>& tls_authenticate = std::nullopt,
    const std::optional<bool>& tls_enable = std::nullopt,
    const std::optional<bool>& tls_require = std::nullopt,
    const std::optional<bool>& tls_verify_peer = std::nullopt,
    const std::optional<std::string>& tls_cipher_list = std::nullopt,
    const std::optional<std::string>& tls_cipher_suites = std::nullopt,
    const std::optional<std::string>& tls_dh_file = std::nullopt,
    const std::optional<std::string>& tls_protocol = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_file = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_dir = std::nullopt,
    const std::optional<std::string>& tls_certificate_revocation_list
    = std::nullopt,
    const std::optional<std::string>& tls_certificate = std::nullopt,
    const std::optional<std::string>& tls_key = std::nullopt,
    const std::optional<std::vector<std::string>>& tls_allowed_cn
    = std::nullopt,
    const std::optional<bool>& connection_from_director_to_client
    = std::nullopt,
    const std::optional<bool>& connection_from_client_to_director
    = std::nullopt,
    const std::optional<bool>& monitor = std::nullopt,
    const std::optional<uint64_t>& maximum_bandwidth_per_job = std::nullopt)
{
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n";
  AppendBareosDirective(content, "Address", address);
  AppendIntegerDirective(content, "Port", port);
  if (allowed_script_dirs) {
    AppendRepeatedQuotedDirective(content, "AllowedScriptDir",
                                  *allowed_script_dirs);
  }
  if (allowed_job_commands) {
    AppendRepeatedQuotedDirective(content, "AllowedJobCommand",
                                  *allowed_job_commands);
  }
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  AppendBoolDirective(content, "ConnectionFromDirectorToClient",
                      connection_from_director_to_client);
  AppendBoolDirective(content, "ConnectionFromClientToDirector",
                      connection_from_client_to_director);
  AppendBoolDirective(content, "Monitor", monitor);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         maximum_bandwidth_per_job);
  content << "}\n";
  return content.str();
}

constexpr uint16_t kDefaultFileDaemonPort = 9102;
constexpr uint16_t kDefaultStorageDaemonPort = 9103;

constexpr std::array<std::string_view, directordaemon::Num_ACL>
    kDirectorAclDirectiveNames
    = {"JobACL",   "ClientACL",       "StorageACL", "ScheduleACL",
       "PoolACL",  "CommandACL",      "FileSetACL", "CatalogACL",
       "WhereACL", "PluginOptionsACL"};

void ApplyDirectorAclOverrides(
    std::array<std::vector<std::string>, directordaemon::Num_ACL>& acl,
    const std::optional<std::vector<std::string>>& job_acl,
    const std::optional<std::vector<std::string>>& client_acl,
    const std::optional<std::vector<std::string>>& storage_acl,
    const std::optional<std::vector<std::string>>& schedule_acl,
    const std::optional<std::vector<std::string>>& pool_acl,
    const std::optional<std::vector<std::string>>& command_acl,
    const std::optional<std::vector<std::string>>& fileset_acl,
    const std::optional<std::vector<std::string>>& catalog_acl,
    const std::optional<std::vector<std::string>>& where_acl,
    const std::optional<std::vector<std::string>>& plugin_options_acl)
{
  if (job_acl) { acl[directordaemon::Job_ACL] = *job_acl; }
  if (client_acl) { acl[directordaemon::Client_ACL] = *client_acl; }
  if (storage_acl) { acl[directordaemon::Storage_ACL] = *storage_acl; }
  if (schedule_acl) { acl[directordaemon::Schedule_ACL] = *schedule_acl; }
  if (pool_acl) { acl[directordaemon::Pool_ACL] = *pool_acl; }
  if (command_acl) { acl[directordaemon::Command_ACL] = *command_acl; }
  if (fileset_acl) { acl[directordaemon::FileSet_ACL] = *fileset_acl; }
  if (catalog_acl) { acl[directordaemon::Catalog_ACL] = *catalog_acl; }
  if (where_acl) { acl[directordaemon::Where_ACL] = *where_acl; }
  if (plugin_options_acl) {
    acl[directordaemon::PluginOptions_ACL] = *plugin_options_acl;
  }
}

void ApplyDirectorProfileOverrides(
    std::vector<std::string>& profiles,
    const std::optional<std::vector<std::string>>& configured_profiles)
{
  if (configured_profiles) { profiles = *configured_profiles; }
}

struct DirectorPoolContentSpec {
  std::optional<std::string> description{};
  std::optional<std::string> pool_type{};
  std::optional<std::string> label_format{};
  std::optional<std::string> cleaning_prefix{};
  std::optional<std::string> label_type{};
  std::optional<uint32_t> maximum_volumes{};
  std::optional<uint32_t> maximum_volume_jobs{};
  std::optional<uint32_t> maximum_volume_files{};
  std::optional<uint64_t> maximum_volume_bytes{};
  std::optional<uint64_t> volume_retention{};
  std::optional<uint64_t> volume_use_duration{};
  std::optional<uint64_t> migration_time{};
  std::optional<uint64_t> migration_high_bytes{};
  std::optional<uint64_t> migration_low_bytes{};
  std::optional<std::string> next_pool{};
  std::vector<std::string> storages{};
  std::optional<bool> use_catalog{};
  std::optional<bool> catalog_files{};
  std::optional<bool> purge_oldest_volume{};
  std::optional<std::string> action_on_purge{};
  std::optional<bool> recycle_oldest_volume{};
  std::optional<bool> recycle_current_volume{};
  std::optional<bool> auto_prune{};
  std::optional<bool> recycle{};
  std::optional<std::string> recycle_pool{};
  std::optional<std::string> scratch_pool{};
  std::optional<std::string> catalog{};
  std::optional<uint64_t> file_retention{};
  std::optional<uint64_t> job_retention{};
  std::optional<uint32_t> minimum_block_size{};
  std::optional<uint32_t> maximum_block_size{};
};

struct DirectorCatalogContentSpec {
  std::optional<std::string> description{};
  std::optional<std::string> db_address{};
  std::optional<uint32_t> db_port{};
  std::optional<std::string> db_socket{};
  std::optional<std::string> db_password{};
  std::optional<std::string> db_user{};
  std::optional<std::string> db_name{};
  std::optional<bool> multiple_connections{};
  std::optional<bool> disable_batch_insert{};
  std::optional<bool> reconnect{};
  std::optional<bool> exit_on_fatal{};
  std::optional<uint32_t> min_connections{};
  std::optional<uint32_t> max_connections{};
  std::optional<uint32_t> inc_connections{};
  std::optional<uint32_t> idle_timeout{};
  std::optional<uint32_t> validate_timeout{};
};

struct DirectorMessagesContentSpec {
  std::optional<std::string> description{};
  std::optional<std::string> mail_command{};
  std::optional<std::string> operator_command{};
  std::optional<std::string> timestamp_format{};
  std::vector<std::string> syslog_entries{};
  std::vector<std::string> mail_entries{};
  std::vector<std::string> mail_on_error_entries{};
  std::vector<std::string> mail_on_success_entries{};
  std::vector<std::string> file_entries{};
  std::vector<std::string> append_entries{};
  std::vector<std::string> stdout_entries{};
  std::vector<std::string> stderr_entries{};
  std::vector<std::string> director_entries{};
  std::vector<std::string> console_entries{};
  std::vector<std::string> operator_entries{};
  std::vector<std::string> catalog_entries{};
  std::vector<std::string> entries{};
};

struct StorageDaemonContentSpec {
  std::optional<std::string> address{};
  std::vector<std::string> addresses{};
  std::optional<std::string> source_address{};
  std::vector<std::string> source_addresses{};
  std::optional<uint16_t> port{};
  std::optional<std::string> query_file{};
  std::optional<uint32_t> subscriptions{};
  std::optional<bool> just_in_time_reservation{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint32_t> maximum_console_connections{};
  std::optional<uint32_t> maximum_workers_per_job{};
  std::optional<uint32_t> absolute_job_timeout{};
  std::optional<bool> allow_bandwidth_bursting{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::vector<std::string> tls_allowed_cn{};
  std::optional<bool> pki_signatures{};
  std::optional<bool> pki_encryption{};
  std::optional<std::string> pki_key_pair{};
  std::vector<std::string> pki_signers{};
  std::vector<std::string> pki_master_keys{};
  std::optional<std::string> pki_cipher{};
  std::optional<bool> always_use_lmdb{};
  std::optional<uint32_t> lmdb_threshold{};
  std::optional<bool> ndmp_enable{};
  std::optional<bool> ndmp_snooping{};
  std::optional<uint32_t> ndmp_log_level{};
  std::optional<std::string> ndmp_address{};
  std::optional<uint16_t> ndmp_port{};
  std::vector<std::string> ndmp_addresses{};
  std::optional<bool> autoxflate_on_replication{};
  std::optional<bool> collect_device_statistics{};
  std::optional<bool> collect_job_statistics{};
  std::optional<uint32_t> statistics_collect_interval{};
  std::optional<bool> device_reserve_by_media_type{};
  std::optional<bool> file_device_concurrent_read{};
  std::optional<std::string> ver_id{};
  std::optional<std::string> log_timestamp_format{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> secure_erase_command{};
  std::optional<std::string> grpc_module{};
  std::optional<bool> enable_ktls{};
  std::optional<uint64_t> sd_connect_timeout{};
  std::optional<uint64_t> fd_connect_timeout{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> statistics_retention{};
  std::optional<bool> ndmp_namelist_fhinfo_set_zero_for_invalid_uquad{};
  std::optional<uint64_t> checkpoint_interval{};
  std::optional<uint64_t> client_connect_wait{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<std::string> description{};
  std::optional<std::string> key_encryption_key{};
  std::optional<bool> auditing{};
  std::vector<std::string> audit_events{};
  std::optional<std::string> working_directory{};
  std::optional<std::string> plugin_directory{};
  std::vector<std::string> plugin_names{};
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  std::vector<std::string> backend_directories{};
#endif
  std::vector<std::string> allowed_script_dirs{};
  std::vector<std::string> allowed_job_commands{};
  std::optional<std::string> scripts_directory{};
  std::optional<std::string> messages{};
};

struct DirectorScheduleContentSpec {
  std::optional<std::string> description{};
  bool enabled{true};
  std::vector<std::string> run_entries{};
};

struct DirectorCounterContentSpec {
  std::optional<std::string> description{};
  int32_t minimum{0};
  int32_t maximum{2147483647};
  std::optional<std::string> wrap_counter{};
  std::optional<std::string> catalog{};
};

struct DirectorFilesetContentSpec {
  std::optional<std::string> description{};
  bool ignore_fileset_changes{false};
  bool enable_vss{true};
  std::vector<std::string> include_blocks{};
  std::vector<std::string> exclude_blocks{};
};

struct DirectorJobContentSpec {
  std::optional<std::string> description{};
  std::optional<std::string> type{};
  std::optional<std::string> backup_format{};
  std::optional<std::string> protocol{};
  std::optional<std::string> level{};
  std::optional<std::string> messages{};
  std::vector<std::string> storages{};
  std::optional<std::string> pool{};
  std::optional<std::string> full_backup_pool{};
  std::optional<std::string> virtual_full_backup_pool{};
  std::optional<std::string> incremental_backup_pool{};
  std::optional<std::string> differential_backup_pool{};
  std::optional<std::string> next_pool{};
  std::optional<std::string> client{};
  std::optional<std::string> fileset{};
  std::optional<std::string> schedule{};
  std::optional<std::string> verify_job{};
  std::optional<std::string> catalog{};
  std::optional<std::string> jobdefs{};
  std::vector<std::string> run_entries{};
  std::vector<std::string> run_before_job_entries{};
  std::vector<std::string> run_after_job_entries{};
  std::vector<std::string> run_after_failed_job_entries{};
  std::vector<std::string> client_run_before_job_entries{};
  std::vector<std::string> client_run_after_job_entries{};
  std::vector<std::string> runscript_blocks{};
  std::optional<std::string> where{};
  std::optional<std::string> replace{};
  std::optional<std::string> regex_where{};
  std::optional<std::string> strip_prefix{};
  std::optional<std::string> add_prefix{};
  std::optional<std::string> add_suffix{};
  std::optional<std::string> bootstrap{};
  std::optional<std::string> write_bootstrap{};
  std::optional<std::string> write_verify_list{};
  std::optional<uint64_t> maximum_bandwidth{};
  std::optional<uint64_t> max_run_sched_time{};
  std::optional<uint64_t> max_run_time{};
  std::optional<uint64_t> full_max_runtime{};
  std::optional<uint64_t> incremental_max_runtime{};
  std::optional<uint64_t> differential_max_runtime{};
  std::optional<uint64_t> max_wait_time{};
  std::optional<uint64_t> max_start_delay{};
  std::optional<uint64_t> max_full_interval{};
  std::optional<uint64_t> max_virtual_full_interval{};
  std::optional<uint64_t> max_diff_interval{};
  std::optional<bool> prefix_links{};
  std::optional<bool> prune_jobs{};
  std::optional<bool> prune_files{};
  std::optional<bool> prune_volumes{};
  std::optional<bool> purge_migration_job{};
  std::optional<bool> spool_attributes{};
  std::optional<bool> spool_data{};
  std::optional<uint64_t> spool_size{};
  std::optional<bool> rerun_failed_levels{};
  std::optional<bool> prefer_mounted_volumes{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<bool> reschedule_on_error{};
  std::optional<uint64_t> reschedule_interval{};
  std::optional<uint32_t> reschedule_times{};
  std::optional<int32_t> priority{};
  std::optional<bool> allow_mixed_priority{};
  std::optional<std::string> selection_type{};
  std::optional<std::string> selection_pattern{};
  std::optional<bool> accurate{};
  std::optional<bool> allow_duplicate_jobs{};
  std::optional<bool> allow_higher_duplicates{};
  std::optional<bool> cancel_lower_level_duplicates{};
  std::optional<bool> cancel_queued_duplicates{};
  std::optional<bool> cancel_running_duplicates{};
  std::optional<bool> save_file_history{};
  std::optional<uint64_t> file_history_size{};
  std::vector<std::string> fd_plugin_options{};
  std::vector<std::string> sd_plugin_options{};
  std::vector<std::string> dir_plugin_options{};
  std::optional<uint32_t> max_concurrent_copies{};
  std::optional<bool> always_incremental{};
  std::optional<uint64_t> always_incremental_job_retention{};
  std::optional<uint32_t> always_incremental_keep_number{};
  std::optional<uint64_t> always_incremental_max_full_age{};
  std::optional<uint32_t> max_full_consolidations{};
  std::optional<uint64_t> run_on_incoming_connect_interval{};
  std::optional<bool> enabled{};
  std::vector<std::string> passthrough_entries{};
};

std::string BuildDirectorClientResourceContent(
    std::string_view client_name,
    std::string_view address,
    const std::optional<std::string>& lan_address,
    const std::optional<std::string>& protocol,
    const std::optional<std::string>& auth_type,
    const std::optional<std::string>& catalog,
    const std::optional<std::string>& username,
    std::string_view password,
    uint32_t port,
    const std::optional<bool>& enabled,
    const std::optional<bool>& passive,
    const std::optional<bool>& strict_quotas,
    const std::optional<bool>& quota_include_failed_jobs,
    const std::optional<uint64_t>& soft_quota,
    const std::optional<uint64_t>& hard_quota,
    const std::optional<uint64_t>& soft_quota_grace_period,
    const std::optional<uint64_t>& file_retention,
    const std::optional<uint64_t>& job_retention,
    const std::optional<uint32_t>& ndmp_log_level,
    const std::optional<uint32_t>& ndmp_block_size,
    const std::optional<bool>& ndmp_use_lmdb,
    const std::optional<bool>& auto_prune,
    const std::optional<bool>& tls_authenticate,
    const std::optional<bool>& tls_enable,
    const std::optional<bool>& tls_require,
    const std::optional<bool>& tls_verify_peer,
    const std::optional<std::string>& tls_cipher_list,
    const std::optional<std::string>& tls_cipher_suites,
    const std::optional<std::string>& tls_dh_file,
    const std::optional<std::string>& tls_protocol,
    const std::optional<std::string>& tls_ca_certificate_file,
    const std::optional<std::string>& tls_ca_certificate_dir,
    const std::optional<std::string>& tls_certificate_revocation_list,
    const std::optional<std::string>& tls_certificate,
    const std::optional<std::string>& tls_key,
    const std::optional<std::vector<std::string>>& tls_allowed_cn,
    const std::optional<bool>& connection_from_director_to_client,
    const std::optional<bool>& connection_from_client_to_director,
    const std::optional<uint32_t>& maximum_concurrent_jobs,
    const std::optional<uint64_t>& heartbeat_interval,
    const std::optional<uint64_t>& maximum_bandwidth_per_job,
    std::string_view description)
{
  std::ostringstream content;
  content << "Client {\n"
          << "  Name = " << QuoteBareosString(client_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Address = " << address << "\n";
  AppendBareosDirective(content, "LanAddress", lan_address);
  AppendBareosDirective(content, "Protocol", protocol);
  AppendBareosDirective(content, "AuthType", auth_type);
  AppendBareosDirective(content, "Catalog", catalog);
  content << "  Password = " << QuoteBareosString(password) << "\n"
          << "  Port = " << port << "\n";
  AppendQuotedDirective(content, "Username", username);
  AppendBoolDirective(content, "Enabled", enabled);
  AppendBoolDirective(content, "Passive", passive);
  AppendBoolDirective(content, "StrictQuotas", strict_quotas);
  AppendBoolDirective(content, "QuotaIncludeFailedJobs",
                      quota_include_failed_jobs);
  AppendIntegerDirective(content, "SoftQuota", soft_quota);
  AppendIntegerDirective(content, "HardQuota", hard_quota);
  AppendIntegerDirective(content, "SoftQuotaGracePeriod",
                         soft_quota_grace_period);
  AppendIntegerDirective(content, "FileRetention", file_retention);
  AppendIntegerDirective(content, "JobRetention", job_retention);
  AppendIntegerDirective(content, "NdmpLogLevel", ndmp_log_level);
  AppendIntegerDirective(content, "NdmpBlockSize", ndmp_block_size);
  AppendBoolDirective(content, "NdmpUseLmdb", ndmp_use_lmdb);
  AppendBoolDirective(content, "AutoPrune", auto_prune);
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  AppendBoolDirective(content, "ConnectionFromDirectorToClient",
                      connection_from_director_to_client);
  AppendBoolDirective(content, "ConnectionFromClientToDirector",
                      connection_from_client_to_director);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         maximum_concurrent_jobs);
  AppendIntegerDirective(content, "HeartbeatInterval", heartbeat_interval);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         maximum_bandwidth_per_job);
  content << "}\n";
  return content.str();
}

std::string BuildDirectorStorageResourceContent(
    std::string_view storage_name,
    std::string_view address,
    const std::optional<std::string>& lan_address,
    const std::optional<std::string>& protocol,
    const std::optional<std::string>& auth_type,
    const std::optional<std::string>& username,
    std::string_view password,
    const std::vector<std::string>& devices,
    std::string_view media_type,
    uint32_t port,
    const std::optional<bool>& autochanger,
    const std::optional<bool>& enabled,
    const std::optional<bool>& allow_compression,
    const std::optional<uint64_t>& heartbeat_interval,
    const std::optional<uint64_t>& cache_status_interval,
    const std::optional<uint32_t>& maximum_concurrent_jobs,
    const std::optional<uint32_t>& maximum_concurrent_read_jobs,
    const std::optional<std::string>& paired_storage,
    const std::optional<uint64_t>& maximum_bandwidth_per_job,
    const std::optional<bool>& collect_statistics,
    const std::optional<std::string>& ndmp_changer_device,
    const std::optional<bool>& tls_authenticate,
    const std::optional<bool>& tls_enable,
    const std::optional<bool>& tls_require,
    const std::optional<bool>& tls_verify_peer,
    const std::optional<std::string>& tls_cipher_list,
    const std::optional<std::string>& tls_cipher_suites,
    const std::optional<std::string>& tls_dh_file,
    const std::optional<std::string>& tls_protocol,
    const std::optional<std::string>& tls_ca_certificate_file,
    const std::optional<std::string>& tls_ca_certificate_dir,
    const std::optional<std::string>& tls_certificate_revocation_list,
    const std::optional<std::string>& tls_certificate,
    const std::optional<std::string>& tls_key,
    const std::optional<std::vector<std::string>>& tls_allowed_cn,
    std::string_view description)
{
  std::ostringstream content;
  content << "Storage {\n"
          << "  Name = " << QuoteBareosString(storage_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Address = " << address << "\n";
  AppendBareosDirective(content, "Protocol", protocol);
  AppendBareosDirective(content, "AuthType", auth_type);
  AppendBareosDirective(content, "LanAddress", lan_address);
  AppendQuotedDirective(content, "Username", username);
  content << "  Password = " << QuoteBareosString(password) << "\n";
  AppendRepeatedBareosDirective(content, "Device", devices);
  content << "  Media Type = " << media_type << "\n"
          << "  Port = " << port << "\n";
  AppendBoolDirective(content, "AutoChanger", autochanger);
  AppendBoolDirective(content, "Enabled", enabled);
  AppendBoolDirective(content, "AllowCompression", allow_compression);
  AppendIntegerDirective(content, "HeartbeatInterval", heartbeat_interval);
  AppendIntegerDirective(content, "CacheStatusInterval", cache_status_interval);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         maximum_concurrent_jobs);
  AppendIntegerDirective(content, "MaximumConcurrentReadJobs",
                         maximum_concurrent_read_jobs);
  AppendBareosDirective(content, "PairedStorage", paired_storage);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         maximum_bandwidth_per_job);
  AppendBoolDirective(content, "CollectStatistics", collect_statistics);
  AppendBareosDirective(content, "NdmpChangerDevice", ndmp_changer_device);
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  content << "}\n";
  return content.str();
}

std::string RenderBareosDirectiveValue(std::string_view value)
{
  if (IsSafeBareosToken(value)) { return std::string{value}; }
  return QuoteBareosString(value);
}

std::string JoinDirectiveValues(const std::vector<std::string>& values)
{
  std::ostringstream rendered;
  for (size_t index = 0; index < values.size(); ++index) {
    if (index != 0) { rendered << ", "; }
    rendered << RenderBareosDirectiveValue(values[index]);
  }
  return rendered.str();
}

std::string RenderBareosBool(bool value) { return value ? "yes" : "no"; }

void AppendQuotedDirective(std::ostringstream& content,
                           std::string_view name,
                           const std::optional<std::string>& value)
{
  if (!value) { return; }
  content << "  " << name << " = " << QuoteBareosString(*value) << "\n";
}

void AppendBareosDirective(std::ostringstream& content,
                           std::string_view name,
                           const std::optional<std::string>& value)
{
  if (!value) { return; }
  content << "  " << name << " = " << RenderBareosDirectiveValue(*value)
          << "\n";
}

template <typename Integer>
void AppendIntegerDirective(std::ostringstream& content,
                            std::string_view name,
                            const std::optional<Integer>& value)
{
  if (!value) { return; }
  content << "  " << name << " = " << *value << "\n";
}

void AppendBoolDirective(std::ostringstream& content,
                         std::string_view name,
                         const std::optional<bool>& value)
{
  if (!value) { return; }
  content << "  " << name << " = " << RenderBareosBool(*value) << "\n";
}

void AppendRepeatedBareosDirective(std::ostringstream& content,
                                   std::string_view name,
                                   const std::vector<std::string>& values)
{
  for (const auto& value : values) {
    content << "  " << name << " = " << RenderBareosDirectiveValue(value)
            << "\n";
  }
}

void AppendRepeatedQuotedDirective(std::ostringstream& content,
                                   std::string_view name,
                                   const std::vector<std::string>& values)
{
  for (const auto& value : values) {
    content << "  " << name << " = " << QuoteBareosString(value) << "\n";
  }
}

void AppendRepeatedRawDirective(std::ostringstream& content,
                                std::string_view name,
                                const std::vector<std::string>& values)
{
  for (const auto& value : values) {
    content << "  " << name << " = " << value << "\n";
  }
}

std::vector<std::string> CopyAddressEntries(dlist<IPADDR>* addrs)
{
  std::vector<std::string> values;
  if (!addrs) { return values; }

  IPADDR* addr;
  foreach_dlist (addr, addrs) {
    char address[1024];
    const auto family = addr->GetFamily() == AF_INET    ? "ipv4"
                        : addr->GetFamily() == AF_INET6 ? "ipv6"
                                                        : nullptr;
    if (!family) { continue; }

    std::ostringstream entry;
    entry << "host[" << family << ";"
          << addr->GetAddress(address, sizeof(address));
    if (const auto port = addr->GetPortHostOrder(); port > 0) {
      entry << ";" << port;
    }
    entry << "]";
    values.emplace_back(entry.str());
  }
  return values;
}

std::vector<std::string> CopyAddressEntries(dlist<IPADDR>* addrs, uint16_t port)
{
  auto values = CopyAddressEntries(addrs);
  if (port == 0) { return values; }

  for (auto& value : values) {
    constexpr std::string_view prefix = "host[";
    if (!value.starts_with(prefix) || value.empty() || value.back() != ']') {
      continue;
    }

    const auto inner = std::string_view{value}.substr(
        prefix.size(), value.size() - prefix.size() - 1);
    const auto first = inner.find(';');
    const auto second
        = inner.find(';', first == std::string_view::npos ? first : first + 1);
    if (first == std::string_view::npos || second != std::string_view::npos) {
      continue;
    }

    value.insert(value.size() - 1, ";" + std::to_string(port));
  }
  return values;
}

std::vector<std::string> CopyAddressValues(dlist<IPADDR>* addrs)
{
  std::vector<std::string> values;
  if (!addrs) { return values; }

  IPADDR* addr;
  foreach_dlist (addr, addrs) {
    char address[1024];
    values.emplace_back(addr->GetAddress(address, sizeof(address)));
  }
  return values;
}

struct AddressEntrySpec {
  std::string family{};
  std::string address{};
  uint16_t port{};
};

std::optional<AddressEntrySpec> ParseAddressEntry(std::string_view value)
{
  constexpr std::string_view prefix = "host[";
  if (!value.starts_with(prefix) || value.size() <= prefix.size()
      || value.back() != ']') {
    return std::nullopt;
  }

  const auto inner
      = value.substr(prefix.size(), value.size() - prefix.size() - 1);
  const auto first = inner.find(';');
  const auto second
      = inner.find(';', first == std::string_view::npos ? first : first + 1);
  if (first == std::string_view::npos || second == std::string_view::npos
      || inner.find(';', second + 1) != std::string_view::npos) {
    return std::nullopt;
  }

  const auto family = inner.substr(0, first);
  const auto address = inner.substr(first + 1, second - first - 1);
  const auto port_text = inner.substr(second + 1);
  if ((family != "ipv4" && family != "ipv6") || address.empty()
      || port_text.empty()) {
    return std::nullopt;
  }

  uint32_t port = 0;
  for (const char ch : port_text) {
    if (ch < '0' || ch > '9') { return std::nullopt; }
    port = port * 10 + static_cast<uint32_t>(ch - '0');
    if (port > 65535) { return std::nullopt; }
  }
  if (port == 0) { return std::nullopt; }

  return AddressEntrySpec{std::string{family}, std::string{address},
                          static_cast<uint16_t>(port)};
}

bool AppendAddressBlock(std::ostringstream& content,
                        std::string_view name,
                        const std::vector<std::string>& values,
                        std::string& error)
{
  if (values.empty()) { return true; }

  content << "  " << name << " = {\n";
  for (const auto& value : values) {
    auto parsed = ParseAddressEntry(value);
    if (!parsed) {
      error = std::string{name}
              + " entries must use host[ipv4;addr;port] or "
                "host[ipv6;addr;port].";
      return false;
    }
    content << "    " << parsed->family << " = { addr = " << parsed->address
            << " ; port = " << parsed->port << " }\n";
  }
  content << "  }\n";
  return true;
}

bool HasMemberSource(const BareosResource& resource,
                     std::initializer_list<const char*> member_names)
{
  for (const auto* member_name : member_names) {
    if (resource.GetMemberSource(member_name)) { return true; }
  }
  return false;
}

std::optional<uint16_t> LoadIntegerMemberSourceValue(
    const BareosResource& resource,
    std::initializer_list<const char*> member_names)
{
  const BareosResource::SourceLocation* source = nullptr;
  for (const auto* member_name : member_names) {
    source = resource.GetMemberSource(member_name);
    if (source) { break; }
  }
  if (!source || source->file.empty() || source->line <= 0) {
    return std::nullopt;
  }

  std::ifstream input(source->file);
  if (!input) { return std::nullopt; }

  std::string line;
  for (int current_line = 1; current_line <= source->line; ++current_line) {
    if (!std::getline(input, line)) { return std::nullopt; }
  }

  const auto equals = line.find('=');
  if (equals == std::string::npos) { return std::nullopt; }

  size_t pos = equals + 1;
  while (pos < line.size()
         && std::isspace(static_cast<unsigned char>(line[pos]))) {
    ++pos;
  }

  uint32_t value = 0;
  bool found_digit = false;
  while (pos < line.size()
         && std::isdigit(static_cast<unsigned char>(line[pos]))) {
    found_digit = true;
    value = value * 10 + static_cast<uint32_t>(line[pos] - '0');
    if (value > 65535) { return std::nullopt; }
    ++pos;
  }

  if (!found_digit || value == 0) { return std::nullopt; }
  return static_cast<uint16_t>(value);
}

OperationResult<std::string> RenderPasswordForConfig(const s_password& password,
                                                     std::string_view context);

const BareosResource::SourceLocation* FindMemberSource(
    const BareosResource& resource,
    std::initializer_list<const char*> member_names)
{
  for (const auto* member_name : member_names) {
    if (auto* source = resource.GetMemberSource(member_name)) { return source; }
  }
  return nullptr;
}

std::optional<std::string> LoadStringMemberSourceValue(
    const BareosResource& resource,
    std::initializer_list<const char*> member_names)
{
  const auto* source = FindMemberSource(resource, member_names);
  if (!source || source->file.empty() || source->line <= 0) {
    return std::nullopt;
  }

  std::ifstream input(source->file);
  if (!input) { return std::nullopt; }

  std::string line;
  for (int current_line = 1; current_line <= source->line; ++current_line) {
    if (!std::getline(input, line)) { return std::nullopt; }
  }

  const auto equals = line.find('=');
  if (equals == std::string::npos) { return std::nullopt; }

  size_t pos = equals + 1;
  while (pos < line.size()
         && std::isspace(static_cast<unsigned char>(line[pos]))) {
    ++pos;
  }
  if (pos >= line.size()) { return std::nullopt; }

  if (line[pos] == '"') {
    ++pos;
    std::string value;
    while (pos < line.size()) {
      const char ch = line[pos++];
      if (ch == '\\' && pos < line.size()) {
        value.push_back(line[pos++]);
        continue;
      }
      if (ch == '"') { return value; }
      value.push_back(ch);
    }
    return std::nullopt;
  }

  const auto end = line.find_first_of(" \t\r\n#", pos);
  return line.substr(pos,
                     end == std::string::npos ? std::string::npos : end - pos);
}

OperationResult<std::string> LoadSecretMemberSourceValue(
    const BareosResource& resource,
    std::initializer_list<const char*> member_names,
    const s_password& password,
    std::string_view context)
{
  if (auto value = LoadStringMemberSourceValue(resource, member_names)) {
    return {.value = std::move(*value)};
  }
  return RenderPasswordForConfig(password, context);
}

std::optional<std::string> CopyFirstAddress(dlist<IPADDR>* addrs)
{
  if (!addrs || addrs->size() == 0) { return std::nullopt; }

  auto* address = static_cast<IPADDR*>(addrs->first());
  if (!address) { return std::nullopt; }

  char buffer[1024];
  return std::string{address->GetAddress(buffer, sizeof(buffer))};
}

std::string BuildDirectorConsoleResourceContent(
    std::string_view console_name,
    std::string_view password,
    std::string_view description,
    bool use_pam_authentication,
    const std::array<std::vector<std::string>, directordaemon::Num_ACL>& acl,
    const std::vector<std::string>& profiles,
    const std::optional<bool>& tls_authenticate = std::nullopt,
    const std::optional<bool>& tls_enable = std::nullopt,
    const std::optional<bool>& tls_require = std::nullopt,
    const std::optional<bool>& tls_verify_peer = std::nullopt,
    const std::optional<std::string>& tls_cipher_list = std::nullopt,
    const std::optional<std::string>& tls_cipher_suites = std::nullopt,
    const std::optional<std::string>& tls_dh_file = std::nullopt,
    const std::optional<std::string>& tls_protocol = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_file = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_dir = std::nullopt,
    const std::optional<std::string>& tls_certificate_revocation_list
    = std::nullopt,
    const std::optional<std::string>& tls_certificate = std::nullopt,
    const std::optional<std::string>& tls_key = std::nullopt,
    const std::optional<std::vector<std::string>>& tls_allowed_cn
    = std::nullopt)
{
  std::ostringstream content;
  content << "Console {\n"
          << "  Name = " << QuoteBareosString(console_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n";
  for (size_t index = 0; index < acl.size(); ++index) {
    if (acl[index].empty()) { continue; }
    content << "  " << kDirectorAclDirectiveNames[index] << " = "
            << JoinDirectiveValues(acl[index]) << "\n";
  }
  for (const auto& profile : profiles) {
    content << "  Profile = " << RenderBareosDirectiveValue(profile) << "\n";
  }
  if (use_pam_authentication) { content << "  UsePamAuthentication = yes\n"; }
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  content << "}\n";
  return content.str();
}

std::string BuildConsoleConsoleResourceContent(
    std::string_view console_name,
    std::string_view director_name,
    std::string_view password,
    std::string_view description,
    const std::optional<std::string>& rc_file = std::nullopt,
    const std::optional<std::string>& history_file = std::nullopt,
    const std::optional<uint32_t>& history_length = std::nullopt,
    const std::optional<uint64_t>& heartbeat_interval = std::nullopt,
    const std::optional<bool>& tls_authenticate = std::nullopt,
    const std::optional<bool>& tls_enable = std::nullopt,
    const std::optional<bool>& tls_require = std::nullopt,
    const std::optional<bool>& tls_verify_peer = std::nullopt,
    const std::optional<std::string>& tls_cipher_list = std::nullopt,
    const std::optional<std::string>& tls_cipher_suites = std::nullopt,
    const std::optional<std::string>& tls_dh_file = std::nullopt,
    const std::optional<std::string>& tls_protocol = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_file = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_dir = std::nullopt,
    const std::optional<std::string>& tls_certificate_revocation_list
    = std::nullopt,
    const std::optional<std::string>& tls_certificate = std::nullopt,
    const std::optional<std::string>& tls_key = std::nullopt,
    const std::optional<std::vector<std::string>>& tls_allowed_cn
    = std::nullopt)
{
  std::ostringstream content;
  content << "Console {\n"
          << "  Name = " << QuoteBareosString(console_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "  Director = " << RenderBareosDirectiveValue(director_name)
          << "\n";
  AppendQuotedDirective(content, "RcFile", rc_file);
  AppendQuotedDirective(content, "HistoryFile", history_file);
  AppendIntegerDirective(content, "HistoryLength", history_length);
  AppendIntegerDirective(content, "HeartbeatInterval", heartbeat_interval);
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  content << "}\n";
  return content.str();
}

std::string BuildConsoleDirectorResourceContent(
    std::string_view director_name,
    std::string_view address,
    const std::optional<uint32_t>& port,
    const std::optional<std::string>& password,
    std::string_view description,
    const std::optional<uint64_t>& heartbeat_interval = std::nullopt,
    const std::optional<bool>& tls_authenticate = std::nullopt,
    const std::optional<bool>& tls_enable = std::nullopt,
    const std::optional<bool>& tls_require = std::nullopt,
    const std::optional<bool>& tls_verify_peer = std::nullopt,
    const std::optional<std::string>& tls_cipher_list = std::nullopt,
    const std::optional<std::string>& tls_cipher_suites = std::nullopt,
    const std::optional<std::string>& tls_dh_file = std::nullopt,
    const std::optional<std::string>& tls_protocol = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_file = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_dir = std::nullopt,
    const std::optional<std::string>& tls_certificate_revocation_list
    = std::nullopt,
    const std::optional<std::string>& tls_certificate = std::nullopt,
    const std::optional<std::string>& tls_key = std::nullopt,
    const std::optional<std::vector<std::string>>& tls_allowed_cn
    = std::nullopt)
{
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Address = " << RenderBareosDirectiveValue(address) << "\n";
  if (port) { content << "  Port = " << *port << "\n"; }
  if (password && !password->empty()) {
    content << "  Password = " << QuoteBareosString(*password) << "\n";
  }
  AppendIntegerDirective(content, "HeartbeatInterval", heartbeat_interval);
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorUserResourceContent(
    std::string_view user_name,
    std::string_view description,
    const std::array<std::vector<std::string>, directordaemon::Num_ACL>& acl,
    const std::vector<std::string>& profiles)
{
  std::ostringstream content;
  content << "User {\n"
          << "  Name = " << QuoteBareosString(user_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n";
  for (size_t index = 0; index < acl.size(); ++index) {
    if (acl[index].empty()) { continue; }
    content << "  " << kDirectorAclDirectiveNames[index] << " = "
            << JoinDirectiveValues(acl[index]) << "\n";
  }
  for (const auto& profile : profiles) {
    content << "  Profile = " << RenderBareosDirectiveValue(profile) << "\n";
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorProfileResourceContent(
    std::string_view profile_name,
    std::string_view description,
    const std::array<std::vector<std::string>, directordaemon::Num_ACL>& acl)
{
  std::ostringstream content;
  content << "Profile {\n"
          << "  Name = " << QuoteBareosString(profile_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n";
  for (size_t index = 0; index < acl.size(); ++index) {
    if (acl[index].empty()) { continue; }
    content << "  " << kDirectorAclDirectiveNames[index] << " = "
            << JoinDirectiveValues(acl[index]) << "\n";
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorPoolResourceContent(
    std::string_view pool_name,
    const DirectorPoolContentSpec& spec)
{
  std::ostringstream content;
  content << "Pool {\n"
          << "  Name = " << QuoteBareosString(pool_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  AppendBareosDirective(content, "PoolType", spec.pool_type);
  AppendQuotedDirective(content, "LabelFormat", spec.label_format);
  AppendBareosDirective(content, "LabelType", spec.label_type);
  AppendQuotedDirective(content, "CleaningPrefix", spec.cleaning_prefix);
  AppendBoolDirective(content, "UseCatalog", spec.use_catalog);
  AppendBoolDirective(content, "PurgeOldestVolume", spec.purge_oldest_volume);
  AppendBareosDirective(content, "ActionOnPurge", spec.action_on_purge);
  AppendBoolDirective(content, "RecycleOldestVolume",
                      spec.recycle_oldest_volume);
  AppendBoolDirective(content, "RecycleCurrentVolume",
                      spec.recycle_current_volume);
  AppendIntegerDirective(content, "MaximumVolumes", spec.maximum_volumes);
  AppendIntegerDirective(content, "MaximumVolumeJobs",
                         spec.maximum_volume_jobs);
  AppendIntegerDirective(content, "MaximumVolumeFiles",
                         spec.maximum_volume_files);
  AppendIntegerDirective(content, "MaximumVolumeBytes",
                         spec.maximum_volume_bytes);
  AppendBoolDirective(content, "CatalogFiles", spec.catalog_files);
  AppendIntegerDirective(content, "VolumeRetention", spec.volume_retention);
  AppendIntegerDirective(content, "VolumeUseDuration",
                         spec.volume_use_duration);
  AppendIntegerDirective(content, "MigrationTime", spec.migration_time);
  AppendIntegerDirective(content, "MigrationHighBytes",
                         spec.migration_high_bytes);
  AppendIntegerDirective(content, "MigrationLowBytes",
                         spec.migration_low_bytes);
  AppendBareosDirective(content, "NextPool", spec.next_pool);
  AppendRepeatedBareosDirective(content, "Storage", spec.storages);
  AppendBoolDirective(content, "AutoPrune", spec.auto_prune);
  AppendBoolDirective(content, "Recycle", spec.recycle);
  AppendBareosDirective(content, "RecyclePool", spec.recycle_pool);
  AppendBareosDirective(content, "ScratchPool", spec.scratch_pool);
  AppendBareosDirective(content, "Catalog", spec.catalog);
  AppendIntegerDirective(content, "FileRetention", spec.file_retention);
  AppendIntegerDirective(content, "JobRetention", spec.job_retention);
  AppendIntegerDirective(content, "MinimumBlockSize", spec.minimum_block_size);
  AppendIntegerDirective(content, "MaximumBlockSize", spec.maximum_block_size);
  content << "}\n";
  return content.str();
}

std::string BuildDirectorCatalogResourceContent(
    std::string_view catalog_name,
    const DirectorCatalogContentSpec& spec)
{
  std::ostringstream content;
  content << "Catalog {\n"
          << "  Name = " << QuoteBareosString(catalog_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  AppendQuotedDirective(content, "DbAddress", spec.db_address);
  AppendIntegerDirective(content, "DbPort", spec.db_port);
  AppendQuotedDirective(content, "DbSocket", spec.db_socket);
  content << "  DbPassword = "
          << QuoteBareosString(spec.db_password.value_or("")) << "\n";
  AppendQuotedDirective(content, "DbUser", spec.db_user);
  AppendQuotedDirective(content, "DbName", spec.db_name);
  AppendBoolDirective(content, "MultipleConnections",
                      spec.multiple_connections);
  AppendBoolDirective(content, "DisableBatchInsert", spec.disable_batch_insert);
  AppendBoolDirective(content, "Reconnect", spec.reconnect);
  AppendBoolDirective(content, "ExitOnFatal", spec.exit_on_fatal);
  AppendIntegerDirective(content, "MinConnections", spec.min_connections);
  AppendIntegerDirective(content, "MaxConnections", spec.max_connections);
  AppendIntegerDirective(content, "IncConnections", spec.inc_connections);
  AppendIntegerDirective(content, "IdleTimeout", spec.idle_timeout);
  AppendIntegerDirective(content, "ValidateTimeout", spec.validate_timeout);
  content << "}\n";
  return content.str();
}

std::string BuildDirectorScheduleResourceContent(
    std::string_view schedule_name,
    const DirectorScheduleContentSpec& spec)
{
  std::ostringstream content;
  content << "Schedule {\n"
          << "  Name = " << QuoteBareosString(schedule_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  content << "  Enabled = " << RenderBareosBool(spec.enabled) << "\n";
  for (const auto& entry : spec.run_entries) {
    content << "  Run = " << entry << "\n";
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorCounterResourceContent(
    std::string_view counter_name,
    const DirectorCounterContentSpec& spec)
{
  std::ostringstream content;
  content << "Counter {\n"
          << "  Name = " << QuoteBareosString(counter_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  content << "  Minimum = " << spec.minimum << "\n"
          << "  Maximum = " << spec.maximum << "\n";
  AppendBareosDirective(content, "WrapCounter", spec.wrap_counter);
  AppendBareosDirective(content, "Catalog", spec.catalog);
  content << "}\n";
  return content.str();
}

std::string BuildDirectorFilesetResourceContent(
    std::string_view fileset_name,
    const DirectorFilesetContentSpec& spec)
{
  std::ostringstream content;
  content << "FileSet {\n"
          << "  Name = " << QuoteBareosString(fileset_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  content << "  IgnoreFileSetChanges = "
          << RenderBareosBool(spec.ignore_fileset_changes) << "\n"
          << "  EnableVSS = " << RenderBareosBool(spec.enable_vss) << "\n";
  for (const auto& block : spec.include_blocks) {
    content << block;
    if (block.empty() || block.back() != '\n') { content << "\n"; }
  }
  for (const auto& block : spec.exclude_blocks) {
    content << block;
    if (block.empty() || block.back() != '\n') { content << "\n"; }
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorJobResourceContent(std::string_view job_name,
                                            const DirectorJobContentSpec& spec)
{
  std::ostringstream content;
  content << "Job {\n"
          << "  Name = " << QuoteBareosString(job_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  AppendBareosDirective(content, "Type", spec.type);
  AppendQuotedDirective(content, "BackupFormat", spec.backup_format);
  AppendBareosDirective(content, "Protocol", spec.protocol);
  AppendBareosDirective(content, "Level", spec.level);
  AppendBareosDirective(content, "Messages", spec.messages);
  AppendRepeatedBareosDirective(content, "Storage", spec.storages);
  AppendBareosDirective(content, "Pool", spec.pool);
  AppendBareosDirective(content, "FullBackupPool", spec.full_backup_pool);
  AppendBareosDirective(content, "VirtualFullBackupPool",
                        spec.virtual_full_backup_pool);
  AppendBareosDirective(content, "IncrementalBackupPool",
                        spec.incremental_backup_pool);
  AppendBareosDirective(content, "DifferentialBackupPool",
                        spec.differential_backup_pool);
  AppendBareosDirective(content, "NextPool", spec.next_pool);
  AppendBareosDirective(content, "Client", spec.client);
  AppendBareosDirective(content, "FileSet", spec.fileset);
  AppendBareosDirective(content, "Schedule", spec.schedule);
  AppendBareosDirective(content, "JobToVerify", spec.verify_job);
  AppendBareosDirective(content, "Catalog", spec.catalog);
  AppendBareosDirective(content, "JobDefs", spec.jobdefs);
  for (const auto& entry : spec.run_entries) {
    content << "  Run = " << entry << "\n";
  }
  AppendRepeatedQuotedDirective(content, "RunBeforeJob",
                                spec.run_before_job_entries);
  AppendRepeatedQuotedDirective(content, "RunAfterJob",
                                spec.run_after_job_entries);
  AppendRepeatedQuotedDirective(content, "RunAfterFailedJob",
                                spec.run_after_failed_job_entries);
  AppendRepeatedQuotedDirective(content, "ClientRunBeforeJob",
                                spec.client_run_before_job_entries);
  AppendRepeatedQuotedDirective(content, "ClientRunAfterJob",
                                spec.client_run_after_job_entries);
  for (const auto& block : spec.runscript_blocks) {
    content << block;
    if (block.empty() || block.back() != '\n') { content << "\n"; }
  }
  AppendBareosDirective(content, "Where", spec.where);
  AppendBareosDirective(content, "Replace", spec.replace);
  AppendQuotedDirective(content, "RegexWhere", spec.regex_where);
  AppendQuotedDirective(content, "StripPrefix", spec.strip_prefix);
  AppendQuotedDirective(content, "AddPrefix", spec.add_prefix);
  AppendQuotedDirective(content, "AddSuffix", spec.add_suffix);
  AppendQuotedDirective(content, "Bootstrap", spec.bootstrap);
  AppendQuotedDirective(content, "WriteBootstrap", spec.write_bootstrap);
  AppendQuotedDirective(content, "WriteVerifyList", spec.write_verify_list);
  AppendIntegerDirective(content, "MaximumBandwidth", spec.maximum_bandwidth);
  AppendIntegerDirective(content, "MaxRunSchedTime", spec.max_run_sched_time);
  AppendIntegerDirective(content, "MaxRunTime", spec.max_run_time);
  AppendIntegerDirective(content, "FullMaxRuntime", spec.full_max_runtime);
  AppendIntegerDirective(content, "IncrementalMaxRuntime",
                         spec.incremental_max_runtime);
  AppendIntegerDirective(content, "DifferentialMaxRuntime",
                         spec.differential_max_runtime);
  AppendIntegerDirective(content, "MaxWaitTime", spec.max_wait_time);
  AppendIntegerDirective(content, "MaxStartDelay", spec.max_start_delay);
  AppendIntegerDirective(content, "MaxFullInterval", spec.max_full_interval);
  AppendIntegerDirective(content, "MaxVirtualFullInterval",
                         spec.max_virtual_full_interval);
  AppendIntegerDirective(content, "MaxDiffInterval", spec.max_diff_interval);
  AppendBoolDirective(content, "PrefixLinks", spec.prefix_links);
  AppendBoolDirective(content, "PruneJobs", spec.prune_jobs);
  AppendBoolDirective(content, "PruneFiles", spec.prune_files);
  AppendBoolDirective(content, "PruneVolumes", spec.prune_volumes);
  AppendBoolDirective(content, "PurgeMigrationJob", spec.purge_migration_job);
  AppendBoolDirective(content, "SpoolAttributes", spec.spool_attributes);
  AppendBoolDirective(content, "SpoolData", spec.spool_data);
  AppendIntegerDirective(content, "SpoolSize", spec.spool_size);
  AppendBoolDirective(content, "RerunFailedLevels", spec.rerun_failed_levels);
  AppendBoolDirective(content, "PreferMountedVolumes",
                      spec.prefer_mounted_volumes);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         spec.maximum_concurrent_jobs);
  AppendBoolDirective(content, "RescheduleOnError", spec.reschedule_on_error);
  AppendIntegerDirective(content, "RescheduleInterval",
                         spec.reschedule_interval);
  AppendIntegerDirective(content, "RescheduleTimes", spec.reschedule_times);
  AppendIntegerDirective(content, "Priority", spec.priority);
  AppendBoolDirective(content, "AllowMixedPriority", spec.allow_mixed_priority);
  AppendBareosDirective(content, "SelectionType", spec.selection_type);
  AppendQuotedDirective(content, "SelectionPattern", spec.selection_pattern);
  AppendBoolDirective(content, "Accurate", spec.accurate);
  AppendBoolDirective(content, "AllowDuplicateJobs", spec.allow_duplicate_jobs);
  AppendBoolDirective(content, "AllowHigherDuplicates",
                      spec.allow_higher_duplicates);
  AppendBoolDirective(content, "CancelLowerLevelDuplicates",
                      spec.cancel_lower_level_duplicates);
  AppendBoolDirective(content, "CancelQueuedDuplicates",
                      spec.cancel_queued_duplicates);
  AppendBoolDirective(content, "CancelRunningDuplicates",
                      spec.cancel_running_duplicates);
  AppendBoolDirective(content, "SaveFileHistory", spec.save_file_history);
  AppendIntegerDirective(content, "FileHistorySize", spec.file_history_size);
  for (const auto& entry : spec.fd_plugin_options) {
    content << "  FdPluginOptions = " << QuoteBareosString(entry) << "\n";
  }
  for (const auto& entry : spec.sd_plugin_options) {
    content << "  SdPluginOptions = " << QuoteBareosString(entry) << "\n";
  }
  for (const auto& entry : spec.dir_plugin_options) {
    content << "  DirPluginOptions = " << QuoteBareosString(entry) << "\n";
  }
  AppendIntegerDirective(content, "MaxConcurrentCopies",
                         spec.max_concurrent_copies);
  AppendBoolDirective(content, "AlwaysIncremental", spec.always_incremental);
  AppendIntegerDirective(content, "AlwaysIncrementalJobRetention",
                         spec.always_incremental_job_retention);
  AppendIntegerDirective(content, "AlwaysIncrementalKeepNumber",
                         spec.always_incremental_keep_number);
  AppendIntegerDirective(content, "AlwaysIncrementalMaxFullAge",
                         spec.always_incremental_max_full_age);
  AppendIntegerDirective(content, "MaxFullConsolidations",
                         spec.max_full_consolidations);
  AppendIntegerDirective(content, "RunOnIncomingConnectInterval",
                         spec.run_on_incoming_connect_interval);
  AppendBoolDirective(content, "Enabled", spec.enabled);
  for (const auto& entry : spec.passthrough_entries) {
    content << entry;
    if (entry.empty() || entry.back() != '\n') { content << "\n"; }
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorJobDefsResourceContent(
    std::string_view jobdefs_name,
    const DirectorJobContentSpec& spec)
{
  std::ostringstream content;
  content << "JobDefs {\n"
          << "  Name = " << QuoteBareosString(jobdefs_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  AppendBareosDirective(content, "Type", spec.type);
  AppendQuotedDirective(content, "BackupFormat", spec.backup_format);
  AppendBareosDirective(content, "Protocol", spec.protocol);
  AppendBareosDirective(content, "Level", spec.level);
  AppendBareosDirective(content, "Messages", spec.messages);
  AppendRepeatedBareosDirective(content, "Storage", spec.storages);
  AppendBareosDirective(content, "Pool", spec.pool);
  AppendBareosDirective(content, "FullBackupPool", spec.full_backup_pool);
  AppendBareosDirective(content, "VirtualFullBackupPool",
                        spec.virtual_full_backup_pool);
  AppendBareosDirective(content, "IncrementalBackupPool",
                        spec.incremental_backup_pool);
  AppendBareosDirective(content, "DifferentialBackupPool",
                        spec.differential_backup_pool);
  AppendBareosDirective(content, "NextPool", spec.next_pool);
  AppendBareosDirective(content, "Client", spec.client);
  AppendBareosDirective(content, "FileSet", spec.fileset);
  AppendBareosDirective(content, "Schedule", spec.schedule);
  AppendBareosDirective(content, "JobToVerify", spec.verify_job);
  AppendBareosDirective(content, "Catalog", spec.catalog);
  AppendBareosDirective(content, "JobDefs", spec.jobdefs);
  for (const auto& entry : spec.run_entries) {
    content << "  Run = " << entry << "\n";
  }
  AppendRepeatedQuotedDirective(content, "RunBeforeJob",
                                spec.run_before_job_entries);
  AppendRepeatedQuotedDirective(content, "RunAfterJob",
                                spec.run_after_job_entries);
  AppendRepeatedQuotedDirective(content, "RunAfterFailedJob",
                                spec.run_after_failed_job_entries);
  AppendRepeatedQuotedDirective(content, "ClientRunBeforeJob",
                                spec.client_run_before_job_entries);
  AppendRepeatedQuotedDirective(content, "ClientRunAfterJob",
                                spec.client_run_after_job_entries);
  for (const auto& block : spec.runscript_blocks) {
    content << block;
    if (block.empty() || block.back() != '\n') { content << "\n"; }
  }
  AppendBareosDirective(content, "Where", spec.where);
  AppendBareosDirective(content, "Replace", spec.replace);
  AppendQuotedDirective(content, "RegexWhere", spec.regex_where);
  AppendQuotedDirective(content, "StripPrefix", spec.strip_prefix);
  AppendQuotedDirective(content, "AddPrefix", spec.add_prefix);
  AppendQuotedDirective(content, "AddSuffix", spec.add_suffix);
  AppendQuotedDirective(content, "Bootstrap", spec.bootstrap);
  AppendQuotedDirective(content, "WriteBootstrap", spec.write_bootstrap);
  AppendQuotedDirective(content, "WriteVerifyList", spec.write_verify_list);
  AppendIntegerDirective(content, "MaximumBandwidth", spec.maximum_bandwidth);
  AppendIntegerDirective(content, "MaxRunSchedTime", spec.max_run_sched_time);
  AppendIntegerDirective(content, "MaxRunTime", spec.max_run_time);
  AppendIntegerDirective(content, "FullMaxRuntime", spec.full_max_runtime);
  AppendIntegerDirective(content, "IncrementalMaxRuntime",
                         spec.incremental_max_runtime);
  AppendIntegerDirective(content, "DifferentialMaxRuntime",
                         spec.differential_max_runtime);
  AppendIntegerDirective(content, "MaxWaitTime", spec.max_wait_time);
  AppendIntegerDirective(content, "MaxStartDelay", spec.max_start_delay);
  AppendIntegerDirective(content, "MaxFullInterval", spec.max_full_interval);
  AppendIntegerDirective(content, "MaxVirtualFullInterval",
                         spec.max_virtual_full_interval);
  AppendIntegerDirective(content, "MaxDiffInterval", spec.max_diff_interval);
  AppendBoolDirective(content, "PrefixLinks", spec.prefix_links);
  AppendBoolDirective(content, "PruneJobs", spec.prune_jobs);
  AppendBoolDirective(content, "PruneFiles", spec.prune_files);
  AppendBoolDirective(content, "PruneVolumes", spec.prune_volumes);
  AppendBoolDirective(content, "PurgeMigrationJob", spec.purge_migration_job);
  AppendBoolDirective(content, "SpoolAttributes", spec.spool_attributes);
  AppendBoolDirective(content, "SpoolData", spec.spool_data);
  AppendIntegerDirective(content, "SpoolSize", spec.spool_size);
  AppendBoolDirective(content, "RerunFailedLevels", spec.rerun_failed_levels);
  AppendBoolDirective(content, "PreferMountedVolumes",
                      spec.prefer_mounted_volumes);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         spec.maximum_concurrent_jobs);
  AppendBoolDirective(content, "RescheduleOnError", spec.reschedule_on_error);
  AppendIntegerDirective(content, "RescheduleInterval",
                         spec.reschedule_interval);
  AppendIntegerDirective(content, "RescheduleTimes", spec.reschedule_times);
  AppendIntegerDirective(content, "Priority", spec.priority);
  AppendBoolDirective(content, "AllowMixedPriority", spec.allow_mixed_priority);
  AppendBareosDirective(content, "SelectionType", spec.selection_type);
  AppendQuotedDirective(content, "SelectionPattern", spec.selection_pattern);
  AppendBoolDirective(content, "Accurate", spec.accurate);
  AppendBoolDirective(content, "AllowDuplicateJobs", spec.allow_duplicate_jobs);
  AppendBoolDirective(content, "AllowHigherDuplicates",
                      spec.allow_higher_duplicates);
  AppendBoolDirective(content, "CancelLowerLevelDuplicates",
                      spec.cancel_lower_level_duplicates);
  AppendBoolDirective(content, "CancelQueuedDuplicates",
                      spec.cancel_queued_duplicates);
  AppendBoolDirective(content, "CancelRunningDuplicates",
                      spec.cancel_running_duplicates);
  AppendBoolDirective(content, "SaveFileHistory", spec.save_file_history);
  AppendIntegerDirective(content, "FileHistorySize", spec.file_history_size);
  for (const auto& entry : spec.fd_plugin_options) {
    content << "  FdPluginOptions = " << QuoteBareosString(entry) << "\n";
  }
  for (const auto& entry : spec.sd_plugin_options) {
    content << "  SdPluginOptions = " << QuoteBareosString(entry) << "\n";
  }
  for (const auto& entry : spec.dir_plugin_options) {
    content << "  DirPluginOptions = " << QuoteBareosString(entry) << "\n";
  }
  AppendIntegerDirective(content, "MaxConcurrentCopies",
                         spec.max_concurrent_copies);
  AppendBoolDirective(content, "AlwaysIncremental", spec.always_incremental);
  AppendIntegerDirective(content, "AlwaysIncrementalJobRetention",
                         spec.always_incremental_job_retention);
  AppendIntegerDirective(content, "AlwaysIncrementalKeepNumber",
                         spec.always_incremental_keep_number);
  AppendIntegerDirective(content, "AlwaysIncrementalMaxFullAge",
                         spec.always_incremental_max_full_age);
  AppendIntegerDirective(content, "MaxFullConsolidations",
                         spec.max_full_consolidations);
  AppendIntegerDirective(content, "RunOnIncomingConnectInterval",
                         spec.run_on_incoming_connect_interval);
  AppendBoolDirective(content, "Enabled", spec.enabled);
  for (const auto& entry : spec.passthrough_entries) {
    content << entry;
    if (entry.empty() || entry.back() != '\n') { content << "\n"; }
  }
  content << "}\n";
  return content.str();
}

std::string BuildDirectorMessagesResourceContent(
    std::string_view messages_name,
    const DirectorMessagesContentSpec& spec)
{
  std::ostringstream content;
  content << "Messages {\n"
          << "  Name = " << QuoteBareosString(messages_name) << "\n";
  AppendQuotedDirective(content, "Description", spec.description);
  AppendQuotedDirective(content, "MailCommand", spec.mail_command);
  AppendQuotedDirective(content, "OperatorCommand", spec.operator_command);
  AppendQuotedDirective(content, "TimestampFormat", spec.timestamp_format);
  AppendRepeatedRawDirective(content, "Syslog", spec.syslog_entries);
  AppendRepeatedRawDirective(content, "Mail", spec.mail_entries);
  AppendRepeatedRawDirective(content, "MailOnError",
                             spec.mail_on_error_entries);
  AppendRepeatedRawDirective(content, "MailOnSuccess",
                             spec.mail_on_success_entries);
  AppendRepeatedRawDirective(content, "File", spec.file_entries);
  AppendRepeatedRawDirective(content, "Append", spec.append_entries);
  AppendRepeatedRawDirective(content, "Stdout", spec.stdout_entries);
  AppendRepeatedRawDirective(content, "Stderr", spec.stderr_entries);
  AppendRepeatedRawDirective(content, "Director", spec.director_entries);
  AppendRepeatedRawDirective(content, "Console", spec.console_entries);
  AppendRepeatedRawDirective(content, "Operator", spec.operator_entries);
  AppendRepeatedRawDirective(content, "Catalog", spec.catalog_entries);
  for (const auto& entry : spec.entries) {
    content << entry;
    if (entry.empty() || entry.back() != '\n') { content << "\n"; }
  }
  content << "}\n";
  return content.str();
}

std::string BuildStorageDaemonResourceContent(
    std::string_view storage_name,
    const StorageDaemonContentSpec& spec,
    std::string* error = nullptr)
{
  std::string local_error;
  if (!error) { error = &local_error; }
  std::ostringstream content;
  content << "Storage {\n"
          << "  Name = " << QuoteBareosString(storage_name) << "\n";
  if (!AppendAddressBlock(content, "Addresses", spec.addresses, *error)) {
    return {};
  }
  if (spec.addresses.empty()) {
    AppendBareosDirective(content, "Address", spec.address);
    AppendIntegerDirective(content, "Port", spec.port);
  }
  if (spec.source_addresses.empty()) {
    AppendBareosDirective(content, "SourceAddress", spec.source_address);
  } else {
    AppendRepeatedBareosDirective(content, "SourceAddress",
                                  spec.source_addresses);
  }
  AppendBoolDirective(content, "JustInTimeReservation",
                      spec.just_in_time_reservation);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         spec.maximum_concurrent_jobs);
  AppendIntegerDirective(content, "AbsoluteJobTimeout",
                         spec.absolute_job_timeout);
  AppendBoolDirective(content, "AllowBandwidthBursting",
                      spec.allow_bandwidth_bursting);
  AppendBoolDirective(content, "TlsAuthenticate", spec.tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", spec.tls_enable);
  AppendBoolDirective(content, "TlsRequire", spec.tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", spec.tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", spec.tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", spec.tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", spec.tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", spec.tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        spec.tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir",
                        spec.tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        spec.tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", spec.tls_certificate);
  AppendQuotedDirective(content, "TlsKey", spec.tls_key);
  AppendRepeatedQuotedDirective(content, "TlsAllowedCn", spec.tls_allowed_cn);
  AppendBoolDirective(content, "NdmpEnable", spec.ndmp_enable);
  AppendBoolDirective(content, "NdmpSnooping", spec.ndmp_snooping);
  AppendIntegerDirective(content, "NdmpLogLevel", spec.ndmp_log_level);
  if (!AppendAddressBlock(content, "NdmpAddresses", spec.ndmp_addresses,
                          *error)) {
    return {};
  }
  if (spec.ndmp_addresses.empty()) {
    AppendBareosDirective(content, "NdmpAddress", spec.ndmp_address);
    AppendIntegerDirective(content, "NdmpPort", spec.ndmp_port);
  }
  AppendBoolDirective(content, "AutoXFlateOnReplication",
                      spec.autoxflate_on_replication);
  AppendBoolDirective(content, "CollectDeviceStatistics",
                      spec.collect_device_statistics);
  AppendBoolDirective(content, "CollectJobStatistics",
                      spec.collect_job_statistics);
  AppendIntegerDirective(content, "StatisticsCollectInterval",
                         spec.statistics_collect_interval);
  AppendBoolDirective(content, "DeviceReserveByMediaType",
                      spec.device_reserve_by_media_type);
  AppendBoolDirective(content, "FileDeviceConcurrentRead",
                      spec.file_device_concurrent_read);
  AppendQuotedDirective(content, "VerId", spec.ver_id);
  AppendQuotedDirective(content, "LogTimestampFormat",
                        spec.log_timestamp_format);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         spec.maximum_bandwidth_per_job);
  AppendQuotedDirective(content, "SecureEraseCommand",
                        spec.secure_erase_command);
  AppendBoolDirective(content, "EnableKtls", spec.enable_ktls);
  AppendIntegerDirective(content, "SdConnectTimeout", spec.sd_connect_timeout);
  AppendIntegerDirective(content, "FdConnectTimeout", spec.fd_connect_timeout);
  AppendIntegerDirective(content, "HeartbeatInterval", spec.heartbeat_interval);
  AppendIntegerDirective(content, "CheckpointInterval",
                         spec.checkpoint_interval);
  AppendIntegerDirective(content, "ClientConnectWait",
                         spec.client_connect_wait);
  AppendIntegerDirective(content, "MaximumNetworkBufferSize",
                         spec.maximum_network_buffer_size);
  AppendQuotedDirective(content, "Description", spec.description);
  AppendQuotedDirective(content, "WorkingDirectory", spec.working_directory);
  AppendQuotedDirective(content, "PluginDirectory", spec.plugin_directory);
  AppendRepeatedBareosDirective(content, "PluginNames", spec.plugin_names);
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  AppendRepeatedQuotedDirective(content, "BackendDirectory",
                                spec.backend_directories);
#endif
  AppendRepeatedQuotedDirective(content, "AllowedScriptDir",
                                spec.allowed_script_dirs);
  AppendRepeatedQuotedDirective(content, "AllowedJobCommand",
                                spec.allowed_job_commands);
  AppendQuotedDirective(content, "ScriptsDirectory", spec.scripts_directory);
  AppendBareosDirective(content, "Messages", spec.messages);
  content << "}\n";
  return content.str();
}

std::string BuildClientDaemonResourceContent(
    std::string_view client_name,
    const StorageDaemonContentSpec& spec,
    std::string* error = nullptr)
{
  std::string local_error;
  if (!error) { error = &local_error; }
  std::ostringstream content;
  content << "Client {\n"
          << "  Name = " << QuoteBareosString(client_name) << "\n";
  if (!AppendAddressBlock(content, "Addresses", spec.addresses, *error)) {
    return {};
  }
  if (spec.addresses.empty()) {
    AppendBareosDirective(content, "Address", spec.address);
    AppendIntegerDirective(content, "Port", spec.port);
  }
  if (spec.source_addresses.empty()) {
    AppendBareosDirective(content, "SourceAddress", spec.source_address);
  } else {
    AppendRepeatedBareosDirective(content, "SourceAddress",
                                  spec.source_addresses);
  }
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         spec.maximum_concurrent_jobs);
  AppendIntegerDirective(content, "MaximumWorkersPerJob",
                         spec.maximum_workers_per_job);
  AppendIntegerDirective(content, "AbsoluteJobTimeout",
                         spec.absolute_job_timeout);
  AppendBoolDirective(content, "AllowBandwidthBursting",
                      spec.allow_bandwidth_bursting);
  AppendBoolDirective(content, "TlsAuthenticate", spec.tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", spec.tls_enable);
  AppendBoolDirective(content, "TlsRequire", spec.tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", spec.tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", spec.tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", spec.tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", spec.tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", spec.tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        spec.tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir",
                        spec.tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        spec.tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", spec.tls_certificate);
  AppendQuotedDirective(content, "TlsKey", spec.tls_key);
  AppendRepeatedQuotedDirective(content, "TlsAllowedCn", spec.tls_allowed_cn);
  AppendBoolDirective(content, "PkiSignatures", spec.pki_signatures);
  AppendBoolDirective(content, "PkiEncryption", spec.pki_encryption);
  AppendQuotedDirective(content, "PkiKeyPair", spec.pki_key_pair);
  AppendRepeatedQuotedDirective(content, "PkiSigner", spec.pki_signers);
  AppendRepeatedQuotedDirective(content, "PkiMasterKey", spec.pki_master_keys);
  AppendBareosDirective(content, "PkiCipher", spec.pki_cipher);
  AppendBoolDirective(content, "AlwaysUseLmdb", spec.always_use_lmdb);
  AppendIntegerDirective(content, "LmdbThreshold", spec.lmdb_threshold);
  AppendQuotedDirective(content, "VerId", spec.ver_id);
  AppendQuotedDirective(content, "LogTimestampFormat",
                        spec.log_timestamp_format);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         spec.maximum_bandwidth_per_job);
  AppendQuotedDirective(content, "SecureEraseCommand",
                        spec.secure_erase_command);
  AppendQuotedDirective(content, "GrpcModule", spec.grpc_module);
  AppendBoolDirective(content, "EnableKtls", spec.enable_ktls);
  AppendIntegerDirective(content, "SdConnectTimeout", spec.sd_connect_timeout);
  AppendIntegerDirective(content, "HeartbeatInterval", spec.heartbeat_interval);
  AppendIntegerDirective(content, "MaximumNetworkBufferSize",
                         spec.maximum_network_buffer_size);
  AppendQuotedDirective(content, "Description", spec.description);
  AppendQuotedDirective(content, "WorkingDirectory", spec.working_directory);
  AppendQuotedDirective(content, "PluginDirectory", spec.plugin_directory);
  AppendRepeatedBareosDirective(content, "PluginNames", spec.plugin_names);
  AppendRepeatedQuotedDirective(content, "AllowedScriptDir",
                                spec.allowed_script_dirs);
  AppendRepeatedQuotedDirective(content, "AllowedJobCommand",
                                spec.allowed_job_commands);
  AppendQuotedDirective(content, "ScriptsDirectory", spec.scripts_directory);
  AppendBareosDirective(content, "Messages", spec.messages);
  content << "}\n";
  return content.str();
}

std::string BuildDirectorDaemonResourceContent(
    std::string_view director_name,
    std::string_view password,
    const StorageDaemonContentSpec& spec,
    std::string* error = nullptr)
{
  std::string local_error;
  if (!error) { error = &local_error; }
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n";
  if (!AppendAddressBlock(content, "Addresses", spec.addresses, *error)) {
    return {};
  }
  if (spec.addresses.empty()) {
    AppendBareosDirective(content, "Address", spec.address);
    AppendIntegerDirective(content, "Port", spec.port);
  }
  if (spec.source_addresses.empty()) {
    AppendBareosDirective(content, "SourceAddress", spec.source_address);
  } else {
    AppendRepeatedBareosDirective(content, "SourceAddress",
                                  spec.source_addresses);
  }
  AppendQuotedDirective(content, "QueryFile", spec.query_file);
  AppendIntegerDirective(content, "Subscriptions", spec.subscriptions);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         spec.maximum_concurrent_jobs);
  AppendIntegerDirective(content, "MaximumConsoleConnections",
                         spec.maximum_console_connections);
  AppendIntegerDirective(content, "AbsoluteJobTimeout",
                         spec.absolute_job_timeout);
  AppendBoolDirective(content, "TlsAuthenticate", spec.tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", spec.tls_enable);
  AppendBoolDirective(content, "TlsRequire", spec.tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", spec.tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", spec.tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", spec.tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", spec.tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", spec.tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        spec.tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir",
                        spec.tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        spec.tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", spec.tls_certificate);
  AppendQuotedDirective(content, "TlsKey", spec.tls_key);
  AppendRepeatedQuotedDirective(content, "TlsAllowedCn", spec.tls_allowed_cn);
  AppendQuotedDirective(content, "VerId", spec.ver_id);
  AppendQuotedDirective(content, "LogTimestampFormat",
                        spec.log_timestamp_format);
  AppendQuotedDirective(content, "SecureEraseCommand",
                        spec.secure_erase_command);
  AppendBoolDirective(content, "EnableKtls", spec.enable_ktls);
  AppendIntegerDirective(content, "FdConnectTimeout", spec.fd_connect_timeout);
  AppendIntegerDirective(content, "SdConnectTimeout", spec.sd_connect_timeout);
  AppendIntegerDirective(content, "HeartbeatInterval", spec.heartbeat_interval);
  AppendIntegerDirective(content, "StatisticsRetention",
                         spec.statistics_retention);
  AppendIntegerDirective(content, "StatisticsCollectInterval",
                         spec.statistics_collect_interval);
  AppendQuotedDirective(content, "Description", spec.description);
  AppendQuotedDirective(content, "KeyEncryptionKey", spec.key_encryption_key);
  AppendBoolDirective(content, "NdmpSnooping", spec.ndmp_snooping);
  AppendIntegerDirective(content, "NdmpLogLevel", spec.ndmp_log_level);
  AppendBoolDirective(content, "NdmpNamelistFhinfoSetZeroForInvalidUquad",
                      spec.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad);
  AppendBoolDirective(content, "Auditing", spec.auditing);
  AppendRepeatedBareosDirective(content, "AuditEvents", spec.audit_events);
  AppendQuotedDirective(content, "WorkingDirectory", spec.working_directory);
  AppendQuotedDirective(content, "PluginDirectory", spec.plugin_directory);
  AppendRepeatedBareosDirective(content, "PluginNames", spec.plugin_names);
  AppendQuotedDirective(content, "ScriptsDirectory", spec.scripts_directory);
  AppendBareosDirective(content, "Messages", spec.messages);
  content << "}\n";
  return content.str();
}

std::string BuildStorageDaemonDirectorResourceContent(
    std::string_view director_name,
    std::string_view password,
    std::string_view description,
    const std::optional<bool>& monitor = std::nullopt,
    const std::optional<uint64_t>& maximum_bandwidth_per_job = std::nullopt,
    const std::optional<std::string>& key_encryption_key = std::nullopt,
    const std::optional<bool>& tls_authenticate = std::nullopt,
    const std::optional<bool>& tls_enable = std::nullopt,
    const std::optional<bool>& tls_require = std::nullopt,
    const std::optional<bool>& tls_verify_peer = std::nullopt,
    const std::optional<std::string>& tls_cipher_list = std::nullopt,
    const std::optional<std::string>& tls_cipher_suites = std::nullopt,
    const std::optional<std::string>& tls_dh_file = std::nullopt,
    const std::optional<std::string>& tls_protocol = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_file = std::nullopt,
    const std::optional<std::string>& tls_ca_certificate_dir = std::nullopt,
    const std::optional<std::string>& tls_certificate_revocation_list
    = std::nullopt,
    const std::optional<std::string>& tls_certificate = std::nullopt,
    const std::optional<std::string>& tls_key = std::nullopt,
    const std::optional<std::vector<std::string>>& tls_allowed_cn
    = std::nullopt)
{
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n";
  AppendBoolDirective(content, "Monitor", monitor);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         maximum_bandwidth_per_job);
  AppendQuotedDirective(content, "KeyEncryptionKey", key_encryption_key);
  AppendBoolDirective(content, "TlsAuthenticate", tls_authenticate);
  AppendBoolDirective(content, "TlsEnable", tls_enable);
  AppendBoolDirective(content, "TlsRequire", tls_require);
  AppendBoolDirective(content, "TlsVerifyPeer", tls_verify_peer);
  AppendQuotedDirective(content, "TlsCipherList", tls_cipher_list);
  AppendQuotedDirective(content, "TlsCipherSuites", tls_cipher_suites);
  AppendQuotedDirective(content, "TlsDhFile", tls_dh_file);
  AppendQuotedDirective(content, "TlsProtocol", tls_protocol);
  AppendQuotedDirective(content, "TlsCaCertificateFile",
                        tls_ca_certificate_file);
  AppendQuotedDirective(content, "TlsCaCertificateDir", tls_ca_certificate_dir);
  AppendQuotedDirective(content, "TlsCertificateRevocationList",
                        tls_certificate_revocation_list);
  AppendQuotedDirective(content, "TlsCertificate", tls_certificate);
  AppendQuotedDirective(content, "TlsKey", tls_key);
  if (tls_allowed_cn) {
    AppendRepeatedQuotedDirective(content, "TlsAllowedCn", *tls_allowed_cn);
  }
  content << "}\n";
  return content.str();
}

std::string BuildStorageDaemonDeviceResourceContent(
    std::string_view device_name,
    std::string_view media_type,
    std::string_view archive_device,
    std::string_view device_type,
    const std::optional<std::string>& access_mode,
    const std::optional<std::string>& device_options,
    const std::optional<std::string>& diagnostic_device,
    const std::optional<bool>& hardware_end_of_file,
    const std::optional<bool>& hardware_end_of_medium,
    const std::optional<bool>& backward_space_record,
    const std::optional<bool>& backward_space_file,
    const std::optional<bool>& bsf_at_eom,
    const std::optional<bool>& two_eof,
    const std::optional<bool>& forward_space_record,
    const std::optional<bool>& forward_space_file,
    const std::optional<bool>& fast_forward_space_file,
    const std::optional<bool>& removable_media,
    const std::optional<bool>& random_access,
    const std::optional<bool>& automatic_mount,
    const std::optional<bool>& label_media,
    const std::optional<bool>& always_open,
    const std::optional<bool>& autochanger,
    const std::optional<bool>& close_on_poll,
    const std::optional<bool>& block_positioning,
    const std::optional<bool>& use_mtiocget,
    const std::optional<bool>& check_labels,
    const std::optional<bool>& requires_mount,
    const std::optional<bool>& offline_on_unmount,
    const std::optional<bool>& block_checksum,
    const std::optional<bool>& auto_select,
    const std::optional<std::string>& changer_device,
    const std::optional<std::string>& changer_command,
    const std::optional<std::string>& alert_command,
    const std::optional<uint64_t>& maximum_changer_wait,
    const std::optional<uint64_t>& maximum_open_wait,
    const std::optional<uint32_t>& maximum_open_volumes,
    const std::optional<uint32_t>& maximum_network_buffer_size,
    const std::optional<uint64_t>& volume_poll_interval,
    const std::optional<uint64_t>& maximum_rewind_wait,
    const std::optional<uint32_t>& label_block_size,
    const std::optional<uint32_t>& minimum_block_size,
    const std::optional<uint32_t>& maximum_block_size,
    const std::optional<uint64_t>& maximum_file_size,
    const std::optional<uint64_t>& volume_capacity,
    const std::optional<uint32_t>& maximum_concurrent_jobs,
    const std::optional<std::string>& spool_directory,
    const std::optional<uint64_t>& maximum_spool_size,
    const std::optional<uint64_t>& maximum_job_spool_size,
    const std::optional<uint16_t>& drive_index,
    const std::optional<std::string>& mount_point,
    const std::optional<std::string>& mount_command,
    const std::optional<std::string>& unmount_command,
    const std::optional<std::string>& label_type,
    const std::optional<bool>& no_rewind_on_close,
    const std::optional<bool>& drive_tape_alert_enabled,
    const std::optional<bool>& drive_crypto_enabled,
    const std::optional<bool>& query_crypto_status,
    const std::optional<std::string>& auto_deflate,
    const std::optional<std::string>& auto_deflate_algorithm,
    const std::optional<uint16_t>& auto_deflate_level,
    const std::optional<std::string>& auto_inflate,
    const std::optional<bool>& collect_statistics,
    const std::optional<bool>& eof_on_error_is_eot,
    const std::optional<uint32_t>& count,
    std::string_view description)
{
  std::ostringstream content;
  content << "Device {\n"
          << "  Name = " << QuoteBareosString(device_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Media Type = " << media_type << "\n"
          << "  Device Type = " << QuoteBareosString(device_type) << "\n"
          << "  Archive Device = " << archive_device << "\n";
  AppendBareosDirective(content, "AccessMode", access_mode);
  AppendQuotedDirective(content, "DeviceOptions", device_options);
  AppendQuotedDirective(content, "DiagnosticDevice", diagnostic_device);
  AppendBoolDirective(content, "HardwareEndOfFile", hardware_end_of_file);
  AppendBoolDirective(content, "HardwareEndOfMedium", hardware_end_of_medium);
  AppendBoolDirective(content, "BackwardSpaceRecord", backward_space_record);
  AppendBoolDirective(content, "BackwardSpaceFile", backward_space_file);
  AppendBoolDirective(content, "BsfAtEom", bsf_at_eom);
  AppendBoolDirective(content, "TwoEof", two_eof);
  AppendBoolDirective(content, "ForwardSpaceRecord", forward_space_record);
  AppendBoolDirective(content, "ForwardSpaceFile", forward_space_file);
  AppendBoolDirective(content, "FastForwardSpaceFile", fast_forward_space_file);
  AppendBoolDirective(content, "RemovableMedia", removable_media);
  AppendBoolDirective(content, "RandomAccess", random_access);
  AppendBoolDirective(content, "AutomaticMount", automatic_mount);
  AppendBoolDirective(content, "LabelMedia", label_media);
  AppendBoolDirective(content, "AlwaysOpen", always_open);
  AppendBoolDirective(content, "Autochanger", autochanger);
  AppendBoolDirective(content, "CloseOnPoll", close_on_poll);
  AppendBoolDirective(content, "BlockPositioning", block_positioning);
  AppendBoolDirective(content, "UseMtiocget", use_mtiocget);
  AppendBoolDirective(content, "CheckLabels", check_labels);
  AppendBoolDirective(content, "RequiresMount", requires_mount);
  AppendBoolDirective(content, "OfflineOnUnmount", offline_on_unmount);
  AppendBoolDirective(content, "BlockChecksum", block_checksum);
  AppendBoolDirective(content, "AutoSelect", auto_select);
  AppendQuotedDirective(content, "ChangerDevice", changer_device);
  AppendQuotedDirective(content, "ChangerCommand", changer_command);
  AppendQuotedDirective(content, "AlertCommand", alert_command);
  AppendIntegerDirective(content, "MaximumChangerWait", maximum_changer_wait);
  AppendIntegerDirective(content, "MaximumOpenWait", maximum_open_wait);
  AppendIntegerDirective(content, "MaximumOpenVolumes", maximum_open_volumes);
  AppendIntegerDirective(content, "MaximumNetworkBufferSize",
                         maximum_network_buffer_size);
  AppendIntegerDirective(content, "VolumePollInterval", volume_poll_interval);
  AppendIntegerDirective(content, "MaximumRewindWait", maximum_rewind_wait);
  AppendIntegerDirective(content, "LabelBlockSize", label_block_size);
  AppendIntegerDirective(content, "MinimumBlockSize", minimum_block_size);
  AppendIntegerDirective(content, "MaximumBlockSize", maximum_block_size);
  AppendIntegerDirective(content, "MaximumFileSize", maximum_file_size);
  AppendIntegerDirective(content, "VolumeCapacity", volume_capacity);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         maximum_concurrent_jobs);
  AppendQuotedDirective(content, "SpoolDirectory", spool_directory);
  AppendIntegerDirective(content, "MaximumSpoolSize", maximum_spool_size);
  AppendIntegerDirective(content, "MaximumJobSpoolSize",
                         maximum_job_spool_size);
  AppendIntegerDirective(content, "DriveIndex", drive_index);
  AppendQuotedDirective(content, "MountPoint", mount_point);
  AppendQuotedDirective(content, "MountCommand", mount_command);
  AppendQuotedDirective(content, "UnmountCommand", unmount_command);
  AppendBareosDirective(content, "LabelType", label_type);
  AppendBoolDirective(content, "NoRewindOnClose", no_rewind_on_close);
  AppendBoolDirective(content, "DriveTapeAlertEnabled",
                      drive_tape_alert_enabled);
  AppendBoolDirective(content, "DriveCryptoEnabled", drive_crypto_enabled);
  AppendBoolDirective(content, "QueryCryptoStatus", query_crypto_status);
  AppendBareosDirective(content, "AutoDeflate", auto_deflate);
  AppendBareosDirective(content, "AutoDeflateAlgorithm",
                        auto_deflate_algorithm);
  AppendIntegerDirective(content, "AutoDeflateLevel", auto_deflate_level);
  AppendBareosDirective(content, "AutoInflate", auto_inflate);
  AppendBoolDirective(content, "CollectStatistics", collect_statistics);
  AppendBoolDirective(content, "EofOnErrorIsEot", eof_on_error_is_eot);
  AppendIntegerDirective(content, "Count", count);
  content << "}\n";
  return content.str();
}

std::string BuildStorageDaemonNdmpResourceContent(
    std::string_view ndmp_name,
    const std::optional<std::string>& description,
    std::string_view username,
    std::string_view password,
    std::string_view auth_type,
    uint32_t log_level)
{
  std::ostringstream content;
  content << "Ndmp {\n"
          << "  Name = " << QuoteBareosString(ndmp_name) << "\n";
  AppendQuotedDirective(content, "Description", description);
  content << "  Username = " << QuoteBareosString(username) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "  AuthType = " << auth_type << "\n"
          << "  LogLevel = " << log_level << "\n"
          << "}\n";
  return content.str();
}

std::string BuildStorageDaemonAutochangerResourceContent(
    std::string_view autochanger_name,
    const std::vector<std::string>& devices,
    std::string_view changer_device,
    std::string_view changer_command,
    std::string_view description)
{
  std::ostringstream content;
  content << "Autochanger {\n"
          << "  Name = " << QuoteBareosString(autochanger_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n";
  AppendRepeatedBareosDirective(content, "Device", devices);
  AppendQuotedDirective(
      content, "Changer Device",
      std::optional<std::string>{std::string{changer_device}});
  AppendQuotedDirective(
      content, "Changer Command",
      std::optional<std::string>{std::string{changer_command}});
  content << "}\n";
  return content.str();
}

std::string DefaultClientDirectorStubDescription(std::string_view client_name,
                                                 std::string_view director_name)
{
  return "Managed director stub for client " + std::string{client_name}
         + " and director " + std::string{director_name};
}

std::string DefaultDirectorClientDescription(std::string_view client_name,
                                             std::string_view director_name)
{
  return "Managed client resource for " + std::string{client_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorStorageDescription(std::string_view storage_name,
                                              std::string_view director_name)
{
  return "Managed storage resource for " + std::string{storage_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorConsoleDescription(std::string_view console_name,
                                              std::string_view director_name)
{
  return "Managed console resource for " + std::string{console_name}
         + " in director " + std::string{director_name};
}

std::string DefaultConsoleConsoleDescription(
    std::string_view console_name,
    std::string_view console_config_name)
{
  return "Managed bconsole resource for " + std::string{console_name}
         + " in console config " + std::string{console_config_name};
}

std::string DefaultConsoleDirectorDescription(
    std::string_view director_name,
    std::string_view console_config_name)
{
  return "Managed bconsole director resource for " + std::string{director_name}
         + " in console config " + std::string{console_config_name};
}

std::string DefaultDirectorUserDescription(std::string_view user_name,
                                           std::string_view director_name)
{
  return "Managed user resource for " + std::string{user_name} + " in director "
         + std::string{director_name};
}

std::string DefaultDirectorProfileDescription(std::string_view profile_name,
                                              std::string_view director_name)
{
  return "Managed profile resource for " + std::string{profile_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorPoolDescription(std::string_view pool_name,
                                           std::string_view director_name)
{
  return "Managed pool resource for " + std::string{pool_name} + " in director "
         + std::string{director_name};
}

std::string DefaultDirectorCatalogDescription(std::string_view catalog_name,
                                              std::string_view director_name)
{
  return "Managed catalog resource for " + std::string{catalog_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorMessagesDescription(std::string_view messages_name,
                                               std::string_view director_name)
{
  return "Managed messages resource for " + std::string{messages_name}
         + " in director " + std::string{director_name};
}

std::string DefaultStorageDaemonMessagesDescription(
    std::string_view messages_name,
    std::string_view storage_name)
{
  return "Managed storage-daemon messages resource for "
         + std::string{messages_name} + " in storage "
         + std::string{storage_name};
}

std::string DefaultDirectorScheduleDescription(std::string_view schedule_name,
                                               std::string_view director_name)
{
  return "Managed schedule resource for " + std::string{schedule_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorCounterDescription(std::string_view counter_name,
                                              std::string_view director_name)
{
  return "Managed counter resource for " + std::string{counter_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorFilesetDescription(std::string_view fileset_name,
                                              std::string_view director_name)
{
  return "Managed fileset resource for " + std::string{fileset_name}
         + " in director " + std::string{director_name};
}

std::string DefaultDirectorJobDescription(std::string_view job_name,
                                          std::string_view director_name)
{
  return "Managed job resource for " + std::string{job_name} + " in director "
         + std::string{director_name};
}

std::string DefaultDirectorJobDefsDescription(std::string_view jobdefs_name,
                                              std::string_view director_name)
{
  return "Managed jobdefs resource for " + std::string{jobdefs_name}
         + " in director " + std::string{director_name};
}

std::string DefaultStorageDaemonDirectorDescription(
    std::string_view director_name,
    std::string_view storage_name)
{
  return "Managed storage-daemon director resource for "
         + std::string{director_name} + " synced from storage "
         + std::string{storage_name};
}

std::string DefaultStorageDaemonNdmpDescription(std::string_view ndmp_name,
                                                std::string_view storage_name)
{
  return "Managed storage-daemon NDMP resource for " + std::string{ndmp_name}
         + " in storage " + std::string{storage_name};
}

std::string DefaultStorageDaemonDeviceDescription(std::string_view device_name,
                                                  std::string_view storage_name)
{
  return "Managed storage-daemon device resource for "
         + std::string{device_name} + " synced from storage "
         + std::string{storage_name};
}

std::string DefaultStorageDaemonAutochangerDescription(
    std::string_view autochanger_name,
    std::string_view storage_name)
{
  return "Managed storage-daemon autochanger resource for "
         + std::string{autochanger_name} + " in storage "
         + std::string{storage_name};
}

OperationResult<std::string> NormalizeAuthType(std::string_view auth_type)
{
  std::string normalized;
  normalized.reserve(auth_type.size());
  std::transform(auth_type.begin(), auth_type.end(),
                 std::back_inserter(normalized),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "none") { return {.value = std::string{"None"}}; }
  if (normalized == "clear") { return {.value = std::string{"Clear"}}; }
  if (normalized == "md5") { return {.value = std::string{"MD5"}}; }
  return {.error = "field 'auth_type' must be one of None, Clear, or MD5."};
}

OperationResult<std::string> NormalizeDirectorClientProtocol(
    std::string_view protocol)
{
  std::string normalized;
  normalized.reserve(protocol.size());
  std::transform(protocol.begin(), protocol.end(),
                 std::back_inserter(normalized),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "native") { return {.value = std::string{"Native"}}; }
  if (normalized == "ndmpv2") { return {.value = std::string{"NDMPV2"}}; }
  if (normalized == "ndmpv3") { return {.value = std::string{"NDMPV3"}}; }
  if (normalized == "ndmpv4") { return {.value = std::string{"NDMPV4"}}; }
  return {.error
          = "field 'protocol' must be one of Native, NDMPV2, NDMPV3, or "
            "NDMPV4."};
}

OperationResult<std::string> NormalizeDirectorJobProtocol(
    std::string_view protocol)
{
  std::string normalized;
  normalized.reserve(protocol.size());
  std::transform(protocol.begin(), protocol.end(),
                 std::back_inserter(normalized),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "native") { return {.value = std::string{"Native"}}; }
  if (normalized == "ndmp" || normalized == "ndmp_bareos") {
    return {.value = std::string{"NDMP_BAREOS"}};
  }
  if (normalized == "ndmp_native") {
    return {.value = std::string{"NDMP_NATIVE"}};
  }
  return {.error
          = "field 'protocol' must be one of Native, NDMP_BAREOS, NDMP, or "
            "NDMP_NATIVE."};
}

OperationResult<std::string> NormalizeDirectorJobReplace(
    std::string_view replace)
{
  std::string normalized;
  normalized.reserve(replace.size());
  std::transform(replace.begin(), replace.end(), std::back_inserter(normalized),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "always") { return {.value = std::string{"Always"}}; }
  if (normalized == "ifnewer") { return {.value = std::string{"IfNewer"}}; }
  if (normalized == "ifolder") { return {.value = std::string{"IfOlder"}}; }
  if (normalized == "never") { return {.value = std::string{"Never"}}; }
  return {.error
          = "field 'replace' must be one of Always, IfNewer, IfOlder, or "
            "Never."};
}

OperationResult<std::string> NormalizeDirectorJobSelectionType(
    std::string_view selection_type)
{
  std::string normalized;
  normalized.reserve(selection_type.size());
  std::transform(selection_type.begin(), selection_type.end(),
                 std::back_inserter(normalized),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "smallestvolume") {
    return {.value = std::string{"SmallestVolume"}};
  }
  if (normalized == "oldestvolume") {
    return {.value = std::string{"OldestVolume"}};
  }
  if (normalized == "pooloccupancy") {
    return {.value = std::string{"PoolOccupancy"}};
  }
  if (normalized == "pooltime") { return {.value = std::string{"PoolTime"}}; }
  if (normalized == "pooluncopiedjobs") {
    return {.value = std::string{"PoolUncopiedJobs"}};
  }
  if (normalized == "client") { return {.value = std::string{"Client"}}; }
  if (normalized == "volume") { return {.value = std::string{"Volume"}}; }
  if (normalized == "job") { return {.value = std::string{"Job"}}; }
  if (normalized == "sqlquery") { return {.value = std::string{"SqlQuery"}}; }
  return {.error
          = "field 'selection_type' must be one of SmallestVolume, "
            "OldestVolume, PoolOccupancy, PoolTime, PoolUncopiedJobs, "
            "Client, Volume, Job, or SqlQuery."};
}

std::string RenderDirectorClientProtocol(uint32_t protocol)
{
  switch (protocol) {
    case APT_NATIVE:
      return "Native";
    case APT_NDMPV2:
      return "NDMPV2";
    case APT_NDMPV3:
      return "NDMPV3";
    case APT_NDMPV4:
      return "NDMPV4";
    default:
      return "Unknown";
  }
}

std::string RenderDirectorJobProtocol(uint32_t protocol)
{
  switch (protocol) {
    case PT_NATIVE:
      return "Native";
    case PT_NDMP_BAREOS:
      return "NDMP_BAREOS";
    case PT_NDMP_NATIVE:
      return "NDMP_NATIVE";
    default:
      return "Unknown";
  }
}

std::string RenderDirectorJobReplace(uint32_t replace)
{
  switch (replace) {
    case REPLACE_ALWAYS:
      return "Always";
    case REPLACE_IFNEWER:
      return "IfNewer";
    case REPLACE_IFOLDER:
      return "IfOlder";
    case REPLACE_NEVER:
      return "Never";
    default:
      return "Unknown";
  }
}

std::string RenderDirectorJobSelectionType(uint32_t selection_type)
{
  switch (selection_type) {
    case MT_SMALLEST_VOL:
      return "SmallestVolume";
    case MT_OLDEST_VOL:
      return "OldestVolume";
    case MT_POOL_OCCUPANCY:
      return "PoolOccupancy";
    case MT_POOL_TIME:
      return "PoolTime";
    case MT_POOL_UNCOPIED_JOBS:
      return "PoolUncopiedJobs";
    case MT_CLIENT:
      return "Client";
    case MT_VOLUME:
      return "Volume";
    case MT_JOB:
      return "Job";
    case MT_SQLQUERY:
      return "SqlQuery";
    default:
      return "Unknown";
  }
}

OperationResult<std::string> NormalizeStorageDeviceIoDirection(
    std::string_view value)
{
  std::string normalized{value};
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "in" || normalized == "read" || normalized == "readonly") {
    return {.value = std::string{"read"}};
  }
  if (normalized == "out" || normalized == "write"
      || normalized == "writeonly") {
    return {.value = std::string{"write"}};
  }
  if (normalized == "both" || normalized == "readwrite") {
    return {.value = std::string{"readwrite"}};
  }
  return {.error = "unsupported storage device io direction value '"
                   + std::string{value} + "'."};
}

std::string RenderStorageDeviceIoDirection(storagedaemon::IODirection value)
{
  switch (value) {
    case storagedaemon::IODirection::READ:
      return "read";
    case storagedaemon::IODirection::WRITE:
      return "write";
    case storagedaemon::IODirection::READ_WRITE:
      return "readwrite";
    default:
      return "Unknown";
  }
}

OperationResult<std::string> NormalizeStorageDeviceCompressionAlgorithm(
    std::string_view value)
{
  std::string normalized{value};
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "gzip" || normalized == "lzo" || normalized == "lzfast"
      || normalized == "lz4" || normalized == "lz4hc") {
    return {.value = normalized};
  }
  return {.error = "unsupported storage device compression algorithm value '"
                   + std::string{value} + "'."};
}

std::string RenderStorageDeviceCompressionAlgorithm(uint32_t value)
{
  switch (value) {
    case COMPRESS_GZIP:
      return "gzip";
    case COMPRESS_LZO1X:
      return "lzo";
    case COMPRESS_FZFZ:
      return "lzfast";
    case COMPRESS_FZ4L:
      return "lz4";
    case COMPRESS_FZ4H:
      return "lz4hc";
    default:
      return "Unknown";
  }
}

std::string RenderAuthType(uint32_t auth_type)
{
  switch (auth_type) {
    case AT_NONE:
      return "None";
    case AT_CLEAR:
      return "Clear";
    case AT_MD5:
      return "MD5";
    default:
      return "None";
  }
}

OperationResult<std::vector<std::string>> ExtractAutochangerDeviceNames(
    const storagedaemon::AutochangerResource& autochanger,
    std::string_view autochanger_name)
{
  std::vector<std::string> device_names;
  std::unordered_set<std::string> seen;
  if (!autochanger.device_resources) {
    return {.value = std::move(device_names)};
  }
  for (auto* device_resource : autochanger.device_resources) {
    if (!device_resource) { continue; }
    if (device_resource->multiplied_device_resource) { continue; }
    if (!device_resource->resource_name_
        || device_resource->resource_name_[0] == '\0') {
      return {.error = "storage-daemon autochanger '"
                       + std::string{autochanger_name}
                       + "' references a device without a name."};
    }
    std::string device_name{device_resource->resource_name_};
    if (!device_name.empty() && device_name[0] == '$') { continue; }
    if (seen.insert(device_name).second) {
      device_names.emplace_back(std::move(device_name));
    }
  }
  return {.value = std::move(device_names)};
}

OperationResult<std::string> RenderPasswordForConfig(const s_password& password,
                                                     std::string_view context)
{
  switch (password.encoding) {
    case p_encoding_clear:
      return {.value = std::string{password.value ? password.value : ""}};
    case p_encoding_md5: {
      std::string rendered = "[md5]";
      if (password.value) { rendered += password.value; }
      return {.value = std::move(rendered)};
    }
    default:
      return {.error = "failed to render " + std::string{context} + "."};
  }
}

bool IsSafePathSegment(std::string_view value)
{
  return !value.empty() && value != "." && value != ".."
         && value.find('/') == std::string_view::npos
         && value.find('\\') == std::string_view::npos;
}

bool IsSafeBareosToken(std::string_view value)
{
  return !value.empty()
         && value.find_first_of(" \t\r\n{}\"'\\") == std::string_view::npos;
}

std::optional<std::string> CleanupCreatedClientStub(
    const std::filesystem::path& client_root,
    const std::filesystem::path& stub_path,
    bool client_root_existed)
{
  std::error_code error_code;
  if (client_root_existed) {
    std::filesystem::remove(stub_path, error_code);
    error_code.clear();
    std::filesystem::remove(stub_path.parent_path(), error_code);
    error_code.clear();
    std::filesystem::remove(stub_path.parent_path().parent_path(), error_code);
    return std::nullopt;
  }

  std::filesystem::remove_all(client_root, error_code);
  if (error_code) {
    return "failed to roll back generated client root '" + client_root.string()
           + "': " + error_code.message();
  }
  return std::nullopt;
}

std::optional<std::string> RestoreClientStubFile(
    const std::filesystem::path& stub_path,
    const std::string& content)
{
  if (WriteFile(stub_path, content)) { return std::nullopt; }
  return "failed to restore client stub '" + stub_path.string() + "'.";
}

std::optional<std::string> RestoreDeletedFile(const std::filesystem::path& path,
                                              const std::string& content)
{
  std::error_code error_code;
  std::filesystem::create_directories(path.parent_path(), error_code);
  if (error_code) {
    return "failed to recreate directory '" + path.parent_path().string()
           + "': " + error_code.message();
  }
  if (WriteFile(path, content)) { return std::nullopt; }
  return "failed to restore file '" + path.string() + "'.";
}

std::optional<std::string> DeleteFileAndEmptyParents(
    const std::filesystem::path& file_path,
    const std::filesystem::path& stop_directory)
{
  std::error_code error_code;
  std::filesystem::remove(file_path, error_code);
  if (error_code) {
    return "failed to delete file '" + file_path.string()
           + "': " + error_code.message();
  }

  auto current = file_path.parent_path();
  while (!current.empty() && current != stop_directory) {
    std::filesystem::remove(current, error_code);
    if (error_code) {
      error_code.clear();
      break;
    }
    current = current.parent_path();
  }

  return std::nullopt;
}

std::optional<std::string> CleanupCreatedFile(
    const std::filesystem::path& file_path,
    const std::filesystem::path& stop_directory)
{
  std::error_code error_code;
  std::filesystem::remove(file_path, error_code);
  if (error_code) {
    return "failed to roll back created file '" + file_path.string()
           + "': " + error_code.message();
  }

  auto current = file_path.parent_path();
  while (!current.empty() && current != stop_directory) {
    std::filesystem::remove(current, error_code);
    if (error_code) {
      error_code.clear();
      break;
    }
    current = current.parent_path();
  }

  return std::nullopt;
}

std::string FormatParseFailure(std::string_view prefix,
                               const bconfig::ParseMessages& messages)
{
  std::vector<std::string> logs;
  AppendParseMessages(logs, messages, prefix);
  std::ostringstream rendered;
  rendered << prefix << "did not validate cleanly.";
  for (const auto& message : logs) { rendered << "\n" << message; }
  return rendered.str();
}

struct DirectorClientWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> address{};
  std::optional<std::string> lan_address{};
  std::optional<uint32_t> port{};
  std::optional<std::string> protocol{};
  std::optional<std::string> auth_type{};
  std::optional<std::string> catalog{};
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<bool> enabled{};
  std::optional<bool> passive{};
  std::optional<bool> strict_quotas{};
  std::optional<bool> quota_include_failed_jobs{};
  std::optional<uint64_t> soft_quota{};
  std::optional<uint64_t> hard_quota{};
  std::optional<uint64_t> soft_quota_grace_period{};
  std::optional<uint64_t> file_retention{};
  std::optional<uint64_t> job_retention{};
  std::optional<uint32_t> ndmp_log_level{};
  std::optional<uint32_t> ndmp_block_size{};
  std::optional<bool> ndmp_use_lmdb{};
  std::optional<bool> auto_prune{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::optional<bool> connection_from_director_to_client{};
  std::optional<bool> connection_from_client_to_director{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> description{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorStorageWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> address{};
  std::optional<std::string> lan_address{};
  std::optional<uint32_t> port{};
  std::optional<std::string> protocol{};
  std::optional<std::string> auth_type{};
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<std::string> device{};
  std::vector<std::string> devices{};
  std::optional<std::string> media_type{};
  std::optional<bool> autochanger{};
  std::optional<bool> enabled{};
  std::optional<bool> allow_compression{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> cache_status_interval{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint32_t> maximum_concurrent_read_jobs{};
  std::optional<std::string> paired_storage{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<bool> collect_statistics{};
  std::optional<std::string> ndmp_changer_device{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::optional<std::string> description{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorConsoleWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> use_pam_authentication{};
  std::array<std::vector<std::string>, directordaemon::Num_ACL> acl{};
  std::vector<std::string> profiles{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct ConsoleConsoleWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> director{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<std::string> rc_file{};
  std::optional<std::string> history_file{};
  std::optional<uint32_t> history_length{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::vector<std::string> director_resources{};
  std::vector<std::string> console_resources_before{};
  std::vector<std::string> console_resources_after{};
  bool exists{false};
};

std::string BuildRewrittenConsoleConfigContent(
    const ConsoleConsoleWriteContext& context,
    const std::optional<std::string>& managed_console_resource)
{
  std::vector<std::string> console_resources;
  console_resources.reserve(context.console_resources_before.size()
                            + context.console_resources_after.size()
                            + (managed_console_resource ? 1u : 0u));
  console_resources.insert(console_resources.end(),
                           context.console_resources_before.begin(),
                           context.console_resources_before.end());
  if (managed_console_resource) {
    console_resources.emplace_back(*managed_console_resource);
  }
  console_resources.insert(console_resources.end(),
                           context.console_resources_after.begin(),
                           context.console_resources_after.end());
  return BuildConsoleConfigFileContent(context.director_resources,
                                       console_resources);
}

struct ConsoleDirectorWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> address{};
  std::optional<uint32_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::vector<std::string> director_resources_before{};
  std::vector<std::string> director_resources_after{};
  std::vector<std::string> console_resources{};
  bool has_named_console{false};
  bool exists{false};
};

std::string BuildRewrittenConsoleConfigContent(
    const ConsoleDirectorWriteContext& context,
    const std::optional<std::string>& managed_director_resource)
{
  std::vector<std::string> director_resources;
  director_resources.reserve(context.director_resources_before.size()
                             + context.director_resources_after.size()
                             + (managed_director_resource ? 1u : 0u));
  director_resources.insert(director_resources.end(),
                            context.director_resources_before.begin(),
                            context.director_resources_before.end());
  if (managed_director_resource) {
    director_resources.emplace_back(*managed_director_resource);
  }
  director_resources.insert(director_resources.end(),
                            context.director_resources_after.begin(),
                            context.director_resources_after.end());
  return BuildConsoleConfigFileContent(director_resources,
                                       context.console_resources);
}

struct DirectorUserWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> description{};
  std::array<std::vector<std::string>, directordaemon::Num_ACL> acl{};
  std::vector<std::string> profiles{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorProfileWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> description{};
  std::array<std::vector<std::string>, directordaemon::Num_ACL> acl{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorPoolWriteContext {
  std::filesystem::path file_path{};
  DirectorPoolContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorCatalogWriteContext {
  std::filesystem::path file_path{};
  DirectorCatalogContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorMessagesWriteContext {
  std::filesystem::path file_path{};
  DirectorMessagesContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct ClientMessagesWriteContext {
  std::filesystem::path file_path{};
  DirectorMessagesContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct ClientDirectorStubWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> description{};
  std::optional<std::string> password{};
  std::optional<std::string> address{};
  std::optional<uint32_t> port{};
  std::optional<std::vector<std::string>> allowed_script_dirs{};
  std::optional<std::vector<std::string>> allowed_job_commands{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::optional<bool> connection_from_director_to_client{};
  std::optional<bool> connection_from_client_to_director{};
  std::optional<bool> monitor{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct StorageMessagesWriteContext {
  std::filesystem::path file_path{};
  DirectorMessagesContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct ClientDaemonWriteContext {
  std::filesystem::path file_path{};
  StorageDaemonContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorDaemonWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> password{};
  StorageDaemonContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct StorageDaemonWriteContext {
  std::filesystem::path file_path{};
  StorageDaemonContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorScheduleWriteContext {
  std::filesystem::path file_path{};
  DirectorScheduleContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorCounterWriteContext {
  std::filesystem::path file_path{};
  DirectorCounterContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorFilesetWriteContext {
  std::filesystem::path file_path{};
  DirectorFilesetContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorJobWriteContext {
  std::filesystem::path file_path{};
  DirectorJobContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorJobDefsWriteContext {
  std::filesystem::path file_path{};
  DirectorJobContentSpec content{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct StorageDaemonDirectorWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> monitor{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> key_encryption_key{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  bool exists{false};
  bool is_standalone_file{false};
  bool is_managed{false};
};

struct StorageDaemonDeviceWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> media_type{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> access_mode{};
  std::optional<std::string> device_options{};
  std::optional<std::string> diagnostic_device{};
  std::optional<bool> hardware_end_of_file{};
  std::optional<bool> hardware_end_of_medium{};
  std::optional<bool> backward_space_record{};
  std::optional<bool> backward_space_file{};
  std::optional<bool> bsf_at_eom{};
  std::optional<bool> two_eof{};
  std::optional<bool> forward_space_record{};
  std::optional<bool> forward_space_file{};
  std::optional<bool> fast_forward_space_file{};
  std::optional<bool> removable_media{};
  std::optional<bool> random_access{};
  std::optional<bool> automatic_mount{};
  std::optional<bool> label_media{};
  std::optional<bool> always_open{};
  std::optional<bool> autochanger{};
  std::optional<bool> close_on_poll{};
  std::optional<bool> block_positioning{};
  std::optional<bool> use_mtiocget{};
  std::optional<bool> check_labels{};
  std::optional<bool> requires_mount{};
  std::optional<bool> offline_on_unmount{};
  std::optional<bool> block_checksum{};
  std::optional<bool> auto_select{};
  std::optional<std::string> changer_device{};
  std::optional<std::string> changer_command{};
  std::optional<std::string> alert_command{};
  std::optional<uint64_t> maximum_changer_wait{};
  std::optional<uint64_t> maximum_open_wait{};
  std::optional<uint32_t> maximum_open_volumes{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<uint64_t> volume_poll_interval{};
  std::optional<uint64_t> maximum_rewind_wait{};
  std::optional<uint32_t> label_block_size{};
  std::optional<uint32_t> minimum_block_size{};
  std::optional<uint32_t> maximum_block_size{};
  std::optional<uint64_t> maximum_file_size{};
  std::optional<uint64_t> volume_capacity{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<std::string> spool_directory{};
  std::optional<uint64_t> maximum_spool_size{};
  std::optional<uint64_t> maximum_job_spool_size{};
  std::optional<uint16_t> drive_index{};
  std::optional<std::string> mount_point{};
  std::optional<std::string> mount_command{};
  std::optional<std::string> unmount_command{};
  std::optional<std::string> label_type{};
  std::optional<bool> no_rewind_on_close{};
  std::optional<bool> drive_tape_alert_enabled{};
  std::optional<bool> drive_crypto_enabled{};
  std::optional<bool> query_crypto_status{};
  std::optional<std::string> auto_deflate{};
  std::optional<std::string> auto_deflate_algorithm{};
  std::optional<uint16_t> auto_deflate_level{};
  std::optional<std::string> auto_inflate{};
  std::optional<bool> collect_statistics{};
  std::optional<bool> eof_on_error_is_eot{};
  std::optional<uint32_t> count{};
  std::optional<std::string> description{};
  bool exists{false};
  bool is_standalone_file{false};
  bool is_managed{false};
};

struct StorageNdmpWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> description{};
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<std::string> auth_type{};
  std::optional<uint32_t> log_level{};
  bool exists{false};
  bool is_standalone_file{false};
  bool is_managed{false};
};

struct StorageAutochangerWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::vector<std::string>> devices{};
  std::optional<std::string> changer_device{};
  std::optional<std::string> changer_command{};
  std::optional<std::string> description{};
  bool exists{false};
  bool is_standalone_file{false};
  bool is_managed{false};
};

struct StorageSyncTarget {
  DeploymentConfigRecord storage_config{};
};

OperationResult<DirectorClientWriteContext> LoadDirectorClientWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view client_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorClientWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "client"
                   / (std::string{client_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CLIENT, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CLIENT, resource)) {
    auto* client = dynamic_cast<directordaemon::ClientResource*>(resource);
    if (!client) { continue; }
    if (auto source = client->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CLIENT, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CLIENT, resource)) {
    auto* client = dynamic_cast<directordaemon::ClientResource*>(resource);
    if (!client || !client->resource_name_
        || client->resource_name_ != client_name) {
      continue;
    }

    context.exists = true;
    if (client->address && client->address[0] != '\0') {
      context.address = std::string{client->address};
    }
    if (client->lanaddress && client->lanaddress[0] != '\0') {
      context.lan_address = std::string{client->lanaddress};
    }
    if (client->FDport != 0) { context.port = client->FDport; }
    if (HasMemberSource(*client, {"Protocol"})) {
      const auto protocol = RenderDirectorClientProtocol(client->Protocol);
      if (protocol == "Unknown") {
        return {.error = "director client '" + std::string{client_name}
                         + "' has an unsupported Protocol value."};
      }
      context.protocol = protocol;
    }
    if (HasMemberSource(*client, {"AuthType"})) {
      const auto auth_type = RenderAuthType(client->AuthType);
      if (auth_type == "Unknown") {
        return {.error = "director client '" + std::string{client_name}
                         + "' has an unsupported AuthType value."};
      }
      context.auth_type = auth_type;
    }
    if (HasMemberSource(*client, {"Catalog"})) {
      context.catalog = CopyResourceName(client->catalog);
    }
    if (client->username && client->username[0] != '\0') {
      context.username = std::string{client->username};
    }
    if (HasMemberSource(*client, {"Enabled"})) {
      context.enabled = client->enabled;
    }
    if (HasMemberSource(*client, {"Passive"})) {
      context.passive = client->passive;
    }
    if (HasMemberSource(*client, {"StrictQuotas"})) {
      context.strict_quotas = client->StrictQuotas;
    }
    if (HasMemberSource(*client, {"QuotaIncludeFailedJobs"})) {
      context.quota_include_failed_jobs = client->QuotaIncludeFailedJobs;
    }
    if (HasMemberSource(*client, {"SoftQuota"})) {
      context.soft_quota = client->SoftQuota;
    }
    if (HasMemberSource(*client, {"HardQuota"})) {
      context.hard_quota = client->HardQuota;
    }
    if (HasMemberSource(*client, {"SoftQuotaGracePeriod"})) {
      context.soft_quota_grace_period
          = static_cast<uint64_t>(client->SoftQuotaGracePeriod);
    }
    if (HasMemberSource(*client, {"FileRetention"})) {
      context.file_retention = client->FileRetention;
    }
    if (HasMemberSource(*client, {"JobRetention"})) {
      context.job_retention = client->JobRetention;
    }
    if (HasMemberSource(*client, {"NdmpLogLevel"})) {
      context.ndmp_log_level = client->ndmp_loglevel;
    }
    if (HasMemberSource(*client, {"NdmpBlockSize"})) {
      context.ndmp_block_size = client->ndmp_blocksize;
    }
    if (HasMemberSource(*client, {"NdmpUseLmdb"})) {
      context.ndmp_use_lmdb = client->ndmp_use_lmdb;
    }
    if (HasMemberSource(*client, {"AutoPrune"})) {
      context.auto_prune = client->AutoPrune;
    }
    if (HasMemberSource(*client, {"TlsAuthenticate"})) {
      context.tls_authenticate = client->authenticate_;
    }
    if (HasMemberSource(*client, {"TlsEnable"})) {
      context.tls_enable = client->tls_enable_;
    }
    if (HasMemberSource(*client, {"TlsRequire"})) {
      context.tls_require = client->tls_require_;
    }
    if (HasMemberSource(*client, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = client->tls_cert_.verify_peer_;
    }
    if (HasMemberSource(*client, {"TlsCipherList"})
        && !client->cipherlist_.empty()) {
      context.tls_cipher_list = client->cipherlist_;
    }
    if (HasMemberSource(*client, {"TlsCipherSuites"})
        && !client->ciphersuites_.empty()) {
      context.tls_cipher_suites = client->ciphersuites_;
    }
    if (HasMemberSource(*client, {"TlsDhFile"})
        && !client->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = client->tls_cert_.dhfile_;
    }
    if (HasMemberSource(*client, {"TlsProtocol"})
        && !client->protocol_.empty()) {
      context.tls_protocol = client->protocol_;
    }
    if (HasMemberSource(*client, {"TlsCaCertificateFile"})
        && !client->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file = client->tls_cert_.ca_certfile_;
    }
    if (HasMemberSource(*client, {"TlsCaCertificateDir"})
        && !client->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir = client->tls_cert_.ca_certdir_;
    }
    if (HasMemberSource(*client, {"TlsCertificateRevocationList"})
        && !client->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list = client->tls_cert_.crlfile_;
    }
    if (HasMemberSource(*client, {"TlsCertificate"})
        && !client->tls_cert_.certfile_.empty()) {
      context.tls_certificate = client->tls_cert_.certfile_;
    }
    if (HasMemberSource(*client, {"TlsKey"})
        && !client->tls_cert_.keyfile_.empty()) {
      context.tls_key = client->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*client, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = client->tls_cert_.allowed_certificate_common_names_;
    }
    if (HasMemberSource(*client, {"ConnectionFromDirectorToClient"})) {
      context.connection_from_director_to_client = client->conn_from_dir_to_fd;
    }
    if (HasMemberSource(*client, {"ConnectionFromClientToDirector"})) {
      context.connection_from_client_to_director = client->conn_from_fd_to_dir;
    }
    if (HasMemberSource(*client, {"MaximumConcurrentJobs"})) {
      context.maximum_concurrent_jobs = client->MaxConcurrentJobs;
    }
    if (HasMemberSource(*client, {"HeartbeatInterval"})) {
      context.heartbeat_interval
          = static_cast<uint64_t>(client->heartbeat_interval);
    }
    if (HasMemberSource(*client, {"MaximumBandwidthPerJob"})) {
      if (client->max_bandwidth < 0) {
        return {.error = "director client '" + std::string{client_name}
                         + "' has a negative MaximumBandwidthPerJob."};
      }
      context.maximum_bandwidth_per_job
          = static_cast<uint64_t>(client->max_bandwidth);
    }
    if (client->description_ && client->description_[0] != '\0') {
      context.description = std::string{client->description_};
    }
    auto rendered_password = LoadSecretMemberSourceValue(
        *client, {"Password"}, client->password_,
        "director-side client password for '" + std::string{client_name} + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;

    auto source = client->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director client '" + std::string{client_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorStorageWriteContext> LoadDirectorStorageWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view storage_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorStorageWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "storage"
                   / (std::string{storage_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_STORAGE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_STORAGE, resource)) {
    auto* storage = dynamic_cast<directordaemon::StorageResource*>(resource);
    if (!storage) { continue; }
    if (auto source = storage->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_STORAGE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_STORAGE, resource)) {
    auto* storage = dynamic_cast<directordaemon::StorageResource*>(resource);
    if (!storage || !storage->resource_name_
        || storage->resource_name_ != storage_name) {
      continue;
    }

    context.exists = true;
    if (storage->address && storage->address[0] != '\0') {
      context.address = std::string{storage->address};
    }
    if (storage->lanaddress && storage->lanaddress[0] != '\0') {
      context.lan_address = std::string{storage->lanaddress};
    }
    if (storage->SDport != 0) { context.port = storage->SDport; }
    if (HasMemberSource(*storage, {"Protocol"})) {
      const auto protocol = RenderDirectorClientProtocol(storage->Protocol);
      if (protocol == "Unknown") {
        return {.error = "director storage '" + std::string{storage_name}
                         + "' has an unsupported Protocol value."};
      }
      context.protocol = protocol;
    }
    if (HasMemberSource(*storage, {"AuthType"})) {
      const auto auth_type = RenderAuthType(storage->AuthType);
      if (auth_type == "Unknown") {
        return {.error = "director storage '" + std::string{storage_name}
                         + "' has an unsupported AuthType value."};
      }
      context.auth_type = auth_type;
    }
    if (storage->username && storage->username[0] != '\0') {
      context.username = std::string{storage->username};
    }
    if (HasMemberSource(*storage, {"AutoChanger"})) {
      context.autochanger = storage->autochanger;
    }
    if (HasMemberSource(*storage, {"Enabled"})) {
      context.enabled = storage->enabled;
    }
    if (HasMemberSource(*storage, {"AllowCompression"})) {
      context.allow_compression = storage->AllowCompress;
    }
    if (HasMemberSource(*storage, {"HeartbeatInterval"})) {
      context.heartbeat_interval
          = static_cast<uint64_t>(storage->heartbeat_interval);
    }
    if (HasMemberSource(*storage, {"CacheStatusInterval"})) {
      context.cache_status_interval
          = static_cast<uint64_t>(storage->cache_status_interval);
    }
    if (HasMemberSource(*storage, {"MaximumConcurrentJobs"})) {
      context.maximum_concurrent_jobs = storage->MaxConcurrentJobs;
    }
    if (HasMemberSource(*storage, {"MaximumConcurrentReadJobs"})) {
      context.maximum_concurrent_read_jobs = storage->MaxConcurrentReadJobs;
    }
    if (HasMemberSource(*storage, {"PairedStorage"})) {
      context.paired_storage = CopyResourceName(storage->paired_storage);
    }
    if (storage->description_ && storage->description_[0] != '\0') {
      context.description = std::string{storage->description_};
    }
    if (HasMemberSource(*storage, {"MaximumBandwidthPerJob"})) {
      if (storage->max_bandwidth < 0) {
        return {.error = "director storage '" + std::string{storage_name}
                         + "' has a negative MaximumBandwidthPerJob."};
      }
      context.maximum_bandwidth_per_job
          = static_cast<uint64_t>(storage->max_bandwidth);
    }
    if (HasMemberSource(*storage, {"CollectStatistics"})) {
      context.collect_statistics = storage->collectstats;
    }
    if (storage->ndmp_changer_device
        && storage->ndmp_changer_device[0] != '\0') {
      context.ndmp_changer_device = std::string{storage->ndmp_changer_device};
    }
    if (HasMemberSource(*storage, {"TlsAuthenticate"})) {
      context.tls_authenticate = storage->authenticate_;
    }
    if (HasMemberSource(*storage, {"TlsEnable"})) {
      context.tls_enable = storage->tls_enable_;
    }
    if (HasMemberSource(*storage, {"TlsRequire"})) {
      context.tls_require = storage->tls_require_;
    }
    if (HasMemberSource(*storage, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = storage->tls_cert_.verify_peer_;
    }
    if (!storage->cipherlist_.empty()) {
      context.tls_cipher_list = storage->cipherlist_;
    }
    if (!storage->ciphersuites_.empty()) {
      context.tls_cipher_suites = storage->ciphersuites_;
    }
    if (!storage->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = storage->tls_cert_.dhfile_;
    }
    if (!storage->protocol_.empty()) {
      context.tls_protocol = storage->protocol_;
    }
    if (!storage->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file = storage->tls_cert_.ca_certfile_;
    }
    if (!storage->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir = storage->tls_cert_.ca_certdir_;
    }
    if (!storage->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list = storage->tls_cert_.crlfile_;
    }
    if (!storage->tls_cert_.certfile_.empty()) {
      context.tls_certificate = storage->tls_cert_.certfile_;
    }
    if (!storage->tls_cert_.keyfile_.empty()) {
      context.tls_key = storage->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*storage, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = storage->tls_cert_.allowed_certificate_common_names_;
    }
    if (storage->media_type && storage->media_type[0] != '\0') {
      context.media_type = std::string{storage->media_type};
    }
    auto rendered_password = LoadSecretMemberSourceValue(
        *storage, {"Password"}, storage->password_,
        "director-side storage password for '" + std::string{storage_name}
            + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;

    auto source = storage->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director storage '" + std::string{storage_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    static const std::set<std::string> kDeviceDirectives{"Device"};
    auto raw_devices = ExtractNamedTopLevelDirectiveValues(
        context.file_path, "Storage", storage_name, kDeviceDirectives);
    if (!raw_devices) { return {.error = raw_devices.error}; }
    if (!raw_devices.value->empty()) {
      context.devices = *raw_devices.value;
      context.device = raw_devices.value->front();
    } else if (!storage->devices.empty()) {
      if (storage->devices.front().name.empty()) {
        return {.error = "director storage '" + std::string{storage_name}
                         + "' has an empty Device value."};
      }
      context.device = storage->devices.front().name;
      context.devices = std::vector<std::string>{storage->devices.front().name};
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

std::vector<std::string> CopyAclValues(alist<const char*>* list)
{
  std::vector<std::string> values;
  if (!list) { return values; }

  values.reserve(list->size());
  for (int index = 0; index < list->size(); ++index) {
    const auto* value = list->get(index);
    values.emplace_back(value ? value : "");
  }
  return values;
}

std::vector<std::string> CopyProfileNames(
    alist<directordaemon::ProfileResource*>* profiles)
{
  std::vector<std::string> names;
  if (!profiles) { return names; }

  names.reserve(profiles->size());
  for (auto* profile : profiles) {
    if (profile && profile->resource_name_) {
      names.emplace_back(profile->resource_name_);
    }
  }
  return names;
}

std::vector<std::string> CopyStorageNames(
    alist<directordaemon::StorageResource*>* storages)
{
  std::vector<std::string> names;
  if (!storages) { return names; }

  names.reserve(storages->size());
  for (auto* storage : storages) {
    if (storage && storage->resource_name_) {
      names.emplace_back(storage->resource_name_);
    }
  }
  return names;
}

std::string TrimAsciiWhitespace(std::string_view value)
{
  size_t begin = 0;
  while (begin < value.size()
         && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  size_t end = value.size();
  while (end > begin
         && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return std::string{value.substr(begin, end - begin)};
}

std::string NormalizeDirectiveName(std::string_view value)
{
  std::string normalized;
  normalized.reserve(value.size());
  for (const char character : value) {
    if (std::isspace(static_cast<unsigned char>(character))) { continue; }
    normalized.push_back(character);
  }
  return normalized;
}

std::string NormalizeDirectiveLookupName(std::string_view value)
{
  auto normalized = NormalizeDirectiveName(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return normalized;
}

std::string UnquoteBareosString(std::string_view value)
{
  if (value.size() >= 2
      && ((value.front() == '"' && value.back() == '"')
          || (value.front() == '\'' && value.back() == '\''))) {
    return std::string{value.substr(1, value.size() - 2)};
  }
  return std::string{value};
}

std::optional<std::string> ParseTopLevelDirectiveValue(std::string_view line,
                                                       std::string_view name)
{
  const auto trimmed = TrimAsciiWhitespace(line);
  if (trimmed.empty() || trimmed[0] == '#') { return std::nullopt; }

  const auto equals = trimmed.find('=');
  if (equals == std::string::npos) { return std::nullopt; }

  const auto directive
      = NormalizeDirectiveName(TrimAsciiWhitespace(trimmed.substr(0, equals)));
  if (directive != NormalizeDirectiveName(name)) { return std::nullopt; }

  return UnquoteBareosString(
      TrimAsciiWhitespace(trimmed.substr(equals + 1, std::string_view::npos)));
}

std::optional<bool> ParseBareosBoolDirectiveValue(std::string_view value)
{
  auto normalized = NormalizeDirectiveLookupName(UnquoteBareosString(value));
  if (normalized == "yes" || normalized == "true" || normalized == "on"
      || normalized == "1") {
    return true;
  }
  if (normalized == "no" || normalized == "false" || normalized == "off"
      || normalized == "0") {
    return false;
  }
  return std::nullopt;
}

int CountBraces(std::string_view value)
{
  int depth = 0;
  for (const char character : value) {
    if (character == '{') {
      ++depth;
    } else if (character == '}') {
      --depth;
    }
  }
  return depth;
}

OperationResult<std::vector<std::string>> ExtractTopLevelResourceEntries(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    const std::set<std::string>& controlled_directives)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::vector<std::string> entries;
  std::istringstream stream{*file.value};
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  bool capturing_entry = false;
  int entry_depth = 0;
  std::ostringstream entry;
  std::set<std::string> normalized_directives;
  for (const auto& directive : controlled_directives) {
    normalized_directives.insert(NormalizeDirectiveLookupName(directive));
  }

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind(resource_keyword, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth += brace_delta;
      }
      continue;
    }

    if (capturing_entry) {
      entry << line << "\n";
      entry_depth += brace_delta;
      resource_depth += brace_delta;
      if (entry_depth <= 0) {
        entries.push_back(entry.str());
        entry.str({});
        entry.clear();
        capturing_entry = false;
      }
      if (resource_depth <= 0) { break; }
      continue;
    }

    if (resource_depth == 1) {
      if (!trimmed.empty() && trimmed[0] != '#') {
        const auto equals = trimmed.find('=');
        const auto open_brace = trimmed.find('{');
        if (open_brace != std::string::npos
            && (equals == std::string::npos || open_brace < equals)) {
          const auto name = NormalizeDirectiveLookupName(
              TrimAsciiWhitespace(trimmed.substr(0, open_brace)));
          if (!normalized_directives.contains(name)) {
            entry << line << "\n";
            entry_depth = brace_delta;
            capturing_entry = entry_depth > 0;
            if (!capturing_entry) {
              entries.push_back(entry.str());
              entry.str({});
              entry.clear();
            }
          }
        } else if (equals != std::string::npos) {
          const auto name = NormalizeDirectiveLookupName(
              TrimAsciiWhitespace(trimmed.substr(0, equals)));
          if (!normalized_directives.contains(name)) {
            entries.push_back(line + "\n");
          }
        }
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) { break; }
  }

  if (capturing_entry) {
    return {.error = "unterminated preserved resource block in '"
                     + file_path.string() + "'."};
  }
  return {.value = std::move(entries)};
}

OperationResult<std::vector<std::string>> ExtractNamedTopLevelResourceEntries(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    std::string_view resource_name,
    const std::set<std::string>& controlled_directives)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::istringstream stream{*file.value};
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  bool capturing_entry = false;
  int entry_depth = 0;
  std::ostringstream entry;
  std::vector<std::string> entries;
  std::optional<std::string> current_name;
  std::set<std::string> normalized_directives;
  for (const auto& directive : controlled_directives) {
    normalized_directives.insert(NormalizeDirectiveLookupName(directive));
  }

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind(resource_keyword, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth = brace_delta;
        capturing_entry = false;
        entry_depth = 0;
        entry.str({});
        entry.clear();
        entries.clear();
        current_name.reset();
      }
      continue;
    }

    if (capturing_entry) {
      entry << line << "\n";
      entry_depth += brace_delta;
      resource_depth += brace_delta;
      if (entry_depth <= 0) {
        entries.push_back(entry.str());
        entry.str({});
        entry.clear();
        capturing_entry = false;
      }
      if (resource_depth <= 0) {
        if (current_name && *current_name == resource_name) {
          return {.value = std::move(entries)};
        }
        in_resource = false;
      }
      continue;
    }

    if (resource_depth == 1) {
      if (auto parsed_name = ParseTopLevelDirectiveValue(line, "Name")) {
        current_name = std::move(*parsed_name);
      }
      if (!trimmed.empty() && trimmed[0] != '#') {
        const auto equals = trimmed.find('=');
        const auto open_brace = trimmed.find('{');
        if (open_brace != std::string::npos
            && (equals == std::string::npos || open_brace < equals)) {
          const auto name = NormalizeDirectiveLookupName(
              TrimAsciiWhitespace(trimmed.substr(0, open_brace)));
          if (!normalized_directives.contains(name)) {
            entry << line << "\n";
            entry_depth = brace_delta;
            capturing_entry = entry_depth > 0;
            if (!capturing_entry) {
              entries.push_back(entry.str());
              entry.str({});
              entry.clear();
            }
          }
        } else if (equals != std::string::npos) {
          const auto name = NormalizeDirectiveLookupName(
              TrimAsciiWhitespace(trimmed.substr(0, equals)));
          if (!normalized_directives.contains(name)) {
            entries.push_back(line + "\n");
          }
        }
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) {
      if (current_name && *current_name == resource_name) {
        return {.value = std::move(entries)};
      }
      in_resource = false;
    }
  }

  if (capturing_entry) {
    return {.error = "unterminated preserved resource block in '"
                     + file_path.string() + "'."};
  }
  return {.error = "resource '" + std::string{resource_name}
                   + "' not found in '" + file_path.string() + "'."};
}

OperationResult<std::vector<std::string>> ExtractTopLevelDirectiveValues(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    const std::set<std::string>& directive_names)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::vector<std::string> values;
  std::istringstream stream{*file.value};
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  std::set<std::string> normalized_directives;
  for (const auto& directive : directive_names) {
    normalized_directives.insert(NormalizeDirectiveLookupName(directive));
  }

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind(resource_keyword, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth = brace_delta;
      }
      continue;
    }

    if (resource_depth == 1 && !trimmed.empty() && trimmed[0] != '#') {
      const auto equals = trimmed.find('=');
      const auto open_brace = trimmed.find('{');
      if (equals != std::string::npos
          && (open_brace == std::string::npos || equals < open_brace)) {
        const auto name = NormalizeDirectiveLookupName(
            TrimAsciiWhitespace(trimmed.substr(0, equals)));
        if (normalized_directives.contains(name)) {
          values.emplace_back(UnquoteBareosString(
              TrimAsciiWhitespace(trimmed.substr(equals + 1))));
        }
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) { return {.value = std::move(values)}; }
  }

  return {.value = std::move(values)};
}

OperationResult<std::vector<std::string>> ExtractNamedTopLevelDirectiveValues(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    std::string_view resource_name,
    const std::set<std::string>& directive_names)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::istringstream stream{*file.value};
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  std::optional<std::string> current_name;
  std::vector<std::string> values;
  std::set<std::string> normalized_directives;
  for (const auto& directive : directive_names) {
    normalized_directives.insert(NormalizeDirectiveLookupName(directive));
  }

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind(resource_keyword, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth = brace_delta;
        current_name.reset();
        values.clear();
      }
      continue;
    }

    if (resource_depth == 1) {
      if (auto parsed_name = ParseTopLevelDirectiveValue(line, "Name")) {
        current_name = std::move(*parsed_name);
      }
      if (!trimmed.empty() && trimmed[0] != '#') {
        const auto equals = trimmed.find('=');
        const auto open_brace = trimmed.find('{');
        if (equals != std::string::npos
            && (open_brace == std::string::npos || equals < open_brace)
            && current_name && *current_name == resource_name) {
          const auto name = NormalizeDirectiveLookupName(
              TrimAsciiWhitespace(trimmed.substr(0, equals)));
          if (normalized_directives.contains(name)) {
            values.emplace_back(UnquoteBareosString(
                TrimAsciiWhitespace(trimmed.substr(equals + 1))));
          }
        }
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) {
      if (current_name && *current_name == resource_name) {
        return {.value = std::move(values)};
      }
      in_resource = false;
    }
  }

  return {.error = "resource '" + std::string{resource_name}
                   + "' not found in '" + file_path.string() + "'."};
}

bool IsNamedTopLevelBlockEntry(std::string_view entry, std::string_view name)
{
  const auto trimmed = TrimAsciiWhitespace(entry);
  if (trimmed.empty() || trimmed[0] == '#') { return false; }

  const auto equals = trimmed.find('=');
  const auto open_brace = trimmed.find('{');
  if (open_brace == std::string::npos
      || (equals != std::string::npos && equals < open_brace)) {
    return false;
  }

  return NormalizeDirectiveLookupName(
             TrimAsciiWhitespace(trimmed.substr(0, open_brace)))
         == NormalizeDirectiveLookupName(name);
}

std::optional<std::string> PopulateMessagesDirectiveEntries(
    const std::filesystem::path& file_path,
    std::string_view resource_name,
    DirectorMessagesContentSpec& content)
{
  auto assign
      = [&](std::string_view directive_name,
            std::vector<std::string>& target) -> std::optional<std::string> {
    auto values = ExtractNamedTopLevelDirectiveValues(
        file_path, "Messages", resource_name, {std::string{directive_name}});
    if (!values) { return values.error; }
    target = std::move(*values.value);
    return std::nullopt;
  };

  if (auto result = assign("Syslog", content.syslog_entries); result) {
    return result;
  }
  if (auto result = assign("Mail", content.mail_entries); result) {
    return result;
  }
  if (auto result = assign("MailOnError", content.mail_on_error_entries);
      result) {
    return result;
  }
  if (auto result = assign("MailOnSuccess", content.mail_on_success_entries);
      result) {
    return result;
  }
  if (auto result = assign("File", content.file_entries); result) {
    return result;
  }
  if (auto result = assign("Append", content.append_entries); result) {
    return result;
  }
  if (auto result = assign("Stdout", content.stdout_entries); result) {
    return result;
  }
  if (auto result = assign("Stderr", content.stderr_entries); result) {
    return result;
  }
  if (auto result = assign("Director", content.director_entries); result) {
    return result;
  }
  if (auto result = assign("Console", content.console_entries); result) {
    return result;
  }
  if (auto result = assign("Operator", content.operator_entries); result) {
    return result;
  }
  if (auto result = assign("Catalog", content.catalog_entries); result) {
    return result;
  }
  return std::nullopt;
}

OperationResult<std::string> RewriteNamedTopLevelResource(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    std::string_view resource_name,
    const std::optional<std::string>& replacement)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::istringstream stream{*file.value};
  std::ostringstream rewritten;
  std::ostringstream block;
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  std::optional<std::string> current_name;
  bool replaced = false;

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind(resource_keyword, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth = brace_delta;
        current_name.reset();
        block.str({});
        block.clear();
        block << line << "\n";
        continue;
      }
      rewritten << line << "\n";
      continue;
    }

    block << line << "\n";
    if (resource_depth == 1) {
      if (auto parsed_name = ParseTopLevelDirectiveValue(line, "Name")) {
        current_name = std::move(*parsed_name);
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) {
      if (!replaced && current_name && *current_name == resource_name) {
        if (replacement) { rewritten << *replacement; }
        replaced = true;
      } else {
        rewritten << block.str();
      }
      in_resource = false;
    }
  }

  if (in_resource) {
    return {.error
            = "unterminated resource block in '" + file_path.string() + "'."};
  }
  if (!replaced) {
    return {.error = "resource '" + std::string{resource_name}
                     + "' not found in '" + file_path.string() + "'."};
  }
  return {.value = rewritten.str()};
}

OperationResult<std::string> RenderJobTypeForConfig(uint32_t job_type)
{
  switch (job_type) {
    case JT_BACKUP:
      return {.value = std::string{"Backup"}};
    case JT_ADMIN:
      return {.value = std::string{"Admin"}};
    case JT_ARCHIVE:
      return {.value = std::string{"Archive"}};
    case JT_VERIFY:
      return {.value = std::string{"Verify"}};
    case JT_RESTORE:
      return {.value = std::string{"Restore"}};
    case JT_MIGRATE:
      return {.value = std::string{"Migrate"}};
    case JT_COPY:
      return {.value = std::string{"Copy"}};
    case JT_CONSOLIDATE:
      return {.value = std::string{"Consolidate"}};
    case 0:
      return {.value = std::string{}};
    default:
      return {.error = "unsupported director job Type value "
                       + std::to_string(job_type) + "."};
  }
}

OperationResult<std::string> RenderJobLevelForConfig(uint32_t job_level,
                                                     uint32_t job_type)
{
  if (job_level == L_NONE || job_level == 0) {
    return {.value = std::string{}};
  }

  switch (job_level) {
    case L_FULL:
      return {.value = std::string{"Full"}};
    case L_BASE:
      return {.value = std::string{"Base"}};
    case L_INCREMENTAL:
      return {.value = std::string{"Incremental"}};
    case L_DIFFERENTIAL:
      return {.value = std::string{"Differential"}};
    case L_SINCE:
      return {.value = std::string{"Since"}};
    case L_VIRTUAL_FULL:
      return {.value = std::string{"VirtualFull"}};
    case L_VERIFY_CATALOG:
      if (job_type == JT_VERIFY) { return {.value = std::string{"Catalog"}}; }
      break;
    case L_VERIFY_INIT:
      if (job_type == JT_VERIFY) {
        return {.value = std::string{"InitCatalog"}};
      }
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      if (job_type == JT_VERIFY) {
        return {.value = std::string{"VolumeToCatalog"}};
      }
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      if (job_type == JT_VERIFY) {
        return {.value = std::string{"DiskToCatalog"}};
      }
      break;
    case L_VERIFY_DATA:
      if (job_type == JT_VERIFY) { return {.value = std::string{"Data"}}; }
      break;
  }

  return {.error = "unsupported director job Level value "
                   + std::to_string(job_level) + " for type "
                   + std::to_string(job_type) + "."};
}

OperationResult<std::vector<std::string>> ExtractScheduleRunEntries(
    const std::filesystem::path& file_path)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::vector<std::string> entries;
  std::istringstream stream{*file.value};
  std::string line;
  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    if (trimmed.rfind("Run", 0) != 0) { continue; }
    const auto equals = trimmed.find('=');
    if (equals == std::string::npos) { continue; }
    auto value = TrimAsciiWhitespace(trimmed.substr(equals + 1));
    if (!value.empty()) { entries.push_back(std::move(value)); }
  }
  return {.value = std::move(entries)};
}

OperationResult<std::vector<std::string>> ExtractNamedScheduleRunEntries(
    const std::filesystem::path& file_path,
    std::string_view schedule_name)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::vector<std::string> entries;
  std::istringstream stream{*file.value};
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  std::optional<std::string> current_name;

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind("Schedule", 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth = brace_delta;
        current_name.reset();
      }
      continue;
    }

    if (resource_depth == 1) {
      if (auto parsed_name = ParseTopLevelDirectiveValue(line, "Name")) {
        current_name = std::move(*parsed_name);
      }
      if (current_name && *current_name == schedule_name
          && trimmed.rfind("Run", 0) == 0) {
        const auto equals = trimmed.find('=');
        if (equals != std::string::npos) {
          auto value = TrimAsciiWhitespace(trimmed.substr(equals + 1));
          if (!value.empty()) { entries.push_back(std::move(value)); }
        }
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) {
      if (current_name && *current_name == schedule_name) {
        return {.value = std::move(entries)};
      }
      in_resource = false;
    }
  }

  if (in_resource) {
    return {.error
            = "unterminated Schedule block in '" + file_path.string() + "'."};
  }
  return {.error = "schedule '" + std::string{schedule_name}
                   + "' not found in '" + file_path.string() + "'."};
}

OperationResult<std::vector<std::string>> ExtractResourceBlocks(
    const std::filesystem::path& file_path,
    std::string_view block_name)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::vector<std::string> blocks;
  std::istringstream stream{*file.value};
  std::string line;
  bool in_block = false;
  int brace_depth = 0;
  std::ostringstream current;
  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    if (!in_block) {
      if (trimmed.rfind(block_name, 0) != 0
          || trimmed.find('{') == std::string::npos) {
        continue;
      }
      in_block = true;
      brace_depth = 0;
      current.str({});
      current.clear();
    }

    current << line << "\n";
    for (const char character : line) {
      if (character == '{') {
        ++brace_depth;
      } else if (character == '}') {
        --brace_depth;
      }
    }

    if (in_block && brace_depth <= 0) {
      blocks.push_back(current.str());
      in_block = false;
    }
  }

  if (in_block) {
    return {.error = "unterminated " + std::string{block_name} + " block in '"
                     + file_path.string() + "'."};
  }
  return {.value = std::move(blocks)};
}

OperationResult<std::vector<std::string>> ExtractNamedResourceBlocks(
    const std::filesystem::path& file_path,
    std::string_view resource_keyword,
    std::string_view resource_name,
    std::string_view block_name)
{
  auto file = ReadFile(file_path);
  if (!file) { return {.error = file.error}; }

  std::vector<std::string> blocks;
  std::istringstream stream{*file.value};
  std::string line;
  bool in_resource = false;
  int resource_depth = 0;
  std::optional<std::string> current_name;
  bool capturing_block = false;
  int block_depth = 0;
  std::ostringstream current_block;

  while (std::getline(stream, line)) {
    const auto trimmed = TrimAsciiWhitespace(line);
    const int brace_delta = CountBraces(line);

    if (!in_resource) {
      if (trimmed.rfind(resource_keyword, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_resource = true;
        resource_depth = brace_delta;
        current_name.reset();
      }
      continue;
    }

    if (capturing_block) {
      current_block << line << "\n";
      block_depth += brace_delta;
      resource_depth += brace_delta;
      if (block_depth <= 0) {
        blocks.push_back(current_block.str());
        current_block.str({});
        current_block.clear();
        capturing_block = false;
      }
      if (resource_depth <= 0) {
        if (current_name && *current_name == resource_name) {
          return {.value = std::move(blocks)};
        }
        in_resource = false;
      }
      continue;
    }

    if (resource_depth == 1) {
      if (auto parsed_name = ParseTopLevelDirectiveValue(line, "Name")) {
        current_name = std::move(*parsed_name);
      }
      if (current_name && *current_name == resource_name
          && trimmed.rfind(block_name, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        current_block << line << "\n";
        block_depth = brace_delta;
        capturing_block = block_depth > 0;
        if (!capturing_block) {
          blocks.push_back(current_block.str());
          current_block.str({});
          current_block.clear();
        }
      }
    }

    resource_depth += brace_delta;
    if (resource_depth <= 0) {
      if (current_name && *current_name == resource_name) {
        return {.value = std::move(blocks)};
      }
      in_resource = false;
    }
  }

  if (capturing_block) {
    return {.error = "unterminated " + std::string{block_name} + " block in '"
                     + file_path.string() + "'."};
  }
  if (in_resource) {
    return {.error = "unterminated " + std::string{resource_keyword}
                     + " block in '" + file_path.string() + "'."};
  }
  return {.error = "resource '" + std::string{resource_name}
                   + "' not found in '" + file_path.string() + "'."};
}

template <typename Resource>
std::optional<std::string> CopyResourceName(const Resource* resource)
{
  if (!resource || !resource->resource_name_
      || resource->resource_name_[0] == '\0') {
    return std::nullopt;
  }

  return std::string{resource->resource_name_};
}

OperationResult<std::string> RenderLabelTypeForConfig(int32_t label_type)
{
  switch (label_type) {
    case B_BAREOS_LABEL:
      return {.value = std::string{}};
    case B_ANSI_LABEL:
      return {.value = std::string{"ansi"}};
    case B_IBM_LABEL:
      return {.value = std::string{"ibm"}};
    default:
      return {.error = "unsupported LabelType value "
                       + std::to_string(label_type) + "."};
  }
}

OperationResult<std::string> RenderPoolActionOnPurgeForConfig(
    uint32_t action_on_purge)
{
  switch (action_on_purge) {
    case ON_PURGE_NONE:
      return {.value = std::string{}};
    case ON_PURGE_TRUNCATE:
      return {.value = std::string{"Truncate"}};
    default:
      return {.error = "unsupported director pool ActionOnPurge value "
                       + std::to_string(action_on_purge) + "."};
  }
}

OperationResult<DirectorConsoleWriteContext> LoadDirectorConsoleWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view console_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorConsoleWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "console"
                   / (std::string{console_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CONSOLE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CONSOLE, resource)) {
    auto* console = dynamic_cast<directordaemon::ConsoleResource*>(resource);
    if (!console) { continue; }
    if (auto source = console->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CONSOLE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CONSOLE, resource)) {
    auto* console = dynamic_cast<directordaemon::ConsoleResource*>(resource);
    if (!console || !console->resource_name_
        || console->resource_name_ != console_name) {
      continue;
    }

    context.exists = true;
    if (console->description_ && console->description_[0] != '\0') {
      context.description = std::string{console->description_};
    }
    context.use_pam_authentication = console->use_pam_authentication_;
    auto rendered_password = LoadSecretMemberSourceValue(
        *console, {"Password"}, console->password_,
        "director-side console password for '" + std::string{console_name}
            + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;
    if (HasMemberSource(*console, {"TlsAuthenticate"})) {
      context.tls_authenticate = console->authenticate_;
    }
    if (HasMemberSource(*console, {"TlsEnable"})) {
      context.tls_enable = console->tls_enable_;
    }
    if (HasMemberSource(*console, {"TlsRequire"})) {
      context.tls_require = console->tls_require_;
    }
    if (HasMemberSource(*console, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = console->tls_cert_.verify_peer_;
    }
    if (HasMemberSource(*console, {"TlsCipherList"})
        && !console->cipherlist_.empty()) {
      context.tls_cipher_list = console->cipherlist_;
    }
    if (HasMemberSource(*console, {"TlsCipherSuites"})
        && !console->ciphersuites_.empty()) {
      context.tls_cipher_suites = console->ciphersuites_;
    }
    if (HasMemberSource(*console, {"TlsDhFile"})
        && !console->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = console->tls_cert_.dhfile_;
    }
    if (HasMemberSource(*console, {"TlsProtocol"})
        && !console->protocol_.empty()) {
      context.tls_protocol = console->protocol_;
    }
    if (HasMemberSource(*console, {"TlsCaCertificateFile"})
        && !console->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file = console->tls_cert_.ca_certfile_;
    }
    if (HasMemberSource(*console, {"TlsCaCertificateDir"})
        && !console->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir = console->tls_cert_.ca_certdir_;
    }
    if (HasMemberSource(*console, {"TlsCertificateRevocationList"})
        && !console->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list = console->tls_cert_.crlfile_;
    }
    if (HasMemberSource(*console, {"TlsCertificate"})
        && !console->tls_cert_.certfile_.empty()) {
      context.tls_certificate = console->tls_cert_.certfile_;
    }
    if (HasMemberSource(*console, {"TlsKey"})
        && !console->tls_cert_.keyfile_.empty()) {
      context.tls_key = console->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*console, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = console->tls_cert_.allowed_certificate_common_names_;
    }

    for (size_t index = 0; index < context.acl.size(); ++index) {
      context.acl[index] = CopyAclValues(console->user_acl.ACL_lists[index]);
    }
    context.profiles = CopyProfileNames(console->user_acl.profiles);

    auto source = console->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director console '" + std::string{console_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<ConsoleConsoleWriteContext> LoadConsoleConsoleWriteContext(
    const DeploymentConfigRecord& console_config,
    std::string_view console_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kConsole,
                                    console_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("console config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("console config ", loaded.messages)};
  }

  ConsoleConsoleWriteContext context{
      .file_path
      = console_config.path
        / ComponentDefaultConfigFilename(bconfig::Component::kConsole)};
  for (auto* resource = loaded.parser->GetNextRes(console::R_DIRECTOR, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_DIRECTOR, resource)) {
    auto* configured_director
        = dynamic_cast<console::DirectorResource*>(resource);
    if (!configured_director) { continue; }
    auto rendered = RenderParsedConsoleDirectorResource(*configured_director,
                                                        *loaded.parser, false);
    if (!rendered) { return {.error = rendered.error}; }
    context.director_resources.emplace_back(std::move(*rendered.value));
  }

  bool seen_target = false;
  for (auto* resource = loaded.parser->GetNextRes(console::R_CONSOLE, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_CONSOLE, resource)) {
    auto* configured_console
        = dynamic_cast<console::ConsoleResource*>(resource);
    if (!configured_console) { continue; }
    if (!configured_console->resource_name_
        || configured_console->resource_name_ != console_name) {
      auto rendered
          = RenderParsedConsoleResource(*configured_console, *loaded.parser);
      if (!rendered) { return {.error = rendered.error}; }
      if (seen_target) {
        context.console_resources_after.emplace_back(
            std::move(*rendered.value));
      } else {
        context.console_resources_before.emplace_back(
            std::move(*rendered.value));
      }
      continue;
    }

    seen_target = true;
    context.exists = true;
    if (configured_console->description_
        && configured_console->description_[0] != '\0') {
      context.description = std::string{configured_console->description_};
    }
    auto rendered_password = LoadSecretMemberSourceValue(
        *configured_console, {"Password"}, configured_console->password_,
        "console password for '" + std::string{console_name} + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;
    if (HasMemberSource(*configured_console, {"Director"})
        && configured_console->director
        && configured_console->director->resource_name_) {
      context.director
          = std::string{configured_console->director->resource_name_};
    }
    if (HasMemberSource(*configured_console, {"RcFile"})
        && configured_console->rc_file
        && configured_console->rc_file[0] != '\0') {
      context.rc_file = std::string{configured_console->rc_file};
    }
    if (configured_console->history_file
        && configured_console->history_file[0] != '\0') {
      context.history_file = std::string{configured_console->history_file};
    }
    if (HasMemberSource(*configured_console, {"HistoryLength"})) {
      context.history_length = configured_console->history_length;
    }
    if (HasMemberSource(*configured_console, {"HeartbeatInterval"})) {
      context.heartbeat_interval
          = static_cast<uint64_t>(configured_console->heartbeat_interval);
    }
    if (HasMemberSource(*configured_console, {"TlsAuthenticate"})) {
      context.tls_authenticate = configured_console->authenticate_;
    }
    if (HasMemberSource(*configured_console, {"TlsEnable"})) {
      context.tls_enable = configured_console->tls_enable_;
    }
    if (HasMemberSource(*configured_console, {"TlsRequire"})) {
      context.tls_require = configured_console->tls_require_;
    }
    if (HasMemberSource(*configured_console, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = configured_console->tls_cert_.verify_peer_;
    }
    if (HasMemberSource(*configured_console, {"TlsCipherList"})
        && !configured_console->cipherlist_.empty()) {
      context.tls_cipher_list = configured_console->cipherlist_;
    }
    if (HasMemberSource(*configured_console, {"TlsCipherSuites"})
        && !configured_console->ciphersuites_.empty()) {
      context.tls_cipher_suites = configured_console->ciphersuites_;
    }
    if (HasMemberSource(*configured_console, {"TlsDhFile"})
        && !configured_console->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = configured_console->tls_cert_.dhfile_;
    }
    if (HasMemberSource(*configured_console, {"TlsProtocol"})
        && !configured_console->protocol_.empty()) {
      context.tls_protocol = configured_console->protocol_;
    }
    if (HasMemberSource(*configured_console, {"TlsCaCertificateFile"})
        && !configured_console->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file
          = configured_console->tls_cert_.ca_certfile_;
    }
    if (HasMemberSource(*configured_console, {"TlsCaCertificateDir"})
        && !configured_console->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir
          = configured_console->tls_cert_.ca_certdir_;
    }
    if (HasMemberSource(*configured_console, {"TlsCertificateRevocationList"})
        && !configured_console->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list
          = configured_console->tls_cert_.crlfile_;
    }
    if (HasMemberSource(*configured_console, {"TlsCertificate"})
        && !configured_console->tls_cert_.certfile_.empty()) {
      context.tls_certificate = configured_console->tls_cert_.certfile_;
    }
    if (HasMemberSource(*configured_console, {"TlsKey"})
        && !configured_console->tls_cert_.keyfile_.empty()) {
      context.tls_key = configured_console->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*configured_console, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = configured_console->tls_cert_.allowed_certificate_common_names_;
    }
  }

  return {.value = std::move(context)};
}

OperationResult<ConsoleDirectorWriteContext> LoadConsoleDirectorWriteContext(
    const DeploymentConfigRecord& console_config,
    std::string_view director_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kConsole,
                                    console_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("console config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("console config ", loaded.messages)};
  }

  ConsoleDirectorWriteContext context{
      .file_path
      = console_config.path
        / ComponentDefaultConfigFilename(bconfig::Component::kConsole)};
  for (auto* resource = loaded.parser->GetNextRes(console::R_CONSOLE, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_CONSOLE, resource)) {
    auto* configured_console
        = dynamic_cast<console::ConsoleResource*>(resource);
    if (!configured_console) { continue; }
    context.has_named_console = true;
    auto rendered
        = RenderParsedConsoleResource(*configured_console, *loaded.parser);
    if (!rendered) { return {.error = rendered.error}; }
    context.console_resources.emplace_back(std::move(*rendered.value));
  }

  bool seen_target = false;
  for (auto* resource = loaded.parser->GetNextRes(console::R_DIRECTOR, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_DIRECTOR, resource)) {
    auto* configured_director
        = dynamic_cast<console::DirectorResource*>(resource);
    if (!configured_director) { continue; }
    if (!configured_director->resource_name_
        || configured_director->resource_name_ != director_name) {
      auto rendered = RenderParsedConsoleDirectorResource(
          *configured_director, *loaded.parser, !context.has_named_console);
      if (!rendered) { return {.error = rendered.error}; }
      if (seen_target) {
        context.director_resources_after.emplace_back(
            std::move(*rendered.value));
      } else {
        context.director_resources_before.emplace_back(
            std::move(*rendered.value));
      }
      continue;
    }

    seen_target = true;
    context.exists = true;
    if (configured_director->address
        && configured_director->address[0] != '\0') {
      context.address = std::string{configured_director->address};
    }
    if (HasMemberSource(*configured_director, {"Port", "DirPort"})) {
      context.port = configured_director->DIRport;
    }
    if (HasMemberSource(*configured_director, {"Password"})) {
      auto rendered_password = LoadSecretMemberSourceValue(
          *configured_director, {"Password"}, configured_director->password_,
          "console director password for '" + std::string{director_name} + "'");
      if (!rendered_password) { return {.error = rendered_password.error}; }
      if (!context.has_named_console && !rendered_password.value->empty()) {
        context.password = *rendered_password.value;
      }
    }
    if (configured_director->description_
        && configured_director->description_[0] != '\0') {
      context.description = std::string{configured_director->description_};
    }
    if (HasMemberSource(*configured_director, {"HeartbeatInterval"})) {
      context.heartbeat_interval
          = static_cast<uint64_t>(configured_director->heartbeat_interval);
    }
    if (HasMemberSource(*configured_director, {"TlsAuthenticate"})) {
      context.tls_authenticate = configured_director->authenticate_;
    }
    if (HasMemberSource(*configured_director, {"TlsEnable"})) {
      context.tls_enable = configured_director->tls_enable_;
    }
    if (HasMemberSource(*configured_director, {"TlsRequire"})) {
      context.tls_require = configured_director->tls_require_;
    }
    if (HasMemberSource(*configured_director, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = configured_director->tls_cert_.verify_peer_;
    }
    if (HasMemberSource(*configured_director, {"TlsCipherList"})
        && !configured_director->cipherlist_.empty()) {
      context.tls_cipher_list = configured_director->cipherlist_;
    }
    if (HasMemberSource(*configured_director, {"TlsCipherSuites"})
        && !configured_director->ciphersuites_.empty()) {
      context.tls_cipher_suites = configured_director->ciphersuites_;
    }
    if (HasMemberSource(*configured_director, {"TlsDhFile"})
        && !configured_director->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = configured_director->tls_cert_.dhfile_;
    }
    if (HasMemberSource(*configured_director, {"TlsProtocol"})
        && !configured_director->protocol_.empty()) {
      context.tls_protocol = configured_director->protocol_;
    }
    if (HasMemberSource(*configured_director, {"TlsCaCertificateFile"})
        && !configured_director->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file
          = configured_director->tls_cert_.ca_certfile_;
    }
    if (HasMemberSource(*configured_director, {"TlsCaCertificateDir"})
        && !configured_director->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir
          = configured_director->tls_cert_.ca_certdir_;
    }
    if (HasMemberSource(*configured_director, {"TlsCertificateRevocationList"})
        && !configured_director->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list
          = configured_director->tls_cert_.crlfile_;
    }
    if (HasMemberSource(*configured_director, {"TlsCertificate"})
        && !configured_director->tls_cert_.certfile_.empty()) {
      context.tls_certificate = configured_director->tls_cert_.certfile_;
    }
    if (HasMemberSource(*configured_director, {"TlsKey"})
        && !configured_director->tls_cert_.keyfile_.empty()) {
      context.tls_key = configured_director->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*configured_director, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = configured_director->tls_cert_.allowed_certificate_common_names_;
    }
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorUserWriteContext> LoadDirectorUserWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view user_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorUserWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "user"
                   / (std::string{user_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_USER, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_USER, resource)) {
    auto* user = dynamic_cast<directordaemon::UserResource*>(resource);
    if (!user) { continue; }
    if (auto source = user->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_USER, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_USER, resource)) {
    auto* user = dynamic_cast<directordaemon::UserResource*>(resource);
    if (!user || !user->resource_name_ || user->resource_name_ != user_name) {
      continue;
    }

    context.exists = true;
    if (user->description_ && user->description_[0] != '\0') {
      context.description = std::string{user->description_};
    }
    for (size_t index = 0; index < context.acl.size(); ++index) {
      context.acl[index] = CopyAclValues(user->user_acl.ACL_lists[index]);
    }
    context.profiles = CopyProfileNames(user->user_acl.profiles);

    auto source = user->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director user '" + std::string{user_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorProfileWriteContext> LoadDirectorProfileWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view profile_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorProfileWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "profile"
                   / (std::string{profile_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_PROFILE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_PROFILE, resource)) {
    auto* profile = dynamic_cast<directordaemon::ProfileResource*>(resource);
    if (!profile) { continue; }
    if (auto source = profile->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_PROFILE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_PROFILE, resource)) {
    auto* profile = dynamic_cast<directordaemon::ProfileResource*>(resource);
    if (!profile || !profile->resource_name_
        || profile->resource_name_ != profile_name) {
      continue;
    }

    context.exists = true;
    if (profile->description_ && profile->description_[0] != '\0') {
      context.description = std::string{profile->description_};
    }
    for (size_t index = 0; index < context.acl.size(); ++index) {
      context.acl[index] = CopyAclValues(profile->ACL_lists[index]);
    }

    auto source = profile->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director profile '" + std::string{profile_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorPoolWriteContext> LoadDirectorPoolWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view pool_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorPoolWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "pool"
                   / (std::string{pool_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_POOL, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_POOL, resource)) {
    auto* pool = dynamic_cast<directordaemon::PoolResource*>(resource);
    if (!pool) { continue; }
    if (auto source = pool->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_POOL, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_POOL, resource)) {
    auto* pool = dynamic_cast<directordaemon::PoolResource*>(resource);
    if (!pool || !pool->resource_name_ || pool->resource_name_ != pool_name) {
      continue;
    }

    context.exists = true;
    if (pool->description_ && pool->description_[0] != '\0') {
      context.content.description = std::string{pool->description_};
    }
    if (pool->pool_type && pool->pool_type[0] != '\0') {
      context.content.pool_type = std::string{pool->pool_type};
    }
    if (pool->label_format && pool->label_format[0] != '\0') {
      context.content.label_format = std::string{pool->label_format};
    }
    if (pool->cleaning_prefix && pool->cleaning_prefix[0] != '\0') {
      context.content.cleaning_prefix = std::string{pool->cleaning_prefix};
    }
    if (pool->max_volumes != 0) {
      context.content.maximum_volumes = pool->max_volumes;
    }
    if (pool->MaxVolJobs != 0) {
      context.content.maximum_volume_jobs = pool->MaxVolJobs;
    }
    if (pool->MaxVolFiles != 0) {
      context.content.maximum_volume_files = pool->MaxVolFiles;
    }
    if (pool->MaxVolBytes != 0) {
      context.content.maximum_volume_bytes = pool->MaxVolBytes;
    }
    if (pool->VolRetention != 0) {
      context.content.volume_retention = pool->VolRetention;
    }
    if (pool->VolUseDuration != 0) {
      context.content.volume_use_duration = pool->VolUseDuration;
    }
    if (pool->MigrationTime != 0) {
      context.content.migration_time = pool->MigrationTime;
    }
    if (pool->MigrationHighBytes != 0) {
      context.content.migration_high_bytes = pool->MigrationHighBytes;
    }
    if (pool->MigrationLowBytes != 0) {
      context.content.migration_low_bytes = pool->MigrationLowBytes;
    }
    context.content.use_catalog = pool->use_catalog;
    context.content.catalog_files = pool->catalog_files;
    context.content.purge_oldest_volume = pool->purge_oldest_volume;
    context.content.recycle_oldest_volume = pool->recycle_oldest_volume;
    context.content.recycle_current_volume = pool->recycle_current_volume;
    context.content.auto_prune = pool->AutoPrune;
    context.content.recycle = pool->Recycle;
    if (pool->FileRetention != 0) {
      context.content.file_retention = pool->FileRetention;
    }
    if (pool->JobRetention != 0) {
      context.content.job_retention = pool->JobRetention;
    }
    if (pool->MinBlocksize != 0) {
      context.content.minimum_block_size = pool->MinBlocksize;
    }
    if (pool->MaxBlocksize != 0) {
      context.content.maximum_block_size = pool->MaxBlocksize;
    }
    context.content.next_pool = CopyResourceName(pool->NextPool);
    context.content.storages = CopyStorageNames(pool->storage);
    context.content.recycle_pool = CopyResourceName(pool->RecyclePool);
    context.content.scratch_pool = CopyResourceName(pool->ScratchPool);
    context.content.catalog = CopyResourceName(pool->catalog);

    auto label_type = RenderLabelTypeForConfig(pool->LabelType);
    if (!label_type) { return {.error = label_type.error}; }
    if (!label_type.value->empty()) {
      context.content.label_type = std::move(*label_type.value);
    }

    auto action_on_purge
        = RenderPoolActionOnPurgeForConfig(pool->action_on_purge);
    if (!action_on_purge) { return {.error = action_on_purge.error}; }
    if (!action_on_purge.value->empty()) {
      context.content.action_on_purge = std::move(*action_on_purge.value);
    }

    auto source = pool->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director pool '" + std::string{pool_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorCatalogWriteContext> LoadDirectorCatalogWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view catalog_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorCatalogWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "catalog"
                   / (std::string{catalog_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CATALOG, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CATALOG, resource)) {
    auto* catalog = dynamic_cast<directordaemon::CatalogResource*>(resource);
    if (!catalog) { continue; }
    if (auto source = catalog->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CATALOG, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CATALOG, resource)) {
    auto* catalog = dynamic_cast<directordaemon::CatalogResource*>(resource);
    if (!catalog || !catalog->resource_name_
        || catalog->resource_name_ != catalog_name) {
      continue;
    }

    context.exists = true;
    if (catalog->description_ && catalog->description_[0] != '\0') {
      context.content.description = std::string{catalog->description_};
    }
    if (catalog->db_address && catalog->db_address[0] != '\0') {
      context.content.db_address = std::string{catalog->db_address};
    }
    if (catalog->db_port != 0) { context.content.db_port = catalog->db_port; }
    if (catalog->db_socket && catalog->db_socket[0] != '\0') {
      context.content.db_socket = std::string{catalog->db_socket};
    }
    if (catalog->db_user && catalog->db_user[0] != '\0') {
      context.content.db_user = std::string{catalog->db_user};
    }
    if (catalog->db_name && catalog->db_name[0] != '\0') {
      context.content.db_name = std::string{catalog->db_name};
    }
    if (HasMemberSource(*catalog, {"MultipleConnections"})) {
      context.content.multiple_connections = catalog->mult_db_connections != 0;
    }
    if (HasMemberSource(*catalog, {"DisableBatchInsert"})) {
      context.content.disable_batch_insert = catalog->disable_batch_insert;
    }
    context.content.reconnect = catalog->try_reconnect;
    context.content.exit_on_fatal = catalog->exit_on_fatal;
    if (catalog->pooling_min_connections != 0) {
      context.content.min_connections = catalog->pooling_min_connections;
    }
    if (catalog->pooling_max_connections != 0) {
      context.content.max_connections = catalog->pooling_max_connections;
    }
    if (catalog->pooling_increment_connections != 0) {
      context.content.inc_connections = catalog->pooling_increment_connections;
    }
    if (catalog->pooling_idle_timeout != 0) {
      context.content.idle_timeout = catalog->pooling_idle_timeout;
    }
    if (catalog->pooling_validate_timeout != 0) {
      context.content.validate_timeout = catalog->pooling_validate_timeout;
    }

    auto rendered_password = LoadSecretMemberSourceValue(
        *catalog, {"DbPassword"}, catalog->db_password,
        "director-side catalog password for '" + std::string{catalog_name}
            + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.content.db_password = *rendered_password.value;

    auto source = catalog->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director catalog '" + std::string{catalog_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorMessagesWriteContext> LoadDirectorMessagesWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view messages_name)
{
  DirectorMessagesWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "messages"
                   / (std::string{messages_name} + ".conf"),
      .is_standalone_file = true};
  if (!HasManagedConfigFiles(director_config)) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_MSGS, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_MSGS, resource)) {
    auto* messages = dynamic_cast<MessagesResource*>(resource);
    if (!messages) { continue; }
    if (auto source = messages->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_MSGS, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_MSGS, resource)) {
    auto* messages = dynamic_cast<MessagesResource*>(resource);
    if (!messages || !messages->resource_name_
        || messages->resource_name_ != messages_name) {
      continue;
    }

    context.exists = true;
    if (messages->description_ && messages->description_[0] != '\0') {
      context.content.description = std::string{messages->description_};
    }
    if (!messages->mail_cmd_.empty()) {
      context.content.mail_command = messages->mail_cmd_;
    }
    if (!messages->operator_cmd_.empty()) {
      context.content.operator_command = messages->operator_cmd_;
    }
    if (!messages->timestamp_format_.empty()) {
      context.content.timestamp_format = messages->timestamp_format_;
    }

    auto source = messages->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director messages '" + std::string{messages_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    if (auto directive_entries = PopulateMessagesDirectiveEntries(
            context.file_path, messages_name, context.content);
        directive_entries) {
      return {.error = *directive_entries};
    }
    static const std::set<std::string> kControlledMessagesDirectives{
        "Name",
        "Description",
        "MailCommand",
        "OperatorCommand",
        "TimestampFormat",
        "Syslog",
        "Mail",
        "MailOnError",
        "MailOnSuccess",
        "File",
        "Append",
        "Stdout",
        "Stderr",
        "Director",
        "Console",
        "Operator",
        "Catalog"};
    auto entries = ExtractNamedTopLevelResourceEntries(
        context.file_path, "Messages", messages_name,
        kControlledMessagesDirectives);
    if (!entries) { return {.error = entries.error}; }
    context.content.entries = std::move(*entries.value);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<ClientMessagesWriteContext> LoadClientMessagesWriteContext(
    const DeploymentConfigRecord& client_config,
    std::string_view messages_name)
{
  ClientMessagesWriteContext context{
      .file_path = client_config.path / "bareos-fd.d" / "messages"
                   / (std::string{messages_name} + ".conf"),
      .is_standalone_file = true};
  if (!HasManagedConfigFiles(client_config)) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("client config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("client config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource = loaded.parser->GetNextRes(filedaemon::R_MSGS, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(filedaemon::R_MSGS, resource)) {
    auto* messages = dynamic_cast<MessagesResource*>(resource);
    if (!messages) { continue; }
    if (auto source = messages->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource = loaded.parser->GetNextRes(filedaemon::R_MSGS, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(filedaemon::R_MSGS, resource)) {
    auto* messages = dynamic_cast<MessagesResource*>(resource);
    if (!messages || !messages->resource_name_
        || messages->resource_name_ != messages_name) {
      continue;
    }

    context.exists = true;
    if (messages->description_ && messages->description_[0] != '\0') {
      context.content.description = std::string{messages->description_};
    }
    if (!messages->mail_cmd_.empty()) {
      context.content.mail_command = messages->mail_cmd_;
    }
    if (!messages->operator_cmd_.empty()) {
      context.content.operator_command = messages->operator_cmd_;
    }
    if (!messages->timestamp_format_.empty()) {
      context.content.timestamp_format = messages->timestamp_format_;
    }

    auto source = messages->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "client messages '" + std::string{messages_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    if (auto directive_entries = PopulateMessagesDirectiveEntries(
            context.file_path, messages_name, context.content);
        directive_entries) {
      return {.error = *directive_entries};
    }
    static const std::set<std::string> kControlledMessagesDirectives{
        "Name",
        "Description",
        "MailCommand",
        "OperatorCommand",
        "TimestampFormat",
        "Syslog",
        "Mail",
        "MailOnError",
        "MailOnSuccess",
        "File",
        "Append",
        "Stdout",
        "Stderr",
        "Director",
        "Console",
        "Operator",
        "Catalog"};
    auto entries = ExtractNamedTopLevelResourceEntries(
        context.file_path, "Messages", messages_name,
        kControlledMessagesDirectives);
    if (!entries) { return {.error = entries.error}; }
    context.content.entries = std::move(*entries.value);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageMessagesWriteContext> LoadStorageMessagesWriteContext(
    const DeploymentConfigRecord& storage_config,
    std::string_view messages_name)
{
  StorageMessagesWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "messages"
                   / (std::string{messages_name} + ".conf"),
      .is_standalone_file = true};
  if (!HasManagedConfigFiles(storage_config)) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_MSGS, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(storagedaemon::R_MSGS, resource)) {
    auto* messages = dynamic_cast<MessagesResource*>(resource);
    if (!messages) { continue; }
    if (auto source = messages->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_MSGS, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(storagedaemon::R_MSGS, resource)) {
    auto* messages = dynamic_cast<MessagesResource*>(resource);
    if (!messages || !messages->resource_name_
        || messages->resource_name_ != messages_name) {
      continue;
    }

    context.exists = true;
    if (messages->description_ && messages->description_[0] != '\0') {
      context.content.description = std::string{messages->description_};
    }
    if (!messages->mail_cmd_.empty()) {
      context.content.mail_command = messages->mail_cmd_;
    }
    if (!messages->operator_cmd_.empty()) {
      context.content.operator_command = messages->operator_cmd_;
    }
    if (!messages->timestamp_format_.empty()) {
      context.content.timestamp_format = messages->timestamp_format_;
    }

    auto source = messages->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon messages '" + std::string{messages_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    if (auto directive_entries = PopulateMessagesDirectiveEntries(
            context.file_path, messages_name, context.content);
        directive_entries) {
      return {.error = *directive_entries};
    }
    static const std::set<std::string> kControlledMessagesDirectives{
        "Name",
        "Description",
        "MailCommand",
        "OperatorCommand",
        "TimestampFormat",
        "Syslog",
        "Mail",
        "MailOnError",
        "MailOnSuccess",
        "File",
        "Append",
        "Stdout",
        "Stderr",
        "Director",
        "Console",
        "Operator",
        "Catalog"};
    auto entries = ExtractNamedTopLevelResourceEntries(
        context.file_path, "Messages", messages_name,
        kControlledMessagesDirectives);
    if (!entries) { return {.error = entries.error}; }
    context.content.entries = std::move(*entries.value);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<ClientDaemonWriteContext> LoadClientDaemonWriteContext(
    const DeploymentConfigRecord& client_config)
{
  ClientDaemonWriteContext context{
      .file_path = client_config.path / "bareos-fd.d" / "client"
                   / (client_config.name + ".conf"),
      .is_standalone_file = true};
  if (!HasManagedConfigFiles(client_config)) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("client config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("client config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(filedaemon::R_CLIENT, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(filedaemon::R_CLIENT, resource)) {
    auto* client = dynamic_cast<filedaemon::ClientResource*>(resource);
    if (!client) { continue; }
    if (auto source = client->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(filedaemon::R_CLIENT, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(filedaemon::R_CLIENT, resource)) {
    auto* client = dynamic_cast<filedaemon::ClientResource*>(resource);
    if (!client || !client->resource_name_
        || client->resource_name_ != client_config.name) {
      continue;
    }

    context.exists = true;
    const bool has_addresses_source
        = HasMemberSource(*client, {"Addresses", "FdAddresses"});
    const bool has_address_source
        = HasMemberSource(*client, {"Address", "FdAddress"});
    const bool has_source_address_source
        = HasMemberSource(*client, {"SourceAddress", "FdSourceAddress"});
    const bool has_port_source = HasMemberSource(*client, {"Port", "FdPort"});
    if (has_addresses_source) {
      context.content.addresses = CopyAddressEntries(client->FDaddrs);
    } else if (has_address_source) {
      context.content.address = CopyFirstAddress(client->FDaddrs);
    }
    if (!has_addresses_source && has_port_source) {
      const auto port = GetFirstPortHostOrder(client->FDaddrs);
      if (port > 0) { context.content.port = static_cast<uint16_t>(port); }
    }
    if (has_source_address_source) {
      if (client->FDsrc_addr && client->FDsrc_addr->size() > 1) {
        context.content.source_addresses
            = CopyAddressValues(client->FDsrc_addr);
      } else {
        context.content.source_address = CopyFirstAddress(client->FDsrc_addr);
      }
    }
    if (HasMemberSource(*client, {"MaximumConcurrentJobs"})) {
      context.content.maximum_concurrent_jobs = client->MaxConcurrentJobs;
    }
    if (HasMemberSource(*client, {"MaximumWorkersPerJob"})) {
      context.content.maximum_workers_per_job = client->MaxWorkersPerJob;
    }
    if (HasMemberSource(*client, {"AbsoluteJobTimeout"})) {
      context.content.absolute_job_timeout = client->jcr_watchdog_time;
    }
    if (HasMemberSource(*client, {"AllowBandwidthBursting"})) {
      context.content.allow_bandwidth_bursting = client->allow_bw_bursting;
    }
    if (HasMemberSource(*client, {"TlsAuthenticate"})) {
      context.content.tls_authenticate = client->authenticate_;
    }
    if (HasMemberSource(*client, {"TlsEnable"})) {
      context.content.tls_enable = client->tls_enable_;
    }
    if (HasMemberSource(*client, {"TlsRequire"})) {
      context.content.tls_require = client->tls_require_;
    }
    if (HasMemberSource(*client, {"TlsVerifyPeer"})) {
      context.content.tls_verify_peer = client->tls_cert_.verify_peer_;
    }
    if (!client->cipherlist_.empty()) {
      context.content.tls_cipher_list = client->cipherlist_;
    }
    if (!client->ciphersuites_.empty()) {
      context.content.tls_cipher_suites = client->ciphersuites_;
    }
    if (!client->tls_cert_.dhfile_.empty()) {
      context.content.tls_dh_file = client->tls_cert_.dhfile_;
    }
    if (!client->protocol_.empty()) {
      context.content.tls_protocol = client->protocol_;
    }
    if (!client->tls_cert_.ca_certfile_.empty()) {
      context.content.tls_ca_certificate_file = client->tls_cert_.ca_certfile_;
    }
    if (!client->tls_cert_.ca_certdir_.empty()) {
      context.content.tls_ca_certificate_dir = client->tls_cert_.ca_certdir_;
    }
    if (!client->tls_cert_.crlfile_.empty()) {
      context.content.tls_certificate_revocation_list
          = client->tls_cert_.crlfile_;
    }
    if (!client->tls_cert_.certfile_.empty()) {
      context.content.tls_certificate = client->tls_cert_.certfile_;
    }
    if (!client->tls_cert_.keyfile_.empty()) {
      context.content.tls_key = client->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*client, {"TlsAllowedCn"})) {
      context.content.tls_allowed_cn
          = client->tls_cert_.allowed_certificate_common_names_;
    }
    if (HasMemberSource(*client, {"PkiSignatures"})) {
      context.content.pki_signatures = client->pki_sign;
    }
    if (HasMemberSource(*client, {"PkiEncryption"})) {
      context.content.pki_encryption = client->pki_encrypt;
    }
    if (client->pki_keypair_file && client->pki_keypair_file[0] != '\0') {
      context.content.pki_key_pair = std::string{client->pki_keypair_file};
    }
    if (HasMemberSource(*client, {"PkiSigner"})) {
      context.content.pki_signers
          = CopyAclValues(client->pki_signing_key_files);
    }
    if (HasMemberSource(*client, {"PkiMasterKey"})) {
      context.content.pki_master_keys
          = CopyAclValues(client->pki_master_key_files);
    }
    if (HasMemberSource(*client, {"PkiCipher"})) {
      if (client->pki_cipher_config_name && client->pki_cipher_config_name[0]) {
        context.content.pki_cipher
            = std::string{client->pki_cipher_config_name};
      }
    }
    if (HasMemberSource(*client, {"AlwaysUseLmdb"})) {
      context.content.always_use_lmdb = client->always_use_lmdb;
    }
    if (HasMemberSource(*client, {"LmdbThreshold"})) {
      context.content.lmdb_threshold = client->lmdb_threshold;
    }
    if (client->verid && client->verid[0] != '\0') {
      context.content.ver_id = std::string{client->verid};
    }
    if (client->log_timestamp_format
        && client->log_timestamp_format[0] != '\0') {
      context.content.log_timestamp_format
          = std::string{client->log_timestamp_format};
    }
    if (HasMemberSource(*client, {"MaximumBandwidthPerJob"})) {
      context.content.maximum_bandwidth_per_job = client->max_bandwidth_per_job;
    }
    if (client->secure_erase_cmdline
        && client->secure_erase_cmdline[0] != '\0') {
      context.content.secure_erase_command
          = std::string{client->secure_erase_cmdline};
    }
    if (!client->grpc_module.empty()) {
      context.content.grpc_module = client->grpc_module;
    }
    if (HasMemberSource(*client, {"EnableKtls"})) {
      context.content.enable_ktls = client->enable_ktls;
    }
    if (HasMemberSource(*client, {"HeartbeatInterval"})) {
      context.content.heartbeat_interval
          = static_cast<uint64_t>(client->heartbeat_interval);
    }
    if (HasMemberSource(*client, {"SdConnectTimeout"})) {
      context.content.sd_connect_timeout
          = static_cast<uint64_t>(client->SDConnectTimeout);
    }
    if (HasMemberSource(*client, {"MaximumNetworkBufferSize"})) {
      context.content.maximum_network_buffer_size
          = client->max_network_buffer_size;
    }
    if (client->description_ && client->description_[0] != '\0') {
      context.content.description = std::string{client->description_};
    }
    if (client->working_directory && client->working_directory[0] != '\0') {
      context.content.working_directory
          = std::string{client->working_directory};
    }
    if (client->plugin_directory && client->plugin_directory[0] != '\0') {
      context.content.plugin_directory = std::string{client->plugin_directory};
    }
    if (HasMemberSource(*client, {"PluginNames"})) {
      context.content.plugin_names = CopyAclValues(client->plugin_names);
    }
    if (HasMemberSource(*client, {"AllowedScriptDir"})) {
      context.content.allowed_script_dirs
          = CopyAclValues(client->allowed_script_dirs);
    }
    if (HasMemberSource(*client, {"AllowedJobCommand"})) {
      context.content.allowed_job_commands
          = CopyAclValues(client->allowed_job_cmds);
    }
    if (client->scripts_directory && client->scripts_directory[0] != '\0') {
      context.content.scripts_directory
          = std::string{client->scripts_directory};
    }
    context.content.messages = CopyResourceName(client->messages);

    auto source = client->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "client daemon resource '" + client_config.name
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    if (has_source_address_source) {
      static const std::set<std::string> kSourceAddressDirectives{
          "SourceAddress", "FdSourceAddress"};
      auto raw_source_addresses
          = context.is_standalone_file
                ? ExtractTopLevelDirectiveValues(context.file_path, "Client",
                                                 kSourceAddressDirectives)
                : ExtractNamedTopLevelDirectiveValues(
                      context.file_path, "Client", client_config.name,
                      kSourceAddressDirectives);
      if (!raw_source_addresses) {
        return {.error = raw_source_addresses.error};
      }
      if (!raw_source_addresses.value->empty()) {
        context.content.source_address.reset();
        context.content.source_addresses.clear();
        if (raw_source_addresses.value->size() > 1) {
          context.content.source_addresses
              = std::move(*raw_source_addresses.value);
        } else {
          context.content.source_address
              = std::move(raw_source_addresses.value->front());
        }
      }
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorDaemonWriteContext> LoadDirectorDaemonWriteContext(
    const DeploymentConfigRecord& director_config)
{
  DirectorDaemonWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "director"
                   / (director_config.name + ".conf"),
      .is_standalone_file = true};
  if (!HasManagedConfigFiles(director_config)) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_DIRECTOR, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_DIRECTOR, resource)) {
    auto* director = dynamic_cast<directordaemon::DirectorResource*>(resource);
    if (!director) { continue; }
    if (auto source = director->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_DIRECTOR, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_DIRECTOR, resource)) {
    auto* director = dynamic_cast<directordaemon::DirectorResource*>(resource);
    if (!director || !director->resource_name_
        || director->resource_name_ != director_config.name) {
      continue;
    }

    context.exists = true;
    const bool has_addresses_source = HasMemberSource(*director, {"Addresses"});
    const bool has_address_source = HasMemberSource(*director, {"Address"});
    const bool has_source_address_source
        = HasMemberSource(*director, {"SourceAddress"});
    const bool has_port_source = HasMemberSource(*director, {"Port"});
    if (has_addresses_source) {
      context.content.addresses = CopyAddressEntries(director->DIRaddrs);
    } else if (has_address_source) {
      context.content.address = CopyFirstAddress(director->DIRaddrs);
    }
    if (!has_addresses_source && has_port_source) {
      const auto port = GetFirstPortHostOrder(director->DIRaddrs);
      if (port > 0) { context.content.port = static_cast<uint16_t>(port); }
    }
    if (has_source_address_source) {
      if (director->DIRsrc_addr && director->DIRsrc_addr->size() > 1) {
        context.content.source_addresses
            = CopyAddressValues(director->DIRsrc_addr);
      } else {
        context.content.source_address
            = CopyFirstAddress(director->DIRsrc_addr);
      }
    }
    if (director->password_.value && director->password_.value[0] != '\0') {
      context.password = std::string{director->password_.value};
    }
    if (director->query_file && director->query_file[0] != '\0') {
      context.content.query_file = std::string{director->query_file};
    }
    if (HasMemberSource(*director, {"Subscriptions"})) {
      context.content.subscriptions = director->subscriptions;
    }
    if (HasMemberSource(*director, {"MaximumConcurrentJobs"})) {
      context.content.maximum_concurrent_jobs = director->MaxConcurrentJobs;
    }
    if (HasMemberSource(*director, {"MaximumConsoleConnections"})) {
      context.content.maximum_console_connections
          = director->MaxConsoleConnections;
    }
    if (HasMemberSource(*director, {"AbsoluteJobTimeout"})) {
      context.content.absolute_job_timeout = director->jcr_watchdog_time;
    }
    if (HasMemberSource(*director, {"TlsAuthenticate"})) {
      context.content.tls_authenticate = director->authenticate_;
    }
    if (HasMemberSource(*director, {"TlsEnable"})) {
      context.content.tls_enable = director->tls_enable_;
    }
    if (HasMemberSource(*director, {"TlsRequire"})) {
      context.content.tls_require = director->tls_require_;
    }
    if (HasMemberSource(*director, {"TlsVerifyPeer"})) {
      context.content.tls_verify_peer = director->tls_cert_.verify_peer_;
    }
    if (!director->cipherlist_.empty()) {
      context.content.tls_cipher_list = director->cipherlist_;
    }
    if (!director->ciphersuites_.empty()) {
      context.content.tls_cipher_suites = director->ciphersuites_;
    }
    if (!director->tls_cert_.dhfile_.empty()) {
      context.content.tls_dh_file = director->tls_cert_.dhfile_;
    }
    if (!director->protocol_.empty()) {
      context.content.tls_protocol = director->protocol_;
    }
    if (!director->tls_cert_.ca_certfile_.empty()) {
      context.content.tls_ca_certificate_file
          = director->tls_cert_.ca_certfile_;
    }
    if (!director->tls_cert_.ca_certdir_.empty()) {
      context.content.tls_ca_certificate_dir = director->tls_cert_.ca_certdir_;
    }
    if (!director->tls_cert_.crlfile_.empty()) {
      context.content.tls_certificate_revocation_list
          = director->tls_cert_.crlfile_;
    }
    if (!director->tls_cert_.certfile_.empty()) {
      context.content.tls_certificate = director->tls_cert_.certfile_;
    }
    if (!director->tls_cert_.keyfile_.empty()) {
      context.content.tls_key = director->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*director, {"TlsAllowedCn"})) {
      context.content.tls_allowed_cn
          = director->tls_cert_.allowed_certificate_common_names_;
    }
    if (director->verid && director->verid[0] != '\0') {
      context.content.ver_id = std::string{director->verid};
    }
    if (director->log_timestamp_format
        && director->log_timestamp_format[0] != '\0') {
      context.content.log_timestamp_format
          = std::string{director->log_timestamp_format};
    }
    if (director->secure_erase_cmdline
        && director->secure_erase_cmdline[0] != '\0') {
      context.content.secure_erase_command
          = std::string{director->secure_erase_cmdline};
    }
    if (HasMemberSource(*director, {"EnableKtls"})) {
      context.content.enable_ktls = director->enable_ktls;
    }
    if (HasMemberSource(*director, {"FdConnectTimeout"})) {
      context.content.fd_connect_timeout
          = static_cast<uint64_t>(director->FDConnectTimeout);
    }
    if (HasMemberSource(*director, {"SdConnectTimeout"})) {
      context.content.sd_connect_timeout
          = static_cast<uint64_t>(director->SDConnectTimeout);
    }
    if (HasMemberSource(*director, {"HeartbeatInterval"})) {
      context.content.heartbeat_interval
          = static_cast<uint64_t>(director->heartbeat_interval);
    }
    if (HasMemberSource(*director, {"StatisticsRetention"})) {
      context.content.statistics_retention
          = static_cast<uint64_t>(director->stats_retention);
    }
    if (HasMemberSource(*director, {"StatisticsCollectInterval"})) {
      context.content.statistics_collect_interval
          = director->stats_collect_interval;
    }
    if (director->description_ && director->description_[0] != '\0') {
      context.content.description = std::string{director->description_};
    }
    if (HasMemberSource(*director, {"KeyEncryptionKey"})) {
      auto rendered_key = LoadSecretMemberSourceValue(
          *director, {"KeyEncryptionKey"}, director->keyencrkey,
          "director daemon key encryption key for '" + director_config.name
              + "'");
      if (!rendered_key) { return {.error = rendered_key.error}; }
      if (!rendered_key.value->empty()) {
        context.content.key_encryption_key = *rendered_key.value;
      }
    }
    if (HasMemberSource(*director, {"NdmpSnooping"})) {
      context.content.ndmp_snooping = director->ndmp_snooping;
    }
    if (HasMemberSource(*director, {"NdmpLogLevel"})) {
      context.content.ndmp_log_level = director->ndmp_loglevel;
    }
    if (HasMemberSource(*director,
                        {"NdmpNamelistFhinfoSetZeroForInvalidUquad"})) {
      context.content.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
          = director->ndmp_fhinfo_set_zero_for_invalid_u_quad;
    }
    if (HasMemberSource(*director, {"Auditing"})) {
      context.content.auditing = director->auditing;
    }
    if (HasMemberSource(*director, {"AuditEvents"})) {
      context.content.audit_events = CopyAclValues(director->audit_events);
    }
    if (director->working_directory && director->working_directory[0] != '\0') {
      context.content.working_directory
          = std::string{director->working_directory};
    }
    if (director->plugin_directory && director->plugin_directory[0] != '\0') {
      context.content.plugin_directory
          = std::string{director->plugin_directory};
    }
    if (HasMemberSource(*director, {"PluginNames"})) {
      context.content.plugin_names = CopyAclValues(director->plugin_names);
    }
    if (director->scripts_directory && director->scripts_directory[0] != '\0') {
      context.content.scripts_directory
          = std::string{director->scripts_directory};
    }
    context.content.messages = CopyResourceName(director->messages);

    auto source = director->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director daemon resource '" + director_config.name
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    if (has_source_address_source) {
      static const std::set<std::string> kSourceAddressDirectives{
          "SourceAddress"};
      auto raw_source_addresses
          = context.is_standalone_file
                ? ExtractTopLevelDirectiveValues(context.file_path, "Director",
                                                 kSourceAddressDirectives)
                : ExtractNamedTopLevelDirectiveValues(
                      context.file_path, "Director", director_config.name,
                      kSourceAddressDirectives);
      if (!raw_source_addresses) {
        return {.error = raw_source_addresses.error};
      }
      if (!raw_source_addresses.value->empty()) {
        context.content.source_address.reset();
        context.content.source_addresses.clear();
        if (raw_source_addresses.value->size() > 1) {
          context.content.source_addresses
              = std::move(*raw_source_addresses.value);
        } else {
          context.content.source_address
              = std::move(raw_source_addresses.value->front());
        }
      }
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageDaemonWriteContext> LoadStorageDaemonWriteContext(
    const DeploymentConfigRecord& storage_config)
{
  StorageDaemonWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "storage"
                   / (storage_config.name + ".conf"),
      .is_standalone_file = true};
  if (!HasManagedConfigFiles(storage_config)) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_STORAGE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_STORAGE, resource)) {
    auto* storage = dynamic_cast<storagedaemon::StorageResource*>(resource);
    if (!storage) { continue; }
    if (auto source = storage->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_STORAGE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_STORAGE, resource)) {
    auto* storage = dynamic_cast<storagedaemon::StorageResource*>(resource);
    if (!storage || !storage->resource_name_
        || storage->resource_name_ != storage_config.name) {
      continue;
    }

    context.exists = true;
    const bool has_addresses_source
        = HasMemberSource(*storage, {"Addresses", "SdAddresses"});
    const bool has_address_source
        = HasMemberSource(*storage, {"Address", "SdAddress"});
    const bool has_source_address_source
        = HasMemberSource(*storage, {"SourceAddress", "SdSourceAddress"});
    const bool has_port_source = HasMemberSource(*storage, {"Port", "SdPort"});
    const bool has_ndmp_addresses_source
        = HasMemberSource(*storage, {"NdmpAddresses"});
    const bool has_ndmp_address_source
        = HasMemberSource(*storage, {"NdmpAddress"});
    const bool has_ndmp_port_source = HasMemberSource(*storage, {"NdmpPort"});
    if (has_addresses_source) {
      context.content.addresses = CopyAddressEntries(storage->SDaddrs);
    } else if (has_address_source) {
      context.content.address = CopyFirstAddress(storage->SDaddrs);
    }
    if (!has_addresses_source && has_port_source) {
      const auto port = GetFirstPortHostOrder(storage->SDaddrs);
      if (port > 0) { context.content.port = static_cast<uint16_t>(port); }
    }
    if (has_source_address_source) {
      if (storage->SDsrc_addr && storage->SDsrc_addr->size() > 1) {
        context.content.source_addresses
            = CopyAddressValues(storage->SDsrc_addr);
      } else {
        context.content.source_address = CopyFirstAddress(storage->SDsrc_addr);
      }
    }
    uint16_t ndmp_port = 0;
    if (has_ndmp_port_source) {
      const auto port = GetFirstPortHostOrder(storage->NDMPaddrs);
      if (port > 0) {
        ndmp_port = static_cast<uint16_t>(port);
      } else if (auto parsed_port
                 = LoadIntegerMemberSourceValue(*storage, {"NdmpPort"})) {
        ndmp_port = *parsed_port;
      }
      if (ndmp_port > 0) { context.content.ndmp_port = ndmp_port; }
    }
    if (has_ndmp_addresses_source
        || (has_ndmp_address_source && storage->NDMPaddrs
            && storage->NDMPaddrs->size() > 1)) {
      context.content.ndmp_addresses
          = CopyAddressEntries(storage->NDMPaddrs, ndmp_port);
    } else if (has_ndmp_address_source) {
      context.content.ndmp_address = CopyFirstAddress(storage->NDMPaddrs);
    }
    if (HasMemberSource(*storage, {"JustInTimeReservation"})) {
      context.content.just_in_time_reservation
          = storage->just_in_time_reservation;
    }
    if (HasMemberSource(*storage, {"MaximumConcurrentJobs"})) {
      context.content.maximum_concurrent_jobs = storage->MaxConcurrentJobs;
    }
    if (HasMemberSource(*storage, {"AbsoluteJobTimeout"})) {
      context.content.absolute_job_timeout = storage->jcr_watchdog_time;
    }
    if (HasMemberSource(*storage, {"AllowBandwidthBursting"})) {
      context.content.allow_bandwidth_bursting = storage->allow_bw_bursting;
    }
    if (HasMemberSource(*storage, {"TlsAuthenticate"})) {
      context.content.tls_authenticate = storage->authenticate_;
    }
    if (HasMemberSource(*storage, {"TlsEnable"})) {
      context.content.tls_enable = storage->tls_enable_;
    }
    if (HasMemberSource(*storage, {"TlsRequire"})) {
      context.content.tls_require = storage->tls_require_;
    }
    if (HasMemberSource(*storage, {"TlsVerifyPeer"})) {
      context.content.tls_verify_peer = storage->tls_cert_.verify_peer_;
    }
    if (!storage->cipherlist_.empty()) {
      context.content.tls_cipher_list = storage->cipherlist_;
    }
    if (!storage->ciphersuites_.empty()) {
      context.content.tls_cipher_suites = storage->ciphersuites_;
    }
    if (!storage->tls_cert_.dhfile_.empty()) {
      context.content.tls_dh_file = storage->tls_cert_.dhfile_;
    }
    if (!storage->protocol_.empty()) {
      context.content.tls_protocol = storage->protocol_;
    }
    if (!storage->tls_cert_.ca_certfile_.empty()) {
      context.content.tls_ca_certificate_file = storage->tls_cert_.ca_certfile_;
    }
    if (!storage->tls_cert_.ca_certdir_.empty()) {
      context.content.tls_ca_certificate_dir = storage->tls_cert_.ca_certdir_;
    }
    if (!storage->tls_cert_.crlfile_.empty()) {
      context.content.tls_certificate_revocation_list
          = storage->tls_cert_.crlfile_;
    }
    if (!storage->tls_cert_.certfile_.empty()) {
      context.content.tls_certificate = storage->tls_cert_.certfile_;
    }
    if (!storage->tls_cert_.keyfile_.empty()) {
      context.content.tls_key = storage->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*storage, {"TlsAllowedCn"})) {
      context.content.tls_allowed_cn
          = storage->tls_cert_.allowed_certificate_common_names_;
    }
    if (HasMemberSource(*storage, {"NdmpEnable"})) {
      context.content.ndmp_enable = storage->ndmp_enable;
    }
    if (HasMemberSource(*storage, {"NdmpSnooping"})) {
      context.content.ndmp_snooping = storage->ndmp_snooping;
    }
    if (HasMemberSource(*storage, {"NdmpLogLevel"})) {
      context.content.ndmp_log_level = storage->ndmploglevel;
    }
    if (HasMemberSource(*storage, {"AutoXFlateOnReplication"})) {
      context.content.autoxflate_on_replication
          = storage->autoxflateonreplication;
    }
    if (HasMemberSource(*storage, {"CollectDeviceStatistics"})) {
      context.content.collect_device_statistics = storage->collect_dev_stats;
    }
    if (HasMemberSource(*storage, {"CollectJobStatistics"})) {
      context.content.collect_job_statistics = storage->collect_job_stats;
    }
    if (HasMemberSource(*storage, {"StatisticsCollectInterval"})) {
      context.content.statistics_collect_interval
          = storage->stats_collect_interval;
    }
    if (HasMemberSource(*storage, {"DeviceReserveByMediaType"})) {
      context.content.device_reserve_by_media_type
          = storage->device_reserve_by_mediatype;
    }
    if (HasMemberSource(*storage, {"FileDeviceConcurrentRead"})) {
      context.content.file_device_concurrent_read
          = storage->filedevice_concurrent_read;
    }
    if (storage->verid && storage->verid[0] != '\0') {
      context.content.ver_id = std::string{storage->verid};
    }
    if (storage->log_timestamp_format
        && storage->log_timestamp_format[0] != '\0') {
      context.content.log_timestamp_format
          = std::string{storage->log_timestamp_format};
    }
    if (HasMemberSource(*storage, {"MaximumBandwidthPerJob"})) {
      context.content.maximum_bandwidth_per_job
          = storage->max_bandwidth_per_job;
    }
    if (storage->secure_erase_cmdline
        && storage->secure_erase_cmdline[0] != '\0') {
      context.content.secure_erase_command
          = std::string{storage->secure_erase_cmdline};
    }
    if (HasMemberSource(*storage, {"EnableKtls"})) {
      context.content.enable_ktls = storage->enable_ktls;
    }
    if (HasMemberSource(*storage, {"HeartbeatInterval"})) {
      context.content.heartbeat_interval
          = static_cast<uint64_t>(storage->heartbeat_interval);
    }
    if (HasMemberSource(*storage, {"SdConnectTimeout"})) {
      context.content.sd_connect_timeout
          = static_cast<uint64_t>(storage->SDConnectTimeout);
    }
    if (HasMemberSource(*storage, {"FdConnectTimeout"})) {
      context.content.fd_connect_timeout
          = static_cast<uint64_t>(storage->FDConnectTimeout);
    }
    if (HasMemberSource(*storage, {"CheckpointInterval"})) {
      context.content.checkpoint_interval
          = static_cast<uint64_t>(storage->checkpoint_interval);
    }
    if (HasMemberSource(*storage, {"ClientConnectWait"})) {
      context.content.client_connect_wait
          = static_cast<uint64_t>(storage->client_wait);
    }
    if (HasMemberSource(*storage, {"MaximumNetworkBufferSize"})) {
      context.content.maximum_network_buffer_size
          = storage->max_network_buffer_size;
    }
    if (storage->description_ && storage->description_[0] != '\0') {
      context.content.description = std::string{storage->description_};
    }
    if (storage->working_directory && storage->working_directory[0] != '\0') {
      context.content.working_directory
          = std::string{storage->working_directory};
    }
    if (storage->plugin_directory && storage->plugin_directory[0] != '\0') {
      context.content.plugin_directory = std::string{storage->plugin_directory};
    }
    if (HasMemberSource(*storage, {"PluginNames"})) {
      context.content.plugin_names = CopyAclValues(storage->plugin_names);
    }
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
    if (HasMemberSource(*storage, {"BackendDirectory"})) {
      context.content.backend_directories = storage->backend_directories;
    }
#endif
    if (storage->scripts_directory && storage->scripts_directory[0] != '\0') {
      context.content.scripts_directory
          = std::string{storage->scripts_directory};
    }
    context.content.messages = CopyResourceName(storage->messages);

    auto source = storage->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon storage resource '" + storage_config.name
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    if (has_source_address_source) {
      static const std::set<std::string> kSourceAddressDirectives{
          "SourceAddress", "SdSourceAddress"};
      auto raw_source_addresses
          = context.is_standalone_file
                ? ExtractTopLevelDirectiveValues(context.file_path, "Storage",
                                                 kSourceAddressDirectives)
                : ExtractNamedTopLevelDirectiveValues(
                      context.file_path, "Storage", storage_config.name,
                      kSourceAddressDirectives);
      if (!raw_source_addresses) {
        return {.error = raw_source_addresses.error};
      }
      if (!raw_source_addresses.value->empty()) {
        context.content.source_address.reset();
        context.content.source_addresses.clear();
        if (raw_source_addresses.value->size() > 1) {
          context.content.source_addresses
              = std::move(*raw_source_addresses.value);
        } else {
          context.content.source_address
              = std::move(raw_source_addresses.value->front());
        }
      }
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorScheduleWriteContext> LoadDirectorScheduleWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view schedule_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorScheduleWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "schedule"
                   / (std::string{schedule_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_SCHEDULE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_SCHEDULE, resource)) {
    auto* schedule = dynamic_cast<directordaemon::ScheduleResource*>(resource);
    if (!schedule) { continue; }
    if (auto source = schedule->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_SCHEDULE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_SCHEDULE, resource)) {
    auto* schedule = dynamic_cast<directordaemon::ScheduleResource*>(resource);
    if (!schedule || !schedule->resource_name_
        || schedule->resource_name_ != schedule_name) {
      continue;
    }

    context.exists = true;
    if (schedule->description_ && schedule->description_[0] != '\0') {
      context.content.description = std::string{schedule->description_};
    }
    context.content.enabled = schedule->enabled;

    auto source = schedule->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director schedule '" + std::string{schedule_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    auto runs = context.is_standalone_file
                    ? ExtractScheduleRunEntries(context.file_path)
                    : ExtractNamedScheduleRunEntries(context.file_path,
                                                     schedule_name);
    if (!runs) { return {.error = runs.error}; }
    context.content.run_entries = std::move(*runs.value);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorCounterWriteContext> LoadDirectorCounterWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view counter_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorCounterWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "counter"
                   / (std::string{counter_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_COUNTER, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_COUNTER, resource)) {
    auto* counter = dynamic_cast<directordaemon::CounterResource*>(resource);
    if (!counter) { continue; }
    if (auto source = counter->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_COUNTER, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_COUNTER, resource)) {
    auto* counter = dynamic_cast<directordaemon::CounterResource*>(resource);
    if (!counter || !counter->resource_name_
        || counter->resource_name_ != counter_name) {
      continue;
    }

    context.exists = true;
    if (counter->description_ && counter->description_[0] != '\0') {
      context.content.description = std::string{counter->description_};
    }
    context.content.minimum = counter->MinValue;
    context.content.maximum = counter->MaxValue;
    context.content.wrap_counter = CopyResourceName(counter->WrapCounter);
    context.content.catalog = CopyResourceName(counter->Catalog);

    auto source = counter->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director counter '" + std::string{counter_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorFilesetWriteContext> LoadDirectorFilesetWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view fileset_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorFilesetWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "fileset"
                   / (std::string{fileset_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_FILESET, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_FILESET, resource)) {
    auto* fileset = dynamic_cast<directordaemon::FilesetResource*>(resource);
    if (!fileset) { continue; }
    if (auto source = fileset->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_FILESET, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_FILESET, resource)) {
    auto* fileset = dynamic_cast<directordaemon::FilesetResource*>(resource);
    if (!fileset || !fileset->resource_name_
        || fileset->resource_name_ != fileset_name) {
      continue;
    }

    context.exists = true;
    if (fileset->description_ && fileset->description_[0] != '\0') {
      context.content.description = std::string{fileset->description_};
    }
    context.content.ignore_fileset_changes = fileset->ignore_fs_changes;
    context.content.enable_vss = fileset->enable_vss;

    auto source = fileset->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director fileset '" + std::string{fileset_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    auto include_blocks
        = context.is_standalone_file
              ? ExtractResourceBlocks(context.file_path, "Include")
              : ExtractNamedResourceBlocks(context.file_path, "FileSet",
                                           fileset_name, "Include");
    if (!include_blocks) { return {.error = include_blocks.error}; }
    context.content.include_blocks = std::move(*include_blocks.value);

    auto exclude_blocks
        = context.is_standalone_file
              ? ExtractResourceBlocks(context.file_path, "Exclude")
              : ExtractNamedResourceBlocks(context.file_path, "FileSet",
                                           fileset_name, "Exclude");
    if (!exclude_blocks) { return {.error = exclude_blocks.error}; }
    context.content.exclude_blocks = std::move(*exclude_blocks.value);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorJobWriteContext> LoadDirectorJobWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view job_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorJobWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "job"
                   / (std::string{job_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_JOB, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_JOB, resource)) {
    auto* job = dynamic_cast<directordaemon::JobResource*>(resource);
    if (!job) { continue; }
    if (auto source = job->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_JOB, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(directordaemon::R_JOB, resource)) {
    auto* job = dynamic_cast<directordaemon::JobResource*>(resource);
    if (!job || !job->resource_name_ || job->resource_name_ != job_name) {
      continue;
    }

    context.exists = true;
    if (job->description_ && job->description_[0] != '\0') {
      context.content.description = std::string{job->description_};
    }
    auto type = RenderJobTypeForConfig(job->JobType);
    if (!type) { return {.error = type.error}; }
    if (!type.value->empty()) { context.content.type = *type.value; }
    auto level = RenderJobLevelForConfig(job->JobLevel, job->JobType);
    if (!level) { return {.error = level.error}; }
    if (!level.value->empty()) { context.content.level = *level.value; }
    if (HasMemberSource(*job, {"BackupFormat"}) && job->backup_format
        && job->backup_format[0] != '\0') {
      context.content.backup_format = std::string{job->backup_format};
    }
    if (HasMemberSource(*job, {"Protocol"})) {
      const auto protocol = RenderDirectorJobProtocol(job->Protocol);
      if (protocol == "Unknown") {
        return {.error = "director job '" + std::string{job_name}
                         + "' has an unsupported Protocol value."};
      }
      context.content.protocol = protocol;
    }
    context.content.messages = CopyResourceName(job->messages);
    context.content.storages = CopyStorageNames(job->storage);
    context.content.pool = CopyResourceName(job->pool);
    context.content.full_backup_pool = CopyResourceName(job->full_pool);
    context.content.virtual_full_backup_pool
        = CopyResourceName(job->vfull_pool);
    context.content.incremental_backup_pool = CopyResourceName(job->inc_pool);
    context.content.differential_backup_pool = CopyResourceName(job->diff_pool);
    context.content.next_pool = CopyResourceName(job->next_pool);
    context.content.client = CopyResourceName(job->client);
    context.content.fileset = CopyResourceName(job->fileset);
    context.content.schedule = CopyResourceName(job->schedule);
    context.content.verify_job = CopyResourceName(job->verify_job);
    context.content.catalog = CopyResourceName(job->catalog);
    context.content.jobdefs = CopyResourceName(job->jobdefs);
    if (HasMemberSource(*job, {"Run"})) {
      context.content.run_entries = CopyAclValues(job->run_cmds);
    }
    if (job->RestoreWhere && job->RestoreWhere[0] != '\0') {
      context.content.where = std::string{job->RestoreWhere};
    }
    if (HasMemberSource(*job, {"Replace"})) {
      const auto replace = RenderDirectorJobReplace(job->replace);
      if (replace == "Unknown") {
        return {.error = "director job '" + std::string{job_name}
                         + "' has an unsupported Replace value."};
      }
      context.content.replace = replace;
    }
    if (HasMemberSource(*job, {"RegexWhere"}) && job->RegexWhere
        && job->RegexWhere[0] != '\0') {
      context.content.regex_where = std::string{job->RegexWhere};
    }
    if (HasMemberSource(*job, {"StripPrefix"}) && job->strip_prefix
        && job->strip_prefix[0] != '\0') {
      context.content.strip_prefix = std::string{job->strip_prefix};
    }
    if (HasMemberSource(*job, {"AddPrefix"}) && job->add_prefix
        && job->add_prefix[0] != '\0') {
      context.content.add_prefix = std::string{job->add_prefix};
    }
    if (HasMemberSource(*job, {"AddSuffix"}) && job->add_suffix
        && job->add_suffix[0] != '\0') {
      context.content.add_suffix = std::string{job->add_suffix};
    }
    if (HasMemberSource(*job, {"Bootstrap"}) && job->RestoreBootstrap
        && job->RestoreBootstrap[0] != '\0') {
      context.content.bootstrap = std::string{job->RestoreBootstrap};
    }
    if (HasMemberSource(*job, {"WriteBootstrap"}) && job->WriteBootstrap
        && job->WriteBootstrap[0] != '\0') {
      context.content.write_bootstrap = std::string{job->WriteBootstrap};
    }
    if (HasMemberSource(*job, {"WriteVerifyList"}) && job->WriteVerifyList
        && job->WriteVerifyList[0] != '\0') {
      context.content.write_verify_list = std::string{job->WriteVerifyList};
    }
    if (HasMemberSource(*job, {"MaximumBandwidth"})) {
      if (job->max_bandwidth < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative MaximumBandwidth."};
      }
      context.content.maximum_bandwidth = job->max_bandwidth;
    }
    if (HasMemberSource(*job, {"MaxRunSchedTime"})) {
      context.content.max_run_sched_time = job->MaxRunSchedTime;
    }
    if (HasMemberSource(*job, {"MaxRunTime"})) {
      context.content.max_run_time = job->MaxRunTime;
    }
    if (HasMemberSource(*job, {"FullMaxRuntime"})) {
      context.content.full_max_runtime = job->FullMaxRunTime;
    }
    if (HasMemberSource(*job, {"IncrementalMaxRuntime"})) {
      context.content.incremental_max_runtime = job->IncMaxRunTime;
    }
    if (HasMemberSource(*job, {"DifferentialMaxRuntime"})) {
      context.content.differential_max_runtime = job->DiffMaxRunTime;
    }
    if (HasMemberSource(*job, {"MaxWaitTime"})) {
      context.content.max_wait_time = job->MaxWaitTime;
    }
    if (HasMemberSource(*job, {"MaxStartDelay"})) {
      context.content.max_start_delay = job->MaxStartDelay;
    }
    if (HasMemberSource(*job, {"MaxFullInterval"})) {
      context.content.max_full_interval = job->MaxFullInterval;
    }
    if (HasMemberSource(*job, {"MaxVirtualFullInterval"})) {
      context.content.max_virtual_full_interval = job->MaxVFullInterval;
    }
    if (HasMemberSource(*job, {"MaxDiffInterval"})) {
      context.content.max_diff_interval = job->MaxDiffInterval;
    }
    if (HasMemberSource(*job, {"PrefixLinks"})) {
      context.content.prefix_links = job->PrefixLinks;
    }
    if (HasMemberSource(*job, {"PruneJobs"})) {
      context.content.prune_jobs = job->PruneJobs;
    }
    if (HasMemberSource(*job, {"PruneFiles"})) {
      context.content.prune_files = job->PruneFiles;
    }
    if (HasMemberSource(*job, {"PruneVolumes"})) {
      context.content.prune_volumes = job->PruneVolumes;
    }
    if (HasMemberSource(*job, {"PurgeMigrationJob"})) {
      context.content.purge_migration_job = job->PurgeMigrateJob;
    }
    if (HasMemberSource(*job, {"SpoolAttributes"})) {
      context.content.spool_attributes = job->SpoolAttributes;
    }
    if (HasMemberSource(*job, {"SpoolData"})) {
      context.content.spool_data = job->spool_data;
    }
    if (HasMemberSource(*job, {"SpoolSize"})) {
      if (job->spool_size < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative SpoolSize."};
      }
      context.content.spool_size = job->spool_size;
    }
    if (HasMemberSource(*job, {"RerunFailedLevels"})) {
      context.content.rerun_failed_levels = job->rerun_failed_levels;
    }
    if (HasMemberSource(*job, {"PreferMountedVolumes"})) {
      context.content.prefer_mounted_volumes = job->PreferMountedVolumes;
    }
    if (HasMemberSource(*job, {"MaximumConcurrentJobs"})) {
      if (job->MaxConcurrentJobs < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative MaximumConcurrentJobs."};
      }
      context.content.maximum_concurrent_jobs = job->MaxConcurrentJobs;
    }
    if (HasMemberSource(*job, {"RescheduleOnError"})) {
      context.content.reschedule_on_error = job->RescheduleOnError;
    }
    if (HasMemberSource(*job, {"RescheduleInterval"})) {
      context.content.reschedule_interval = job->RescheduleInterval;
    }
    if (HasMemberSource(*job, {"RescheduleTimes"})) {
      if (job->RescheduleTimes < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative RescheduleTimes."};
      }
      context.content.reschedule_times = job->RescheduleTimes;
    }
    if (job->Priority != 0) { context.content.priority = job->Priority; }
    if (HasMemberSource(*job, {"AllowMixedPriority"})) {
      context.content.allow_mixed_priority = job->allow_mixed_priority;
    }
    if (HasMemberSource(*job, {"SelectionType"})) {
      const auto selection_type
          = RenderDirectorJobSelectionType(job->selection_type);
      if (selection_type == "Unknown") {
        return {.error = "director job '" + std::string{job_name}
                         + "' has an unsupported SelectionType value."};
      }
      context.content.selection_type = selection_type;
    }
    if (HasMemberSource(*job, {"SelectionPattern"}) && job->selection_pattern
        && job->selection_pattern[0] != '\0') {
      context.content.selection_pattern = std::string{job->selection_pattern};
    }
    if (HasMemberSource(*job, {"Accurate"})) {
      context.content.accurate = job->accurate;
    }
    if (HasMemberSource(*job, {"AllowDuplicateJobs"})) {
      context.content.allow_duplicate_jobs = job->AllowDuplicateJobs;
    }
    if (HasMemberSource(*job, {"AllowHigherDuplicates"})) {
      context.content.allow_higher_duplicates = job->AllowHigherDuplicates;
    }
    if (HasMemberSource(*job, {"CancelLowerLevelDuplicates"})) {
      context.content.cancel_lower_level_duplicates
          = job->CancelLowerLevelDuplicates;
    }
    if (HasMemberSource(*job, {"CancelQueuedDuplicates"})) {
      context.content.cancel_queued_duplicates = job->CancelQueuedDuplicates;
    }
    if (HasMemberSource(*job, {"CancelRunningDuplicates"})) {
      context.content.cancel_running_duplicates = job->CancelRunningDuplicates;
    }
    if (HasMemberSource(*job, {"SaveFileHistory"})) {
      context.content.save_file_history = job->SaveFileHist;
    }
    if (HasMemberSource(*job, {"FileHistorySize"})) {
      if (job->FileHistSize < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative FileHistorySize."};
      }
      context.content.file_history_size = job->FileHistSize;
    }
    if (HasMemberSource(*job, {"FdPluginOptions"})) {
      context.content.fd_plugin_options = CopyAclValues(job->FdPluginOptions);
    }
    if (HasMemberSource(*job, {"SdPluginOptions"})) {
      context.content.sd_plugin_options = CopyAclValues(job->SdPluginOptions);
    }
    if (HasMemberSource(*job, {"DirPluginOptions"})) {
      context.content.dir_plugin_options = CopyAclValues(job->DirPluginOptions);
    }
    if (HasMemberSource(*job, {"MaxConcurrentCopies"})) {
      if (job->MaxConcurrentCopies < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative MaxConcurrentCopies."};
      }
      context.content.max_concurrent_copies = job->MaxConcurrentCopies;
    }
    if (HasMemberSource(*job, {"AlwaysIncremental"})) {
      context.content.always_incremental = job->AlwaysIncremental;
    }
    if (HasMemberSource(*job, {"AlwaysIncrementalJobRetention"})) {
      context.content.always_incremental_job_retention
          = job->AlwaysIncrementalJobRetention;
    }
    if (HasMemberSource(*job, {"AlwaysIncrementalKeepNumber"})) {
      if (job->AlwaysIncrementalKeepNumber < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative AlwaysIncrementalKeepNumber."};
      }
      context.content.always_incremental_keep_number
          = job->AlwaysIncrementalKeepNumber;
    }
    if (HasMemberSource(*job, {"AlwaysIncrementalMaxFullAge"})) {
      context.content.always_incremental_max_full_age
          = job->AlwaysIncrementalMaxFullAge;
    }
    if (HasMemberSource(*job, {"MaxFullConsolidations"})) {
      if (job->MaxFullConsolidations < 0) {
        return {.error = "director job '" + std::string{job_name}
                         + "' has negative MaxFullConsolidations."};
      }
      context.content.max_full_consolidations = job->MaxFullConsolidations;
    }
    if (HasMemberSource(*job, {"RunOnIncomingConnectInterval"})) {
      context.content.run_on_incoming_connect_interval
          = job->RunOnIncomingConnectInterval;
    }
    context.content.enabled = job->enabled;

    auto source = job->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director job '" + std::string{job_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    auto run_before_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "Job",
                                               {"RunBeforeJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path, "Job",
                                                    job_name, {"RunBeforeJob"});
    if (!run_before_job_entries) {
      return {.error = run_before_job_entries.error};
    }
    context.content.run_before_job_entries
        = std::move(*run_before_job_entries.value);
    auto run_after_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "Job",
                                               {"RunAfterJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path, "Job",
                                                    job_name, {"RunAfterJob"});
    if (!run_after_job_entries) {
      return {.error = run_after_job_entries.error};
    }
    context.content.run_after_job_entries
        = std::move(*run_after_job_entries.value);
    auto run_after_failed_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "Job",
                                               {"RunAfterFailedJob"})
              : ExtractNamedTopLevelDirectiveValues(
                    context.file_path, "Job", job_name, {"RunAfterFailedJob"});
    if (!run_after_failed_job_entries) {
      return {.error = run_after_failed_job_entries.error};
    }
    context.content.run_after_failed_job_entries
        = std::move(*run_after_failed_job_entries.value);
    auto client_run_before_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "Job",
                                               {"ClientRunBeforeJob"})
              : ExtractNamedTopLevelDirectiveValues(
                    context.file_path, "Job", job_name, {"ClientRunBeforeJob"});
    if (!client_run_before_job_entries) {
      return {.error = client_run_before_job_entries.error};
    }
    context.content.client_run_before_job_entries
        = std::move(*client_run_before_job_entries.value);
    auto client_run_after_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "Job",
                                               {"ClientRunAfterJob"})
              : ExtractNamedTopLevelDirectiveValues(
                    context.file_path, "Job", job_name, {"ClientRunAfterJob"});
    if (!client_run_after_job_entries) {
      return {.error = client_run_after_job_entries.error};
    }
    context.content.client_run_after_job_entries
        = std::move(*client_run_after_job_entries.value);

    static const std::set<std::string> kControlledJobDirectives{
        "Name",
        "Description",
        "Type",
        "BackupFormat",
        "Protocol",
        "Level",
        "Messages",
        "Storage",
        "Pool",
        "FullBackupPool",
        "VirtualFullBackupPool",
        "IncrementalBackupPool",
        "DifferentialBackupPool",
        "NextPool",
        "Client",
        "FileSet",
        "Schedule",
        "JobToVerify",
        "VerifyJob",
        "Catalog",
        "JobDefs",
        "Run",
        "RunBeforeJob",
        "RunAfterJob",
        "RunAfterFailedJob",
        "ClientRunBeforeJob",
        "ClientRunAfterJob",
        "Where",
        "Replace",
        "RegexWhere",
        "StripPrefix",
        "AddPrefix",
        "AddSuffix",
        "Bootstrap",
        "WriteBootstrap",
        "WriteVerifyList",
        "MaximumBandwidth",
        "MaxRunSchedTime",
        "MaxRunTime",
        "FullMaxRuntime",
        "IncrementalMaxRuntime",
        "DifferentialMaxRuntime",
        "MaxWaitTime",
        "MaxStartDelay",
        "MaxFullInterval",
        "MaxVirtualFullInterval",
        "MaxDiffInterval",
        "PrefixLinks",
        "PruneJobs",
        "PruneFiles",
        "PruneVolumes",
        "PurgeMigrationJob",
        "SpoolAttributes",
        "SpoolData",
        "SpoolSize",
        "RerunFailedLevels",
        "PreferMountedVolumes",
        "MaximumConcurrentJobs",
        "RescheduleOnError",
        "RescheduleInterval",
        "RescheduleTimes",
        "Priority",
        "AllowMixedPriority",
        "SelectionType",
        "SelectionPattern",
        "Accurate",
        "AllowDuplicateJobs",
        "AllowHigherDuplicates",
        "CancelLowerLevelDuplicates",
        "CancelQueuedDuplicates",
        "CancelRunningDuplicates",
        "SaveFileHistory",
        "FileHistorySize",
        "FdPluginOptions",
        "SdPluginOptions",
        "DirPluginOptions",
        "MaxConcurrentCopies",
        "AlwaysIncremental",
        "AlwaysIncrementalJobRetention",
        "AlwaysIncrementalKeepNumber",
        "AlwaysIncrementalMaxFullAge",
        "MaxFullConsolidations",
        "RunOnIncomingConnectInterval",
        "Enabled"};
    auto passthrough
        = context.is_standalone_file
              ? ExtractTopLevelResourceEntries(context.file_path, "Job",
                                               kControlledJobDirectives)
              : ExtractNamedTopLevelResourceEntries(context.file_path, "Job",
                                                    job_name,
                                                    kControlledJobDirectives);
    if (!passthrough) { return {.error = passthrough.error}; }
    for (auto& entry : *passthrough.value) {
      if (IsNamedTopLevelBlockEntry(entry, "RunScript")) {
        context.content.runscript_blocks.push_back(std::move(entry));
      } else {
        context.content.passthrough_entries.push_back(std::move(entry));
      }
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorJobDefsWriteContext> LoadDirectorJobDefsWriteContext(
    const DeploymentConfigRecord& director_config,
    std::string_view jobdefs_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorJobDefsWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "jobdefs"
                   / (std::string{jobdefs_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_JOBDEFS, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_JOBDEFS, resource)) {
    auto* jobdefs = dynamic_cast<directordaemon::JobResource*>(resource);
    if (!jobdefs) { continue; }
    if (auto source = jobdefs->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_JOBDEFS, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_JOBDEFS, resource)) {
    auto* jobdefs = dynamic_cast<directordaemon::JobResource*>(resource);
    if (!jobdefs || !jobdefs->resource_name_
        || jobdefs->resource_name_ != jobdefs_name) {
      continue;
    }

    context.exists = true;
    if (jobdefs->description_ && jobdefs->description_[0] != '\0') {
      context.content.description = std::string{jobdefs->description_};
    }
    auto type = RenderJobTypeForConfig(jobdefs->JobType);
    if (!type) { return {.error = type.error}; }
    if (!type.value->empty()) { context.content.type = *type.value; }
    auto level = RenderJobLevelForConfig(jobdefs->JobLevel, jobdefs->JobType);
    if (!level) { return {.error = level.error}; }
    if (!level.value->empty()) { context.content.level = *level.value; }
    if (HasMemberSource(*jobdefs, {"BackupFormat"}) && jobdefs->backup_format
        && jobdefs->backup_format[0] != '\0') {
      context.content.backup_format = std::string{jobdefs->backup_format};
    }
    if (HasMemberSource(*jobdefs, {"Protocol"})) {
      const auto protocol = RenderDirectorJobProtocol(jobdefs->Protocol);
      if (protocol == "Unknown") {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has an unsupported Protocol value."};
      }
      context.content.protocol = protocol;
    }
    context.content.messages = CopyResourceName(jobdefs->messages);
    context.content.storages = CopyStorageNames(jobdefs->storage);
    context.content.pool = CopyResourceName(jobdefs->pool);
    context.content.full_backup_pool = CopyResourceName(jobdefs->full_pool);
    context.content.virtual_full_backup_pool
        = CopyResourceName(jobdefs->vfull_pool);
    context.content.incremental_backup_pool
        = CopyResourceName(jobdefs->inc_pool);
    context.content.differential_backup_pool
        = CopyResourceName(jobdefs->diff_pool);
    context.content.next_pool = CopyResourceName(jobdefs->next_pool);
    context.content.client = CopyResourceName(jobdefs->client);
    context.content.fileset = CopyResourceName(jobdefs->fileset);
    context.content.schedule = CopyResourceName(jobdefs->schedule);
    context.content.verify_job = CopyResourceName(jobdefs->verify_job);
    context.content.catalog = CopyResourceName(jobdefs->catalog);
    context.content.jobdefs = CopyResourceName(jobdefs->jobdefs);
    if (HasMemberSource(*jobdefs, {"Run"})) {
      context.content.run_entries = CopyAclValues(jobdefs->run_cmds);
    }
    if (jobdefs->RestoreWhere && jobdefs->RestoreWhere[0] != '\0') {
      context.content.where = std::string{jobdefs->RestoreWhere};
    }
    if (HasMemberSource(*jobdefs, {"Replace"})) {
      const auto replace = RenderDirectorJobReplace(jobdefs->replace);
      if (replace == "Unknown") {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has an unsupported Replace value."};
      }
      context.content.replace = replace;
    }
    if (HasMemberSource(*jobdefs, {"RegexWhere"}) && jobdefs->RegexWhere
        && jobdefs->RegexWhere[0] != '\0') {
      context.content.regex_where = std::string{jobdefs->RegexWhere};
    }
    if (HasMemberSource(*jobdefs, {"StripPrefix"}) && jobdefs->strip_prefix
        && jobdefs->strip_prefix[0] != '\0') {
      context.content.strip_prefix = std::string{jobdefs->strip_prefix};
    }
    if (HasMemberSource(*jobdefs, {"AddPrefix"}) && jobdefs->add_prefix
        && jobdefs->add_prefix[0] != '\0') {
      context.content.add_prefix = std::string{jobdefs->add_prefix};
    }
    if (HasMemberSource(*jobdefs, {"AddSuffix"}) && jobdefs->add_suffix
        && jobdefs->add_suffix[0] != '\0') {
      context.content.add_suffix = std::string{jobdefs->add_suffix};
    }
    if (HasMemberSource(*jobdefs, {"Bootstrap"}) && jobdefs->RestoreBootstrap
        && jobdefs->RestoreBootstrap[0] != '\0') {
      context.content.bootstrap = std::string{jobdefs->RestoreBootstrap};
    }
    if (HasMemberSource(*jobdefs, {"WriteBootstrap"}) && jobdefs->WriteBootstrap
        && jobdefs->WriteBootstrap[0] != '\0') {
      context.content.write_bootstrap = std::string{jobdefs->WriteBootstrap};
    }
    if (HasMemberSource(*jobdefs, {"WriteVerifyList"})
        && jobdefs->WriteVerifyList && jobdefs->WriteVerifyList[0] != '\0') {
      context.content.write_verify_list = std::string{jobdefs->WriteVerifyList};
    }
    if (HasMemberSource(*jobdefs, {"MaximumBandwidth"})) {
      if (jobdefs->max_bandwidth < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative MaximumBandwidth."};
      }
      context.content.maximum_bandwidth = jobdefs->max_bandwidth;
    }
    if (HasMemberSource(*jobdefs, {"MaxRunSchedTime"})) {
      context.content.max_run_sched_time = jobdefs->MaxRunSchedTime;
    }
    if (HasMemberSource(*jobdefs, {"MaxRunTime"})) {
      context.content.max_run_time = jobdefs->MaxRunTime;
    }
    if (HasMemberSource(*jobdefs, {"FullMaxRuntime"})) {
      context.content.full_max_runtime = jobdefs->FullMaxRunTime;
    }
    if (HasMemberSource(*jobdefs, {"IncrementalMaxRuntime"})) {
      context.content.incremental_max_runtime = jobdefs->IncMaxRunTime;
    }
    if (HasMemberSource(*jobdefs, {"DifferentialMaxRuntime"})) {
      context.content.differential_max_runtime = jobdefs->DiffMaxRunTime;
    }
    if (HasMemberSource(*jobdefs, {"MaxWaitTime"})) {
      context.content.max_wait_time = jobdefs->MaxWaitTime;
    }
    if (HasMemberSource(*jobdefs, {"MaxStartDelay"})) {
      context.content.max_start_delay = jobdefs->MaxStartDelay;
    }
    if (HasMemberSource(*jobdefs, {"MaxFullInterval"})) {
      context.content.max_full_interval = jobdefs->MaxFullInterval;
    }
    if (HasMemberSource(*jobdefs, {"MaxVirtualFullInterval"})) {
      context.content.max_virtual_full_interval = jobdefs->MaxVFullInterval;
    }
    if (HasMemberSource(*jobdefs, {"MaxDiffInterval"})) {
      context.content.max_diff_interval = jobdefs->MaxDiffInterval;
    }
    if (HasMemberSource(*jobdefs, {"PrefixLinks"})) {
      context.content.prefix_links = jobdefs->PrefixLinks;
    }
    if (HasMemberSource(*jobdefs, {"PruneJobs"})) {
      context.content.prune_jobs = jobdefs->PruneJobs;
    }
    if (HasMemberSource(*jobdefs, {"PruneFiles"})) {
      context.content.prune_files = jobdefs->PruneFiles;
    }
    if (HasMemberSource(*jobdefs, {"PruneVolumes"})) {
      context.content.prune_volumes = jobdefs->PruneVolumes;
    }
    if (HasMemberSource(*jobdefs, {"PurgeMigrationJob"})) {
      context.content.purge_migration_job = jobdefs->PurgeMigrateJob;
    }
    if (HasMemberSource(*jobdefs, {"SpoolAttributes"})) {
      context.content.spool_attributes = jobdefs->SpoolAttributes;
    }
    if (HasMemberSource(*jobdefs, {"SpoolData"})) {
      context.content.spool_data = jobdefs->spool_data;
    }
    if (HasMemberSource(*jobdefs, {"SpoolSize"})) {
      if (jobdefs->spool_size < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative SpoolSize."};
      }
      context.content.spool_size = jobdefs->spool_size;
    }
    if (HasMemberSource(*jobdefs, {"RerunFailedLevels"})) {
      context.content.rerun_failed_levels = jobdefs->rerun_failed_levels;
    }
    if (HasMemberSource(*jobdefs, {"PreferMountedVolumes"})) {
      context.content.prefer_mounted_volumes = jobdefs->PreferMountedVolumes;
    }
    if (HasMemberSource(*jobdefs, {"MaximumConcurrentJobs"})) {
      if (jobdefs->MaxConcurrentJobs < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative MaximumConcurrentJobs."};
      }
      context.content.maximum_concurrent_jobs = jobdefs->MaxConcurrentJobs;
    }
    if (HasMemberSource(*jobdefs, {"RescheduleOnError"})) {
      context.content.reschedule_on_error = jobdefs->RescheduleOnError;
    }
    if (HasMemberSource(*jobdefs, {"RescheduleInterval"})) {
      context.content.reschedule_interval = jobdefs->RescheduleInterval;
    }
    if (HasMemberSource(*jobdefs, {"RescheduleTimes"})) {
      if (jobdefs->RescheduleTimes < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative RescheduleTimes."};
      }
      context.content.reschedule_times = jobdefs->RescheduleTimes;
    }
    if (jobdefs->Priority != 0) {
      context.content.priority = jobdefs->Priority;
    }
    if (HasMemberSource(*jobdefs, {"AllowMixedPriority"})) {
      context.content.allow_mixed_priority = jobdefs->allow_mixed_priority;
    }
    if (HasMemberSource(*jobdefs, {"SelectionType"})) {
      const auto selection_type
          = RenderDirectorJobSelectionType(jobdefs->selection_type);
      if (selection_type == "Unknown") {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has an unsupported SelectionType value."};
      }
      context.content.selection_type = selection_type;
    }
    if (HasMemberSource(*jobdefs, {"SelectionPattern"})
        && jobdefs->selection_pattern
        && jobdefs->selection_pattern[0] != '\0') {
      context.content.selection_pattern
          = std::string{jobdefs->selection_pattern};
    }
    if (HasMemberSource(*jobdefs, {"Accurate"})) {
      context.content.accurate = jobdefs->accurate;
    }
    if (HasMemberSource(*jobdefs, {"AllowDuplicateJobs"})) {
      context.content.allow_duplicate_jobs = jobdefs->AllowDuplicateJobs;
    }
    if (HasMemberSource(*jobdefs, {"AllowHigherDuplicates"})) {
      context.content.allow_higher_duplicates = jobdefs->AllowHigherDuplicates;
    }
    if (HasMemberSource(*jobdefs, {"CancelLowerLevelDuplicates"})) {
      context.content.cancel_lower_level_duplicates
          = jobdefs->CancelLowerLevelDuplicates;
    }
    if (HasMemberSource(*jobdefs, {"CancelQueuedDuplicates"})) {
      context.content.cancel_queued_duplicates
          = jobdefs->CancelQueuedDuplicates;
    }
    if (HasMemberSource(*jobdefs, {"CancelRunningDuplicates"})) {
      context.content.cancel_running_duplicates
          = jobdefs->CancelRunningDuplicates;
    }
    if (HasMemberSource(*jobdefs, {"SaveFileHistory"})) {
      context.content.save_file_history = jobdefs->SaveFileHist;
    }
    if (HasMemberSource(*jobdefs, {"FileHistorySize"})) {
      if (jobdefs->FileHistSize < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative FileHistorySize."};
      }
      context.content.file_history_size = jobdefs->FileHistSize;
    }
    if (HasMemberSource(*jobdefs, {"FdPluginOptions"})) {
      context.content.fd_plugin_options
          = CopyAclValues(jobdefs->FdPluginOptions);
    }
    if (HasMemberSource(*jobdefs, {"SdPluginOptions"})) {
      context.content.sd_plugin_options
          = CopyAclValues(jobdefs->SdPluginOptions);
    }
    if (HasMemberSource(*jobdefs, {"DirPluginOptions"})) {
      context.content.dir_plugin_options
          = CopyAclValues(jobdefs->DirPluginOptions);
    }
    if (HasMemberSource(*jobdefs, {"MaxConcurrentCopies"})) {
      if (jobdefs->MaxConcurrentCopies < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative MaxConcurrentCopies."};
      }
      context.content.max_concurrent_copies = jobdefs->MaxConcurrentCopies;
    }
    if (HasMemberSource(*jobdefs, {"AlwaysIncremental"})) {
      context.content.always_incremental = jobdefs->AlwaysIncremental;
    }
    if (HasMemberSource(*jobdefs, {"AlwaysIncrementalJobRetention"})) {
      context.content.always_incremental_job_retention
          = jobdefs->AlwaysIncrementalJobRetention;
    }
    if (HasMemberSource(*jobdefs, {"AlwaysIncrementalKeepNumber"})) {
      if (jobdefs->AlwaysIncrementalKeepNumber < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative AlwaysIncrementalKeepNumber."};
      }
      context.content.always_incremental_keep_number
          = jobdefs->AlwaysIncrementalKeepNumber;
    }
    if (HasMemberSource(*jobdefs, {"AlwaysIncrementalMaxFullAge"})) {
      context.content.always_incremental_max_full_age
          = jobdefs->AlwaysIncrementalMaxFullAge;
    }
    if (HasMemberSource(*jobdefs, {"MaxFullConsolidations"})) {
      if (jobdefs->MaxFullConsolidations < 0) {
        return {.error = "director jobdefs '" + std::string{jobdefs_name}
                         + "' has negative MaxFullConsolidations."};
      }
      context.content.max_full_consolidations = jobdefs->MaxFullConsolidations;
    }
    if (HasMemberSource(*jobdefs, {"RunOnIncomingConnectInterval"})) {
      context.content.run_on_incoming_connect_interval
          = jobdefs->RunOnIncomingConnectInterval;
    }
    context.content.enabled = jobdefs->enabled;

    auto source = jobdefs->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director jobdefs '" + std::string{jobdefs_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    auto run_before_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "JobDefs",
                                               {"RunBeforeJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                    "JobDefs", jobdefs_name,
                                                    {"RunBeforeJob"});
    if (!run_before_job_entries) {
      return {.error = run_before_job_entries.error};
    }
    context.content.run_before_job_entries
        = std::move(*run_before_job_entries.value);
    auto run_after_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "JobDefs",
                                               {"RunAfterJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                    "JobDefs", jobdefs_name,
                                                    {"RunAfterJob"});
    if (!run_after_job_entries) {
      return {.error = run_after_job_entries.error};
    }
    context.content.run_after_job_entries
        = std::move(*run_after_job_entries.value);
    auto run_after_failed_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "JobDefs",
                                               {"RunAfterFailedJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                    "JobDefs", jobdefs_name,
                                                    {"RunAfterFailedJob"});
    if (!run_after_failed_job_entries) {
      return {.error = run_after_failed_job_entries.error};
    }
    context.content.run_after_failed_job_entries
        = std::move(*run_after_failed_job_entries.value);
    auto client_run_before_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "JobDefs",
                                               {"ClientRunBeforeJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                    "JobDefs", jobdefs_name,
                                                    {"ClientRunBeforeJob"});
    if (!client_run_before_job_entries) {
      return {.error = client_run_before_job_entries.error};
    }
    context.content.client_run_before_job_entries
        = std::move(*client_run_before_job_entries.value);
    auto client_run_after_job_entries
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "JobDefs",
                                               {"ClientRunAfterJob"})
              : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                    "JobDefs", jobdefs_name,
                                                    {"ClientRunAfterJob"});
    if (!client_run_after_job_entries) {
      return {.error = client_run_after_job_entries.error};
    }
    context.content.client_run_after_job_entries
        = std::move(*client_run_after_job_entries.value);

    static const std::set<std::string> kControlledJobDefsDirectives{
        "Name",
        "Description",
        "Type",
        "BackupFormat",
        "Protocol",
        "Level",
        "Messages",
        "Storage",
        "Pool",
        "FullBackupPool",
        "VirtualFullBackupPool",
        "IncrementalBackupPool",
        "DifferentialBackupPool",
        "NextPool",
        "Client",
        "FileSet",
        "Schedule",
        "JobToVerify",
        "VerifyJob",
        "Catalog",
        "JobDefs",
        "Run",
        "RunBeforeJob",
        "RunAfterJob",
        "RunAfterFailedJob",
        "ClientRunBeforeJob",
        "ClientRunAfterJob",
        "Where",
        "Replace",
        "RegexWhere",
        "StripPrefix",
        "AddPrefix",
        "AddSuffix",
        "Bootstrap",
        "WriteBootstrap",
        "WriteVerifyList",
        "MaximumBandwidth",
        "MaxRunSchedTime",
        "MaxRunTime",
        "FullMaxRuntime",
        "IncrementalMaxRuntime",
        "DifferentialMaxRuntime",
        "MaxWaitTime",
        "MaxStartDelay",
        "MaxFullInterval",
        "MaxVirtualFullInterval",
        "MaxDiffInterval",
        "PrefixLinks",
        "PruneJobs",
        "PruneFiles",
        "PruneVolumes",
        "PurgeMigrationJob",
        "SpoolAttributes",
        "SpoolData",
        "SpoolSize",
        "RerunFailedLevels",
        "PreferMountedVolumes",
        "MaximumConcurrentJobs",
        "RescheduleOnError",
        "RescheduleInterval",
        "RescheduleTimes",
        "Priority",
        "AllowMixedPriority",
        "SelectionType",
        "SelectionPattern",
        "Accurate",
        "AllowDuplicateJobs",
        "AllowHigherDuplicates",
        "CancelLowerLevelDuplicates",
        "CancelQueuedDuplicates",
        "CancelRunningDuplicates",
        "SaveFileHistory",
        "FileHistorySize",
        "FdPluginOptions",
        "SdPluginOptions",
        "DirPluginOptions",
        "MaxConcurrentCopies",
        "AlwaysIncremental",
        "AlwaysIncrementalJobRetention",
        "AlwaysIncrementalKeepNumber",
        "AlwaysIncrementalMaxFullAge",
        "MaxFullConsolidations",
        "RunOnIncomingConnectInterval",
        "Enabled"};
    auto passthrough
        = context.is_standalone_file
              ? ExtractTopLevelResourceEntries(context.file_path, "JobDefs",
                                               kControlledJobDefsDirectives)
              : ExtractNamedTopLevelResourceEntries(
                    context.file_path, "JobDefs", jobdefs_name,
                    kControlledJobDefsDirectives);
    if (!passthrough) { return {.error = passthrough.error}; }
    for (auto& entry : *passthrough.value) {
      if (IsNamedTopLevelBlockEntry(entry, "RunScript")) {
        context.content.runscript_blocks.push_back(std::move(entry));
      } else {
        context.content.passthrough_entries.push_back(std::move(entry));
      }
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageDaemonDirectorWriteContext>
LoadStorageDaemonDirectorWriteContext(
    const DeploymentConfigRecord& storage_config,
    std::string_view director_name,
    const std::set<std::string>& managed_paths)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.path);
  StorageDaemonDirectorWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "director"
                   / (std::string{director_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_DIRECTOR, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_DIRECTOR, resource)) {
    auto* director = dynamic_cast<storagedaemon::DirectorResource*>(resource);
    if (!director) { continue; }
    if (auto source = director->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_DIRECTOR, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_DIRECTOR, resource)) {
    auto* director = dynamic_cast<storagedaemon::DirectorResource*>(resource);
    if (!director || !director->resource_name_
        || director->resource_name_ != director_name) {
      continue;
    }

    context.exists = true;
    if (director->description_ && director->description_[0] != '\0') {
      context.description = std::string{director->description_};
    }
    auto rendered_password = LoadSecretMemberSourceValue(
        *director, {"Password"}, director->password_,
        "storage-daemon director password for '" + std::string{director_name}
            + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;
    if (HasMemberSource(*director, {"Monitor"})) {
      context.monitor = director->monitor;
    }
    if (HasMemberSource(*director, {"MaximumBandwidthPerJob"})) {
      context.maximum_bandwidth_per_job = director->max_bandwidth_per_job;
    }
    if (HasMemberSource(*director, {"KeyEncryptionKey"})) {
      auto rendered_key = LoadSecretMemberSourceValue(
          *director, {"KeyEncryptionKey"}, director->keyencrkey,
          "storage-daemon director key encryption key for '"
              + std::string{director_name} + "'");
      if (!rendered_key) { return {.error = rendered_key.error}; }
      if (!rendered_key.value->empty()) {
        context.key_encryption_key = *rendered_key.value;
      }
    }
    if (HasMemberSource(*director, {"TlsAuthenticate"})) {
      context.tls_authenticate = director->authenticate_;
    }
    if (HasMemberSource(*director, {"TlsEnable"})) {
      context.tls_enable = director->tls_enable_;
    }
    if (HasMemberSource(*director, {"TlsRequire"})) {
      context.tls_require = director->tls_require_;
    }
    if (HasMemberSource(*director, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = director->tls_cert_.verify_peer_;
    }
    if (HasMemberSource(*director, {"TlsCipherList"})
        && !director->cipherlist_.empty()) {
      context.tls_cipher_list = director->cipherlist_;
    }
    if (HasMemberSource(*director, {"TlsCipherSuites"})
        && !director->ciphersuites_.empty()) {
      context.tls_cipher_suites = director->ciphersuites_;
    }
    if (HasMemberSource(*director, {"TlsDhFile"})
        && !director->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = director->tls_cert_.dhfile_;
    }
    if (HasMemberSource(*director, {"TlsProtocol"})
        && !director->protocol_.empty()) {
      context.tls_protocol = director->protocol_;
    }
    if (HasMemberSource(*director, {"TlsCaCertificateFile"})
        && !director->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file = director->tls_cert_.ca_certfile_;
    }
    if (HasMemberSource(*director, {"TlsCaCertificateDir"})
        && !director->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir = director->tls_cert_.ca_certdir_;
    }
    if (HasMemberSource(*director, {"TlsCertificateRevocationList"})
        && !director->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list = director->tls_cert_.crlfile_;
    }
    if (HasMemberSource(*director, {"TlsCertificate"})
        && !director->tls_cert_.certfile_.empty()) {
      context.tls_certificate = director->tls_cert_.certfile_;
    }
    if (HasMemberSource(*director, {"TlsKey"})
        && !director->tls_cert_.keyfile_.empty()) {
      context.tls_key = director->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*director, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = director->tls_cert_.allowed_certificate_common_names_;
    }

    auto source = director->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon director '" + std::string{director_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    context.is_managed
        = IsManagedPath(managed_paths, repository_root, context.file_path);
    if (!context.tls_require.has_value()) {
      static const std::set<std::string> kTlsRequireDirective{"TlsRequire"};
      auto raw_tls_require
          = context.is_standalone_file
                ? ExtractTopLevelDirectiveValues(context.file_path, "Director",
                                                 kTlsRequireDirective)
                : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                      "Director", director_name,
                                                      kTlsRequireDirective);
      if (!raw_tls_require) { return {.error = raw_tls_require.error}; }
      if (!raw_tls_require.value->empty()) {
        context.tls_require
            = ParseBareosBoolDirectiveValue(raw_tls_require.value->back());
      }
    }
    if (!context.tls_cipher_list.has_value()) {
      static const std::set<std::string> kTlsCipherListDirective{
          "TlsCipherList"};
      auto raw_tls_cipher_list
          = context.is_standalone_file
                ? ExtractTopLevelDirectiveValues(context.file_path, "Director",
                                                 kTlsCipherListDirective)
                : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                      "Director", director_name,
                                                      kTlsCipherListDirective);
      if (!raw_tls_cipher_list) { return {.error = raw_tls_cipher_list.error}; }
      if (!raw_tls_cipher_list.value->empty()) {
        context.tls_cipher_list = raw_tls_cipher_list.value->back();
      }
    }
    if (!context.tls_allowed_cn.has_value()) {
      static const std::set<std::string> kTlsAllowedCnDirective{"TlsAllowedCn"};
      auto raw_tls_allowed_cn
          = context.is_standalone_file
                ? ExtractTopLevelDirectiveValues(context.file_path, "Director",
                                                 kTlsAllowedCnDirective)
                : ExtractNamedTopLevelDirectiveValues(context.file_path,
                                                      "Director", director_name,
                                                      kTlsAllowedCnDirective);
      if (!raw_tls_allowed_cn) { return {.error = raw_tls_allowed_cn.error}; }
      if (!raw_tls_allowed_cn.value->empty()) {
        context.tls_allowed_cn = std::move(*raw_tls_allowed_cn.value);
      }
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageDaemonDeviceWriteContext>
LoadStorageDaemonDeviceWriteContext(
    const DeploymentConfigRecord& storage_config,
    std::string_view device_name,
    const std::set<std::string>& managed_paths)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.path);
  StorageDaemonDeviceWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "device"
                   / (std::string{device_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_DEVICE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_DEVICE, resource)) {
    auto* device = dynamic_cast<storagedaemon::DeviceResource*>(resource);
    if (!device) { continue; }
    if (auto source = device->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_DEVICE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_DEVICE, resource)) {
    auto* device = dynamic_cast<storagedaemon::DeviceResource*>(resource);
    if (!device || !device->resource_name_
        || device->resource_name_ != device_name) {
      continue;
    }

    context.exists = true;
    if (device->description_ && device->description_[0] != '\0') {
      context.description = std::string{device->description_};
    }
    if (device->media_type && device->media_type[0] != '\0') {
      context.media_type = std::string{device->media_type};
    }
    if (device->archive_device_string
        && device->archive_device_string[0] != '\0') {
      context.archive_device = std::string{device->archive_device_string};
    }
    if (!device->device_type.empty()) {
      context.device_type = std::string{device->device_type};
    }
    if (HasMemberSource(*device, {"AccessMode"})) {
      const auto access_mode
          = RenderStorageDeviceIoDirection(device->access_mode);
      if (access_mode == "Unknown") {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has an unsupported AccessMode value."};
      }
      context.access_mode = access_mode;
    }
    if (device->device_options && device->device_options[0] != '\0') {
      context.device_options = std::string{device->device_options};
    }
    if (device->diag_device_name && device->diag_device_name[0] != '\0') {
      context.diagnostic_device = std::string{device->diag_device_name};
    }
    if (HasMemberSource(*device, {"HardwareEndOfFile"})) {
      context.hardware_end_of_file
          = BitIsSet(storagedaemon::CAP_EOF, device->cap_bits);
    }
    if (HasMemberSource(*device, {"HardwareEndOfMedium"})) {
      context.hardware_end_of_medium
          = BitIsSet(storagedaemon::CAP_EOM, device->cap_bits);
    }
    if (HasMemberSource(*device, {"BackwardSpaceRecord"})) {
      context.backward_space_record
          = BitIsSet(storagedaemon::CAP_BSR, device->cap_bits);
    }
    if (HasMemberSource(*device, {"BackwardSpaceFile"})) {
      context.backward_space_file
          = BitIsSet(storagedaemon::CAP_BSF, device->cap_bits);
    }
    if (HasMemberSource(*device, {"BsfAtEom"})) {
      context.bsf_at_eom
          = BitIsSet(storagedaemon::CAP_BSFATEOM, device->cap_bits);
    }
    if (HasMemberSource(*device, {"TwoEof"})) {
      context.two_eof = BitIsSet(storagedaemon::CAP_TWOEOF, device->cap_bits);
    }
    if (HasMemberSource(*device, {"ForwardSpaceRecord"})) {
      context.forward_space_record
          = BitIsSet(storagedaemon::CAP_FSR, device->cap_bits);
    }
    if (HasMemberSource(*device, {"ForwardSpaceFile"})) {
      context.forward_space_file
          = BitIsSet(storagedaemon::CAP_FSF, device->cap_bits);
    }
    if (HasMemberSource(*device, {"FastForwardSpaceFile"})) {
      context.fast_forward_space_file
          = BitIsSet(storagedaemon::CAP_FASTFSF, device->cap_bits);
    }
    if (HasMemberSource(*device, {"RemovableMedia"})) {
      context.removable_media
          = BitIsSet(storagedaemon::CAP_REM, device->cap_bits);
    }
    if (HasMemberSource(*device, {"RandomAccess"})) {
      context.random_access
          = BitIsSet(storagedaemon::CAP_RACCESS, device->cap_bits);
    }
    if (HasMemberSource(*device, {"AutomaticMount"})) {
      context.automatic_mount
          = BitIsSet(storagedaemon::CAP_AUTOMOUNT, device->cap_bits);
    }
    if (HasMemberSource(*device, {"LabelMedia"})) {
      context.label_media
          = BitIsSet(storagedaemon::CAP_LABEL, device->cap_bits);
    }
    if (HasMemberSource(*device, {"AlwaysOpen"})) {
      context.always_open
          = BitIsSet(storagedaemon::CAP_ALWAYSOPEN, device->cap_bits);
    }
    if (HasMemberSource(*device, {"Autochanger"})) {
      context.autochanger = BitIsSet(storagedaemon::CAP_ATTACHED_TO_AUTOCHANGER,
                                     device->cap_bits);
    }
    if (HasMemberSource(*device, {"CloseOnPoll"})) {
      context.close_on_poll
          = BitIsSet(storagedaemon::CAP_CLOSEONPOLL, device->cap_bits);
    }
    if (HasMemberSource(*device, {"BlockPositioning"})) {
      context.block_positioning
          = BitIsSet(storagedaemon::CAP_POSITIONBLOCKS, device->cap_bits);
    }
    if (HasMemberSource(*device, {"UseMtiocget"})) {
      context.use_mtiocget
          = BitIsSet(storagedaemon::CAP_MTIOCGET, device->cap_bits);
    }
    if (HasMemberSource(*device, {"CheckLabels"})) {
      context.check_labels
          = BitIsSet(storagedaemon::CAP_CHECKLABELS, device->cap_bits);
    }
    if (HasMemberSource(*device, {"RequiresMount"})) {
      context.requires_mount
          = BitIsSet(storagedaemon::CAP_REQMOUNT, device->cap_bits);
    }
    if (HasMemberSource(*device, {"OfflineOnUnmount"})) {
      context.offline_on_unmount
          = BitIsSet(storagedaemon::CAP_OFFLINEUNMOUNT, device->cap_bits);
    }
    if (HasMemberSource(*device, {"BlockChecksum"})) {
      context.block_checksum
          = BitIsSet(storagedaemon::CAP_BLOCKCHECKSUM, device->cap_bits);
    }
    if (HasMemberSource(*device, {"AutoSelect"})) {
      context.auto_select = device->autoselect;
    }
    if (device->changer_name && device->changer_name[0] != '\0') {
      context.changer_device = std::string{device->changer_name};
    }
    if (device->changer_command && device->changer_command[0] != '\0') {
      context.changer_command = std::string{device->changer_command};
    }
    if (device->alert_command && device->alert_command[0] != '\0') {
      context.alert_command = std::string{device->alert_command};
    }
    if (HasMemberSource(*device, {"MaximumChangerWait"})) {
      context.maximum_changer_wait
          = static_cast<uint64_t>(device->max_changer_wait);
    }
    if (HasMemberSource(*device, {"MaximumOpenWait"})) {
      context.maximum_open_wait = static_cast<uint64_t>(device->max_open_wait);
    }
    if (HasMemberSource(*device, {"MaximumOpenVolumes"})) {
      context.maximum_open_volumes = device->max_open_vols;
    }
    if (HasMemberSource(*device, {"MaximumNetworkBufferSize"})) {
      context.maximum_network_buffer_size = device->max_network_buffer_size;
    }
    if (HasMemberSource(*device, {"VolumePollInterval"})) {
      context.volume_poll_interval
          = static_cast<uint64_t>(device->vol_poll_interval);
    }
    if (HasMemberSource(*device, {"MaximumRewindWait"})) {
      context.maximum_rewind_wait
          = static_cast<uint64_t>(device->max_rewind_wait);
    }
    if (HasMemberSource(*device, {"LabelBlockSize"})) {
      context.label_block_size = device->label_block_size;
    }
    if (HasMemberSource(*device, {"MinimumBlockSize"})) {
      context.minimum_block_size = device->min_block_size;
    }
    if (HasMemberSource(*device, {"MaximumBlockSize"})) {
      context.maximum_block_size = device->max_block_size;
    }
    if (HasMemberSource(*device, {"MaximumFileSize"})) {
      if (device->max_file_size < 0) {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has a negative MaximumFileSize."};
      }
      context.maximum_file_size = static_cast<uint64_t>(device->max_file_size);
    }
    if (HasMemberSource(*device, {"VolumeCapacity"})) {
      if (device->volume_capacity < 0) {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has a negative VolumeCapacity."};
      }
      context.volume_capacity = static_cast<uint64_t>(device->volume_capacity);
    }
    if (HasMemberSource(*device, {"MaximumConcurrentJobs"})) {
      context.maximum_concurrent_jobs = device->max_concurrent_jobs;
    }
    if (device->spool_directory && device->spool_directory[0] != '\0') {
      context.spool_directory = std::string{device->spool_directory};
    }
    if (HasMemberSource(*device, {"MaximumSpoolSize"})) {
      if (device->max_spool_size < 0) {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has a negative MaximumSpoolSize."};
      }
      context.maximum_spool_size
          = static_cast<uint64_t>(device->max_spool_size);
    }
    if (HasMemberSource(*device, {"MaximumJobSpoolSize"})) {
      if (device->max_job_spool_size < 0) {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has a negative MaximumJobSpoolSize."};
      }
      context.maximum_job_spool_size
          = static_cast<uint64_t>(device->max_job_spool_size);
    }
    if (HasMemberSource(*device, {"DriveIndex"})) {
      context.drive_index = device->drive_index;
    }
    if (device->mount_point && device->mount_point[0] != '\0') {
      context.mount_point = std::string{device->mount_point};
    }
    if (device->mount_command && device->mount_command[0] != '\0') {
      context.mount_command = std::string{device->mount_command};
    }
    if (device->unmount_command && device->unmount_command[0] != '\0') {
      context.unmount_command = std::string{device->unmount_command};
    }
    if (HasMemberSource(*device, {"LabelType"})) {
      auto label_type = RenderLabelTypeForConfig(device->label_type);
      if (!label_type) {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has an unsupported LabelType value."};
      }
      if (!label_type.value->empty()) {
        context.label_type = std::move(*label_type.value);
      }
    }
    if (HasMemberSource(*device, {"NoRewindOnClose"})) {
      context.no_rewind_on_close = device->norewindonclose;
    }
    if (HasMemberSource(*device, {"DriveTapeAlertEnabled"})) {
      context.drive_tape_alert_enabled = device->drive_tapealert_enabled;
    }
    if (HasMemberSource(*device, {"DriveCryptoEnabled"})) {
      context.drive_crypto_enabled = device->drive_crypto_enabled;
    }
    if (HasMemberSource(*device, {"QueryCryptoStatus"})) {
      context.query_crypto_status = device->query_crypto_status;
    }
    if (HasMemberSource(*device, {"AutoDeflate"})) {
      const auto auto_deflate
          = RenderStorageDeviceIoDirection(device->autodeflate);
      if (auto_deflate == "Unknown") {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has an unsupported AutoDeflate value."};
      }
      context.auto_deflate = auto_deflate;
    }
    if (HasMemberSource(*device, {"AutoDeflateAlgorithm"})) {
      const auto auto_deflate_algorithm
          = RenderStorageDeviceCompressionAlgorithm(
              device->autodeflate_algorithm);
      if (auto_deflate_algorithm == "Unknown") {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has an unsupported AutoDeflateAlgorithm value."};
      }
      context.auto_deflate_algorithm = auto_deflate_algorithm;
    }
    if (HasMemberSource(*device, {"AutoDeflateLevel"})) {
      context.auto_deflate_level = device->autodeflate_level;
    }
    if (HasMemberSource(*device, {"AutoInflate"})) {
      const auto auto_inflate
          = RenderStorageDeviceIoDirection(device->autoinflate);
      if (auto_inflate == "Unknown") {
        return {.error = "storage-daemon device '" + std::string{device_name}
                         + "' has an unsupported AutoInflate value."};
      }
      context.auto_inflate = auto_inflate;
    }
    if (HasMemberSource(*device, {"CollectStatistics"})) {
      context.collect_statistics = device->collectstats;
    }
    if (HasMemberSource(*device, {"EofOnErrorIsEot"})) {
      context.eof_on_error_is_eot = device->eof_on_error_is_eot;
    }
    if (HasMemberSource(*device, {"Count"})) { context.count = device->count; }

    auto source = device->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon device '" + std::string{device_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    context.is_managed
        = IsManagedPath(managed_paths, repository_root, context.file_path);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageNdmpWriteContext> LoadStorageNdmpWriteContext(
    const DeploymentConfigRecord& storage_config,
    std::string_view ndmp_name,
    const std::set<std::string>& managed_paths)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.path);
  StorageNdmpWriteContext context{.file_path
                                  = storage_config.path / "bareos-sd.d" / "ndmp"
                                    / (std::string{ndmp_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_NDMP, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(storagedaemon::R_NDMP, resource)) {
    auto* ndmp = dynamic_cast<storagedaemon::NdmpResource*>(resource);
    if (!ndmp) { continue; }
    if (auto source = ndmp->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_NDMP, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(storagedaemon::R_NDMP, resource)) {
    auto* ndmp = dynamic_cast<storagedaemon::NdmpResource*>(resource);
    if (!ndmp || !ndmp->resource_name_ || ndmp->resource_name_ != ndmp_name) {
      continue;
    }

    context.exists = true;
    if (ndmp->description_ && ndmp->description_[0] != '\0') {
      context.description = std::string{ndmp->description_};
    }
    if (ndmp->username && ndmp->username[0] != '\0') {
      context.username = std::string{ndmp->username};
    }
    auto rendered_password = LoadSecretMemberSourceValue(
        *ndmp, {"Password"}, ndmp->password,
        "storage-daemon NDMP password for '" + std::string{ndmp_name} + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;
    if (HasMemberSource(*ndmp, {"AuthType"})) {
      context.auth_type = RenderAuthType(ndmp->AuthType);
    }
    if (HasMemberSource(*ndmp, {"LogLevel"})) {
      context.log_level = ndmp->LogLevel;
    }

    auto source = ndmp->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon NDMP resource '" + std::string{ndmp_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    context.is_managed
        = IsManagedPath(managed_paths, repository_root, context.file_path);
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageAutochangerWriteContext>
LoadStorageAutochangerWriteContext(const DeploymentConfigRecord& storage_config,
                                   std::string_view autochanger_name,
                                   const std::set<std::string>& managed_paths)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.path);
  StorageAutochangerWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "autochanger"
                   / (std::string{autochanger_name} + ".conf")};
  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_AUTOCHANGER, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_AUTOCHANGER, resource)) {
    auto* autochanger
        = dynamic_cast<storagedaemon::AutochangerResource*>(resource);
    if (!autochanger) { continue; }
    if (auto source = autochanger->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(storagedaemon::R_AUTOCHANGER, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                storagedaemon::R_AUTOCHANGER, resource)) {
    auto* autochanger
        = dynamic_cast<storagedaemon::AutochangerResource*>(resource);
    if (!autochanger || !autochanger->resource_name_
        || autochanger->resource_name_ != autochanger_name) {
      continue;
    }

    context.exists = true;
    if (autochanger->description_ && autochanger->description_[0] != '\0') {
      context.description = std::string{autochanger->description_};
    }
    auto extracted_devices
        = ExtractAutochangerDeviceNames(*autochanger, autochanger_name);
    if (!extracted_devices) { return {.error = extracted_devices.error}; }
    context.devices = *extracted_devices.value;
    if (autochanger->changer_name && autochanger->changer_name[0] != '\0') {
      context.changer_device = std::string{autochanger->changer_name};
    }
    if (autochanger->changer_command
        && autochanger->changer_command[0] != '\0') {
      context.changer_command = std::string{autochanger->changer_command};
    }

    auto source = autochanger->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon autochanger '"
                       + std::string{autochanger_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    context.is_managed
        = IsManagedPath(managed_paths, repository_root, context.file_path);
    static const std::set<std::string> kAutochangerDeviceDirectives{"Device"};
    auto raw_devices
        = context.is_standalone_file
              ? ExtractTopLevelDirectiveValues(context.file_path, "Autochanger",
                                               kAutochangerDeviceDirectives)
              : ExtractNamedTopLevelDirectiveValues(
                    context.file_path, "Autochanger", autochanger_name,
                    kAutochangerDeviceDirectives);
    if (!raw_devices) { return {.error = raw_devices.error}; }
    if (!raw_devices.value->empty()) {
      context.devices = std::move(*raw_devices.value);
    }
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<std::optional<StorageSyncTarget>> SelectStorageSyncTarget(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view device_name,
    const ServiceState& state)
{
  auto storage_configs = state.ListDeploymentConfigs(
      deployment_id, bconfig::Component::kStorage);
  if (!storage_configs) { return {.error = storage_configs.error}; }
  if (storage_configs.value->empty()) {
    return {.value = std::optional<StorageSyncTarget>{}};
  }

  std::vector<DeploymentConfigRecord> exact_matches;
  std::vector<DeploymentConfigRecord> director_matches;
  for (const auto& storage_config : *storage_configs.value) {
    auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                      storage_config.path.string(), true);
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage config parser initialization ", loaded.messages)};
    }
    if (!loaded.parse_ok) {
      return {.error = FormatParseFailure("storage config ", loaded.messages)};
    }

    bool has_director = false;
    for (auto* resource
         = loaded.parser->GetNextRes(storagedaemon::R_DIRECTOR, nullptr);
         resource != nullptr; resource = loaded.parser->GetNextRes(
                                  storagedaemon::R_DIRECTOR, resource)) {
      auto* director = dynamic_cast<storagedaemon::DirectorResource*>(resource);
      if (director && director->resource_name_
          && director->resource_name_ == director_name) {
        has_director = true;
        break;
      }
    }
    if (!has_director) { continue; }

    director_matches.push_back(storage_config);

    for (auto* resource
         = loaded.parser->GetNextRes(storagedaemon::R_DEVICE, nullptr);
         resource != nullptr; resource = loaded.parser->GetNextRes(
                                  storagedaemon::R_DEVICE, resource)) {
      auto* device = dynamic_cast<storagedaemon::DeviceResource*>(resource);
      if (device && device->resource_name_
          && device->resource_name_ == device_name) {
        exact_matches.push_back(storage_config);
        break;
      }
    }
  }

  if (exact_matches.size() == 1) {
    return {.value
            = StorageSyncTarget{.storage_config = exact_matches.front()}};
  }
  if (exact_matches.size() > 1) {
    return {.error
            = "automatic storage sync is ambiguous: multiple storage "
              "daemons match director '"
              + std::string{director_name} + "' and device '"
              + std::string{device_name} + "'."};
  }
  if (director_matches.size() == 1) {
    return {.value
            = StorageSyncTarget{.storage_config = director_matches.front()}};
  }
  if (director_matches.size() > 1) {
    return {.error
            = "automatic storage sync is ambiguous: multiple storage "
              "daemons match director '"
              + std::string{director_name} + "'."};
  }
  if (storage_configs.value->size() == 1) {
    return {.value = StorageSyncTarget{.storage_config
                                       = storage_configs.value->front()}};
  }

  return {.error
          = "automatic storage sync could not determine which storage "
            "daemon config to update."};
}

OperationResult<bool> DirectorConfigHasStorageResources(
    const DeploymentConfigRecord& director_config)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  auto* resource
      = loaded.parser->GetNextRes(directordaemon::R_STORAGE, nullptr);
  return {.value = resource != nullptr};
}

OperationResult<bool> DirectorConfigUsesStorageDevice(
    const DeploymentConfigRecord& director_config,
    std::string_view device_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_STORAGE, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_STORAGE, resource)) {
    auto* storage = dynamic_cast<directordaemon::StorageResource*>(resource);
    if (!storage || storage->devices.empty()) { continue; }
    if (storage->devices.front().name == device_name) {
      return {.value = true};
    }
  }

  return {.value = false};
}

OperationResult<std::monostate> SyncStorageDaemonConfig(
    const ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view storage_name,
    const DirectorStorageWriteContext& current_context,
    std::string_view password,
    std::string_view device_name,
    std::string_view media_type,
    const std::optional<uint64_t>& maximum_bandwidth_per_job_spec,
    const std::optional<std::string>& archive_device_spec,
    const std::optional<std::string>& device_type_spec)
{
  if (current_context.device && *current_context.device != device_name) {
    return {.error
            = "automatic storage-daemon sync does not support changing "
              "the Device name yet."};
  }

  auto target = SelectStorageSyncTarget(deployment_id, director_name,
                                        device_name, state);
  if (!target) { return {.error = target.error}; }
  if (!target.value->has_value()) { return {.value = std::monostate{}}; }

  const auto repository_root
      = RepositoryRootFromConfigPath(target.value->value().storage_config.path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto director_context = LoadStorageDaemonDirectorWriteContext(
      target.value->value().storage_config, director_name,
      *managed_paths.value);
  if (!director_context) { return {.error = director_context.error}; }
  auto device_context = LoadStorageDaemonDeviceWriteContext(
      target.value->value().storage_config, device_name, *managed_paths.value);
  if (!device_context) { return {.error = device_context.error}; }

  const bool director_requires_managed_update
      = director_context.value->exists
        && (!director_context.value->is_standalone_file
            || !director_context.value->is_managed);

  const bool device_requires_managed_update
      = device_context.value->exists
        && (!device_context.value->is_standalone_file
            || !device_context.value->is_managed);

  if (director_requires_managed_update && device_requires_managed_update) {
    return {.value = std::monostate{}};
  }

  const auto archive_device = archive_device_spec
                                  ? *archive_device_spec
                                  : device_context.value->archive_device;
  if (!device_requires_managed_update
      && (!archive_device || archive_device->empty())) {
    return {.error
            = "field 'archive_device' is required when automatic "
              "storage-daemon device creation is needed."};
  }

  const auto device_type = device_type_spec ? *device_type_spec
                                            : device_context.value->device_type;
  if (!device_requires_managed_update
      && (!device_type || device_type->empty())) {
    return {.error
            = "field 'device_type' is required when automatic "
              "storage-daemon device creation is needed."};
  }

  const auto director_description
      = director_context.value->description.value_or(
          DefaultStorageDaemonDirectorDescription(director_name, storage_name));
  const auto director_monitor = director_context.value->monitor;
  const auto director_maximum_bandwidth_per_job
      = maximum_bandwidth_per_job_spec
            ? maximum_bandwidth_per_job_spec
            : director_context.value->maximum_bandwidth_per_job;
  const auto device_description = device_context.value->description.value_or(
      DefaultStorageDaemonDeviceDescription(device_name, storage_name));

  const auto director_content = BuildStorageDaemonDirectorResourceContent(
      director_name, password, director_description, director_monitor,
      director_maximum_bandwidth_per_job);
  const auto device_content = BuildStorageDaemonDeviceResourceContent(
      device_name, media_type, *archive_device, *device_type,
      device_context.value->access_mode, device_context.value->device_options,
      device_context.value->diagnostic_device,
      device_context.value->hardware_end_of_file,
      device_context.value->hardware_end_of_medium,
      device_context.value->backward_space_record,
      device_context.value->backward_space_file,
      device_context.value->bsf_at_eom, device_context.value->two_eof,
      device_context.value->forward_space_record,
      device_context.value->forward_space_file,
      device_context.value->fast_forward_space_file,
      device_context.value->removable_media,
      device_context.value->random_access,
      device_context.value->automatic_mount, device_context.value->label_media,
      device_context.value->always_open, device_context.value->autochanger,
      device_context.value->close_on_poll,
      device_context.value->block_positioning,
      device_context.value->use_mtiocget, device_context.value->check_labels,
      device_context.value->requires_mount,
      device_context.value->offline_on_unmount,
      device_context.value->block_checksum, device_context.value->auto_select,
      device_context.value->changer_device,
      device_context.value->changer_command,
      device_context.value->alert_command,
      device_context.value->maximum_changer_wait,
      device_context.value->maximum_open_wait,
      device_context.value->maximum_open_volumes,
      device_context.value->maximum_network_buffer_size,
      device_context.value->volume_poll_interval,
      device_context.value->maximum_rewind_wait,
      device_context.value->label_block_size,
      device_context.value->minimum_block_size,
      device_context.value->maximum_block_size,
      device_context.value->maximum_file_size,
      device_context.value->volume_capacity,
      device_context.value->maximum_concurrent_jobs,
      device_context.value->spool_directory,
      device_context.value->maximum_spool_size,
      device_context.value->maximum_job_spool_size,
      device_context.value->drive_index, device_context.value->mount_point,
      device_context.value->mount_command,
      device_context.value->unmount_command, device_context.value->label_type,
      device_context.value->no_rewind_on_close,
      device_context.value->drive_tape_alert_enabled,
      device_context.value->drive_crypto_enabled,
      device_context.value->query_crypto_status,
      device_context.value->auto_deflate,
      device_context.value->auto_deflate_algorithm,
      device_context.value->auto_deflate_level,
      device_context.value->auto_inflate,
      device_context.value->collect_statistics,
      device_context.value->eof_on_error_is_eot, device_context.value->count,
      device_description);

  const auto director_directory
      = target.value->value().storage_config.path / "bareos-sd.d" / "director";
  const auto device_directory
      = target.value->value().storage_config.path / "bareos-sd.d" / "device";
  std::error_code error_code;
  std::filesystem::create_directories(director_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon director directory '"
                     + director_directory.string()
                     + "': " + error_code.message()};
  }
  error_code.clear();
  std::filesystem::create_directories(device_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon device directory '"
                     + device_directory.string()
                     + "': " + error_code.message()};
  }

  std::string original_director_content;
  if (director_context.value->exists && !director_requires_managed_update) {
    auto existing = ReadFile(director_context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_director_content = std::move(*existing.value);
  }
  std::string original_device_content;
  if (device_context.value->exists && !device_requires_managed_update) {
    auto existing = ReadFile(device_context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_device_content = std::move(*existing.value);
  }

  if (!director_requires_managed_update
      && !WriteFile(director_context.value->file_path, director_content)) {
    return {.error = "failed to write storage-daemon director resource '"
                     + director_context.value->file_path.string() + "'."};
  }
  if (!device_requires_managed_update
      && !WriteFile(device_context.value->file_path, device_content)) {
    std::optional<std::string> rollback_error;
    if (director_context.value->exists && !director_requires_managed_update) {
      rollback_error = RestoreClientStubFile(director_context.value->file_path,
                                             original_director_content);
    } else if (!director_requires_managed_update) {
      rollback_error
          = CleanupCreatedFile(director_context.value->file_path,
                               target.value->value().storage_config.path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = "failed to write storage-daemon device resource '"
                     + device_context.value->file_path.string() + "'."};
  }

  {
    auto loaded = bconfig::LoadConfig(
        bconfig::Component::kStorage,
        target.value->value().storage_config.path.string(), true);
    if (!loaded.parser || !loaded.parse_ok) {
      std::optional<std::string> rollback_error;
      if (device_context.value->exists && !device_requires_managed_update) {
        rollback_error = RestoreClientStubFile(device_context.value->file_path,
                                               original_device_content);
      } else if (!device_requires_managed_update) {
        rollback_error
            = CleanupCreatedFile(device_context.value->file_path,
                                 target.value->value().storage_config.path);
      }
      if (!rollback_error) {
        if (director_context.value->exists
            && !director_requires_managed_update) {
          rollback_error = RestoreClientStubFile(
              director_context.value->file_path, original_director_content);
        } else if (!director_requires_managed_update) {
          rollback_error
              = CleanupCreatedFile(director_context.value->file_path,
                                   target.value->value().storage_config.path);
        }
      }
      if (rollback_error) { return {.error = *rollback_error}; }
      if (!loaded.parser) {
        return {
            .error = FormatParseFailure(
                "storage-daemon sync parser initialization ", loaded.messages)};
      }
      return {.error
              = FormatParseFailure("storage-daemon sync ", loaded.messages)};
    }
  }

  auto updated_managed_paths = *managed_paths.value;
  if (!director_requires_managed_update) {
    SetManagedPath(updated_managed_paths, repository_root,
                   director_context.value->file_path);
  }
  if (!device_requires_managed_update) {
    SetManagedPath(updated_managed_paths, repository_root,
                   device_context.value->file_path);
  }
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    return {.error = *error};
  }

  return {.value = std::monostate{}};
}

OperationResult<std::monostate> DeleteClientDirectorStubFile(
    const std::filesystem::path& repository_root,
    std::string_view client_name,
    std::string_view director_name)
{
  const auto client_root = RepositoryLayout::ClientsDirectory(repository_root)
                           / std::string{client_name};
  const auto stub_path = client_root / "bareos-fd.d" / "director"
                         / (std::string{director_name} + ".conf");
  if (!std::filesystem::exists(stub_path)) {
    return {.value = std::monostate{}};
  }

  auto original_content = ReadFile(stub_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (auto error = DeleteFileAndEmptyParents(
          stub_path, RepositoryLayout::ClientsDirectory(repository_root));
      error) {
    return {.error = *error};
  }

  if (!std::filesystem::exists(client_root / "bareos-fd.d")) {
    return {.value = std::monostate{}};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_root.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto restore_error = RestoreDeletedFile(stub_path, *original_content.value);
    if (restore_error) { return {.error = *restore_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("client stub parser initialization ",
                                          loaded.messages)};
    }
    return {.error
            = FormatParseFailure("client stub delete ", loaded.messages)};
  }

  return {.value = std::monostate{}};
}

OperationResult<std::monostate> DeleteSyncedStorageDaemonConfig(
    const ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view device_name,
    const DeploymentConfigRecord& director_config)
{
  auto target = SelectStorageSyncTarget(deployment_id, director_name,
                                        device_name, state);
  if (!target) { return {.error = target.error}; }
  if (!target.value->has_value()) { return {.value = std::monostate{}}; }

  const auto& storage_config = target.value->value().storage_config;
  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto director_context = LoadStorageDaemonDirectorWriteContext(
      storage_config, director_name, *managed_paths.value);
  if (!director_context) { return {.error = director_context.error}; }
  auto device_context = LoadStorageDaemonDeviceWriteContext(
      storage_config, device_name, *managed_paths.value);
  if (!device_context) { return {.error = device_context.error}; }

  auto has_any_storage = DirectorConfigHasStorageResources(director_config);
  if (!has_any_storage) { return {.error = has_any_storage.error}; }
  auto uses_device
      = DirectorConfigUsesStorageDevice(director_config, device_name);
  if (!uses_device) { return {.error = uses_device.error}; }

  const bool delete_director = director_context.value->exists
                               && director_context.value->is_standalone_file
                               && director_context.value->is_managed
                               && !*has_any_storage.value;
  const bool delete_device
      = device_context.value->exists && device_context.value->is_standalone_file
        && device_context.value->is_managed && !*uses_device.value;
  if (!delete_director && !delete_device) {
    return {.value = std::monostate{}};
  }

  std::optional<std::string> original_director_content;
  if (delete_director) {
    auto content = ReadFile(director_context.value->file_path);
    if (!content) { return {.error = content.error}; }
    original_director_content = *content.value;
  }
  std::optional<std::string> original_device_content;
  if (delete_device) {
    auto content = ReadFile(device_context.value->file_path);
    if (!content) { return {.error = content.error}; }
    original_device_content = *content.value;
  }

  if (delete_director) {
    if (auto error = DeleteFileAndEmptyParents(
            director_context.value->file_path, storage_config.path);
        error) {
      return {.error = *error};
    }
  }
  if (delete_device) {
    if (auto error = DeleteFileAndEmptyParents(device_context.value->file_path,
                                               storage_config.path);
        error) {
      if (delete_director) {
        auto restore_error = RestoreDeletedFile(
            director_context.value->file_path, *original_director_content);
        if (restore_error) { return {.error = *restore_error}; }
      }
      return {.error = *error};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    std::optional<std::string> rollback_error;
    if (delete_device) {
      rollback_error = RestoreDeletedFile(device_context.value->file_path,
                                          *original_device_content);
    }
    if (!rollback_error && delete_director) {
      rollback_error = RestoreDeletedFile(director_context.value->file_path,
                                          *original_director_content);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {
          .error = FormatParseFailure(
              "storage-daemon delete parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("storage-daemon delete ", loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  if (delete_director) {
    RemoveManagedPath(updated_managed_paths, repository_root,
                      director_context.value->file_path);
  }
  if (delete_device) {
    RemoveManagedPath(updated_managed_paths, repository_root,
                      device_context.value->file_path);
  }
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    std::optional<std::string> rollback_error;
    if (delete_device) {
      rollback_error = RestoreDeletedFile(device_context.value->file_path,
                                          *original_device_content);
    }
    if (!rollback_error && delete_director) {
      rollback_error = RestoreDeletedFile(director_context.value->file_path,
                                          *original_director_content);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  return {.value = std::monostate{}};
}

OperationResult<std::string> LoadClientPasswordFromDirectorConfig(
    const DeploymentConfigRecord& director_config,
    std::string_view client_name)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CLIENT, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CLIENT, resource)) {
    auto* client = dynamic_cast<directordaemon::ClientResource*>(resource);
    if (!client || !client->resource_name_
        || client->resource_name_ != client_name) {
      continue;
    }
    if (!client->password_.value || client->password_.value[0] == '\0') {
      return {.error = "director '" + director_config.name + "' client '"
                       + std::string{client_name} + "' has an empty password."};
    }
    return LoadSecretMemberSourceValue(
        *client, {"Password"}, client->password_,
        "director-side client password for '" + std::string{client_name} + "'");
  }

  return {.error = "director '" + director_config.name
                   + "' does not define client '" + std::string{client_name}
                   + "'."};
}

OperationResult<ClientDirectorStubWriteContext>
LoadClientDirectorStubWriteContext(const std::filesystem::path& client_root,
                                   std::string_view director_name)
{
  ClientDirectorStubWriteContext context{
      .file_path = client_root / "bareos-fd.d" / "director"
                   / (std::string{director_name} + ".conf")};
  if (!std::filesystem::exists(client_root / "bareos-fd.d")) {
    return {.value = std::move(context)};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_root.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("client config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("client config ", loaded.messages)};
  }

  std::unordered_map<std::string, size_t> resources_per_file;
  for (auto* resource
       = loaded.parser->GetNextRes(filedaemon::R_DIRECTOR, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(filedaemon::R_DIRECTOR, resource)) {
    auto* director = dynamic_cast<filedaemon::DirectorResource*>(resource);
    if (!director) { continue; }
    if (auto source = director->GetDefinitionSource()) {
      ++resources_per_file[source->file];
    }
  }

  for (auto* resource
       = loaded.parser->GetNextRes(filedaemon::R_DIRECTOR, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(filedaemon::R_DIRECTOR, resource)) {
    auto* director = dynamic_cast<filedaemon::DirectorResource*>(resource);
    if (!director || !director->resource_name_
        || director->resource_name_ != director_name) {
      continue;
    }

    context.exists = true;
    if (director->description_ && director->description_[0] != '\0') {
      context.description = std::string{director->description_};
    }
    if (HasMemberSource(*director, {"Password"})) {
      auto rendered_password = LoadSecretMemberSourceValue(
          *director, {"Password"}, director->password_,
          "client director password for '" + std::string{director_name} + "'");
      if (!rendered_password) { return {.error = rendered_password.error}; }
      context.password = *rendered_password.value;
    }
    if (HasMemberSource(*director, {"Address"}) && director->address
        && director->address[0] != '\0') {
      context.address = std::string{director->address};
    }
    if (HasMemberSource(*director, {"Port"}) && director->port > 0) {
      context.port = director->port;
    }
    if (HasMemberSource(*director, {"AllowedScriptDir"})) {
      context.allowed_script_dirs
          = CopyAclValues(director->allowed_script_dirs);
    }
    if (HasMemberSource(*director, {"AllowedJobCommand"})) {
      context.allowed_job_commands = CopyAclValues(director->allowed_job_cmds);
    }
    if (HasMemberSource(*director, {"TlsAuthenticate"})) {
      context.tls_authenticate = director->authenticate_;
    }
    if (HasMemberSource(*director, {"TlsEnable"})) {
      context.tls_enable = director->tls_enable_;
    }
    if (HasMemberSource(*director, {"TlsRequire"})) {
      context.tls_require = director->tls_require_;
    }
    if (HasMemberSource(*director, {"TlsVerifyPeer"})) {
      context.tls_verify_peer = director->tls_cert_.verify_peer_;
    }
    if (HasMemberSource(*director, {"TlsCipherList"})
        && !director->cipherlist_.empty()) {
      context.tls_cipher_list = director->cipherlist_;
    }
    if (HasMemberSource(*director, {"TlsCipherSuites"})
        && !director->ciphersuites_.empty()) {
      context.tls_cipher_suites = director->ciphersuites_;
    }
    if (HasMemberSource(*director, {"TlsDhFile"})
        && !director->tls_cert_.dhfile_.empty()) {
      context.tls_dh_file = director->tls_cert_.dhfile_;
    }
    if (HasMemberSource(*director, {"TlsProtocol"})
        && !director->protocol_.empty()) {
      context.tls_protocol = director->protocol_;
    }
    if (HasMemberSource(*director, {"TlsCaCertificateFile"})
        && !director->tls_cert_.ca_certfile_.empty()) {
      context.tls_ca_certificate_file = director->tls_cert_.ca_certfile_;
    }
    if (HasMemberSource(*director, {"TlsCaCertificateDir"})
        && !director->tls_cert_.ca_certdir_.empty()) {
      context.tls_ca_certificate_dir = director->tls_cert_.ca_certdir_;
    }
    if (HasMemberSource(*director, {"TlsCertificateRevocationList"})
        && !director->tls_cert_.crlfile_.empty()) {
      context.tls_certificate_revocation_list = director->tls_cert_.crlfile_;
    }
    if (HasMemberSource(*director, {"TlsCertificate"})
        && !director->tls_cert_.certfile_.empty()) {
      context.tls_certificate = director->tls_cert_.certfile_;
    }
    if (HasMemberSource(*director, {"TlsKey"})
        && !director->tls_cert_.keyfile_.empty()) {
      context.tls_key = director->tls_cert_.keyfile_;
    }
    if (HasMemberSource(*director, {"TlsAllowedCn"})) {
      context.tls_allowed_cn
          = director->tls_cert_.allowed_certificate_common_names_;
    }
    if (HasMemberSource(*director, {"ConnectionFromDirectorToClient"})) {
      context.connection_from_director_to_client
          = director->conn_from_dir_to_fd;
    }
    if (HasMemberSource(*director, {"ConnectionFromClientToDirector"})) {
      context.connection_from_client_to_director
          = director->conn_from_fd_to_dir;
    }
    if (HasMemberSource(*director, {"Monitor"})) {
      context.monitor = director->monitor;
    }
    if (HasMemberSource(*director, {"MaximumBandwidthPerJob"})) {
      context.maximum_bandwidth_per_job = director->max_bandwidth_per_job;
    }

    auto source = director->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "client director '" + std::string{director_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

std::optional<std::string> WriteGeneratedClientDirectorStub(
    const std::filesystem::path& client_root,
    std::string_view director_name,
    std::string_view client_name,
    const s_password& password)
{
  const auto target_directory = client_root / "bareos-fd.d" / "director";
  std::error_code error_code;
  std::filesystem::create_directories(target_directory, error_code);
  if (error_code) {
    return "failed to create generated client stub directory '"
           + target_directory.string() + "': " + error_code.message();
  }

  const auto target_path
      = target_directory / (std::string{director_name} + ".conf");
  auto rendered_password
      = RenderPasswordForConfig(password, "generated client stub password for '"
                                              + std::string{client_name} + "'");
  if (!rendered_password) { return rendered_password.error; }

  const auto stub_content = BuildClientDirectorStubContent(
      director_name, *rendered_password.value,
      "Generated director stub for client " + std::string{client_name});

  if (!WriteFile(target_path, stub_content)) {
    return "failed to write generated client stub '" + target_path.string()
           + "'.";
  }

  return std::nullopt;
}

OperationResult<std::vector<ImportedComponent>> GenerateDirectorClientStubs(
    const ImportedComponent& director_import,
    const std::filesystem::path& deployment_repository_root,
    std::vector<std::string>& logs)
{
  DebugLog("checking director '" + director_import.resource_name
           + "' for missing client stubs");
  if (director_import.component != bconfig::Component::kDirector) {
    return {.value = std::vector<ImportedComponent>{}};
  }

  auto loaded
      = bconfig::LoadConfig(bconfig::Component::kDirector,
                            director_import.destination_root.string(), true);
  if (!loaded.parser) {
    AppendParseMessages(logs, loaded.messages, "director stub source ");
    return {.error
            = "failed to initialize parser for imported director config."};
  }

  AppendParseMessages(logs, loaded.messages, "director stub source ");
  if (!loaded.parse_ok) {
    return {.error
            = "imported director config did not validate cleanly for "
              "client stub generation."};
  }

  std::vector<ImportedComponent> generated;
  for (auto* resource
       = loaded.parser->GetNextRes(directordaemon::R_CLIENT, nullptr);
       resource != nullptr; resource = loaded.parser->GetNextRes(
                                directordaemon::R_CLIENT, resource)) {
    auto* client = dynamic_cast<directordaemon::ClientResource*>(resource);
    if (!client || !client->resource_name_
        || client->resource_name_[0] == '\0') {
      continue;
    }

    const std::string client_name{client->resource_name_};
    const auto client_root
        = RepositoryLayout::ClientsDirectory(deployment_repository_root)
          / client_name;
    if (std::filesystem::exists(client_root)) { continue; }

    if (!client->password_.value || client->password_.value[0] == '\0') {
      return {.error
              = "cannot generate client stub for '" + client_name
                + "' because the director-side client password is empty."};
    }

    if (auto error = WriteGeneratedClientDirectorStub(
            client_root, director_import.resource_name, client_name,
            client->password_);
        error) {
      RemoveTreeQuietly(client_root);
      return {.error = *error};
    }

    auto imported = bconfig::LoadConfig(bconfig::Component::kClient,
                                        client_root.string(), true);
    if (!imported.parser) {
      RemoveTreeQuietly(client_root);
      AppendParseMessages(logs, imported.messages, "generated client stub ");
      return {.error = "failed to initialize parser for generated client stub '"
                       + client_name + "'."};
    }

    AppendParseMessages(logs, imported.messages, "generated client stub ");
    if (!imported.parse_ok) {
      RemoveTreeQuietly(client_root);
      return {.error = "generated client stub for '" + client_name
                       + "' did not validate cleanly."};
    }

    logs.emplace_back("generated client stub '" + client_name
                      + "' for director '" + director_import.resource_name
                      + "' into " + client_root.string());
    DebugLog("generated client stub '" + client_name + "' for director '"
             + director_import.resource_name + "'");
    generated.push_back(ImportedComponent{bconfig::Component::kClient,
                                          client_name, client_root});
  }

  return {.value = std::move(generated)};
}

std::optional<std::string> UpdateImportState(
    const std::filesystem::path& repository_root,
    const JobRecord& job,
    const std::vector<ImportedComponent>& imported_items)
{
  const auto import_state_path
      = RepositoryLayout::ImportStatePath(repository_root);
  json_error_t json_error{};
  JsonPtr root = MakeJson(json_object());
  if (std::filesystem::exists(import_state_path)) {
    root = MakeJson(json_load_file(import_state_path.c_str(), 0, &json_error));
    if (!root) {
      return "failed to parse import state '" + import_state_path.string()
             + "': " + json_error.text;
    }
    if (!json_is_object(root.get())) {
      return "import state file '" + import_state_path.string()
             + "' must contain a JSON object.";
    }
  }

  auto* imports_json = json_object_get(root.get(), "imports");
  if (!imports_json) {
    imports_json = json_array();
    json_object_set_new(root.get(), "imports", imports_json);
  } else if (!json_is_array(imports_json)) {
    return "import state file '" + import_state_path.string()
           + "' contains a non-array 'imports' field.";
  }

  for (const auto& imported : imported_items) {
    auto import_entry = MakeJson(json_object());
    json_object_set_new(import_entry.get(), "job_id",
                        json_string(job.id.c_str()));
    json_object_set_new(
        import_entry.get(), "component",
        json_string(bconfig::ComponentToString(imported.component)));
    json_object_set_new(import_entry.get(), "resource_name",
                        json_string(imported.resource_name.c_str()));
    if (job.source_path) {
      json_object_set_new(import_entry.get(), "source_path",
                          json_string(job.source_path->c_str()));
    } else {
      json_object_set_new(import_entry.get(), "source_path", json_null());
    }

    std::error_code error_code;
    auto relative_destination = std::filesystem::relative(
        imported.destination_root, repository_root, error_code);
    const auto destination_text
        = error_code ? imported.destination_root.generic_string()
                     : relative_destination.generic_string();
    json_object_set_new(import_entry.get(), "destination_path",
                        json_string(destination_text.c_str()));
    json_object_set_new(import_entry.get(), "imported_at",
                        json_string(job.updated_at.c_str()));
    json_array_append_new(imports_json, import_entry.release());
  }

  char* dump = json_dumps(root.get(), JSON_INDENT(2));
  if (!dump) {
    return "failed to serialize import state '" + import_state_path.string()
           + "'.";
  }

  std::string content{dump};
  std::free(dump);
  content.push_back('\n');
  if (!WriteFile(import_state_path, content)) {
    return "failed to write import state file '" + import_state_path.string()
           + "'.";
  }

  return std::nullopt;
}

OperationResult<std::vector<DeploymentImportRecord>> LoadDeploymentImports(
    const std::filesystem::path& repository_root)
{
  const auto import_state_path
      = RepositoryLayout::ImportStatePath(repository_root);
  if (!std::filesystem::exists(import_state_path)) {
    return {.value = std::vector<DeploymentImportRecord>{}};
  }

  json_error_t json_error{};
  auto root
      = MakeJson(json_load_file(import_state_path.c_str(), 0, &json_error));
  if (!root) {
    return {.error = "failed to parse import state '"
                     + import_state_path.string() + "': " + json_error.text};
  }
  if (!json_is_object(root.get())) {
    return {.error = "import state file '" + import_state_path.string()
                     + "' must contain a JSON object."};
  }

  auto* imports_json = json_object_get(root.get(), "imports");
  if (!imports_json) {
    return {.value = std::vector<DeploymentImportRecord>{}};
  }
  if (!json_is_array(imports_json)) {
    return {.error = "import state file '" + import_state_path.string()
                     + "' contains a non-array 'imports' field."};
  }

  std::vector<DeploymentImportRecord> imports;
  imports.reserve(json_array_size(imports_json));

  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(imports_json, index, entry)
  {
    if (!json_is_object(entry)) {
      return {.error = "import state file '" + import_state_path.string()
                       + "' contains a non-object import entry."};
    }

    auto* job_id = json_object_get(entry, "job_id");
    auto* component = json_object_get(entry, "component");
    auto* resource_name = json_object_get(entry, "resource_name");
    auto* source_path = json_object_get(entry, "source_path");
    auto* destination_path = json_object_get(entry, "destination_path");
    auto* imported_at = json_object_get(entry, "imported_at");

    if (!json_is_string(job_id) || !json_is_string(component)
        || !json_is_string(resource_name) || !json_is_string(destination_path)
        || !json_is_string(imported_at)) {
      return {.error = "import state file '" + import_state_path.string()
                       + "' contains an import entry with invalid fields."};
    }
    if (source_path && !json_is_null(source_path)
        && !json_is_string(source_path)) {
      return {.error
              = "import state file '" + import_state_path.string()
                + "' contains an import entry with an invalid source_path."};
    }

    DeploymentImportRecord record{
        .job_id = json_string_value(job_id),
        .component = json_string_value(component),
        .resource_name = json_string_value(resource_name),
        .destination_path = json_string_value(destination_path),
        .imported_at = json_string_value(imported_at),
    };
    if (source_path && json_is_string(source_path)) {
      record.source_path = std::string{json_string_value(source_path)};
    }
    imports.emplace_back(std::move(record));
  }

  return {.value = std::move(imports)};
}

OperationResult<DeploymentGitStatusRecord> LoadDeploymentGitStatus(
    const std::filesystem::path& repository_root)
{
  const auto git_directory = repository_root / ".git";
  if (!std::filesystem::exists(git_directory)) {
    return {.value = DeploymentGitStatusRecord{}};
  }

  auto output = RunGitCommand(repository_root, {"status", "--short", "--branch",
                                                "--untracked-files=all"});
  if (!output) { return {.error = output.error}; }

  DeploymentGitStatusRecord status{
      .initialized = true,
      .branch = "",
      .clean = true,
      .has_staged_changes = false,
      .has_untracked_files = false,
      .entries = {},
  };

  std::istringstream stream{*output.value};
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty()) { continue; }
    if (line.rfind("## ", 0) == 0) {
      auto branch = line.substr(3);
      const auto remote_separator = branch.find("...");
      if (remote_separator != std::string::npos) {
        branch.resize(remote_separator);
      }
      status.branch = std::move(branch);
      continue;
    }

    status.entries.emplace_back(line);
    status.clean = false;
    if (line.rfind("?? ", 0) == 0) {
      status.has_untracked_files = true;
      continue;
    }
    if (line.size() >= 1 && line[0] != ' ') {
      status.has_staged_changes = true;
    }
  }

  return {.value = std::move(status)};
}

OperationResult<ImportedComponent> ImportComponentIntoDeployment(
    bconfig::Component component,
    const std::filesystem::path& source_root,
    const std::filesystem::path& deployment_repository_root,
    std::vector<std::string>& logs)
{
  DebugLog("importing " + std::string{bconfig::ComponentToString(component)}
           + " config from " + source_root.string());
  const auto source_config_directory
      = FindComponentConfigDirectory(component, source_root);
  if (!source_config_directory) { return {}; }

  auto loaded = bconfig::LoadConfig(component, source_root.string(), true);
  if (!loaded.parser) {
    AppendParseMessages(
        logs, loaded.messages,
        std::string{bconfig::ComponentToString(component)} + " source ");
    return {.error = "failed to initialize parser for "
                     + std::string{bconfig::ComponentToString(component)}
                     + " import."};
  }

  AppendParseMessages(
      logs, loaded.messages,
      std::string{bconfig::ComponentToString(component)} + " source ");
  if (!loaded.parse_ok) {
    return {.error = std::string{bconfig::ComponentToString(component)}
                     + " source config did not parse cleanly."};
  }

  auto resource_name = DetectPrimaryResourceName(component, loaded);
  if (!resource_name) {
    logs.emplace_back(
        "skipping " + std::string{bconfig::ComponentToString(component)}
        + " import because no primary "
        + std::string{PrimaryResourceType(component)} + " resource was found");
    return {};
  }

  const auto target_bucket
      = ComponentBucketDirectory(deployment_repository_root, component);
  if (target_bucket.empty()) {
    return {.error = "component import target is not configured."};
  }

  const auto final_root = target_bucket / *resource_name;
  if (std::filesystem::exists(final_root)) {
    return {.error = "import target already exists: " + final_root.string()};
  }

  const auto temp_root = target_bucket / (*resource_name + ".tmp-import");
  RemoveTreeQuietly(temp_root);
#ifdef BCONFIG_HAVE_CONSOLE
  if (component == bconfig::Component::kConsole) {
    if (auto error = WriteImportedConsoleConfig(temp_root, loaded); error) {
      RemoveTreeQuietly(temp_root);
      return {.error = *error};
    }
  } else
#endif
  {
    const auto temp_config_directory
        = temp_root / ComponentConfigDirectoryName(component);
    if (auto error
        = CopyDirectoryTree(*source_config_directory, temp_config_directory);
        error) {
      RemoveTreeQuietly(temp_root);
      return {.error = *error};
    }
  }

  auto imported = bconfig::LoadConfig(component, temp_root.string(), true);
  if (!imported.parser) {
    RemoveTreeQuietly(temp_root);
    AppendParseMessages(
        logs, imported.messages,
        std::string{bconfig::ComponentToString(component)} + " imported ");
    return {.error = "failed to initialize parser for imported "
                     + std::string{bconfig::ComponentToString(component)}
                     + " config."};
  }
  AppendParseMessages(
      logs, imported.messages,
      std::string{bconfig::ComponentToString(component)} + " imported ");
  if (!imported.parse_ok) {
    RemoveTreeQuietly(temp_root);
    return {.error = std::string{bconfig::ComponentToString(component)}
                     + " imported config did not validate cleanly."};
  }

  std::error_code error_code;
  std::filesystem::rename(temp_root, final_root, error_code);
  if (error_code) {
    RemoveTreeQuietly(temp_root);
    return {.error = "failed to finalize import into '" + final_root.string()
                     + "': " + error_code.message()};
  }

  logs.emplace_back("imported "
                    + std::string{bconfig::ComponentToString(component)} + " '"
                    + *resource_name + "' into " + final_root.string());
  return {.value = ImportedComponent{component, *resource_name, final_root}};
}

}  // namespace

std::filesystem::path ExpandUserPath(const std::filesystem::path& path)
{
  const auto path_text = path.string();
  if (path_text == "~") {
    const auto home = HomeDirectory();
    return home.empty() ? path : home;
  }

  if (path_text.rfind("~/", 0) == 0) {
    const auto home = HomeDirectory();
    if (home.empty()) { return path; }
    return home / path_text.substr(2);
  }

  return path;
}

std::filesystem::path DefaultStorageBasePath() { return "/var/lib/bconfig"; }

std::filesystem::path DefaultDeploymentRepositoryPath(
    std::string_view deployment_id)
{
  return DefaultStorageBasePath() / "deployments" / std::string{deployment_id};
}

ServiceState::ServiceState(std::filesystem::path state_directory)
    : state_directory_{ExpandUserPath(std::move(state_directory))}
{
  if (!state_directory_.empty()) {
    std::error_code error_code;
    std::filesystem::create_directories(state_directory_, error_code);
    if (error_code) {
      throw std::runtime_error{"failed to create service state directory '"
                               + state_directory_.string()
                               + "': " + error_code.message()};
    }

    if (auto error = LoadState(); error) { throw std::runtime_error{*error}; }
  }

  worker_thread_ = std::thread{&ServiceState::WorkerLoop, this};
}

ServiceState::~ServiceState()
{
  {
    std::lock_guard guard{mutex_};
    stop_worker_ = true;
  }
  worker_condition_.notify_all();
  if (worker_thread_.joinable()) { worker_thread_.join(); }
}

std::filesystem::path RepositoryLayout::ManifestPath(
    const std::filesystem::path& repository_root)
{
  return repository_root / "manifest.json";
}

std::filesystem::path RepositoryLayout::ServiceDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "service";
}

std::filesystem::path RepositoryLayout::OwnershipPath(
    const std::filesystem::path& repository_root)
{
  return ServiceDirectory(repository_root) / "ownership.json";
}

std::filesystem::path RepositoryLayout::ImportStatePath(
    const std::filesystem::path& repository_root)
{
  return ServiceDirectory(repository_root) / "import-state.json";
}

std::filesystem::path RepositoryLayout::DirectorsDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "directors";
}

std::filesystem::path RepositoryLayout::StoragesDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "storages";
}

std::filesystem::path RepositoryLayout::ClientsDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "clients";
}

std::filesystem::path RepositoryLayout::ConsolesDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "consoles";
}

std::vector<std::filesystem::path> RepositoryLayout::Directories(
    const std::filesystem::path& repository_root)
{
  return {repository_root,
          ServiceDirectory(repository_root),
          DirectorsDirectory(repository_root),
          StoragesDirectory(repository_root),
          ClientsDirectory(repository_root),
          ConsolesDirectory(repository_root)};
}

std::string_view ToString(WorkflowMode mode)
{
  switch (mode) {
    case WorkflowMode::kDirectCommit:
      return "direct_commit";
    case WorkflowMode::kReview:
      return "review";
  }

  return "unknown";
}

std::optional<WorkflowMode> ParseWorkflowMode(std::string_view value)
{
  if (value == "direct_commit") { return WorkflowMode::kDirectCommit; }
  if (value == "review") { return WorkflowMode::kReview; }
  return std::nullopt;
}

std::string_view ToString(JobStatus status)
{
  switch (status) {
    case JobStatus::kQueued:
      return "queued";
    case JobStatus::kRunning:
      return "running";
    case JobStatus::kSucceeded:
      return "succeeded";
    case JobStatus::kFailed:
      return "failed";
  }

  return "unknown";
}

std::optional<JobStatus> ParseJobStatus(std::string_view value)
{
  if (value == "queued") { return JobStatus::kQueued; }
  if (value == "running") { return JobStatus::kRunning; }
  if (value == "succeeded") { return JobStatus::kSucceeded; }
  if (value == "failed") { return JobStatus::kFailed; }
  return std::nullopt;
}

OperationResult<DeploymentRecord> ServiceState::CreateDeployment(
    const DeploymentSpec& spec)
{
  DebugLog("creating deployment '" + spec.id + "'");
  if (!IsValidIdentifier(spec.id)) {
    return {.error
            = "deployment id must contain only letters, digits, '-' "
              "or '_'."};
  }

  if (spec.name.empty()) {
    return {.error = "deployment name must not be empty."};
  }

  const auto repository_path = ExpandUserPath(spec.repository_path);
  if (repository_path.empty()) {
    return {.error = "repository_path must not be empty."};
  }

  std::lock_guard guard{mutex_};

  if (deployments_.find(spec.id) != deployments_.end()) {
    return {.error = "deployment id already exists."};
  }

  DeploymentRecord record{.id = spec.id,
                          .name = spec.name,
                          .repository_path = repository_path,
                          .workflow_mode = spec.workflow_mode,
                          .created_at = MakeTimestamp()};

  if (auto error = InitializeRepositoryLayout(record); error) {
    return {.error = *error};
  }

  deployments_.emplace(record.id, record);
  if (auto error = SaveStateLocked(); error) {
    deployments_.erase(record.id);
    return {.error = *error};
  }
  DebugLog("created deployment '" + record.id + "' at "
           + record.repository_path.string());
  return {.value = record};
}

std::vector<DeploymentRecord> ServiceState::ListDeployments() const
{
  std::lock_guard guard{mutex_};
  return SortedValues(deployments_);
}

std::optional<DeploymentRecord> ServiceState::GetDeployment(
    std::string_view id) const
{
  std::lock_guard guard{mutex_};
  auto it = deployments_.find(std::string{id});
  if (it == deployments_.end()) { return std::nullopt; }
  return it->second;
}

OperationResult<std::vector<DeploymentConfigRecord>>
ServiceState::ListDeploymentConfigs(std::string_view deployment_id,
                                    bconfig::Component component) const
{
  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  std::vector<DeploymentConfigRecord> matches;
  for (const auto& config : DiscoverDeploymentConfigRoots(repository_path)) {
    if (config.component == component) { matches.emplace_back(config); }
  }
  return {.value = std::move(matches)};
}

OperationResult<DeploymentConfigRecord> ServiceState::GetDeploymentConfig(
    std::string_view deployment_id,
    bconfig::Component component,
    std::string_view name) const
{
  auto configs = ListDeploymentConfigs(deployment_id, component);
  if (!configs) { return {.error = configs.error}; }

  auto it = std::find_if(configs.value->begin(), configs.value->end(),
                         [name](const DeploymentConfigRecord& config) {
                           return config.name == name;
                         });
  if (it == configs.value->end()) {
    return {.error = "deployment config not found."};
  }

  return {.value = *it};
}

std::optional<uint16_t> ToOptionalUint16(const std::optional<uint32_t>& value)
{
  if (!value || *value > std::numeric_limits<uint16_t>::max()) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(*value);
}

std::optional<std::vector<std::string>> ToOptionalStringVector(
    const std::vector<std::string>& value)
{
  if (value.empty()) { return std::nullopt; }
  return value;
}

ClientDaemonResourceSpec ToClientDaemonResourceSpec(
    const StorageDaemonContentSpec& content)
{
  ClientDaemonResourceSpec spec;
  spec.address = content.address;
  spec.addresses = ToOptionalStringVector(content.addresses);
  spec.source_address = content.source_address;
  spec.source_addresses = ToOptionalStringVector(content.source_addresses);
  spec.port = content.port;
  spec.maximum_concurrent_jobs = content.maximum_concurrent_jobs;
  spec.maximum_workers_per_job = content.maximum_workers_per_job;
  spec.absolute_job_timeout = content.absolute_job_timeout;
  spec.allow_bandwidth_bursting = content.allow_bandwidth_bursting;
  spec.tls_authenticate = content.tls_authenticate;
  spec.tls_enable = content.tls_enable;
  spec.tls_require = content.tls_require;
  spec.tls_verify_peer = content.tls_verify_peer;
  spec.tls_cipher_list = content.tls_cipher_list;
  spec.tls_cipher_suites = content.tls_cipher_suites;
  spec.tls_dh_file = content.tls_dh_file;
  spec.tls_protocol = content.tls_protocol;
  spec.tls_ca_certificate_file = content.tls_ca_certificate_file;
  spec.tls_ca_certificate_dir = content.tls_ca_certificate_dir;
  spec.tls_certificate_revocation_list
      = content.tls_certificate_revocation_list;
  spec.tls_certificate = content.tls_certificate;
  spec.tls_key = content.tls_key;
  spec.tls_allowed_cn = ToOptionalStringVector(content.tls_allowed_cn);
  spec.pki_signatures = content.pki_signatures;
  spec.pki_encryption = content.pki_encryption;
  spec.pki_key_pair = content.pki_key_pair;
  spec.pki_signers = ToOptionalStringVector(content.pki_signers);
  spec.pki_master_keys = ToOptionalStringVector(content.pki_master_keys);
  spec.pki_cipher = content.pki_cipher;
  spec.always_use_lmdb = content.always_use_lmdb;
  spec.lmdb_threshold = content.lmdb_threshold;
  spec.ver_id = content.ver_id;
  spec.log_timestamp_format = content.log_timestamp_format;
  spec.maximum_bandwidth_per_job = content.maximum_bandwidth_per_job;
  spec.secure_erase_command = content.secure_erase_command;
  spec.grpc_module = content.grpc_module;
  spec.enable_ktls = content.enable_ktls;
  spec.sd_connect_timeout = content.sd_connect_timeout;
  spec.heartbeat_interval = content.heartbeat_interval;
  spec.maximum_network_buffer_size = content.maximum_network_buffer_size;
  spec.description = content.description;
  spec.working_directory = content.working_directory;
  spec.plugin_directory = content.plugin_directory;
  spec.plugin_names = ToOptionalStringVector(content.plugin_names);
  spec.allowed_script_dirs
      = ToOptionalStringVector(content.allowed_script_dirs);
  spec.allowed_job_commands
      = ToOptionalStringVector(content.allowed_job_commands);
  spec.scripts_directory = content.scripts_directory;
  spec.messages = content.messages;
  return spec;
}

DirectorDaemonResourceSpec ToDirectorDaemonResourceSpec(
    const std::optional<std::string>& password,
    const StorageDaemonContentSpec& content)
{
  DirectorDaemonResourceSpec spec;
  spec.address = content.address;
  spec.addresses = ToOptionalStringVector(content.addresses);
  spec.source_address = content.source_address;
  spec.source_addresses = ToOptionalStringVector(content.source_addresses);
  spec.port = content.port;
  spec.query_file = content.query_file;
  spec.subscriptions = content.subscriptions;
  spec.maximum_concurrent_jobs = content.maximum_concurrent_jobs;
  spec.maximum_console_connections = content.maximum_console_connections;
  spec.password = password;
  spec.absolute_job_timeout = content.absolute_job_timeout;
  spec.tls_authenticate = content.tls_authenticate;
  spec.tls_enable = content.tls_enable;
  spec.tls_require = content.tls_require;
  spec.tls_verify_peer = content.tls_verify_peer;
  spec.tls_cipher_list = content.tls_cipher_list;
  spec.tls_cipher_suites = content.tls_cipher_suites;
  spec.tls_dh_file = content.tls_dh_file;
  spec.tls_protocol = content.tls_protocol;
  spec.tls_ca_certificate_file = content.tls_ca_certificate_file;
  spec.tls_ca_certificate_dir = content.tls_ca_certificate_dir;
  spec.tls_certificate_revocation_list
      = content.tls_certificate_revocation_list;
  spec.tls_certificate = content.tls_certificate;
  spec.tls_key = content.tls_key;
  spec.tls_allowed_cn = ToOptionalStringVector(content.tls_allowed_cn);
  spec.ver_id = content.ver_id;
  spec.log_timestamp_format = content.log_timestamp_format;
  spec.secure_erase_command = content.secure_erase_command;
  spec.enable_ktls = content.enable_ktls;
  spec.fd_connect_timeout = content.fd_connect_timeout;
  spec.sd_connect_timeout = content.sd_connect_timeout;
  spec.heartbeat_interval = content.heartbeat_interval;
  spec.statistics_retention = content.statistics_retention;
  spec.statistics_collect_interval = content.statistics_collect_interval;
  spec.description = content.description;
  spec.key_encryption_key = content.key_encryption_key;
  spec.ndmp_snooping = content.ndmp_snooping;
  spec.ndmp_log_level = content.ndmp_log_level;
  spec.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
      = content.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad;
  spec.auditing = content.auditing;
  spec.audit_events = ToOptionalStringVector(content.audit_events);
  spec.working_directory = content.working_directory;
  spec.plugin_directory = content.plugin_directory;
  spec.plugin_names = ToOptionalStringVector(content.plugin_names);
  spec.scripts_directory = content.scripts_directory;
  spec.messages = content.messages;
  return spec;
}

StorageDaemonResourceSpec ToStorageDaemonResourceSpec(
    const StorageDaemonContentSpec& content)
{
  StorageDaemonResourceSpec spec;
  spec.address = content.address;
  spec.addresses = ToOptionalStringVector(content.addresses);
  spec.source_address = content.source_address;
  spec.source_addresses = ToOptionalStringVector(content.source_addresses);
  spec.port = content.port;
  spec.just_in_time_reservation = content.just_in_time_reservation;
  spec.maximum_concurrent_jobs = content.maximum_concurrent_jobs;
  spec.absolute_job_timeout = content.absolute_job_timeout;
  spec.allow_bandwidth_bursting = content.allow_bandwidth_bursting;
  spec.tls_authenticate = content.tls_authenticate;
  spec.tls_enable = content.tls_enable;
  spec.tls_require = content.tls_require;
  spec.tls_verify_peer = content.tls_verify_peer;
  spec.tls_cipher_list = content.tls_cipher_list;
  spec.tls_cipher_suites = content.tls_cipher_suites;
  spec.tls_dh_file = content.tls_dh_file;
  spec.tls_protocol = content.tls_protocol;
  spec.tls_ca_certificate_file = content.tls_ca_certificate_file;
  spec.tls_ca_certificate_dir = content.tls_ca_certificate_dir;
  spec.tls_certificate_revocation_list
      = content.tls_certificate_revocation_list;
  spec.tls_certificate = content.tls_certificate;
  spec.tls_key = content.tls_key;
  spec.tls_allowed_cn = ToOptionalStringVector(content.tls_allowed_cn);
  spec.ndmp_enable = content.ndmp_enable;
  spec.ndmp_snooping = content.ndmp_snooping;
  spec.ndmp_log_level = content.ndmp_log_level;
  spec.ndmp_address = content.ndmp_address;
  spec.ndmp_port = content.ndmp_port;
  spec.ndmp_addresses = ToOptionalStringVector(content.ndmp_addresses);
  spec.autoxflate_on_replication = content.autoxflate_on_replication;
  spec.collect_device_statistics = content.collect_device_statistics;
  spec.collect_job_statistics = content.collect_job_statistics;
  spec.statistics_collect_interval = content.statistics_collect_interval;
  spec.device_reserve_by_media_type = content.device_reserve_by_media_type;
  spec.file_device_concurrent_read = content.file_device_concurrent_read;
  spec.ver_id = content.ver_id;
  spec.log_timestamp_format = content.log_timestamp_format;
  spec.maximum_bandwidth_per_job = content.maximum_bandwidth_per_job;
  spec.secure_erase_command = content.secure_erase_command;
  spec.enable_ktls = content.enable_ktls;
  spec.sd_connect_timeout = content.sd_connect_timeout;
  spec.fd_connect_timeout = content.fd_connect_timeout;
  spec.heartbeat_interval = content.heartbeat_interval;
  spec.checkpoint_interval = content.checkpoint_interval;
  spec.client_connect_wait = content.client_connect_wait;
  spec.maximum_network_buffer_size = content.maximum_network_buffer_size;
  spec.description = content.description;
  spec.working_directory = content.working_directory;
  spec.plugin_directory = content.plugin_directory;
  spec.plugin_names = ToOptionalStringVector(content.plugin_names);
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  spec.backend_directories
      = ToOptionalStringVector(content.backend_directories);
#endif
  spec.scripts_directory = content.scripts_directory;
  spec.messages = content.messages;
  return spec;
}

StorageDeviceResourceSpec ToStorageDeviceResourceSpec(
    const StorageDaemonDeviceWriteContext& context)
{
  StorageDeviceResourceSpec spec;
  spec.media_type = context.media_type;
  spec.archive_device = context.archive_device;
  spec.device_type = context.device_type;
  spec.access_mode = context.access_mode;
  spec.device_options = context.device_options;
  spec.diagnostic_device = context.diagnostic_device;
  spec.hardware_end_of_file = context.hardware_end_of_file;
  spec.hardware_end_of_medium = context.hardware_end_of_medium;
  spec.backward_space_record = context.backward_space_record;
  spec.backward_space_file = context.backward_space_file;
  spec.bsf_at_eom = context.bsf_at_eom;
  spec.two_eof = context.two_eof;
  spec.forward_space_record = context.forward_space_record;
  spec.forward_space_file = context.forward_space_file;
  spec.fast_forward_space_file = context.fast_forward_space_file;
  spec.removable_media = context.removable_media;
  spec.random_access = context.random_access;
  spec.automatic_mount = context.automatic_mount;
  spec.label_media = context.label_media;
  spec.always_open = context.always_open;
  spec.autochanger = context.autochanger;
  spec.close_on_poll = context.close_on_poll;
  spec.block_positioning = context.block_positioning;
  spec.use_mtiocget = context.use_mtiocget;
  spec.check_labels = context.check_labels;
  spec.requires_mount = context.requires_mount;
  spec.offline_on_unmount = context.offline_on_unmount;
  spec.block_checksum = context.block_checksum;
  spec.auto_select = context.auto_select;
  spec.changer_device = context.changer_device;
  spec.changer_command = context.changer_command;
  spec.alert_command = context.alert_command;
  spec.maximum_changer_wait = context.maximum_changer_wait;
  spec.maximum_open_wait = context.maximum_open_wait;
  spec.maximum_open_volumes = context.maximum_open_volumes;
  spec.maximum_network_buffer_size = context.maximum_network_buffer_size;
  spec.volume_poll_interval = context.volume_poll_interval;
  spec.maximum_rewind_wait = context.maximum_rewind_wait;
  spec.label_block_size = context.label_block_size;
  spec.minimum_block_size = context.minimum_block_size;
  spec.maximum_block_size = context.maximum_block_size;
  spec.maximum_file_size = context.maximum_file_size;
  spec.volume_capacity = context.volume_capacity;
  spec.maximum_concurrent_jobs = context.maximum_concurrent_jobs;
  spec.spool_directory = context.spool_directory;
  spec.maximum_spool_size = context.maximum_spool_size;
  spec.maximum_job_spool_size = context.maximum_job_spool_size;
  spec.drive_index = context.drive_index;
  spec.mount_point = context.mount_point;
  spec.mount_command = context.mount_command;
  spec.unmount_command = context.unmount_command;
  spec.label_type = context.label_type;
  spec.no_rewind_on_close = context.no_rewind_on_close;
  spec.drive_tape_alert_enabled = context.drive_tape_alert_enabled;
  spec.drive_crypto_enabled = context.drive_crypto_enabled;
  spec.query_crypto_status = context.query_crypto_status;
  spec.auto_deflate = context.auto_deflate;
  spec.auto_deflate_algorithm = context.auto_deflate_algorithm;
  spec.auto_deflate_level = context.auto_deflate_level;
  spec.auto_inflate = context.auto_inflate;
  spec.collect_statistics = context.collect_statistics;
  spec.eof_on_error_is_eot = context.eof_on_error_is_eot;
  spec.count = context.count;
  spec.description = context.description;
  return spec;
}

StorageNdmpResourceSpec ToStorageNdmpResourceSpec(
    const StorageNdmpWriteContext& context)
{
  StorageNdmpResourceSpec spec;
  spec.description = context.description;
  spec.username = context.username;
  spec.password = context.password;
  spec.auth_type = context.auth_type;
  spec.log_level = context.log_level;
  return spec;
}

StorageAutochangerResourceSpec ToStorageAutochangerResourceSpec(
    const StorageAutochangerWriteContext& context)
{
  StorageAutochangerResourceSpec spec;
  spec.devices = context.devices;
  spec.changer_device = context.changer_device;
  spec.changer_command = context.changer_command;
  spec.description = context.description;
  return spec;
}

ConsoleConsoleResourceSpec ToConsoleConsoleResourceSpec(
    const ConsoleConsoleWriteContext& context)
{
  ConsoleConsoleResourceSpec spec;
  spec.director = context.director;
  spec.password = context.password;
  spec.description = context.description;
  spec.rc_file = context.rc_file;
  spec.history_file = context.history_file;
  spec.history_length = context.history_length;
  spec.heartbeat_interval = context.heartbeat_interval;
  spec.tls_authenticate = context.tls_authenticate;
  spec.tls_enable = context.tls_enable;
  spec.tls_require = context.tls_require;
  spec.tls_verify_peer = context.tls_verify_peer;
  spec.tls_cipher_list = context.tls_cipher_list;
  spec.tls_cipher_suites = context.tls_cipher_suites;
  spec.tls_dh_file = context.tls_dh_file;
  spec.tls_protocol = context.tls_protocol;
  spec.tls_ca_certificate_file = context.tls_ca_certificate_file;
  spec.tls_ca_certificate_dir = context.tls_ca_certificate_dir;
  spec.tls_certificate_revocation_list
      = context.tls_certificate_revocation_list;
  spec.tls_certificate = context.tls_certificate;
  spec.tls_key = context.tls_key;
  spec.tls_allowed_cn = context.tls_allowed_cn;
  return spec;
}

ConsoleDirectorResourceSpec ToConsoleDirectorResourceSpec(
    const ConsoleDirectorWriteContext& context)
{
  ConsoleDirectorResourceSpec spec;
  spec.address = context.address;
  spec.port = ToOptionalUint16(context.port);
  spec.password = context.password;
  spec.description = context.description;
  spec.heartbeat_interval = context.heartbeat_interval;
  spec.tls_authenticate = context.tls_authenticate;
  spec.tls_enable = context.tls_enable;
  spec.tls_require = context.tls_require;
  spec.tls_verify_peer = context.tls_verify_peer;
  spec.tls_cipher_list = context.tls_cipher_list;
  spec.tls_cipher_suites = context.tls_cipher_suites;
  spec.tls_dh_file = context.tls_dh_file;
  spec.tls_protocol = context.tls_protocol;
  spec.tls_ca_certificate_file = context.tls_ca_certificate_file;
  spec.tls_ca_certificate_dir = context.tls_ca_certificate_dir;
  spec.tls_certificate_revocation_list
      = context.tls_certificate_revocation_list;
  spec.tls_certificate = context.tls_certificate;
  spec.tls_key = context.tls_key;
  spec.tls_allowed_cn = context.tls_allowed_cn;
  return spec;
}

template <typename Spec>
Spec ToMessagesResourceSpec(const DirectorMessagesContentSpec& content)
{
  Spec spec;
  spec.description = content.description;
  spec.mail_command = content.mail_command;
  spec.operator_command = content.operator_command;
  spec.timestamp_format = content.timestamp_format;
  spec.syslog_entries = ToOptionalStringVector(content.syslog_entries);
  spec.mail_entries = ToOptionalStringVector(content.mail_entries);
  spec.mail_on_error_entries
      = ToOptionalStringVector(content.mail_on_error_entries);
  spec.mail_on_success_entries
      = ToOptionalStringVector(content.mail_on_success_entries);
  spec.file_entries = ToOptionalStringVector(content.file_entries);
  spec.append_entries = ToOptionalStringVector(content.append_entries);
  spec.stdout_entries = ToOptionalStringVector(content.stdout_entries);
  spec.stderr_entries = ToOptionalStringVector(content.stderr_entries);
  spec.director_entries = ToOptionalStringVector(content.director_entries);
  spec.console_entries = ToOptionalStringVector(content.console_entries);
  spec.operator_entries = ToOptionalStringVector(content.operator_entries);
  spec.catalog_entries = ToOptionalStringVector(content.catalog_entries);
  spec.entries = ToOptionalStringVector(content.entries);
  return spec;
}

template <typename Spec>
void FillDirectorAclSpec(
    Spec& spec,
    const std::array<std::vector<std::string>, directordaemon::Num_ACL>& acl)
{
  spec.job_acl = ToOptionalStringVector(acl[directordaemon::Job_ACL]);
  spec.client_acl = ToOptionalStringVector(acl[directordaemon::Client_ACL]);
  spec.storage_acl = ToOptionalStringVector(acl[directordaemon::Storage_ACL]);
  spec.schedule_acl = ToOptionalStringVector(acl[directordaemon::Schedule_ACL]);
  spec.pool_acl = ToOptionalStringVector(acl[directordaemon::Pool_ACL]);
  spec.command_acl = ToOptionalStringVector(acl[directordaemon::Command_ACL]);
  spec.fileset_acl = ToOptionalStringVector(acl[directordaemon::FileSet_ACL]);
  spec.catalog_acl = ToOptionalStringVector(acl[directordaemon::Catalog_ACL]);
  spec.where_acl = ToOptionalStringVector(acl[directordaemon::Where_ACL]);
  spec.plugin_options_acl
      = ToOptionalStringVector(acl[directordaemon::PluginOptions_ACL]);
}

DirectorUserResourceSpec ToDirectorUserResourceSpec(
    const DirectorUserWriteContext& context)
{
  DirectorUserResourceSpec spec;
  spec.description = context.description;
  FillDirectorAclSpec(spec, context.acl);
  spec.profiles = ToOptionalStringVector(context.profiles);
  return spec;
}

DirectorProfileResourceSpec ToDirectorProfileResourceSpec(
    const DirectorProfileWriteContext& context)
{
  DirectorProfileResourceSpec spec;
  spec.description = context.description;
  FillDirectorAclSpec(spec, context.acl);
  return spec;
}

DirectorPoolResourceSpec ToDirectorPoolResourceSpec(
    const DirectorPoolContentSpec& content)
{
  DirectorPoolResourceSpec spec;
  spec.pool_type = content.pool_type;
  spec.label_format = content.label_format;
  spec.cleaning_prefix = content.cleaning_prefix;
  spec.label_type = content.label_type;
  spec.maximum_volumes = content.maximum_volumes;
  spec.maximum_volume_jobs = content.maximum_volume_jobs;
  spec.maximum_volume_files = content.maximum_volume_files;
  spec.maximum_volume_bytes = content.maximum_volume_bytes;
  spec.volume_retention = content.volume_retention;
  spec.volume_use_duration = content.volume_use_duration;
  spec.migration_time = content.migration_time;
  spec.migration_high_bytes = content.migration_high_bytes;
  spec.migration_low_bytes = content.migration_low_bytes;
  spec.next_pool = content.next_pool;
  spec.storages = ToOptionalStringVector(content.storages);
  spec.use_catalog = content.use_catalog;
  spec.catalog_files = content.catalog_files;
  spec.purge_oldest_volume = content.purge_oldest_volume;
  spec.action_on_purge = content.action_on_purge;
  spec.recycle_oldest_volume = content.recycle_oldest_volume;
  spec.recycle_current_volume = content.recycle_current_volume;
  spec.auto_prune = content.auto_prune;
  spec.recycle = content.recycle;
  spec.recycle_pool = content.recycle_pool;
  spec.scratch_pool = content.scratch_pool;
  spec.catalog = content.catalog;
  spec.file_retention = content.file_retention;
  spec.job_retention = content.job_retention;
  spec.minimum_block_size = content.minimum_block_size;
  spec.maximum_block_size = content.maximum_block_size;
  spec.description = content.description;
  return spec;
}

DirectorCatalogResourceSpec ToDirectorCatalogResourceSpec(
    const DirectorCatalogContentSpec& content)
{
  DirectorCatalogResourceSpec spec;
  spec.db_address = content.db_address;
  spec.db_port = content.db_port;
  spec.db_socket = content.db_socket;
  spec.db_password = content.db_password;
  spec.db_user = content.db_user;
  spec.db_name = content.db_name;
  spec.multiple_connections = content.multiple_connections;
  spec.disable_batch_insert = content.disable_batch_insert;
  spec.reconnect = content.reconnect;
  spec.exit_on_fatal = content.exit_on_fatal;
  spec.min_connections = content.min_connections;
  spec.max_connections = content.max_connections;
  spec.inc_connections = content.inc_connections;
  spec.idle_timeout = content.idle_timeout;
  spec.validate_timeout = content.validate_timeout;
  spec.description = content.description;
  return spec;
}

DirectorCounterResourceSpec ToDirectorCounterResourceSpec(
    const DirectorCounterContentSpec& content)
{
  DirectorCounterResourceSpec spec;
  spec.minimum = content.minimum;
  spec.maximum = content.maximum;
  spec.wrap_counter = content.wrap_counter;
  spec.catalog = content.catalog;
  spec.description = content.description;
  return spec;
}

DirectorFilesetResourceSpec ToDirectorFilesetResourceSpec(
    const DirectorFilesetContentSpec& content)
{
  DirectorFilesetResourceSpec spec;
  spec.description = content.description;
  spec.ignore_fileset_changes = content.ignore_fileset_changes;
  spec.enable_vss = content.enable_vss;
  spec.include_blocks = ToOptionalStringVector(content.include_blocks);
  spec.exclude_blocks = ToOptionalStringVector(content.exclude_blocks);
  return spec;
}

template <typename Spec>
Spec ToDirectorJobLikeResourceSpec(const DirectorJobContentSpec& content)
{
  Spec spec;
  spec.description = content.description;
  spec.type = content.type;
  spec.backup_format = content.backup_format;
  spec.protocol = content.protocol;
  spec.level = content.level;
  spec.messages = content.messages;
  spec.storages = content.storages;
  spec.pool = content.pool;
  spec.full_backup_pool = content.full_backup_pool;
  spec.virtual_full_backup_pool = content.virtual_full_backup_pool;
  spec.incremental_backup_pool = content.incremental_backup_pool;
  spec.differential_backup_pool = content.differential_backup_pool;
  spec.next_pool = content.next_pool;
  spec.client = content.client;
  spec.fileset = content.fileset;
  spec.schedule = content.schedule;
  spec.verify_job = content.verify_job;
  spec.catalog = content.catalog;
  spec.jobdefs = content.jobdefs;
  spec.run_entries = content.run_entries;
  spec.run_before_job_entries = content.run_before_job_entries;
  spec.run_after_job_entries = content.run_after_job_entries;
  spec.run_after_failed_job_entries = content.run_after_failed_job_entries;
  spec.client_run_before_job_entries = content.client_run_before_job_entries;
  spec.client_run_after_job_entries = content.client_run_after_job_entries;
  spec.runscript_blocks = content.runscript_blocks;
  spec.where = content.where;
  spec.replace = content.replace;
  spec.regex_where = content.regex_where;
  spec.strip_prefix = content.strip_prefix;
  spec.add_prefix = content.add_prefix;
  spec.add_suffix = content.add_suffix;
  spec.bootstrap = content.bootstrap;
  spec.write_bootstrap = content.write_bootstrap;
  spec.write_verify_list = content.write_verify_list;
  spec.maximum_bandwidth = content.maximum_bandwidth;
  spec.max_run_sched_time = content.max_run_sched_time;
  spec.max_run_time = content.max_run_time;
  spec.full_max_runtime = content.full_max_runtime;
  spec.incremental_max_runtime = content.incremental_max_runtime;
  spec.differential_max_runtime = content.differential_max_runtime;
  spec.max_wait_time = content.max_wait_time;
  spec.max_start_delay = content.max_start_delay;
  spec.max_full_interval = content.max_full_interval;
  spec.max_virtual_full_interval = content.max_virtual_full_interval;
  spec.max_diff_interval = content.max_diff_interval;
  spec.prefix_links = content.prefix_links;
  spec.prune_jobs = content.prune_jobs;
  spec.prune_files = content.prune_files;
  spec.prune_volumes = content.prune_volumes;
  spec.purge_migration_job = content.purge_migration_job;
  spec.spool_attributes = content.spool_attributes;
  spec.spool_data = content.spool_data;
  spec.spool_size = content.spool_size;
  spec.rerun_failed_levels = content.rerun_failed_levels;
  spec.prefer_mounted_volumes = content.prefer_mounted_volumes;
  spec.maximum_concurrent_jobs = content.maximum_concurrent_jobs;
  spec.reschedule_on_error = content.reschedule_on_error;
  spec.reschedule_interval = content.reschedule_interval;
  spec.reschedule_times = content.reschedule_times;
  spec.priority = content.priority;
  spec.allow_mixed_priority = content.allow_mixed_priority;
  spec.selection_type = content.selection_type;
  spec.selection_pattern = content.selection_pattern;
  spec.accurate = content.accurate;
  spec.allow_duplicate_jobs = content.allow_duplicate_jobs;
  spec.allow_higher_duplicates = content.allow_higher_duplicates;
  spec.cancel_lower_level_duplicates = content.cancel_lower_level_duplicates;
  spec.cancel_queued_duplicates = content.cancel_queued_duplicates;
  spec.cancel_running_duplicates = content.cancel_running_duplicates;
  spec.save_file_history = content.save_file_history;
  spec.file_history_size = content.file_history_size;
  spec.fd_plugin_options = content.fd_plugin_options;
  spec.sd_plugin_options = content.sd_plugin_options;
  spec.dir_plugin_options = content.dir_plugin_options;
  spec.max_concurrent_copies = content.max_concurrent_copies;
  spec.always_incremental = content.always_incremental;
  spec.always_incremental_job_retention
      = content.always_incremental_job_retention;
  spec.always_incremental_keep_number = content.always_incremental_keep_number;
  spec.always_incremental_max_full_age
      = content.always_incremental_max_full_age;
  spec.max_full_consolidations = content.max_full_consolidations;
  spec.run_on_incoming_connect_interval
      = content.run_on_incoming_connect_interval;
  spec.enabled = content.enabled;
  return spec;
}

OperationResult<ClientDirectorStubSpec> ServiceState::GetClientDirectorStubSpec(
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view director_name) const
{
  auto client_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) { return {.error = client_config.error}; }

  auto context = LoadClientDirectorStubWriteContext(client_config.value->path,
                                                    director_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "client director '" + std::string{director_name}
                     + "' not found."};
  }

  return {
      .value = ClientDirectorStubSpec{
          .description = context.value->description,
          .password = context.value->password,
          .address = context.value->address,
          .port = ToOptionalUint16(context.value->port),
          .allowed_script_dirs = context.value->allowed_script_dirs,
          .allowed_job_commands = context.value->allowed_job_commands,
          .tls_authenticate = context.value->tls_authenticate,
          .tls_enable = context.value->tls_enable,
          .tls_require = context.value->tls_require,
          .tls_verify_peer = context.value->tls_verify_peer,
          .tls_cipher_list = context.value->tls_cipher_list,
          .tls_cipher_suites = context.value->tls_cipher_suites,
          .tls_dh_file = context.value->tls_dh_file,
          .tls_protocol = context.value->tls_protocol,
          .tls_ca_certificate_file = context.value->tls_ca_certificate_file,
          .tls_ca_certificate_dir = context.value->tls_ca_certificate_dir,
          .tls_certificate_revocation_list
          = context.value->tls_certificate_revocation_list,
          .tls_certificate = context.value->tls_certificate,
          .tls_key = context.value->tls_key,
          .tls_allowed_cn = context.value->tls_allowed_cn,
          .connection_from_director_to_client
          = context.value->connection_from_director_to_client,
          .connection_from_client_to_director
          = context.value->connection_from_client_to_director,
          .monitor = context.value->monitor,
          .maximum_bandwidth_per_job = context.value->maximum_bandwidth_per_job,
      }};
}

OperationResult<DirectorClientResourceSpec>
ServiceState::GetDirectorClientResourceSpec(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view client_name) const
{
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorClientWriteContext(*director_config.value, client_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error
            = "director client '" + std::string{client_name} + "' not found."};
  }

  return {
      .value = DirectorClientResourceSpec{
          .address = context.value->address,
          .lan_address = context.value->lan_address,
          .port = ToOptionalUint16(context.value->port),
          .protocol = context.value->protocol,
          .auth_type = context.value->auth_type,
          .catalog = context.value->catalog,
          .username = context.value->username,
          .password = context.value->password,
          .enabled = context.value->enabled,
          .passive = context.value->passive,
          .strict_quotas = context.value->strict_quotas,
          .quota_include_failed_jobs = context.value->quota_include_failed_jobs,
          .soft_quota = context.value->soft_quota,
          .hard_quota = context.value->hard_quota,
          .soft_quota_grace_period = context.value->soft_quota_grace_period,
          .file_retention = context.value->file_retention,
          .job_retention = context.value->job_retention,
          .ndmp_log_level = context.value->ndmp_log_level,
          .ndmp_block_size = context.value->ndmp_block_size,
          .ndmp_use_lmdb = context.value->ndmp_use_lmdb,
          .auto_prune = context.value->auto_prune,
          .tls_authenticate = context.value->tls_authenticate,
          .tls_enable = context.value->tls_enable,
          .tls_require = context.value->tls_require,
          .tls_verify_peer = context.value->tls_verify_peer,
          .tls_cipher_list = context.value->tls_cipher_list,
          .tls_cipher_suites = context.value->tls_cipher_suites,
          .tls_dh_file = context.value->tls_dh_file,
          .tls_protocol = context.value->tls_protocol,
          .tls_ca_certificate_file = context.value->tls_ca_certificate_file,
          .tls_ca_certificate_dir = context.value->tls_ca_certificate_dir,
          .tls_certificate_revocation_list
          = context.value->tls_certificate_revocation_list,
          .tls_certificate = context.value->tls_certificate,
          .tls_key = context.value->tls_key,
          .tls_allowed_cn = context.value->tls_allowed_cn,
          .connection_from_director_to_client
          = context.value->connection_from_director_to_client,
          .connection_from_client_to_director
          = context.value->connection_from_client_to_director,
          .maximum_concurrent_jobs = context.value->maximum_concurrent_jobs,
          .heartbeat_interval = context.value->heartbeat_interval,
          .maximum_bandwidth_per_job = context.value->maximum_bandwidth_per_job,
          .description = context.value->description,
      }};
}

OperationResult<DirectorStorageResourceSpec>
ServiceState::GetDirectorStorageResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view storage_name) const
{
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorStorageWriteContext(*director_config.value, storage_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director storage '" + std::string{storage_name}
                     + "' not found."};
  }

  return {
      .value = DirectorStorageResourceSpec{
          .address = context.value->address,
          .lan_address = context.value->lan_address,
          .port = ToOptionalUint16(context.value->port),
          .protocol = context.value->protocol,
          .auth_type = context.value->auth_type,
          .username = context.value->username,
          .password = context.value->password,
          .device = context.value->device,
          .media_type = context.value->media_type,
          .autochanger = context.value->autochanger,
          .enabled = context.value->enabled,
          .allow_compression = context.value->allow_compression,
          .heartbeat_interval = context.value->heartbeat_interval,
          .cache_status_interval = context.value->cache_status_interval,
          .maximum_concurrent_jobs = context.value->maximum_concurrent_jobs,
          .maximum_concurrent_read_jobs
          = context.value->maximum_concurrent_read_jobs,
          .paired_storage = context.value->paired_storage,
          .maximum_bandwidth_per_job = context.value->maximum_bandwidth_per_job,
          .collect_statistics = context.value->collect_statistics,
          .ndmp_changer_device = context.value->ndmp_changer_device,
          .tls_authenticate = context.value->tls_authenticate,
          .tls_enable = context.value->tls_enable,
          .tls_require = context.value->tls_require,
          .tls_verify_peer = context.value->tls_verify_peer,
          .tls_cipher_list = context.value->tls_cipher_list,
          .tls_cipher_suites = context.value->tls_cipher_suites,
          .tls_dh_file = context.value->tls_dh_file,
          .tls_protocol = context.value->tls_protocol,
          .tls_ca_certificate_file = context.value->tls_ca_certificate_file,
          .tls_ca_certificate_dir = context.value->tls_ca_certificate_dir,
          .tls_certificate_revocation_list
          = context.value->tls_certificate_revocation_list,
          .tls_certificate = context.value->tls_certificate,
          .tls_key = context.value->tls_key,
          .tls_allowed_cn = context.value->tls_allowed_cn,
          .description = context.value->description,
      }};
}

OperationResult<DirectorConsoleResourceSpec>
ServiceState::GetDirectorConsoleResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view console_name) const
{
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorConsoleWriteContext(*director_config.value, console_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director console '" + std::string{console_name}
                     + "' not found."};
  }

  DirectorConsoleResourceSpec spec;
  spec.password = context.value->password;
  spec.description = context.value->description;
  FillDirectorAclSpec(spec, context.value->acl);
  spec.profiles = ToOptionalStringVector(context.value->profiles);
  spec.use_pam_authentication = context.value->use_pam_authentication;
  spec.tls_authenticate = context.value->tls_authenticate;
  spec.tls_enable = context.value->tls_enable;
  spec.tls_require = context.value->tls_require;
  spec.tls_verify_peer = context.value->tls_verify_peer;
  spec.tls_cipher_list = context.value->tls_cipher_list;
  spec.tls_cipher_suites = context.value->tls_cipher_suites;
  spec.tls_dh_file = context.value->tls_dh_file;
  spec.tls_protocol = context.value->tls_protocol;
  spec.tls_ca_certificate_file = context.value->tls_ca_certificate_file;
  spec.tls_ca_certificate_dir = context.value->tls_ca_certificate_dir;
  spec.tls_certificate_revocation_list
      = context.value->tls_certificate_revocation_list;
  spec.tls_certificate = context.value->tls_certificate;
  spec.tls_key = context.value->tls_key;
  spec.tls_allowed_cn = context.value->tls_allowed_cn;
  return {.value = std::move(spec)};
}

OperationResult<StorageDirectorResourceSpec>
ServiceState::GetStorageDirectorResourceSpec(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view director_name) const
{
  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageDaemonDirectorWriteContext(
      *storage_config.value, director_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage-daemon director '" + std::string{director_name}
                     + "' not found."};
  }

  return {
      .value = StorageDirectorResourceSpec{
          .password = context.value->password,
          .description = context.value->description,
          .monitor = context.value->monitor,
          .maximum_bandwidth_per_job = context.value->maximum_bandwidth_per_job,
          .key_encryption_key = context.value->key_encryption_key,
          .tls_authenticate = context.value->tls_authenticate,
          .tls_enable = context.value->tls_enable,
          .tls_require = context.value->tls_require,
          .tls_verify_peer = context.value->tls_verify_peer,
          .tls_cipher_list = context.value->tls_cipher_list,
          .tls_cipher_suites = context.value->tls_cipher_suites,
          .tls_dh_file = context.value->tls_dh_file,
          .tls_protocol = context.value->tls_protocol,
          .tls_ca_certificate_file = context.value->tls_ca_certificate_file,
          .tls_ca_certificate_dir = context.value->tls_ca_certificate_dir,
          .tls_certificate_revocation_list
          = context.value->tls_certificate_revocation_list,
          .tls_certificate = context.value->tls_certificate,
          .tls_key = context.value->tls_key,
          .tls_allowed_cn = context.value->tls_allowed_cn,
      }};
}

OperationResult<StorageDeviceResourceSpec>
ServiceState::GetStorageDeviceResourceSpec(std::string_view deployment_id,
                                           std::string_view storage_name,
                                           std::string_view device_name) const
{
  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageDaemonDeviceWriteContext(
      *storage_config.value, device_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage-daemon device '" + std::string{device_name}
                     + "' not found."};
  }

  return {.value = ToStorageDeviceResourceSpec(*context.value)};
}

OperationResult<StorageNdmpResourceSpec>
ServiceState::GetStorageNdmpResourceSpec(std::string_view deployment_id,
                                         std::string_view storage_name,
                                         std::string_view ndmp_name) const
{
  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageNdmpWriteContext(*storage_config.value, ndmp_name,
                                             *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage-daemon NDMP resource '" + std::string{ndmp_name}
                     + "' not found."};
  }

  return {.value = ToStorageNdmpResourceSpec(*context.value)};
}

OperationResult<StorageAutochangerResourceSpec>
ServiceState::GetStorageAutochangerResourceSpec(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view autochanger_name) const
{
  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageAutochangerWriteContext(
      *storage_config.value, autochanger_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage-daemon autochanger '"
                     + std::string{autochanger_name} + "' not found."};
  }

  return {.value = ToStorageAutochangerResourceSpec(*context.value)};
}

OperationResult<ClientDaemonResourceSpec>
ServiceState::GetClientDaemonResourceSpec(std::string_view deployment_id,
                                          std::string_view client_name) const
{
  auto client_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) { return {.error = client_config.error}; }

  auto context = LoadClientDaemonWriteContext(*client_config.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "client-daemon resource '" + std::string{client_name}
                     + "' not found."};
  }

  return {.value = ToClientDaemonResourceSpec(context.value->content)};
}

OperationResult<DirectorDaemonResourceSpec>
ServiceState::GetDirectorDaemonResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name) const
{
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context = LoadDirectorDaemonWriteContext(*director_config.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director-daemon resource '" + std::string{director_name}
                     + "' not found."};
  }

  return {.value = ToDirectorDaemonResourceSpec(context.value->password,
                                                context.value->content)};
}

OperationResult<StorageDaemonResourceSpec>
ServiceState::GetStorageDaemonResourceSpec(std::string_view deployment_id,
                                           std::string_view storage_name) const
{
  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  auto context = LoadStorageDaemonWriteContext(*storage_config.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage-daemon resource '" + std::string{storage_name}
                     + "' not found."};
  }

  return {.value = ToStorageDaemonResourceSpec(context.value->content)};
}

OperationResult<ClientMessagesResourceSpec>
ServiceState::GetClientMessagesResourceSpec(
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view messages_name) const
{
  auto client_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) { return {.error = client_config.error}; }

  auto context
      = LoadClientMessagesWriteContext(*client_config.value, messages_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "client messages '" + std::string{messages_name}
                     + "' not found."};
  }

  return {.value = ToMessagesResourceSpec<ClientMessagesResourceSpec>(
              context.value->content)};
}

OperationResult<DirectorMessagesResourceSpec>
ServiceState::GetDirectorMessagesResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view messages_name) const
{
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorMessagesWriteContext(*director_config.value, messages_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director messages '" + std::string{messages_name}
                     + "' not found."};
  }

  return {.value = ToMessagesResourceSpec<DirectorMessagesResourceSpec>(
              context.value->content)};
}

OperationResult<StorageMessagesResourceSpec>
ServiceState::GetStorageMessagesResourceSpec(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view messages_name) const
{
  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  auto context
      = LoadStorageMessagesWriteContext(*storage_config.value, messages_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage messages '" + std::string{messages_name}
                     + "' not found."};
  }

  return {.value = ToMessagesResourceSpec<StorageMessagesResourceSpec>(
              context.value->content)};
}

OperationResult<DirectorUserResourceSpec>
ServiceState::GetDirectorUserResourceSpec(std::string_view deployment_id,
                                          std::string_view director_name,
                                          std::string_view user_name) const
{
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorUserWriteContext(*director_config.value, user_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error
            = "director user '" + std::string{user_name} + "' not found."};
  }

  return {.value = ToDirectorUserResourceSpec(*context.value)};
}

OperationResult<DirectorProfileResourceSpec>
ServiceState::GetDirectorProfileResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view profile_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorProfileWriteContext(*director_config.value, profile_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director profile '" + std::string{profile_name}
                     + "' not found."};
  }

  return {.value = ToDirectorProfileResourceSpec(*context.value)};
}

OperationResult<DirectorPoolResourceSpec>
ServiceState::GetDirectorPoolResourceSpec(std::string_view deployment_id,
                                          std::string_view director_name,
                                          std::string_view pool_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorPoolWriteContext(*director_config.value, pool_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error
            = "director pool '" + std::string{pool_name} + "' not found."};
  }

  return {.value = ToDirectorPoolResourceSpec(context.value->content)};
}

OperationResult<DirectorCatalogResourceSpec>
ServiceState::GetDirectorCatalogResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view catalog_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorCatalogWriteContext(*director_config.value, catalog_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director catalog '" + std::string{catalog_name}
                     + "' not found."};
  }

  return {.value = ToDirectorCatalogResourceSpec(context.value->content)};
}

OperationResult<DirectorScheduleResourceSpec>
ServiceState::GetDirectorScheduleResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view schedule_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorScheduleWriteContext(*director_config.value, schedule_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director schedule '" + std::string{schedule_name}
                     + "' not found."};
  }

  return {.value = DirectorScheduleResourceSpec{
              .description = context.value->content.description,
              .enabled = context.value->content.enabled,
              .run_entries = context.value->content.run_entries,
          }};
}

OperationResult<DirectorCounterResourceSpec>
ServiceState::GetDirectorCounterResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view counter_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorCounterWriteContext(*director_config.value, counter_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director counter '" + std::string{counter_name}
                     + "' not found."};
  }

  return {.value = ToDirectorCounterResourceSpec(context.value->content)};
}

OperationResult<DirectorFilesetResourceSpec>
ServiceState::GetDirectorFilesetResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view fileset_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorFilesetWriteContext(*director_config.value, fileset_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director fileset '" + std::string{fileset_name}
                     + "' not found."};
  }

  return {.value = ToDirectorFilesetResourceSpec(context.value->content)};
}

OperationResult<DirectorJobResourceSpec>
ServiceState::GetDirectorJobResourceSpec(std::string_view deployment_id,
                                         std::string_view director_name,
                                         std::string_view job_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context = LoadDirectorJobWriteContext(*director_config.value, job_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director job '" + std::string{job_name} + "' not found."};
  }

  return {.value = ToDirectorJobLikeResourceSpec<DirectorJobResourceSpec>(
              context.value->content)};
}

OperationResult<DirectorJobDefsResourceSpec>
ServiceState::GetDirectorJobDefsResourceSpec(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view jobdefs_name) const
{
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorJobDefsWriteContext(*director_config.value, jobdefs_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director jobdefs '" + std::string{jobdefs_name}
                     + "' not found."};
  }

  return {.value = ToDirectorJobLikeResourceSpec<DirectorJobDefsResourceSpec>(
              context.value->content)};
}

OperationResult<DeploymentConfigRecord> ServiceState::UpsertClientDirectorStub(
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view director_name,
    const ClientDirectorStubSpec& spec) const
{
  DebugLog("upserting client stub for deployment '" + std::string{deployment_id}
           + "', client '" + std::string{client_name} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(client_name) || !IsSafePathSegment(director_name)) {
    return {.error = "client and director names must be safe path segments."};
  }

  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }
  auto password = LoadClientPasswordFromDirectorConfig(*director_config.value,
                                                       client_name);
  if (!password) { return {.error = password.error}; }
  DebugLog("resolved director-side password for client '"
           + std::string{client_name} + "' from director '"
           + std::string{director_name} + "'");

  const auto client_root
      = RepositoryLayout::ClientsDirectory(repository_path) / client_name;
  auto context = LoadClientDirectorStubWriteContext(client_root, director_name);
  if (!context) { return {.error = context.error}; }

  const auto stub_path = context.value->file_path;
  const auto stub_directory = stub_path.parent_path();
  const bool client_root_existed = std::filesystem::exists(client_root);
  const bool stub_existed = std::filesystem::exists(stub_path);

  std::string original_content;
  if (stub_existed) {
    auto existing = ReadFile(stub_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultClientDirectorStubDescription(
                                         client_name, director_name));
  const auto stub_password = spec.password ? *spec.password : *password.value;
  const auto address = spec.address ? spec.address : context.value->address;
  const auto port
      = spec.port ? std::optional<uint32_t>{*spec.port} : context.value->port;
  const auto allowed_script_dirs = spec.allowed_script_dirs
                                       ? spec.allowed_script_dirs
                                       : context.value->allowed_script_dirs;
  const auto allowed_job_commands = spec.allowed_job_commands
                                        ? spec.allowed_job_commands
                                        : context.value->allowed_job_commands;
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  const auto connection_from_director_to_client
      = spec.connection_from_director_to_client
            ? spec.connection_from_director_to_client
            : context.value->connection_from_director_to_client;
  const auto connection_from_client_to_director
      = spec.connection_from_client_to_director
            ? spec.connection_from_client_to_director
            : context.value->connection_from_client_to_director;
  const auto monitor = spec.monitor ? spec.monitor : context.value->monitor;
  const auto maximum_bandwidth_per_job
      = spec.maximum_bandwidth_per_job
            ? spec.maximum_bandwidth_per_job
            : context.value->maximum_bandwidth_per_job;
  const auto stub_content = BuildClientDirectorStubContent(
      director_name, stub_password, description, address, port,
      allowed_script_dirs, allowed_job_commands, tls_authenticate, tls_enable,
      tls_require, tls_verify_peer, tls_cipher_list, tls_cipher_suites,
      tls_dh_file, tls_protocol, tls_ca_certificate_file,
      tls_ca_certificate_dir, tls_certificate_revocation_list, tls_certificate,
      tls_key, tls_allowed_cn, connection_from_director_to_client,
      connection_from_client_to_director, monitor, maximum_bandwidth_per_job);

  std::error_code error_code;
  std::filesystem::create_directories(stub_directory, error_code);
  if (error_code) {
    return {.error = "failed to create stub directory '"
                     + stub_directory.string() + "': " + error_code.message()};
  }
  std::string file_content = std::string{stub_content};
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(stub_path, "Director",
                                                  director_name, stub_content);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(stub_path, file_content)) {
    return {.error
            = "failed to write client stub '" + stub_path.string() + "'."};
  }
  DebugLog("wrote client stub file '" + stub_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_root.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("client stub update for '" + std::string{client_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (stub_existed) {
      rollback_error = RestoreClientStubFile(stub_path, original_content);
    } else {
      rollback_error = CleanupCreatedClientStub(client_root, stub_path,
                                                client_root_existed);
    }

    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("client stub parser initialization ",
                                          loaded.messages)};
    }
    return {.error
            = FormatParseFailure("client stub update ", loaded.messages)};
  }

  DebugLog("updated client stub for '" + std::string{client_name}
           + "' against director '" + std::string{director_name} + "'");
  return {.value
          = DeploymentConfigRecord{.component = bconfig::Component::kClient,
                                   .name = std::string{client_name},
                                   .path = client_root}};
}

OperationResult<std::optional<DeploymentConfigRecord>>
ServiceState::DeleteClientDirectorStub(std::string_view deployment_id,
                                       std::string_view client_name,
                                       std::string_view director_name) const
{
  DebugLog("deleting client stub for deployment '" + std::string{deployment_id}
           + "', client '" + std::string{client_name} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(client_name) || !IsSafePathSegment(director_name)) {
    return {.error = "client and director names must be safe path segments."};
  }

  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  const auto client_root
      = RepositoryLayout::ClientsDirectory(repository_path) / client_name;
  auto context = LoadClientDirectorStubWriteContext(client_root, director_name);
  if (!context) { return {.error = context.error}; }

  const auto stub_path = context.value->file_path;
  if (!std::filesystem::exists(stub_path)) {
    return {.error = "client '" + std::string{client_name}
                     + "' does not define director '"
                     + std::string{director_name} + "'."};
  }

  if (context.value->is_standalone_file) {
    auto deleted = DeleteClientDirectorStubFile(repository_path, client_name,
                                                director_name);
    if (!deleted) { return {.error = deleted.error}; }
  } else {
    auto original_content = ReadFile(stub_path);
    if (!original_content) { return {.error = original_content.error}; }
    auto rewritten = RewriteNamedTopLevelResource(stub_path, "Director",
                                                  director_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(stub_path, *rewritten.value)) {
      return {.error = "failed to update shared client stub '"
                       + stub_path.string() + "'."};
    }

    auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                      client_root.string(), true);
    if (!loaded.parser || !loaded.parse_ok) {
      auto rollback_error
          = RestoreDeletedFile(stub_path, *original_content.value);
      if (rollback_error) { return {.error = *rollback_error}; }
      if (!loaded.parser) {
        return {.error = FormatParseFailure(
                    "client stub parser initialization ", loaded.messages)};
      }
      return {.error
              = FormatParseFailure("client stub delete ", loaded.messages)};
    }
  }

  if (!std::filesystem::exists(client_root / "bareos-fd.d")) {
    DebugLog("deleted client stub '" + std::string{director_name}
             + "' from client '" + std::string{client_name}
             + "' and removed the now-empty client config root");
    return {.value = std::optional<DeploymentConfigRecord>{}};
  }

  DebugLog("deleted client stub '" + std::string{director_name}
           + "' from client '" + std::string{client_name} + "'");
  return {.value
          = DeploymentConfigRecord{.component = bconfig::Component::kClient,
                                   .name = std::string{client_name},
                                   .path = client_root}};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertClientMessagesResource(
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view messages_name,
    const ClientMessagesResourceSpec& spec) const
{
  DebugLog("upserting client messages resource for deployment '"
           + std::string{deployment_id} + "', client '"
           + std::string{client_name} + "', messages '"
           + std::string{messages_name} + "'");
  if (!IsSafePathSegment(client_name) || !IsSafePathSegment(messages_name)) {
    return {.error = "client and messages names must be safe path segments."};
  }

  auto client_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) { return {.error = client_config.error}; }

  auto context
      = LoadClientMessagesWriteContext(*client_config.value, messages_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description = spec.description
                            ? *spec.description
                            : content.description.value_or(
                                  "Managed client messages resource for "
                                  + std::string{messages_name} + " in client "
                                  + std::string{client_name});
  if (spec.mail_command) { content.mail_command = *spec.mail_command; }
  if (spec.operator_command) {
    content.operator_command = *spec.operator_command;
  }
  if (spec.timestamp_format) {
    content.timestamp_format = *spec.timestamp_format;
  }
  if (spec.syslog_entries) { content.syslog_entries = *spec.syslog_entries; }
  if (spec.mail_entries) { content.mail_entries = *spec.mail_entries; }
  if (spec.mail_on_error_entries) {
    content.mail_on_error_entries = *spec.mail_on_error_entries;
  }
  if (spec.mail_on_success_entries) {
    content.mail_on_success_entries = *spec.mail_on_success_entries;
  }
  if (spec.file_entries) { content.file_entries = *spec.file_entries; }
  if (spec.append_entries) { content.append_entries = *spec.append_entries; }
  if (spec.stdout_entries) { content.stdout_entries = *spec.stdout_entries; }
  if (spec.stderr_entries) { content.stderr_entries = *spec.stderr_entries; }
  if (spec.director_entries) {
    content.director_entries = *spec.director_entries;
  }
  if (spec.console_entries) { content.console_entries = *spec.console_entries; }
  if (spec.operator_entries) {
    content.operator_entries = *spec.operator_entries;
  }
  if (spec.catalog_entries) { content.catalog_entries = *spec.catalog_entries; }
  if (spec.entries) { content.entries = *spec.entries; }

  const auto rendered
      = BuildDirectorMessagesResourceContent(messages_name, content);
  const auto resource_directory
      = client_config.value->path / "bareos-fd.d" / "messages";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create client messages directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Messages", messages_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write client messages resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote client messages file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("client messages update for '" + std::string{messages_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          client_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "client messages parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("client messages update ", loaded.messages)};
  }

  DebugLog("updated client messages resource '" + std::string{messages_name}
           + "' in client '" + std::string{client_name} + "'");
  return {.value = *client_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteClientMessagesResource(std::string_view deployment_id,
                                           std::string_view client_name,
                                           std::string_view messages_name) const
{
  DebugLog("deleting client messages resource for deployment '"
           + std::string{deployment_id} + "', client '"
           + std::string{client_name} + "', messages '"
           + std::string{messages_name} + "'");
  if (!IsSafePathSegment(client_name) || !IsSafePathSegment(messages_name)) {
    return {.error = "client and messages names must be safe path segments."};
  }

  auto client_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) {
    return {.error = "client config not found for '" + std::string{client_name}
                     + "'."};
  }

  auto context
      = LoadClientMessagesWriteContext(*client_config.value, messages_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "client '" + std::string{client_name}
                     + "' does not define messages '"
                     + std::string{messages_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               client_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Messages", messages_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error = "failed to update shared client messages resource file '"
                       + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "client messages parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("client messages delete ", loaded.messages)};
  }

  DebugLog("deleted client messages resource '" + std::string{messages_name}
           + "' from client '" + std::string{client_name} + "'");
  return {.value = *client_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertClientDaemonResource(
    std::string_view deployment_id,
    std::string_view client_name,
    const ClientDaemonResourceSpec& spec) const
{
  DebugLog("upserting client daemon resource for deployment '"
           + std::string{deployment_id} + "', client '"
           + std::string{client_name} + "'");
  if (!IsSafePathSegment(client_name)) {
    return {.error = "client name must be a safe path segment."};
  }
  if (spec.address && !IsSafeBareosToken(*spec.address)) {
    return {.error
            = "client daemon address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  if (spec.source_address && !IsSafeBareosToken(*spec.source_address)) {
    return {.error
            = "client daemon source_address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  if (spec.source_addresses) {
    for (const auto& value : *spec.source_addresses) {
      if (!IsSafeBareosToken(value)) {
        return {.error
                = "client daemon source_addresses entries must be bare "
                  "Bareos tokens without whitespace or quotes."};
      }
    }
  }
  if (spec.pki_cipher && !IsSafeBareosToken(*spec.pki_cipher)) {
    return {.error
            = "client daemon pki_cipher must be a bare Bareos token without "
              "whitespace or quotes."};
  }
  if (spec.port && *spec.port == 0) {
    return {.error = "client daemon port must be greater than zero."};
  }
  auto client_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) {
    return {.error = "client config not found for '" + std::string{client_name}
                     + "'."};
  }

  auto context = LoadClientDaemonWriteContext(*client_config.value);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  if (spec.address) { content.address = spec.address; }
  if (spec.addresses) {
    content.addresses = *spec.addresses;
    content.address.reset();
    content.port.reset();
  }
  if (spec.source_addresses) {
    content.source_addresses = *spec.source_addresses;
    content.source_address.reset();
  } else if (spec.source_address) {
    content.source_address = spec.source_address;
    content.source_addresses.clear();
  }
  if (spec.port) {
    content.port = spec.port;
    content.addresses.clear();
  }
  if (spec.maximum_concurrent_jobs) {
    content.maximum_concurrent_jobs = spec.maximum_concurrent_jobs;
  }
  if (spec.maximum_workers_per_job) {
    content.maximum_workers_per_job = spec.maximum_workers_per_job;
  }
  if (spec.absolute_job_timeout) {
    content.absolute_job_timeout = spec.absolute_job_timeout;
  }
  if (spec.allow_bandwidth_bursting) {
    content.allow_bandwidth_bursting = spec.allow_bandwidth_bursting;
  }
  if (spec.tls_authenticate) {
    content.tls_authenticate = spec.tls_authenticate;
  }
  if (spec.tls_enable) { content.tls_enable = spec.tls_enable; }
  if (spec.tls_require) { content.tls_require = spec.tls_require; }
  if (spec.tls_verify_peer) { content.tls_verify_peer = spec.tls_verify_peer; }
  if (spec.tls_cipher_list) { content.tls_cipher_list = spec.tls_cipher_list; }
  if (spec.tls_cipher_suites) {
    content.tls_cipher_suites = spec.tls_cipher_suites;
  }
  if (spec.tls_dh_file) { content.tls_dh_file = spec.tls_dh_file; }
  if (spec.tls_protocol) { content.tls_protocol = spec.tls_protocol; }
  if (spec.tls_ca_certificate_file) {
    content.tls_ca_certificate_file = spec.tls_ca_certificate_file;
  }
  if (spec.tls_ca_certificate_dir) {
    content.tls_ca_certificate_dir = spec.tls_ca_certificate_dir;
  }
  if (spec.tls_certificate_revocation_list) {
    content.tls_certificate_revocation_list
        = spec.tls_certificate_revocation_list;
  }
  if (spec.tls_certificate) { content.tls_certificate = spec.tls_certificate; }
  if (spec.tls_key) { content.tls_key = spec.tls_key; }
  if (spec.tls_allowed_cn) { content.tls_allowed_cn = *spec.tls_allowed_cn; }
  if (spec.pki_signatures) { content.pki_signatures = spec.pki_signatures; }
  if (spec.pki_encryption) { content.pki_encryption = spec.pki_encryption; }
  if (spec.pki_key_pair) { content.pki_key_pair = spec.pki_key_pair; }
  if (spec.pki_signers) { content.pki_signers = *spec.pki_signers; }
  if (spec.pki_master_keys) { content.pki_master_keys = *spec.pki_master_keys; }
  if (spec.pki_cipher) { content.pki_cipher = spec.pki_cipher; }
  if (spec.always_use_lmdb) { content.always_use_lmdb = spec.always_use_lmdb; }
  if (spec.lmdb_threshold) { content.lmdb_threshold = spec.lmdb_threshold; }
  if (spec.ver_id) { content.ver_id = spec.ver_id; }
  if (spec.log_timestamp_format) {
    content.log_timestamp_format = spec.log_timestamp_format;
  }
  if (spec.maximum_bandwidth_per_job) {
    content.maximum_bandwidth_per_job = spec.maximum_bandwidth_per_job;
  }
  if (spec.secure_erase_command) {
    content.secure_erase_command = spec.secure_erase_command;
  }
  if (spec.grpc_module) { content.grpc_module = spec.grpc_module; }
  if (spec.enable_ktls) { content.enable_ktls = spec.enable_ktls; }
  if (spec.sd_connect_timeout) {
    content.sd_connect_timeout = spec.sd_connect_timeout;
  }
  if (spec.heartbeat_interval) {
    content.heartbeat_interval = spec.heartbeat_interval;
  }
  if (spec.maximum_network_buffer_size) {
    content.maximum_network_buffer_size = spec.maximum_network_buffer_size;
  }
  if (spec.description) { content.description = spec.description; }
  if (spec.working_directory) {
    content.working_directory = spec.working_directory;
  }
  if (spec.plugin_directory) {
    content.plugin_directory = spec.plugin_directory;
  }
  if (spec.plugin_names) { content.plugin_names = *spec.plugin_names; }
  if (spec.allowed_script_dirs) {
    content.allowed_script_dirs = *spec.allowed_script_dirs;
  }
  if (spec.allowed_job_commands) {
    content.allowed_job_commands = *spec.allowed_job_commands;
  }
  if (spec.scripts_directory) {
    content.scripts_directory = spec.scripts_directory;
  }
  if (spec.messages) { content.messages = spec.messages; }

  std::string render_error;
  const auto rendered
      = BuildClientDaemonResourceContent(client_name, content, &render_error);
  if (rendered.empty()) { return {.error = std::move(render_error)}; }
  const auto resource_directory
      = client_config.value->path / "bareos-fd.d" / "client";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create client daemon directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Client", client_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write client daemon resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote client daemon file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("client daemon update for '" + std::string{client_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          client_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "client daemon parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("client daemon update ", loaded.messages)};
  }

  DebugLog("updated client daemon resource '" + std::string{client_name} + "'");
  return {.value = *client_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorDaemonResource(
    std::string_view deployment_id,
    std::string_view director_name,
    const DirectorDaemonResourceSpec& spec) const
{
  DebugLog("upserting director daemon resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(director_name)) {
    return {.error = "director name must be a safe path segment."};
  }
  if (spec.address && !IsSafeBareosToken(*spec.address)) {
    return {.error
            = "director daemon address must be a bare Bareos token without "
              "whitespace or quotes."};
  }
  if (spec.source_address && !IsSafeBareosToken(*spec.source_address)) {
    return {.error
            = "director daemon source_address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  if (spec.source_addresses) {
    for (const auto& value : *spec.source_addresses) {
      if (!IsSafeBareosToken(value)) {
        return {.error
                = "director daemon source_addresses entries must be bare "
                  "Bareos tokens without whitespace or quotes."};
      }
    }
  }
  if (spec.port && *spec.port == 0) {
    return {.error = "director daemon port must be greater than zero."};
  }
  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context = LoadDirectorDaemonWriteContext(*director_config.value);
  if (!context) { return {.error = context.error}; }
  auto password = spec.password ? spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error = "director daemon resource '" + std::string{director_name}
                     + "' does not expose a managed password value."};
  }

  auto content = context.value->content;
  if (spec.address) { content.address = spec.address; }
  if (spec.addresses) {
    content.addresses = *spec.addresses;
    content.address.reset();
    content.port.reset();
  }
  if (spec.source_addresses) {
    content.source_addresses = *spec.source_addresses;
    content.source_address.reset();
  } else if (spec.source_address) {
    content.source_address = spec.source_address;
    content.source_addresses.clear();
  }
  if (spec.port) {
    content.port = spec.port;
    content.addresses.clear();
  }
  if (spec.query_file) { content.query_file = spec.query_file; }
  if (spec.subscriptions) { content.subscriptions = spec.subscriptions; }
  if (spec.maximum_concurrent_jobs) {
    content.maximum_concurrent_jobs = spec.maximum_concurrent_jobs;
  }
  if (spec.maximum_console_connections) {
    content.maximum_console_connections = spec.maximum_console_connections;
  }
  if (spec.absolute_job_timeout) {
    content.absolute_job_timeout = spec.absolute_job_timeout;
  }
  if (spec.tls_authenticate) {
    content.tls_authenticate = spec.tls_authenticate;
  }
  if (spec.tls_enable) { content.tls_enable = spec.tls_enable; }
  if (spec.tls_require) { content.tls_require = spec.tls_require; }
  if (spec.tls_verify_peer) { content.tls_verify_peer = spec.tls_verify_peer; }
  if (spec.tls_cipher_list) { content.tls_cipher_list = spec.tls_cipher_list; }
  if (spec.tls_cipher_suites) {
    content.tls_cipher_suites = spec.tls_cipher_suites;
  }
  if (spec.tls_dh_file) { content.tls_dh_file = spec.tls_dh_file; }
  if (spec.tls_protocol) { content.tls_protocol = spec.tls_protocol; }
  if (spec.tls_ca_certificate_file) {
    content.tls_ca_certificate_file = spec.tls_ca_certificate_file;
  }
  if (spec.tls_ca_certificate_dir) {
    content.tls_ca_certificate_dir = spec.tls_ca_certificate_dir;
  }
  if (spec.tls_certificate_revocation_list) {
    content.tls_certificate_revocation_list
        = spec.tls_certificate_revocation_list;
  }
  if (spec.tls_certificate) { content.tls_certificate = spec.tls_certificate; }
  if (spec.tls_key) { content.tls_key = spec.tls_key; }
  if (spec.tls_allowed_cn) { content.tls_allowed_cn = *spec.tls_allowed_cn; }
  if (spec.ver_id) { content.ver_id = spec.ver_id; }
  if (spec.log_timestamp_format) {
    content.log_timestamp_format = spec.log_timestamp_format;
  }
  if (spec.secure_erase_command) {
    content.secure_erase_command = spec.secure_erase_command;
  }
  if (spec.enable_ktls) { content.enable_ktls = spec.enable_ktls; }
  if (spec.fd_connect_timeout) {
    content.fd_connect_timeout = spec.fd_connect_timeout;
  }
  if (spec.sd_connect_timeout) {
    content.sd_connect_timeout = spec.sd_connect_timeout;
  }
  if (spec.heartbeat_interval) {
    content.heartbeat_interval = spec.heartbeat_interval;
  }
  if (spec.statistics_retention) {
    content.statistics_retention = spec.statistics_retention;
  }
  if (spec.statistics_collect_interval) {
    content.statistics_collect_interval = spec.statistics_collect_interval;
  }
  if (spec.description) { content.description = spec.description; }
  if (spec.key_encryption_key) {
    content.key_encryption_key = spec.key_encryption_key;
  }
  if (spec.ndmp_snooping) { content.ndmp_snooping = spec.ndmp_snooping; }
  if (spec.ndmp_log_level) { content.ndmp_log_level = spec.ndmp_log_level; }
  if (spec.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad) {
    content.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
        = spec.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad;
  }
  if (spec.auditing) { content.auditing = spec.auditing; }
  if (spec.audit_events) { content.audit_events = *spec.audit_events; }
  if (spec.working_directory) {
    content.working_directory = spec.working_directory;
  }
  if (spec.plugin_directory) {
    content.plugin_directory = spec.plugin_directory;
  }
  if (spec.plugin_names) { content.plugin_names = *spec.plugin_names; }
  if (spec.scripts_directory) {
    content.scripts_directory = spec.scripts_directory;
  }
  if (spec.messages) { content.messages = spec.messages; }

  std::string render_error;
  const auto rendered = BuildDirectorDaemonResourceContent(
      director_name, *password, content, &render_error);
  if (rendered.empty()) { return {.error = std::move(render_error)}; }
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "director";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director daemon directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Director", director_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director daemon resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director daemon file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director daemon update for '" + std::string{director_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director daemon parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director daemon update ", loaded.messages)};
  }

  DebugLog("updated director daemon resource '" + std::string{director_name}
           + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorClientResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view client_name,
    const DirectorClientResourceSpec& spec) const
{
  DebugLog("upserting director client resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', client '"
           + std::string{client_name} + "'");
  if (!IsSafePathSegment(client_name) || !IsSafePathSegment(director_name)) {
    return {.error = "client and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorClientWriteContext(*director_config.value, client_name);
  if (!context) { return {.error = context.error}; }

  const auto address = spec.address ? *spec.address : context.value->address;
  if (!address || address->empty()) {
    return {.error
            = "field 'address' is required when creating a director "
              "client resource."};
  }
  if (!IsSafeBareosToken(*address)) {
    return {.error
            = "director client address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  const auto lan_address
      = spec.lan_address ? spec.lan_address : context.value->lan_address;
  std::optional<std::string> protocol = context.value->protocol;
  if (spec.protocol) {
    auto normalized_protocol = NormalizeDirectorClientProtocol(*spec.protocol);
    if (!normalized_protocol) { return {.error = normalized_protocol.error}; }
    protocol = *normalized_protocol.value;
  }
  std::optional<std::string> auth_type = context.value->auth_type;
  if (spec.auth_type) {
    auto normalized_auth_type = NormalizeAuthType(*spec.auth_type);
    if (!normalized_auth_type) { return {.error = normalized_auth_type.error}; }
    auth_type = *normalized_auth_type.value;
  }
  const auto catalog = spec.catalog ? spec.catalog : context.value->catalog;
  const auto username = spec.username ? spec.username : context.value->username;

  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required when creating a director "
              "client resource."};
  }

  const auto port
      = spec.port ? std::optional<uint32_t>{*spec.port} : context.value->port;
  const auto effective_port
      = port.value_or(static_cast<uint32_t>(kDefaultFileDaemonPort));
  if (effective_port == 0) {
    return {.error = "director client port must be greater than zero."};
  }
  const auto heartbeat_interval = spec.heartbeat_interval
                                      ? spec.heartbeat_interval
                                      : context.value->heartbeat_interval;
  const auto connection_from_director_to_client
      = spec.connection_from_director_to_client
            ? spec.connection_from_director_to_client
            : context.value->connection_from_director_to_client;
  const auto connection_from_client_to_director
      = spec.connection_from_client_to_director
            ? spec.connection_from_client_to_director
            : context.value->connection_from_client_to_director;
  const auto enabled = spec.enabled ? spec.enabled : context.value->enabled;
  const auto passive = spec.passive ? spec.passive : context.value->passive;
  const auto strict_quotas
      = spec.strict_quotas ? spec.strict_quotas : context.value->strict_quotas;
  const auto quota_include_failed_jobs
      = spec.quota_include_failed_jobs
            ? spec.quota_include_failed_jobs
            : context.value->quota_include_failed_jobs;
  const auto soft_quota
      = spec.soft_quota ? spec.soft_quota : context.value->soft_quota;
  const auto hard_quota
      = spec.hard_quota ? spec.hard_quota : context.value->hard_quota;
  const auto soft_quota_grace_period
      = spec.soft_quota_grace_period ? spec.soft_quota_grace_period
                                     : context.value->soft_quota_grace_period;
  const auto file_retention = spec.file_retention
                                  ? spec.file_retention
                                  : context.value->file_retention;
  const auto job_retention
      = spec.job_retention ? spec.job_retention : context.value->job_retention;
  const auto ndmp_log_level = spec.ndmp_log_level
                                  ? spec.ndmp_log_level
                                  : context.value->ndmp_log_level;
  const auto ndmp_block_size = spec.ndmp_block_size
                                   ? spec.ndmp_block_size
                                   : context.value->ndmp_block_size;
  const auto ndmp_use_lmdb
      = spec.ndmp_use_lmdb ? spec.ndmp_use_lmdb : context.value->ndmp_use_lmdb;
  const auto auto_prune
      = spec.auto_prune ? spec.auto_prune : context.value->auto_prune;
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  const auto maximum_concurrent_jobs
      = spec.maximum_concurrent_jobs ? spec.maximum_concurrent_jobs
                                     : context.value->maximum_concurrent_jobs;
  const auto maximum_bandwidth_per_job
      = spec.maximum_bandwidth_per_job
            ? spec.maximum_bandwidth_per_job
            : context.value->maximum_bandwidth_per_job;

  const auto description
      = spec.description
            ? *spec.description
            : context.value->description.value_or(
                  DefaultDirectorClientDescription(client_name, director_name));
  const auto content = BuildDirectorClientResourceContent(
      client_name, *address, lan_address, protocol, auth_type, catalog,
      username, *password, effective_port, enabled, passive, strict_quotas,
      quota_include_failed_jobs, soft_quota, hard_quota,
      soft_quota_grace_period, file_retention, job_retention, ndmp_log_level,
      ndmp_block_size, ndmp_use_lmdb, auto_prune, tls_authenticate, tls_enable,
      tls_require, tls_verify_peer, tls_cipher_list, tls_cipher_suites,
      tls_dh_file, tls_protocol, tls_ca_certificate_file,
      tls_ca_certificate_dir, tls_certificate_revocation_list, tls_certificate,
      tls_key, tls_allowed_cn, connection_from_director_to_client,
      connection_from_client_to_director, maximum_concurrent_jobs,
      heartbeat_interval, maximum_bandwidth_per_job, description);

  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "client";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director client directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = content;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Client", client_name, content);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director client resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director client file '" + context.value->file_path.string()
           + "'");

  {
    auto loaded
        = bconfig::LoadConfig(bconfig::Component::kDirector,
                              director_config.value->path.string(), true);
    if (!loaded.parser || !loaded.parse_ok) {
      DebugLog("director client update for '" + std::string{client_name}
               + "' failed validation and will be rolled back");
      std::optional<std::string> rollback_error;
      if (file_existed) {
        rollback_error
            = RestoreClientStubFile(context.value->file_path, original_content);
      } else {
        rollback_error = CleanupCreatedFile(context.value->file_path,
                                            director_config.value->path);
      }
      if (rollback_error) { return {.error = *rollback_error}; }
      if (!loaded.parser) {
        return {.error = FormatParseFailure(
                    "director client parser initialization ", loaded.messages)};
      }
      return {.error
              = FormatParseFailure("director client update ", loaded.messages)};
    }
  }

  auto synced_stub = UpsertClientDirectorStub(
      deployment_id, client_name, director_name,
      {.connection_from_director_to_client = connection_from_director_to_client,
       .connection_from_client_to_director = connection_from_client_to_director,
       .maximum_bandwidth_per_job = maximum_bandwidth_per_job});
  if (!synced_stub) {
    DebugLog("director client update for '" + std::string{client_name}
             + "' failed stub synchronization and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = synced_stub.error};
  }

  DebugLog("updated director client resource '" + std::string{client_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorStorageResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view storage_name,
    const DirectorStorageResourceSpec& spec) const
{
  DebugLog("upserting director storage resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', storage '"
           + std::string{storage_name} + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(director_name)) {
    return {.error = "storage and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorStorageWriteContext(*director_config.value, storage_name);
  if (!context) { return {.error = context.error}; }

  const auto address = spec.address ? *spec.address : context.value->address;
  if (!address || address->empty()) {
    return {.error
            = "field 'address' is required when creating a director "
              "storage resource."};
  }
  if (!IsSafeBareosToken(*address)) {
    return {.error
            = "director storage address must be a bare Bareos token "
              "without whitespace or quotes."};
  }

  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required when creating a director "
              "storage resource."};
  }

  const auto devices = [&]() {
    if (spec.device) { return std::vector<std::string>{*spec.device}; }
    if (!context.value->devices.empty()) { return context.value->devices; }
    if (context.value->device) {
      return std::vector<std::string>{*context.value->device};
    }
    return std::vector<std::string>{std::string{storage_name}};
  }();
  if (devices.empty()) {
    return {.error
            = "field 'device' is required when creating a director "
              "storage resource."};
  }
  for (const auto& device : devices) {
    if (device.empty()) {
      return {.error = "director storage '" + std::string{storage_name}
                       + "' has an empty Device value."};
    }
    if (!IsSafeBareosToken(device)) {
      return {.error
              = "director storage device must be a bare Bareos token "
                "without whitespace or quotes."};
    }
  }

  const auto media_type
      = spec.media_type ? *spec.media_type : context.value->media_type;
  if (!media_type || media_type->empty()) {
    return {.error
            = "field 'media_type' is required when creating a director "
              "storage resource."};
  }
  if (!IsSafeBareosToken(*media_type)) {
    return {.error
            = "director storage media_type must be a bare Bareos token "
              "without whitespace or quotes."};
  }

  const auto port
      = spec.port ? std::optional<uint32_t>{*spec.port} : context.value->port;
  const auto effective_port
      = port.value_or(static_cast<uint32_t>(kDefaultStorageDaemonPort));
  if (effective_port == 0) {
    return {.error = "director storage port must be greater than zero."};
  }

  std::optional<std::string> protocol = context.value->protocol;
  if (spec.protocol) {
    auto normalized_protocol = NormalizeDirectorClientProtocol(*spec.protocol);
    if (!normalized_protocol) { return {.error = normalized_protocol.error}; }
    protocol = *normalized_protocol.value;
  }
  std::optional<std::string> auth_type = context.value->auth_type;
  if (spec.auth_type) {
    auto normalized_auth_type = NormalizeAuthType(*spec.auth_type);
    if (!normalized_auth_type) { return {.error = normalized_auth_type.error}; }
    auth_type = *normalized_auth_type.value;
  }
  const auto lan_address
      = spec.lan_address ? spec.lan_address : context.value->lan_address;
  const auto username = spec.username ? spec.username : context.value->username;
  const auto autochanger
      = spec.autochanger ? spec.autochanger : context.value->autochanger;
  const auto heartbeat_interval = spec.heartbeat_interval
                                      ? spec.heartbeat_interval
                                      : context.value->heartbeat_interval;
  const auto cache_status_interval = spec.cache_status_interval
                                         ? spec.cache_status_interval
                                         : context.value->cache_status_interval;
  const auto maximum_concurrent_jobs
      = spec.maximum_concurrent_jobs ? spec.maximum_concurrent_jobs
                                     : context.value->maximum_concurrent_jobs;
  const auto maximum_concurrent_read_jobs
      = spec.maximum_concurrent_read_jobs
            ? spec.maximum_concurrent_read_jobs
            : context.value->maximum_concurrent_read_jobs;
  const auto paired_storage = spec.paired_storage
                                  ? spec.paired_storage
                                  : context.value->paired_storage;
  const auto enabled = spec.enabled ? spec.enabled : context.value->enabled;
  const auto allow_compression = spec.allow_compression
                                     ? spec.allow_compression
                                     : context.value->allow_compression;
  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultDirectorStorageDescription(
                                         storage_name, director_name));
  const auto maximum_bandwidth_per_job
      = spec.maximum_bandwidth_per_job
            ? spec.maximum_bandwidth_per_job
            : context.value->maximum_bandwidth_per_job;
  const auto collect_statistics = spec.collect_statistics
                                      ? spec.collect_statistics
                                      : context.value->collect_statistics;
  const auto ndmp_changer_device = spec.ndmp_changer_device
                                       ? spec.ndmp_changer_device
                                       : context.value->ndmp_changer_device;
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  if (devices.size() != 1 && (spec.archive_device || spec.device_type)) {
    return {
        .error
        = "automatic storage-daemon sync for director storage archive_device "
          "or device_type requires exactly one Device."};
  }
  if (paired_storage && !IsSafeBareosToken(*paired_storage)) {
    return {.error
            = "director storage paired_storage must be a bare Bareos "
              "token without whitespace or quotes."};
  }
  if (ndmp_changer_device && !IsSafeBareosToken(*ndmp_changer_device)) {
    return {.error
            = "director storage ndmp_changer_device must be a bare "
              "Bareos token without whitespace or quotes."};
  }
  const auto content = BuildDirectorStorageResourceContent(
      storage_name, *address, lan_address, protocol, auth_type, username,
      *password, devices, *media_type, effective_port, autochanger, enabled,
      allow_compression, heartbeat_interval, cache_status_interval,
      maximum_concurrent_jobs, maximum_concurrent_read_jobs, paired_storage,
      maximum_bandwidth_per_job, collect_statistics, ndmp_changer_device,
      tls_authenticate, tls_enable, tls_require, tls_verify_peer,
      tls_cipher_list, tls_cipher_suites, tls_dh_file, tls_protocol,
      tls_ca_certificate_file, tls_ca_certificate_dir,
      tls_certificate_revocation_list, tls_certificate, tls_key, tls_allowed_cn,
      description);

  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "storage";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director storage directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = content;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Storage", storage_name, content);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director storage resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director storage file '" + context.value->file_path.string()
           + "'");

  {
    auto loaded
        = bconfig::LoadConfig(bconfig::Component::kDirector,
                              director_config.value->path.string(), true);
    if (!loaded.parser || !loaded.parse_ok) {
      DebugLog("director storage update for '" + std::string{storage_name}
               + "' failed validation and will be rolled back");
      std::optional<std::string> rollback_error;
      if (file_existed) {
        rollback_error
            = RestoreClientStubFile(context.value->file_path, original_content);
      } else {
        rollback_error = CleanupCreatedFile(context.value->file_path,
                                            director_config.value->path);
      }
      if (rollback_error) { return {.error = *rollback_error}; }
      if (!loaded.parser) {
        return {.error
                = FormatParseFailure("director storage parser initialization ",
                                     loaded.messages)};
      }
      return {.error = FormatParseFailure("director storage update ",
                                          loaded.messages)};
    }
  }

  if (devices.size() == 1) {
    auto synced_storage = SyncStorageDaemonConfig(
        *this, deployment_id, director_name, storage_name, *context.value,
        *password, devices.front(), *media_type, maximum_bandwidth_per_job,
        spec.archive_device, spec.device_type);
    if (!synced_storage) {
      DebugLog("director storage update for '" + std::string{storage_name}
               + "' failed storage-daemon synchronization and will be rolled "
                 "back");
      std::optional<std::string> rollback_error;
      if (file_existed) {
        rollback_error
            = RestoreClientStubFile(context.value->file_path, original_content);
      } else {
        rollback_error = CleanupCreatedFile(context.value->file_path,
                                            director_config.value->path);
      }
      if (rollback_error) { return {.error = *rollback_error}; }
      return {.error = synced_storage.error};
    }
  }

  DebugLog("updated director storage resource '" + std::string{storage_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorClientResource(std::string_view deployment_id,
                                           std::string_view director_name,
                                           std::string_view client_name) const
{
  DebugLog("deleting director client resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', client '"
           + std::string{client_name} + "'");
  if (!IsSafePathSegment(client_name) || !IsSafePathSegment(director_name)) {
    return {.error = "client and director names must be safe path segments."};
  }

  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorClientWriteContext(*director_config.value, client_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define client '" + std::string{client_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Client", client_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error = "failed to update shared director client resource file '"
                       + context.value->file_path.string() + "'."};
    }
  }

  {
    auto loaded
        = bconfig::LoadConfig(bconfig::Component::kDirector,
                              director_config.value->path.string(), true);
    if (!loaded.parser || !loaded.parse_ok) {
      auto rollback_error = RestoreDeletedFile(context.value->file_path,
                                               *original_content.value);
      if (rollback_error) { return {.error = *rollback_error}; }
      if (!loaded.parser) {
        return {.error = FormatParseFailure(
                    "director client parser initialization ", loaded.messages)};
      }
      return {.error
              = FormatParseFailure("director client delete ", loaded.messages)};
    }
  }

  auto deleted_stub = DeleteClientDirectorStubFile(repository_path, client_name,
                                                   director_name);
  if (!deleted_stub) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = deleted_stub.error};
  }

  DebugLog("deleted director client resource '" + std::string{client_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorStorageResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view storage_name) const
{
  DebugLog("deleting director storage resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', storage '"
           + std::string{storage_name} + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(director_name)) {
    return {.error = "storage and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorStorageWriteContext(*director_config.value, storage_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define storage '" + std::string{storage_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Storage", storage_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director storage resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  {
    auto loaded
        = bconfig::LoadConfig(bconfig::Component::kDirector,
                              director_config.value->path.string(), true);
    if (!loaded.parser || !loaded.parse_ok) {
      auto rollback_error = RestoreDeletedFile(context.value->file_path,
                                               *original_content.value);
      if (rollback_error) { return {.error = *rollback_error}; }
      if (!loaded.parser) {
        return {.error
                = FormatParseFailure("director storage parser initialization ",
                                     loaded.messages)};
      }
      return {.error = FormatParseFailure("director storage delete ",
                                          loaded.messages)};
    }
  }

  auto deleted_synced = DeleteSyncedStorageDaemonConfig(
      *this, deployment_id, director_name, *context.value->device,
      *director_config.value);
  if (!deleted_synced) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = deleted_synced.error};
  }

  DebugLog("deleted director storage resource '" + std::string{storage_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorConsoleResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view console_name,
    const DirectorConsoleResourceSpec& spec) const
{
  DebugLog("upserting director console resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', console '"
           + std::string{console_name} + "'");
  if (!IsSafePathSegment(console_name) || !IsSafePathSegment(director_name)) {
    return {.error = "console and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorConsoleWriteContext(*director_config.value, console_name);
  if (!context) { return {.error = context.error}; }

  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required when creating a director "
              "console resource."};
  }

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultDirectorConsoleDescription(
                                         console_name, director_name));
  auto acl = context.value->acl;
  ApplyDirectorAclOverrides(
      acl, spec.job_acl, spec.client_acl, spec.storage_acl, spec.schedule_acl,
      spec.pool_acl, spec.command_acl, spec.fileset_acl, spec.catalog_acl,
      spec.where_acl, spec.plugin_options_acl);
  auto profiles = context.value->profiles;
  ApplyDirectorProfileOverrides(profiles, spec.profiles);
  const auto use_pam_authentication = spec.use_pam_authentication.value_or(
      context.value->use_pam_authentication.value_or(false));
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  const auto content = BuildDirectorConsoleResourceContent(
      console_name, *password, description, use_pam_authentication, acl,
      profiles, tls_authenticate, tls_enable, tls_require, tls_verify_peer,
      tls_cipher_list, tls_cipher_suites, tls_dh_file, tls_protocol,
      tls_ca_certificate_file, tls_ca_certificate_dir,
      tls_certificate_revocation_list, tls_certificate, tls_key,
      tls_allowed_cn);

  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "console";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director console directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = content;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Console", console_name, content);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director console resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director console file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director console update for '" + std::string{console_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director console parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director console update ", loaded.messages)};
  }

  DebugLog("updated director console resource '" + std::string{console_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorConsoleResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view console_name) const
{
  DebugLog("deleting director console resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', console '"
           + std::string{console_name} + "'");
  if (!IsSafePathSegment(console_name) || !IsSafePathSegment(director_name)) {
    return {.error = "console and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorConsoleWriteContext(*director_config.value, console_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define console '" + std::string{console_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Console", console_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director console resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director console parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director console delete ", loaded.messages)};
  }

  DebugLog("deleted director console resource '" + std::string{console_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<ConsoleConsoleResourceSpec>
ServiceState::GetConsoleConsoleResourceSpec(
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view console_name) const
{
  auto console_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) { return {.error = console_config.error}; }

  auto context
      = LoadConsoleConsoleWriteContext(*console_config.value, console_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "console resource '" + std::string{console_name}
                     + "' not found."};
  }

  return {.value = ToConsoleConsoleResourceSpec(*context.value)};
}

OperationResult<ConsoleDirectorResourceSpec>
ServiceState::GetConsoleDirectorResourceSpec(
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view director_name) const
{
  auto console_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) { return {.error = console_config.error}; }

  auto context
      = LoadConsoleDirectorWriteContext(*console_config.value, director_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "console director '" + std::string{director_name}
                     + "' not found."};
  }

  return {.value = ToConsoleDirectorResourceSpec(*context.value)};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertConsoleConsoleResource(
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view console_name,
    const ConsoleConsoleResourceSpec& spec) const
{
  DebugLog("upserting console resource for deployment '"
           + std::string{deployment_id} + "', console config '"
           + std::string{console_config_name} + "', console '"
           + std::string{console_name} + "'");
  if (!IsSafePathSegment(console_config_name)
      || !IsSafePathSegment(console_name)) {
    return {.error
            = "console config and console names must be safe path "
              "segments."};
  }

  auto console_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) { return {.error = console_config.error}; }

  auto context
      = LoadConsoleConsoleWriteContext(*console_config.value, console_name);
  if (!context) { return {.error = context.error}; }

  const auto director
      = spec.director ? *spec.director : context.value->director;
  if (!director || director->empty()) {
    return {.error
            = "field 'director' is required when creating a console resource."};
  }

  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required when creating a console resource."};
  }

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultConsoleConsoleDescription(
                                         console_name, console_config_name));
  const auto rc_file = spec.rc_file ? spec.rc_file : context.value->rc_file;
  const auto history_file
      = spec.history_file ? spec.history_file : context.value->history_file;
  const auto history_length = spec.history_length
                                  ? spec.history_length
                                  : context.value->history_length;
  const auto heartbeat_interval = spec.heartbeat_interval
                                      ? spec.heartbeat_interval
                                      : context.value->heartbeat_interval;
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  const auto managed_resource = BuildConsoleConsoleResourceContent(
      console_name, *director, *password, description, rc_file, history_file,
      history_length, heartbeat_interval, tls_authenticate, tls_enable,
      tls_require, tls_verify_peer, tls_cipher_list, tls_cipher_suites,
      tls_dh_file, tls_protocol, tls_ca_certificate_file,
      tls_ca_certificate_dir, tls_certificate_revocation_list, tls_certificate,
      tls_key, tls_allowed_cn);

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }

  const auto rewritten_content
      = BuildRewrittenConsoleConfigContent(*context.value, managed_resource);
  if (!WriteFile(context.value->file_path, rewritten_content)) {
    return {.error = "failed to write console resource '"
                     + context.value->file_path.string() + "'."};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kConsole,
                                    console_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error = RestoreClientStubFile(context.value->file_path,
                                                *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("console parser initialization ",
                                          loaded.messages)};
    }
    return {.error = FormatParseFailure("console update ", loaded.messages)};
  }

  DebugLog("updated console resource '" + std::string{console_name}
           + "' in console config '" + std::string{console_config_name} + "'");
  return {.value = *console_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteConsoleConsoleResource(std::string_view deployment_id,
                                           std::string_view console_config_name,
                                           std::string_view console_name) const
{
  DebugLog("deleting console resource for deployment '"
           + std::string{deployment_id} + "', console config '"
           + std::string{console_config_name} + "', console '"
           + std::string{console_name} + "'");
  if (!IsSafePathSegment(console_config_name)
      || !IsSafePathSegment(console_name)) {
    return {.error
            = "console config and console names must be safe path "
              "segments."};
  }

  auto console_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) { return {.error = console_config.error}; }

  auto context
      = LoadConsoleConsoleWriteContext(*console_config.value, console_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "console config '" + std::string{console_config_name}
                     + "' does not define console '" + std::string{console_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }

  const auto rewritten_content
      = BuildRewrittenConsoleConfigContent(*context.value, std::nullopt);
  if (!WriteFile(context.value->file_path, rewritten_content)) {
    return {.error = "failed to write console config '"
                     + context.value->file_path.string() + "'."};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kConsole,
                                    console_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error = RestoreClientStubFile(context.value->file_path,
                                                *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("console parser initialization ",
                                          loaded.messages)};
    }
    return {.error = FormatParseFailure("console delete ", loaded.messages)};
  }

  DebugLog("deleted console resource '" + std::string{console_name}
           + "' from console config '" + std::string{console_config_name}
           + "'");
  return {.value = *console_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertConsoleDirectorResource(
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view director_name,
    const ConsoleDirectorResourceSpec& spec) const
{
  DebugLog("upserting console director resource for deployment '"
           + std::string{deployment_id} + "', console config '"
           + std::string{console_config_name} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(console_config_name)
      || !IsSafePathSegment(director_name)) {
    return {.error
            = "console config and director names must be safe path "
              "segments."};
  }

  auto console_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) { return {.error = console_config.error}; }

  auto context
      = LoadConsoleDirectorWriteContext(*console_config.value, director_name);
  if (!context) { return {.error = context.error}; }

  const auto address = spec.address ? *spec.address : context.value->address;
  if (!address || address->empty()) {
    return {.error
            = "field 'address' is required when creating a console director "
              "resource."};
  }

  const auto password
      = context.value->has_named_console
            ? std::optional<std::string>{}
            : (spec.password ? spec.password : context.value->password);
  const auto port
      = spec.port ? std::optional<uint32_t>{*spec.port} : context.value->port;
  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultConsoleDirectorDescription(
                                         director_name, console_config_name));
  const auto heartbeat_interval = spec.heartbeat_interval
                                      ? spec.heartbeat_interval
                                      : context.value->heartbeat_interval;
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  const auto managed_resource = BuildConsoleDirectorResourceContent(
      director_name, *address, port, password, description, heartbeat_interval,
      tls_authenticate, tls_enable, tls_require, tls_verify_peer,
      tls_cipher_list, tls_cipher_suites, tls_dh_file, tls_protocol,
      tls_ca_certificate_file, tls_ca_certificate_dir,
      tls_certificate_revocation_list, tls_certificate, tls_key,
      tls_allowed_cn);

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }

  const auto rewritten_content
      = BuildRewrittenConsoleConfigContent(*context.value, managed_resource);
  if (!WriteFile(context.value->file_path, rewritten_content)) {
    return {.error = "failed to write console config '"
                     + context.value->file_path.string() + "'."};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kConsole,
                                    console_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error = RestoreClientStubFile(context.value->file_path,
                                                *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("console parser initialization ",
                                          loaded.messages)};
    }
    return {.error
            = FormatParseFailure("console director update ", loaded.messages)};
  }

  DebugLog("updated console director resource '" + std::string{director_name}
           + "' in console config '" + std::string{console_config_name} + "'");
  return {.value = *console_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteConsoleDirectorResource(
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view director_name) const
{
  DebugLog("deleting console director resource for deployment '"
           + std::string{deployment_id} + "', console config '"
           + std::string{console_config_name} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(console_config_name)
      || !IsSafePathSegment(director_name)) {
    return {.error
            = "console config and director names must be safe path "
              "segments."};
  }

  auto console_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) { return {.error = console_config.error}; }

  auto context
      = LoadConsoleDirectorWriteContext(*console_config.value, director_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "console config '" + std::string{console_config_name}
                     + "' does not define director '"
                     + std::string{director_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }

  const auto rewritten_content
      = BuildRewrittenConsoleConfigContent(*context.value, std::nullopt);
  if (!WriteFile(context.value->file_path, rewritten_content)) {
    return {.error = "failed to write console config '"
                     + context.value->file_path.string() + "'."};
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kConsole,
                                    console_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error = RestoreClientStubFile(context.value->file_path,
                                                *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("console parser initialization ",
                                          loaded.messages)};
    }
    return {.error
            = FormatParseFailure("console director delete ", loaded.messages)};
  }

  DebugLog("deleted console director resource '" + std::string{director_name}
           + "' from console config '" + std::string{console_config_name}
           + "'");
  return {.value = *console_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorUserResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view user_name,
    const DirectorUserResourceSpec& spec) const
{
  DebugLog("upserting director user resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', user '" + std::string{user_name}
           + "'");
  if (!IsSafePathSegment(user_name) || !IsSafePathSegment(director_name)) {
    return {.error = "user and director names must be safe path segments."};
  }

  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorUserWriteContext(*director_config.value, user_name);
  if (!context) { return {.error = context.error}; }

  const auto description
      = spec.description
            ? *spec.description
            : context.value->description.value_or(
                  DefaultDirectorUserDescription(user_name, director_name));
  auto acl = context.value->acl;
  ApplyDirectorAclOverrides(
      acl, spec.job_acl, spec.client_acl, spec.storage_acl, spec.schedule_acl,
      spec.pool_acl, spec.command_acl, spec.fileset_acl, spec.catalog_acl,
      spec.where_acl, spec.plugin_options_acl);
  auto profiles = context.value->profiles;
  ApplyDirectorProfileOverrides(profiles, spec.profiles);
  const auto content
      = BuildDirectorUserResourceContent(user_name, description, acl, profiles);

  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "user";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director user directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = content;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(context.value->file_path,
                                                  "User", user_name, content);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director user resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director user file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director user update for '" + std::string{user_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director user parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director user update ", loaded.messages)};
  }

  DebugLog("updated director user resource '" + std::string{user_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorUserResource(std::string_view deployment_id,
                                         std::string_view director_name,
                                         std::string_view user_name) const
{
  DebugLog("deleting director user resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', user '" + std::string{user_name}
           + "'");
  if (!IsSafePathSegment(user_name) || !IsSafePathSegment(director_name)) {
    return {.error = "user and director names must be safe path segments."};
  }

  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorUserWriteContext(*director_config.value, user_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define user '" + std::string{user_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "User", user_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error = "failed to update shared director user resource file '"
                       + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director user parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director user delete ", loaded.messages)};
  }

  DebugLog("deleted director user resource '" + std::string{user_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorProfileResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view profile_name,
    const DirectorProfileResourceSpec& spec) const
{
  DebugLog("upserting director profile resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', profile '"
           + std::string{profile_name} + "'");
  if (!IsSafePathSegment(profile_name) || !IsSafePathSegment(director_name)) {
    return {.error = "profile and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorProfileWriteContext(*director_config.value, profile_name);
  if (!context) { return {.error = context.error}; }

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultDirectorProfileDescription(
                                         profile_name, director_name));
  auto acl = context.value->acl;
  ApplyDirectorAclOverrides(
      acl, spec.job_acl, spec.client_acl, spec.storage_acl, spec.schedule_acl,
      spec.pool_acl, spec.command_acl, spec.fileset_acl, spec.catalog_acl,
      spec.where_acl, spec.plugin_options_acl);
  const auto content
      = BuildDirectorProfileResourceContent(profile_name, description, acl);

  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "profile";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director profile directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = content;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Profile", profile_name, content);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director profile resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director profile file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director profile update for '" + std::string{profile_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director profile parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director profile update ", loaded.messages)};
  }

  DebugLog("updated director profile resource '" + std::string{profile_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorProfileResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view profile_name) const
{
  DebugLog("deleting director profile resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', profile '"
           + std::string{profile_name} + "'");
  if (!IsSafePathSegment(profile_name) || !IsSafePathSegment(director_name)) {
    return {.error = "profile and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorProfileWriteContext(*director_config.value, profile_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define profile '" + std::string{profile_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Profile", profile_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director profile resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director profile parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director profile delete ", loaded.messages)};
  }

  DebugLog("deleted director profile resource '" + std::string{profile_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorPoolResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view pool_name,
    const DirectorPoolResourceSpec& spec) const
{
  DebugLog("upserting director pool resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', pool '" + std::string{pool_name}
           + "'");
  if (!IsSafePathSegment(pool_name) || !IsSafePathSegment(director_name)) {
    return {.error = "pool and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorPoolWriteContext(*director_config.value, pool_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(
                  DefaultDirectorPoolDescription(pool_name, director_name));
  content.pool_type
      = spec.pool_type ? *spec.pool_type : content.pool_type.value_or("Backup");
  if (spec.label_format) {
    content.label_format = *spec.label_format;
  } else if (!context.value->exists && !content.label_format) {
    content.label_format = std::nullopt;
  }
  if (spec.maximum_volumes) { content.maximum_volumes = *spec.maximum_volumes; }
  if (spec.cleaning_prefix) { content.cleaning_prefix = *spec.cleaning_prefix; }
  if (spec.label_type) { content.label_type = *spec.label_type; }
  if (spec.maximum_volume_jobs) {
    content.maximum_volume_jobs = *spec.maximum_volume_jobs;
  }
  if (spec.maximum_volume_files) {
    content.maximum_volume_files = *spec.maximum_volume_files;
  }
  if (spec.maximum_volume_bytes) {
    content.maximum_volume_bytes = *spec.maximum_volume_bytes;
  }
  if (spec.volume_retention) {
    content.volume_retention = *spec.volume_retention;
  }
  if (spec.volume_use_duration) {
    content.volume_use_duration = *spec.volume_use_duration;
  }
  if (spec.migration_time) { content.migration_time = *spec.migration_time; }
  if (spec.migration_high_bytes) {
    content.migration_high_bytes = *spec.migration_high_bytes;
  }
  if (spec.migration_low_bytes) {
    content.migration_low_bytes = *spec.migration_low_bytes;
  }
  if (spec.next_pool) { content.next_pool = *spec.next_pool; }
  if (spec.storages) { content.storages = *spec.storages; }
  if (spec.use_catalog) { content.use_catalog = *spec.use_catalog; }
  if (spec.catalog_files) { content.catalog_files = *spec.catalog_files; }
  if (spec.purge_oldest_volume) {
    content.purge_oldest_volume = *spec.purge_oldest_volume;
  }
  if (spec.action_on_purge) { content.action_on_purge = *spec.action_on_purge; }
  if (spec.recycle_oldest_volume) {
    content.recycle_oldest_volume = *spec.recycle_oldest_volume;
  }
  if (spec.recycle_current_volume) {
    content.recycle_current_volume = *spec.recycle_current_volume;
  }
  if (spec.auto_prune) { content.auto_prune = *spec.auto_prune; }
  if (spec.recycle) { content.recycle = *spec.recycle; }
  if (spec.recycle_pool) { content.recycle_pool = *spec.recycle_pool; }
  if (spec.scratch_pool) { content.scratch_pool = *spec.scratch_pool; }
  if (spec.catalog) { content.catalog = *spec.catalog; }
  if (spec.file_retention) { content.file_retention = *spec.file_retention; }
  if (spec.job_retention) { content.job_retention = *spec.job_retention; }
  if (spec.minimum_block_size) {
    content.minimum_block_size = *spec.minimum_block_size;
  }
  if (spec.maximum_block_size) {
    content.maximum_block_size = *spec.maximum_block_size;
  }

  const auto rendered = BuildDirectorPoolResourceContent(pool_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "pool";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director pool directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(context.value->file_path,
                                                  "Pool", pool_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director pool resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director pool file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director pool update for '" + std::string{pool_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director pool parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director pool update ", loaded.messages)};
  }

  DebugLog("updated director pool resource '" + std::string{pool_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorPoolResource(std::string_view deployment_id,
                                         std::string_view director_name,
                                         std::string_view pool_name) const
{
  DebugLog("deleting director pool resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', pool '" + std::string{pool_name}
           + "'");
  if (!IsSafePathSegment(pool_name) || !IsSafePathSegment(director_name)) {
    return {.error = "pool and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorPoolWriteContext(*director_config.value, pool_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define pool '" + std::string{pool_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Pool", pool_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error = "failed to update shared director pool resource file '"
                       + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director pool parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director pool delete ", loaded.messages)};
  }

  DebugLog("deleted director pool resource '" + std::string{pool_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorCatalogResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view catalog_name,
    const DirectorCatalogResourceSpec& spec) const
{
  DebugLog("upserting director catalog resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', catalog '"
           + std::string{catalog_name} + "'");
  if (!IsSafePathSegment(catalog_name) || !IsSafePathSegment(director_name)) {
    return {.error = "catalog and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorCatalogWriteContext(*director_config.value, catalog_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorCatalogDescription(
                  catalog_name, director_name));
  if (spec.db_address) { content.db_address = *spec.db_address; }
  if (spec.db_port) { content.db_port = *spec.db_port; }
  if (spec.db_socket) { content.db_socket = *spec.db_socket; }
  if (spec.db_password) { content.db_password = *spec.db_password; }
  if (spec.db_user) { content.db_user = *spec.db_user; }
  if (spec.db_name) { content.db_name = *spec.db_name; }
  if (spec.multiple_connections) {
    content.multiple_connections = *spec.multiple_connections;
  }
  if (spec.disable_batch_insert) {
    content.disable_batch_insert = *spec.disable_batch_insert;
  }
  if (spec.reconnect) { content.reconnect = *spec.reconnect; }
  if (spec.exit_on_fatal) { content.exit_on_fatal = *spec.exit_on_fatal; }
  if (spec.min_connections) { content.min_connections = *spec.min_connections; }
  if (spec.max_connections) { content.max_connections = *spec.max_connections; }
  if (spec.inc_connections) { content.inc_connections = *spec.inc_connections; }
  if (spec.idle_timeout) { content.idle_timeout = *spec.idle_timeout; }
  if (spec.validate_timeout) {
    content.validate_timeout = *spec.validate_timeout;
  }

  if (!content.db_user || content.db_user->empty()) {
    return {.error = "director catalog '" + std::string{catalog_name}
                     + "' must define DbUser."};
  }
  if (!content.db_name || content.db_name->empty()) {
    return {.error = "director catalog '" + std::string{catalog_name}
                     + "' must define DbName."};
  }
  if (!content.db_password) { content.db_password = std::string{}; }

  const auto rendered
      = BuildDirectorCatalogResourceContent(catalog_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "catalog";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director catalog directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Catalog", catalog_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director catalog resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director catalog file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director catalog update for '" + std::string{catalog_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director catalog parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director catalog update ", loaded.messages)};
  }

  DebugLog("updated director catalog resource '" + std::string{catalog_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorCatalogResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view catalog_name) const
{
  DebugLog("deleting director catalog resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', catalog '"
           + std::string{catalog_name} + "'");
  if (!IsSafePathSegment(catalog_name) || !IsSafePathSegment(director_name)) {
    return {.error = "catalog and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorCatalogWriteContext(*director_config.value, catalog_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define catalog '" + std::string{catalog_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Catalog", catalog_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director catalog resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director catalog parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director catalog delete ", loaded.messages)};
  }

  DebugLog("deleted director catalog resource '" + std::string{catalog_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorMessagesResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view messages_name,
    const DirectorMessagesResourceSpec& spec) const
{
  DebugLog("upserting director messages resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', messages '"
           + std::string{messages_name} + "'");
  if (!IsSafePathSegment(messages_name) || !IsSafePathSegment(director_name)) {
    return {.error = "messages and director names must be safe path segments."};
  }

  auto director_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) { return {.error = director_config.error}; }

  auto context
      = LoadDirectorMessagesWriteContext(*director_config.value, messages_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorMessagesDescription(
                  messages_name, director_name));
  if (spec.mail_command) { content.mail_command = *spec.mail_command; }
  if (spec.operator_command) {
    content.operator_command = *spec.operator_command;
  }
  if (spec.timestamp_format) {
    content.timestamp_format = *spec.timestamp_format;
  }
  if (spec.syslog_entries) { content.syslog_entries = *spec.syslog_entries; }
  if (spec.mail_entries) { content.mail_entries = *spec.mail_entries; }
  if (spec.mail_on_error_entries) {
    content.mail_on_error_entries = *spec.mail_on_error_entries;
  }
  if (spec.mail_on_success_entries) {
    content.mail_on_success_entries = *spec.mail_on_success_entries;
  }
  if (spec.file_entries) { content.file_entries = *spec.file_entries; }
  if (spec.append_entries) { content.append_entries = *spec.append_entries; }
  if (spec.stdout_entries) { content.stdout_entries = *spec.stdout_entries; }
  if (spec.stderr_entries) { content.stderr_entries = *spec.stderr_entries; }
  if (spec.director_entries) {
    content.director_entries = *spec.director_entries;
  }
  if (spec.console_entries) { content.console_entries = *spec.console_entries; }
  if (spec.operator_entries) {
    content.operator_entries = *spec.operator_entries;
  }
  if (spec.catalog_entries) { content.catalog_entries = *spec.catalog_entries; }
  if (spec.entries) { content.entries = *spec.entries; }

  const auto rendered
      = BuildDirectorMessagesResourceContent(messages_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "messages";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director messages directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Messages", messages_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director messages resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director messages file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director messages update for '" + std::string{messages_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director messages parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director messages update ", loaded.messages)};
  }

  DebugLog("updated director messages resource '" + std::string{messages_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorMessagesResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view messages_name) const
{
  DebugLog("deleting director messages resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', messages '"
           + std::string{messages_name} + "'");
  if (!IsSafePathSegment(messages_name) || !IsSafePathSegment(director_name)) {
    return {.error = "messages and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorMessagesWriteContext(*director_config.value, messages_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define messages '"
                     + std::string{messages_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Messages", messages_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director messages resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director messages parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director messages delete ", loaded.messages)};
  }

  DebugLog("deleted director messages resource '" + std::string{messages_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorScheduleResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view schedule_name,
    const DirectorScheduleResourceSpec& spec) const
{
  DebugLog("upserting director schedule resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', schedule '"
           + std::string{schedule_name} + "'");
  if (!IsSafePathSegment(schedule_name) || !IsSafePathSegment(director_name)) {
    return {.error = "schedule and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorScheduleWriteContext(*director_config.value, schedule_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorScheduleDescription(
                  schedule_name, director_name));
  if (spec.enabled) { content.enabled = *spec.enabled; }
  if (spec.run_entries) { content.run_entries = *spec.run_entries; }

  const auto rendered
      = BuildDirectorScheduleResourceContent(schedule_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "schedule";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director schedule directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Schedule", schedule_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director schedule resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director schedule file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director schedule update for '" + std::string{schedule_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director schedule parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director schedule update ", loaded.messages)};
  }

  DebugLog("updated director schedule resource '" + std::string{schedule_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorScheduleResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view schedule_name) const
{
  DebugLog("deleting director schedule resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', schedule '"
           + std::string{schedule_name} + "'");
  if (!IsSafePathSegment(schedule_name) || !IsSafePathSegment(director_name)) {
    return {.error = "schedule and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorScheduleWriteContext(*director_config.value, schedule_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define schedule '"
                     + std::string{schedule_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Schedule", schedule_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director schedule resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director schedule parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director schedule delete ", loaded.messages)};
  }

  DebugLog("deleted director schedule resource '" + std::string{schedule_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorCounterResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view counter_name,
    const DirectorCounterResourceSpec& spec) const
{
  DebugLog("upserting director counter resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', counter '"
           + std::string{counter_name} + "'");
  if (!IsSafePathSegment(counter_name) || !IsSafePathSegment(director_name)) {
    return {.error = "counter and director names must be safe path segments."};
  }

  if (spec.maximum && *spec.maximum > std::numeric_limits<int32_t>::max()) {
    return {.error = "counter maximum must not exceed 2147483647."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorCounterWriteContext(*director_config.value, counter_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorCounterDescription(
                  counter_name, director_name));
  if (spec.minimum) { content.minimum = *spec.minimum; }
  if (spec.maximum) { content.maximum = static_cast<int32_t>(*spec.maximum); }
  if (spec.wrap_counter) { content.wrap_counter = *spec.wrap_counter; }
  if (spec.catalog) { content.catalog = *spec.catalog; }

  const auto rendered
      = BuildDirectorCounterResourceContent(counter_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "counter";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director counter directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Counter", counter_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director counter resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director counter file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director counter update for '" + std::string{counter_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director counter parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director counter update ", loaded.messages)};
  }

  DebugLog("updated director counter resource '" + std::string{counter_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorCounterResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view counter_name) const
{
  DebugLog("deleting director counter resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', counter '"
           + std::string{counter_name} + "'");
  if (!IsSafePathSegment(counter_name) || !IsSafePathSegment(director_name)) {
    return {.error = "counter and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorCounterWriteContext(*director_config.value, counter_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define counter '" + std::string{counter_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Counter", counter_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director counter resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director counter parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director counter delete ", loaded.messages)};
  }

  DebugLog("deleted director counter resource '" + std::string{counter_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorFilesetResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view fileset_name,
    const DirectorFilesetResourceSpec& spec) const
{
  DebugLog("upserting director fileset resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', fileset '"
           + std::string{fileset_name} + "'");
  if (!IsSafePathSegment(fileset_name) || !IsSafePathSegment(director_name)) {
    return {.error = "fileset and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorFilesetWriteContext(*director_config.value, fileset_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorFilesetDescription(
                  fileset_name, director_name));
  if (spec.ignore_fileset_changes) {
    content.ignore_fileset_changes = *spec.ignore_fileset_changes;
  }
  if (spec.enable_vss) { content.enable_vss = *spec.enable_vss; }
  if (spec.include_blocks) { content.include_blocks = *spec.include_blocks; }
  if (spec.exclude_blocks) { content.exclude_blocks = *spec.exclude_blocks; }

  const auto rendered
      = BuildDirectorFilesetResourceContent(fileset_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "fileset";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director fileset directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "FileSet", fileset_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director fileset resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director fileset file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director fileset update for '" + std::string{fileset_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director fileset parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director fileset update ", loaded.messages)};
  }

  DebugLog("updated director fileset resource '" + std::string{fileset_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorFilesetResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view fileset_name) const
{
  DebugLog("deleting director fileset resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', fileset '"
           + std::string{fileset_name} + "'");
  if (!IsSafePathSegment(fileset_name) || !IsSafePathSegment(director_name)) {
    return {.error = "fileset and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorFilesetWriteContext(*director_config.value, fileset_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define fileset '" + std::string{fileset_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "FileSet", fileset_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director fileset resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director fileset parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director fileset delete ", loaded.messages)};
  }

  DebugLog("deleted director fileset resource '" + std::string{fileset_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord> ServiceState::UpsertDirectorJobResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view job_name,
    const DirectorJobResourceSpec& spec) const
{
  DebugLog("upserting director job resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', job '" + std::string{job_name}
           + "'");
  if (!IsSafePathSegment(job_name) || !IsSafePathSegment(director_name)) {
    return {.error = "job and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context = LoadDirectorJobWriteContext(*director_config.value, job_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(
                  DefaultDirectorJobDescription(job_name, director_name));
  if (spec.type) { content.type = *spec.type; }
  if (spec.backup_format) { content.backup_format = *spec.backup_format; }
  if (spec.protocol) {
    auto normalized_protocol = NormalizeDirectorJobProtocol(*spec.protocol);
    if (!normalized_protocol) { return {.error = normalized_protocol.error}; }
    content.protocol = *normalized_protocol.value;
  }
  if (spec.level) { content.level = *spec.level; }
  if (spec.messages) { content.messages = *spec.messages; }
  if (spec.storages) { content.storages = *spec.storages; }
  if (spec.pool) { content.pool = *spec.pool; }
  if (spec.full_backup_pool) {
    content.full_backup_pool = *spec.full_backup_pool;
  }
  if (spec.virtual_full_backup_pool) {
    content.virtual_full_backup_pool = *spec.virtual_full_backup_pool;
  }
  if (spec.incremental_backup_pool) {
    content.incremental_backup_pool = *spec.incremental_backup_pool;
  }
  if (spec.differential_backup_pool) {
    content.differential_backup_pool = *spec.differential_backup_pool;
  }
  if (spec.next_pool) { content.next_pool = *spec.next_pool; }
  if (spec.client) { content.client = *spec.client; }
  if (spec.fileset) { content.fileset = *spec.fileset; }
  if (spec.schedule) { content.schedule = *spec.schedule; }
  if (spec.verify_job) { content.verify_job = *spec.verify_job; }
  if (spec.catalog) { content.catalog = *spec.catalog; }
  if (spec.jobdefs) { content.jobdefs = *spec.jobdefs; }
  if (spec.run_entries) { content.run_entries = *spec.run_entries; }
  if (spec.run_before_job_entries) {
    content.run_before_job_entries = *spec.run_before_job_entries;
  }
  if (spec.run_after_job_entries) {
    content.run_after_job_entries = *spec.run_after_job_entries;
  }
  if (spec.run_after_failed_job_entries) {
    content.run_after_failed_job_entries = *spec.run_after_failed_job_entries;
  }
  if (spec.client_run_before_job_entries) {
    content.client_run_before_job_entries = *spec.client_run_before_job_entries;
  }
  if (spec.client_run_after_job_entries) {
    content.client_run_after_job_entries = *spec.client_run_after_job_entries;
  }
  if (spec.runscript_blocks) {
    content.runscript_blocks = *spec.runscript_blocks;
  }
  if (spec.where) { content.where = *spec.where; }
  if (spec.replace) {
    auto normalized_replace = NormalizeDirectorJobReplace(*spec.replace);
    if (!normalized_replace) { return {.error = normalized_replace.error}; }
    content.replace = *normalized_replace.value;
  }
  if (spec.regex_where) { content.regex_where = *spec.regex_where; }
  if (spec.strip_prefix) { content.strip_prefix = *spec.strip_prefix; }
  if (spec.add_prefix) { content.add_prefix = *spec.add_prefix; }
  if (spec.add_suffix) { content.add_suffix = *spec.add_suffix; }
  if (spec.bootstrap) { content.bootstrap = *spec.bootstrap; }
  if (spec.write_bootstrap) { content.write_bootstrap = *spec.write_bootstrap; }
  if (spec.write_verify_list) {
    content.write_verify_list = *spec.write_verify_list;
  }
  if (spec.maximum_bandwidth) {
    content.maximum_bandwidth = *spec.maximum_bandwidth;
  }
  if (spec.max_run_sched_time) {
    content.max_run_sched_time = *spec.max_run_sched_time;
  }
  if (spec.max_run_time) { content.max_run_time = *spec.max_run_time; }
  if (spec.full_max_runtime) {
    content.full_max_runtime = *spec.full_max_runtime;
  }
  if (spec.incremental_max_runtime) {
    content.incremental_max_runtime = *spec.incremental_max_runtime;
  }
  if (spec.differential_max_runtime) {
    content.differential_max_runtime = *spec.differential_max_runtime;
  }
  if (spec.max_wait_time) { content.max_wait_time = *spec.max_wait_time; }
  if (spec.max_start_delay) { content.max_start_delay = *spec.max_start_delay; }
  if (spec.max_full_interval) {
    content.max_full_interval = *spec.max_full_interval;
  }
  if (spec.max_virtual_full_interval) {
    content.max_virtual_full_interval = *spec.max_virtual_full_interval;
  }
  if (spec.max_diff_interval) {
    content.max_diff_interval = *spec.max_diff_interval;
  }
  if (spec.prefix_links) { content.prefix_links = *spec.prefix_links; }
  if (spec.prune_jobs) { content.prune_jobs = *spec.prune_jobs; }
  if (spec.prune_files) { content.prune_files = *spec.prune_files; }
  if (spec.prune_volumes) { content.prune_volumes = *spec.prune_volumes; }
  if (spec.purge_migration_job) {
    content.purge_migration_job = *spec.purge_migration_job;
  }
  if (spec.spool_attributes) {
    content.spool_attributes = *spec.spool_attributes;
  }
  if (spec.spool_data) { content.spool_data = *spec.spool_data; }
  if (spec.spool_size) { content.spool_size = *spec.spool_size; }
  if (spec.rerun_failed_levels) {
    content.rerun_failed_levels = *spec.rerun_failed_levels;
  }
  if (spec.prefer_mounted_volumes) {
    content.prefer_mounted_volumes = *spec.prefer_mounted_volumes;
  }
  if (spec.maximum_concurrent_jobs) {
    content.maximum_concurrent_jobs = *spec.maximum_concurrent_jobs;
  }
  if (spec.reschedule_on_error) {
    content.reschedule_on_error = *spec.reschedule_on_error;
  }
  if (spec.reschedule_interval) {
    content.reschedule_interval = *spec.reschedule_interval;
  }
  if (spec.reschedule_times) {
    content.reschedule_times = *spec.reschedule_times;
  }
  if (spec.priority) { content.priority = *spec.priority; }
  if (spec.allow_mixed_priority) {
    content.allow_mixed_priority = *spec.allow_mixed_priority;
  }
  if (spec.selection_type) {
    auto normalized_selection_type
        = NormalizeDirectorJobSelectionType(*spec.selection_type);
    if (!normalized_selection_type) {
      return {.error = normalized_selection_type.error};
    }
    content.selection_type = *normalized_selection_type.value;
  }
  if (spec.selection_pattern) {
    content.selection_pattern = *spec.selection_pattern;
  }
  if (spec.accurate) { content.accurate = *spec.accurate; }
  if (spec.allow_duplicate_jobs) {
    content.allow_duplicate_jobs = *spec.allow_duplicate_jobs;
  }
  if (spec.allow_higher_duplicates) {
    content.allow_higher_duplicates = *spec.allow_higher_duplicates;
  }
  if (spec.cancel_lower_level_duplicates) {
    content.cancel_lower_level_duplicates = *spec.cancel_lower_level_duplicates;
  }
  if (spec.cancel_queued_duplicates) {
    content.cancel_queued_duplicates = *spec.cancel_queued_duplicates;
  }
  if (spec.cancel_running_duplicates) {
    content.cancel_running_duplicates = *spec.cancel_running_duplicates;
  }
  if (spec.save_file_history) {
    content.save_file_history = *spec.save_file_history;
  }
  if (spec.file_history_size) {
    content.file_history_size = *spec.file_history_size;
  }
  if (spec.fd_plugin_options) {
    content.fd_plugin_options = *spec.fd_plugin_options;
  }
  if (spec.sd_plugin_options) {
    content.sd_plugin_options = *spec.sd_plugin_options;
  }
  if (spec.dir_plugin_options) {
    content.dir_plugin_options = *spec.dir_plugin_options;
  }
  if (spec.max_concurrent_copies) {
    content.max_concurrent_copies = *spec.max_concurrent_copies;
  }
  if (spec.always_incremental) {
    content.always_incremental = *spec.always_incremental;
  }
  if (spec.always_incremental_job_retention) {
    content.always_incremental_job_retention
        = *spec.always_incremental_job_retention;
  }
  if (spec.always_incremental_keep_number) {
    content.always_incremental_keep_number
        = *spec.always_incremental_keep_number;
  }
  if (spec.always_incremental_max_full_age) {
    content.always_incremental_max_full_age
        = *spec.always_incremental_max_full_age;
  }
  if (spec.max_full_consolidations) {
    content.max_full_consolidations = *spec.max_full_consolidations;
  }
  if (spec.run_on_incoming_connect_interval) {
    content.run_on_incoming_connect_interval
        = *spec.run_on_incoming_connect_interval;
  }
  if (spec.enabled) { content.enabled = *spec.enabled; }

  const auto rendered = BuildDirectorJobResourceContent(job_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "job";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director job directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(context.value->file_path,
                                                  "Job", job_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director job resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director job file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director job update for '" + std::string{job_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("director job parser initialization ",
                                          loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director job update ", loaded.messages)};
  }

  DebugLog("updated director job resource '" + std::string{job_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord> ServiceState::DeleteDirectorJobResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view job_name) const
{
  DebugLog("deleting director job resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', job '" + std::string{job_name}
           + "'");
  if (!IsSafePathSegment(job_name) || !IsSafePathSegment(director_name)) {
    return {.error = "job and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context = LoadDirectorJobWriteContext(*director_config.value, job_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define job '" + std::string{job_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Job", job_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error = "failed to update shared director job resource file '"
                       + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure("director job parser initialization ",
                                          loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director job delete ", loaded.messages)};
  }

  DebugLog("deleted director job resource '" + std::string{job_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertDirectorJobDefsResource(
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view jobdefs_name,
    const DirectorJobDefsResourceSpec& spec) const
{
  DebugLog("upserting director jobdefs resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', jobdefs '"
           + std::string{jobdefs_name} + "'");
  if (!IsSafePathSegment(jobdefs_name) || !IsSafePathSegment(director_name)) {
    return {.error = "jobdefs and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorJobDefsWriteContext(*director_config.value, jobdefs_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorJobDefsDescription(
                  jobdefs_name, director_name));
  if (spec.type) { content.type = *spec.type; }
  if (spec.backup_format) { content.backup_format = *spec.backup_format; }
  if (spec.protocol) {
    auto normalized_protocol = NormalizeDirectorJobProtocol(*spec.protocol);
    if (!normalized_protocol) { return {.error = normalized_protocol.error}; }
    content.protocol = *normalized_protocol.value;
  }
  if (spec.level) { content.level = *spec.level; }
  if (spec.messages) { content.messages = *spec.messages; }
  if (spec.storages) { content.storages = *spec.storages; }
  if (spec.pool) { content.pool = *spec.pool; }
  if (spec.full_backup_pool) {
    content.full_backup_pool = *spec.full_backup_pool;
  }
  if (spec.virtual_full_backup_pool) {
    content.virtual_full_backup_pool = *spec.virtual_full_backup_pool;
  }
  if (spec.incremental_backup_pool) {
    content.incremental_backup_pool = *spec.incremental_backup_pool;
  }
  if (spec.differential_backup_pool) {
    content.differential_backup_pool = *spec.differential_backup_pool;
  }
  if (spec.next_pool) { content.next_pool = *spec.next_pool; }
  if (spec.client) { content.client = *spec.client; }
  if (spec.fileset) { content.fileset = *spec.fileset; }
  if (spec.schedule) { content.schedule = *spec.schedule; }
  if (spec.verify_job) { content.verify_job = *spec.verify_job; }
  if (spec.catalog) { content.catalog = *spec.catalog; }
  if (spec.jobdefs) { content.jobdefs = *spec.jobdefs; }
  if (spec.run_entries) { content.run_entries = *spec.run_entries; }
  if (spec.run_before_job_entries) {
    content.run_before_job_entries = *spec.run_before_job_entries;
  }
  if (spec.run_after_job_entries) {
    content.run_after_job_entries = *spec.run_after_job_entries;
  }
  if (spec.run_after_failed_job_entries) {
    content.run_after_failed_job_entries = *spec.run_after_failed_job_entries;
  }
  if (spec.client_run_before_job_entries) {
    content.client_run_before_job_entries = *spec.client_run_before_job_entries;
  }
  if (spec.client_run_after_job_entries) {
    content.client_run_after_job_entries = *spec.client_run_after_job_entries;
  }
  if (spec.runscript_blocks) {
    content.runscript_blocks = *spec.runscript_blocks;
  }
  if (spec.where) { content.where = *spec.where; }
  if (spec.replace) {
    auto normalized_replace = NormalizeDirectorJobReplace(*spec.replace);
    if (!normalized_replace) { return {.error = normalized_replace.error}; }
    content.replace = *normalized_replace.value;
  }
  if (spec.regex_where) { content.regex_where = *spec.regex_where; }
  if (spec.strip_prefix) { content.strip_prefix = *spec.strip_prefix; }
  if (spec.add_prefix) { content.add_prefix = *spec.add_prefix; }
  if (spec.add_suffix) { content.add_suffix = *spec.add_suffix; }
  if (spec.bootstrap) { content.bootstrap = *spec.bootstrap; }
  if (spec.write_bootstrap) { content.write_bootstrap = *spec.write_bootstrap; }
  if (spec.write_verify_list) {
    content.write_verify_list = *spec.write_verify_list;
  }
  if (spec.maximum_bandwidth) {
    content.maximum_bandwidth = *spec.maximum_bandwidth;
  }
  if (spec.max_run_sched_time) {
    content.max_run_sched_time = *spec.max_run_sched_time;
  }
  if (spec.max_run_time) { content.max_run_time = *spec.max_run_time; }
  if (spec.full_max_runtime) {
    content.full_max_runtime = *spec.full_max_runtime;
  }
  if (spec.incremental_max_runtime) {
    content.incremental_max_runtime = *spec.incremental_max_runtime;
  }
  if (spec.differential_max_runtime) {
    content.differential_max_runtime = *spec.differential_max_runtime;
  }
  if (spec.max_wait_time) { content.max_wait_time = *spec.max_wait_time; }
  if (spec.max_start_delay) { content.max_start_delay = *spec.max_start_delay; }
  if (spec.max_full_interval) {
    content.max_full_interval = *spec.max_full_interval;
  }
  if (spec.max_virtual_full_interval) {
    content.max_virtual_full_interval = *spec.max_virtual_full_interval;
  }
  if (spec.max_diff_interval) {
    content.max_diff_interval = *spec.max_diff_interval;
  }
  if (spec.prefix_links) { content.prefix_links = *spec.prefix_links; }
  if (spec.prune_jobs) { content.prune_jobs = *spec.prune_jobs; }
  if (spec.prune_files) { content.prune_files = *spec.prune_files; }
  if (spec.prune_volumes) { content.prune_volumes = *spec.prune_volumes; }
  if (spec.purge_migration_job) {
    content.purge_migration_job = *spec.purge_migration_job;
  }
  if (spec.spool_attributes) {
    content.spool_attributes = *spec.spool_attributes;
  }
  if (spec.spool_data) { content.spool_data = *spec.spool_data; }
  if (spec.spool_size) { content.spool_size = *spec.spool_size; }
  if (spec.rerun_failed_levels) {
    content.rerun_failed_levels = *spec.rerun_failed_levels;
  }
  if (spec.prefer_mounted_volumes) {
    content.prefer_mounted_volumes = *spec.prefer_mounted_volumes;
  }
  if (spec.maximum_concurrent_jobs) {
    content.maximum_concurrent_jobs = *spec.maximum_concurrent_jobs;
  }
  if (spec.reschedule_on_error) {
    content.reschedule_on_error = *spec.reschedule_on_error;
  }
  if (spec.reschedule_interval) {
    content.reschedule_interval = *spec.reschedule_interval;
  }
  if (spec.reschedule_times) {
    content.reschedule_times = *spec.reschedule_times;
  }
  if (spec.priority) { content.priority = *spec.priority; }
  if (spec.allow_mixed_priority) {
    content.allow_mixed_priority = *spec.allow_mixed_priority;
  }
  if (spec.selection_type) {
    auto normalized_selection_type
        = NormalizeDirectorJobSelectionType(*spec.selection_type);
    if (!normalized_selection_type) {
      return {.error = normalized_selection_type.error};
    }
    content.selection_type = *normalized_selection_type.value;
  }
  if (spec.selection_pattern) {
    content.selection_pattern = *spec.selection_pattern;
  }
  if (spec.accurate) { content.accurate = *spec.accurate; }
  if (spec.allow_duplicate_jobs) {
    content.allow_duplicate_jobs = *spec.allow_duplicate_jobs;
  }
  if (spec.allow_higher_duplicates) {
    content.allow_higher_duplicates = *spec.allow_higher_duplicates;
  }
  if (spec.cancel_lower_level_duplicates) {
    content.cancel_lower_level_duplicates = *spec.cancel_lower_level_duplicates;
  }
  if (spec.cancel_queued_duplicates) {
    content.cancel_queued_duplicates = *spec.cancel_queued_duplicates;
  }
  if (spec.cancel_running_duplicates) {
    content.cancel_running_duplicates = *spec.cancel_running_duplicates;
  }
  if (spec.save_file_history) {
    content.save_file_history = *spec.save_file_history;
  }
  if (spec.file_history_size) {
    content.file_history_size = *spec.file_history_size;
  }
  if (spec.fd_plugin_options) {
    content.fd_plugin_options = *spec.fd_plugin_options;
  }
  if (spec.sd_plugin_options) {
    content.sd_plugin_options = *spec.sd_plugin_options;
  }
  if (spec.dir_plugin_options) {
    content.dir_plugin_options = *spec.dir_plugin_options;
  }
  if (spec.max_concurrent_copies) {
    content.max_concurrent_copies = *spec.max_concurrent_copies;
  }
  if (spec.always_incremental) {
    content.always_incremental = *spec.always_incremental;
  }
  if (spec.always_incremental_job_retention) {
    content.always_incremental_job_retention
        = *spec.always_incremental_job_retention;
  }
  if (spec.always_incremental_keep_number) {
    content.always_incremental_keep_number
        = *spec.always_incremental_keep_number;
  }
  if (spec.always_incremental_max_full_age) {
    content.always_incremental_max_full_age
        = *spec.always_incremental_max_full_age;
  }
  if (spec.max_full_consolidations) {
    content.max_full_consolidations = *spec.max_full_consolidations;
  }
  if (spec.run_on_incoming_connect_interval) {
    content.run_on_incoming_connect_interval
        = *spec.run_on_incoming_connect_interval;
  }
  if (spec.enabled) { content.enabled = *spec.enabled; }

  const auto rendered
      = BuildDirectorJobDefsResourceContent(jobdefs_name, content);
  const auto resource_directory
      = director_config.value->path / "bareos-dir.d" / "jobdefs";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create director jobdefs directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "JobDefs", jobdefs_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write director jobdefs resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote director jobdefs file '" + context.value->file_path.string()
           + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("director jobdefs update for '" + std::string{jobdefs_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          director_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director jobdefs parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director jobdefs update ", loaded.messages)};
  }

  DebugLog("updated director jobdefs resource '" + std::string{jobdefs_name}
           + "' in director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteDirectorJobDefsResource(std::string_view deployment_id,
                                            std::string_view director_name,
                                            std::string_view jobdefs_name) const
{
  DebugLog("deleting director jobdefs resource for deployment '"
           + std::string{deployment_id} + "', director '"
           + std::string{director_name} + "', jobdefs '"
           + std::string{jobdefs_name} + "'");
  if (!IsSafePathSegment(jobdefs_name) || !IsSafePathSegment(director_name)) {
    return {.error = "jobdefs and director names must be safe path segments."};
  }

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorJobDefsWriteContext(*director_config.value, jobdefs_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director '" + std::string{director_name}
                     + "' does not define jobdefs '" + std::string{jobdefs_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               director_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "JobDefs", jobdefs_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared director jobdefs resource file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "director jobdefs parser initialization ", loaded.messages)};
    }
    return {.error
            = FormatParseFailure("director jobdefs delete ", loaded.messages)};
  }

  DebugLog("deleted director jobdefs resource '" + std::string{jobdefs_name}
           + "' from director '" + std::string{director_name} + "'");
  return {.value = *director_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertStorageMessagesResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view messages_name,
    const StorageMessagesResourceSpec& spec) const
{
  DebugLog("upserting storage-daemon messages resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', messages '"
           + std::string{messages_name} + "'");
  if (!IsSafePathSegment(messages_name) || !IsSafePathSegment(storage_name)) {
    return {.error = "messages and storage names must be safe path segments."};
  }

  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  auto context
      = LoadStorageMessagesWriteContext(*storage_config.value, messages_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description = spec.description
                            ? *spec.description
                            : content.description.value_or(
                                  DefaultStorageDaemonMessagesDescription(
                                      messages_name, storage_name));
  if (spec.mail_command) { content.mail_command = *spec.mail_command; }
  if (spec.operator_command) {
    content.operator_command = *spec.operator_command;
  }
  if (spec.timestamp_format) {
    content.timestamp_format = *spec.timestamp_format;
  }
  if (spec.syslog_entries) { content.syslog_entries = *spec.syslog_entries; }
  if (spec.mail_entries) { content.mail_entries = *spec.mail_entries; }
  if (spec.mail_on_error_entries) {
    content.mail_on_error_entries = *spec.mail_on_error_entries;
  }
  if (spec.mail_on_success_entries) {
    content.mail_on_success_entries = *spec.mail_on_success_entries;
  }
  if (spec.file_entries) { content.file_entries = *spec.file_entries; }
  if (spec.append_entries) { content.append_entries = *spec.append_entries; }
  if (spec.stdout_entries) { content.stdout_entries = *spec.stdout_entries; }
  if (spec.stderr_entries) { content.stderr_entries = *spec.stderr_entries; }
  if (spec.director_entries) {
    content.director_entries = *spec.director_entries;
  }
  if (spec.console_entries) { content.console_entries = *spec.console_entries; }
  if (spec.operator_entries) {
    content.operator_entries = *spec.operator_entries;
  }
  if (spec.catalog_entries) { content.catalog_entries = *spec.catalog_entries; }
  if (spec.entries) { content.entries = *spec.entries; }

  const auto rendered
      = BuildDirectorMessagesResourceContent(messages_name, content);
  const auto resource_directory
      = storage_config.value->path / "bareos-sd.d" / "messages";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon messages directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Messages", messages_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write storage-daemon messages resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote storage-daemon messages file '"
           + context.value->file_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("storage-daemon messages update for '" + std::string{messages_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon messages parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon messages update ",
                                        loaded.messages)};
  }

  DebugLog("updated storage-daemon messages resource '"
           + std::string{messages_name} + "' in storage '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteStorageMessagesResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view messages_name) const
{
  DebugLog("deleting storage-daemon messages resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', messages '"
           + std::string{messages_name} + "'");
  if (!IsSafePathSegment(messages_name) || !IsSafePathSegment(storage_name)) {
    return {.error = "messages and storage names must be safe path segments."};
  }

  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  auto context
      = LoadStorageMessagesWriteContext(*storage_config.value, messages_name);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage '" + std::string{storage_name}
                     + "' does not define messages '"
                     + std::string{messages_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               storage_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Messages", messages_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {
          .error
          = "failed to update shared storage-daemon messages resource file '"
            + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon messages parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon messages delete ",
                                        loaded.messages)};
  }

  DebugLog("deleted storage-daemon messages resource '"
           + std::string{messages_name} + "' from storage '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertStorageDirectorResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view director_name,
    const StorageDirectorResourceSpec& spec) const
{
  DebugLog("upserting storage-daemon director resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(director_name)) {
    return {.error = "storage and director names must be safe path segments."};
  }

  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageDaemonDirectorWriteContext(
      *storage_config.value, director_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }

  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required for storage-daemon "
              "director resources."};
  }

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultStorageDaemonDirectorDescription(
                                         director_name, storage_name));
  const auto monitor = spec.monitor ? spec.monitor : context.value->monitor;
  const auto maximum_bandwidth_per_job
      = spec.maximum_bandwidth_per_job
            ? spec.maximum_bandwidth_per_job
            : context.value->maximum_bandwidth_per_job;
  const auto key_encryption_key = spec.key_encryption_key
                                      ? spec.key_encryption_key
                                      : context.value->key_encryption_key;
  const auto tls_authenticate = spec.tls_authenticate
                                    ? spec.tls_authenticate
                                    : context.value->tls_authenticate;
  const auto tls_enable
      = spec.tls_enable ? spec.tls_enable : context.value->tls_enable;
  const auto tls_require
      = spec.tls_require ? spec.tls_require : context.value->tls_require;
  const auto tls_verify_peer = spec.tls_verify_peer
                                   ? spec.tls_verify_peer
                                   : context.value->tls_verify_peer;
  const auto tls_cipher_list = spec.tls_cipher_list
                                   ? spec.tls_cipher_list
                                   : context.value->tls_cipher_list;
  const auto tls_cipher_suites = spec.tls_cipher_suites
                                     ? spec.tls_cipher_suites
                                     : context.value->tls_cipher_suites;
  const auto tls_dh_file
      = spec.tls_dh_file ? spec.tls_dh_file : context.value->tls_dh_file;
  const auto tls_protocol
      = spec.tls_protocol ? spec.tls_protocol : context.value->tls_protocol;
  const auto tls_ca_certificate_file
      = spec.tls_ca_certificate_file ? spec.tls_ca_certificate_file
                                     : context.value->tls_ca_certificate_file;
  const auto tls_ca_certificate_dir
      = spec.tls_ca_certificate_dir ? spec.tls_ca_certificate_dir
                                    : context.value->tls_ca_certificate_dir;
  const auto tls_certificate_revocation_list
      = spec.tls_certificate_revocation_list
            ? spec.tls_certificate_revocation_list
            : context.value->tls_certificate_revocation_list;
  const auto tls_certificate = spec.tls_certificate
                                   ? spec.tls_certificate
                                   : context.value->tls_certificate;
  const auto tls_key = spec.tls_key ? spec.tls_key : context.value->tls_key;
  const auto tls_allowed_cn = spec.tls_allowed_cn
                                  ? spec.tls_allowed_cn
                                  : context.value->tls_allowed_cn;
  const auto rendered = BuildStorageDaemonDirectorResourceContent(
      director_name, *password, description, monitor, maximum_bandwidth_per_job,
      key_encryption_key, tls_authenticate, tls_enable, tls_require,
      tls_verify_peer, tls_cipher_list, tls_cipher_suites, tls_dh_file,
      tls_protocol, tls_ca_certificate_file, tls_ca_certificate_dir,
      tls_certificate_revocation_list, tls_certificate, tls_key,
      tls_allowed_cn);
  const auto resource_directory
      = storage_config.value->path / "bareos-sd.d" / "director";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon director directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Director", director_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write storage-daemon director resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote storage-daemon director file '"
           + context.value->file_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("storage-daemon director update for '" + std::string{director_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon director parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon director update ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  SetManagedPath(updated_managed_paths, repository_root,
                 context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("updated storage-daemon director resource '"
           + std::string{director_name} + "' in storage '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteStorageDirectorResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view director_name) const
{
  DebugLog("deleting storage-daemon director resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', director '"
           + std::string{director_name} + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(director_name)) {
    return {.error = "storage and director names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageDaemonDirectorWriteContext(
      *storage_config.value, director_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage '" + std::string{storage_name}
                     + "' does not define director '"
                     + std::string{director_name} + "'."};
  }
  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               storage_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Director", director_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {
          .error
          = "failed to update shared storage-daemon director resource file '"
            + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon director parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon director delete ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  RemoveManagedPath(updated_managed_paths, repository_root,
                    context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("deleted storage-daemon director resource '"
           + std::string{director_name} + "' from storage '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertStorageDeviceResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view device_name,
    const StorageDeviceResourceSpec& spec) const
{
  DebugLog("upserting storage-daemon device resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', device '"
           + std::string{device_name} + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(device_name)) {
    return {.error = "storage and device names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageDaemonDeviceWriteContext(
      *storage_config.value, device_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }

  const auto media_type
      = spec.media_type ? *spec.media_type : context.value->media_type;
  if (!media_type || media_type->empty()) {
    return {.error
            = "field 'media_type' is required for storage-daemon "
              "device resources."};
  }
  const auto archive_device = spec.archive_device
                                  ? *spec.archive_device
                                  : context.value->archive_device;
  if (!archive_device || archive_device->empty()) {
    return {.error
            = "field 'archive_device' is required for storage-daemon "
              "device resources."};
  }
  const auto device_type
      = spec.device_type ? *spec.device_type : context.value->device_type;
  if (!device_type || device_type->empty()) {
    return {.error
            = "field 'device_type' is required for storage-daemon "
              "device resources."};
  }
  auto access_mode
      = spec.access_mode ? spec.access_mode : context.value->access_mode;
  if (access_mode) {
    auto normalized_access_mode
        = NormalizeStorageDeviceIoDirection(*access_mode);
    if (!normalized_access_mode) {
      return {.error = normalized_access_mode.error};
    }
    access_mode = *normalized_access_mode.value;
  }

  const auto device_options = spec.device_options
                                  ? spec.device_options
                                  : context.value->device_options;
  const auto diagnostic_device = spec.diagnostic_device
                                     ? spec.diagnostic_device
                                     : context.value->diagnostic_device;
  const auto hardware_end_of_file = spec.hardware_end_of_file
                                        ? spec.hardware_end_of_file
                                        : context.value->hardware_end_of_file;
  const auto hardware_end_of_medium
      = spec.hardware_end_of_medium ? spec.hardware_end_of_medium
                                    : context.value->hardware_end_of_medium;
  const auto backward_space_record = spec.backward_space_record
                                         ? spec.backward_space_record
                                         : context.value->backward_space_record;
  const auto backward_space_file = spec.backward_space_file
                                       ? spec.backward_space_file
                                       : context.value->backward_space_file;
  const auto bsf_at_eom
      = spec.bsf_at_eom ? spec.bsf_at_eom : context.value->bsf_at_eom;
  const auto two_eof = spec.two_eof ? spec.two_eof : context.value->two_eof;
  const auto forward_space_record = spec.forward_space_record
                                        ? spec.forward_space_record
                                        : context.value->forward_space_record;
  const auto forward_space_file = spec.forward_space_file
                                      ? spec.forward_space_file
                                      : context.value->forward_space_file;
  const auto fast_forward_space_file
      = spec.fast_forward_space_file ? spec.fast_forward_space_file
                                     : context.value->fast_forward_space_file;
  const auto removable_media = spec.removable_media
                                   ? spec.removable_media
                                   : context.value->removable_media;
  const auto random_access
      = spec.random_access ? spec.random_access : context.value->random_access;
  const auto automatic_mount = spec.automatic_mount
                                   ? spec.automatic_mount
                                   : context.value->automatic_mount;
  const auto label_media
      = spec.label_media ? spec.label_media : context.value->label_media;
  const auto always_open
      = spec.always_open ? spec.always_open : context.value->always_open;
  const auto autochanger
      = spec.autochanger ? spec.autochanger : context.value->autochanger;
  const auto close_on_poll
      = spec.close_on_poll ? spec.close_on_poll : context.value->close_on_poll;
  const auto block_positioning = spec.block_positioning
                                     ? spec.block_positioning
                                     : context.value->block_positioning;
  const auto use_mtiocget
      = spec.use_mtiocget ? spec.use_mtiocget : context.value->use_mtiocget;
  const auto check_labels
      = spec.check_labels ? spec.check_labels : context.value->check_labels;
  const auto requires_mount = spec.requires_mount
                                  ? spec.requires_mount
                                  : context.value->requires_mount;
  const auto offline_on_unmount = spec.offline_on_unmount
                                      ? spec.offline_on_unmount
                                      : context.value->offline_on_unmount;
  const auto block_checksum = spec.block_checksum
                                  ? spec.block_checksum
                                  : context.value->block_checksum;
  const auto auto_select
      = spec.auto_select ? spec.auto_select : context.value->auto_select;
  const auto changer_device = spec.changer_device
                                  ? spec.changer_device
                                  : context.value->changer_device;
  const auto changer_command = spec.changer_command
                                   ? spec.changer_command
                                   : context.value->changer_command;
  const auto alert_command
      = spec.alert_command ? spec.alert_command : context.value->alert_command;
  const auto maximum_changer_wait = spec.maximum_changer_wait
                                        ? spec.maximum_changer_wait
                                        : context.value->maximum_changer_wait;
  const auto maximum_open_wait = spec.maximum_open_wait
                                     ? spec.maximum_open_wait
                                     : context.value->maximum_open_wait;
  const auto maximum_open_volumes = spec.maximum_open_volumes
                                        ? spec.maximum_open_volumes
                                        : context.value->maximum_open_volumes;
  const auto maximum_network_buffer_size
      = spec.maximum_network_buffer_size
            ? spec.maximum_network_buffer_size
            : context.value->maximum_network_buffer_size;
  const auto volume_poll_interval = spec.volume_poll_interval
                                        ? spec.volume_poll_interval
                                        : context.value->volume_poll_interval;
  const auto maximum_rewind_wait = spec.maximum_rewind_wait
                                       ? spec.maximum_rewind_wait
                                       : context.value->maximum_rewind_wait;
  const auto label_block_size = spec.label_block_size
                                    ? spec.label_block_size
                                    : context.value->label_block_size;
  const auto minimum_block_size = spec.minimum_block_size
                                      ? spec.minimum_block_size
                                      : context.value->minimum_block_size;
  const auto maximum_block_size = spec.maximum_block_size
                                      ? spec.maximum_block_size
                                      : context.value->maximum_block_size;
  const auto maximum_file_size = spec.maximum_file_size
                                     ? spec.maximum_file_size
                                     : context.value->maximum_file_size;
  const auto volume_capacity = spec.volume_capacity
                                   ? spec.volume_capacity
                                   : context.value->volume_capacity;
  const auto maximum_concurrent_jobs
      = spec.maximum_concurrent_jobs ? spec.maximum_concurrent_jobs
                                     : context.value->maximum_concurrent_jobs;
  const auto spool_directory = spec.spool_directory
                                   ? spec.spool_directory
                                   : context.value->spool_directory;
  const auto maximum_spool_size = spec.maximum_spool_size
                                      ? spec.maximum_spool_size
                                      : context.value->maximum_spool_size;
  const auto maximum_job_spool_size
      = spec.maximum_job_spool_size ? spec.maximum_job_spool_size
                                    : context.value->maximum_job_spool_size;
  const auto drive_index
      = spec.drive_index ? spec.drive_index : context.value->drive_index;
  const auto mount_point
      = spec.mount_point ? spec.mount_point : context.value->mount_point;
  const auto mount_command
      = spec.mount_command ? spec.mount_command : context.value->mount_command;
  const auto unmount_command = spec.unmount_command
                                   ? spec.unmount_command
                                   : context.value->unmount_command;
  const auto label_type
      = spec.label_type ? spec.label_type : context.value->label_type;
  const auto no_rewind_on_close = spec.no_rewind_on_close
                                      ? spec.no_rewind_on_close
                                      : context.value->no_rewind_on_close;
  const auto drive_tape_alert_enabled
      = spec.drive_tape_alert_enabled ? spec.drive_tape_alert_enabled
                                      : context.value->drive_tape_alert_enabled;
  const auto drive_crypto_enabled = spec.drive_crypto_enabled
                                        ? spec.drive_crypto_enabled
                                        : context.value->drive_crypto_enabled;
  const auto query_crypto_status = spec.query_crypto_status
                                       ? spec.query_crypto_status
                                       : context.value->query_crypto_status;
  auto auto_deflate
      = spec.auto_deflate ? spec.auto_deflate : context.value->auto_deflate;
  if (auto_deflate) {
    auto normalized_auto_deflate
        = NormalizeStorageDeviceIoDirection(*auto_deflate);
    if (!normalized_auto_deflate) {
      return {.error = normalized_auto_deflate.error};
    }
    auto_deflate = *normalized_auto_deflate.value;
  }
  auto auto_deflate_algorithm = spec.auto_deflate_algorithm
                                    ? spec.auto_deflate_algorithm
                                    : context.value->auto_deflate_algorithm;
  if (auto_deflate_algorithm) {
    auto normalized_auto_deflate_algorithm
        = NormalizeStorageDeviceCompressionAlgorithm(*auto_deflate_algorithm);
    if (!normalized_auto_deflate_algorithm) {
      return {.error = normalized_auto_deflate_algorithm.error};
    }
    auto_deflate_algorithm = *normalized_auto_deflate_algorithm.value;
  }
  const auto auto_deflate_level = spec.auto_deflate_level
                                      ? spec.auto_deflate_level
                                      : context.value->auto_deflate_level;
  auto auto_inflate
      = spec.auto_inflate ? spec.auto_inflate : context.value->auto_inflate;
  if (auto_inflate) {
    auto normalized_auto_inflate
        = NormalizeStorageDeviceIoDirection(*auto_inflate);
    if (!normalized_auto_inflate) {
      return {.error = normalized_auto_inflate.error};
    }
    auto_inflate = *normalized_auto_inflate.value;
  }
  const auto collect_statistics = spec.collect_statistics
                                      ? spec.collect_statistics
                                      : context.value->collect_statistics;
  const auto eof_on_error_is_eot = spec.eof_on_error_is_eot
                                       ? spec.eof_on_error_is_eot
                                       : context.value->eof_on_error_is_eot;
  const auto count = spec.count ? spec.count : context.value->count;
  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultStorageDaemonDeviceDescription(
                                         device_name, storage_name));
  const auto rendered = BuildStorageDaemonDeviceResourceContent(
      device_name, *media_type, *archive_device, *device_type, access_mode,
      device_options, diagnostic_device, hardware_end_of_file,
      hardware_end_of_medium, backward_space_record, backward_space_file,
      bsf_at_eom, two_eof, forward_space_record, forward_space_file,
      fast_forward_space_file, removable_media, random_access, automatic_mount,
      label_media, always_open, autochanger, close_on_poll, block_positioning,
      use_mtiocget, check_labels, requires_mount, offline_on_unmount,
      block_checksum, auto_select, changer_device, changer_command,
      alert_command, maximum_changer_wait, maximum_open_wait,
      maximum_open_volumes, maximum_network_buffer_size, volume_poll_interval,
      maximum_rewind_wait, label_block_size, minimum_block_size,
      maximum_block_size, maximum_file_size, volume_capacity,
      maximum_concurrent_jobs, spool_directory, maximum_spool_size,
      maximum_job_spool_size, drive_index, mount_point, mount_command,
      unmount_command, label_type, no_rewind_on_close, drive_tape_alert_enabled,
      drive_crypto_enabled, query_crypto_status, auto_deflate,
      auto_deflate_algorithm, auto_deflate_level, auto_inflate,
      collect_statistics, eof_on_error_is_eot, count, description);
  const auto resource_directory
      = storage_config.value->path / "bareos-sd.d" / "device";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon device directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Device", device_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write storage-daemon device resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote storage-daemon device file '"
           + context.value->file_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("storage-daemon device update for '" + std::string{device_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {
          .error = FormatParseFailure(
              "storage-daemon device parser initialization ", loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon device update ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  SetManagedPath(updated_managed_paths, repository_root,
                 context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("updated storage-daemon device resource '" + std::string{device_name}
           + "' in storage '" + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteStorageDeviceResource(std::string_view deployment_id,
                                          std::string_view storage_name,
                                          std::string_view device_name) const
{
  DebugLog("deleting storage-daemon device resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', device '"
           + std::string{device_name} + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(device_name)) {
    return {.error = "storage and device names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageDaemonDeviceWriteContext(
      *storage_config.value, device_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage '" + std::string{storage_name}
                     + "' does not define device '" + std::string{device_name}
                     + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               storage_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Device", device_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared storage-daemon device resource "
                "file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {
          .error = FormatParseFailure(
              "storage-daemon device parser initialization ", loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon device delete ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  RemoveManagedPath(updated_managed_paths, repository_root,
                    context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("deleted storage-daemon device resource '" + std::string{device_name}
           + "' from storage '" + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord> ServiceState::UpsertStorageNdmpResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view ndmp_name,
    const StorageNdmpResourceSpec& spec) const
{
  DebugLog("upserting storage-daemon NDMP resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', ndmp '" + std::string{ndmp_name}
           + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(ndmp_name)) {
    return {.error = "storage and NDMP names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageNdmpWriteContext(*storage_config.value, ndmp_name,
                                             *managed_paths.value);
  if (!context) { return {.error = context.error}; }

  const auto description
      = spec.description
            ? *spec.description
            : context.value->description.value_or(
                  DefaultStorageDaemonNdmpDescription(ndmp_name, storage_name));
  const auto username
      = spec.username ? *spec.username : context.value->username;
  if (!username || username->empty()) {
    return {.error
            = "field 'username' is required for storage-daemon NDMP "
              "resources."};
  }
  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required for storage-daemon NDMP "
              "resources."};
  }
  const auto auth_type_raw
      = spec.auth_type ? *spec.auth_type
                       : context.value->auth_type.value_or(std::string{"None"});
  auto auth_type = NormalizeAuthType(auth_type_raw);
  if (!auth_type) { return {.error = auth_type.error}; }
  const auto log_level
      = spec.log_level ? *spec.log_level : context.value->log_level.value_or(4);

  const auto rendered = BuildStorageDaemonNdmpResourceContent(
      ndmp_name, description, *username, *password, *auth_type.value,
      log_level);
  const auto resource_directory
      = storage_config.value->path / "bareos-sd.d" / "ndmp";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon NDMP directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(context.value->file_path,
                                                  "Ndmp", ndmp_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write storage-daemon NDMP resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote storage-daemon NDMP file '"
           + context.value->file_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("storage-daemon NDMP update for '" + std::string{ndmp_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error
              = FormatParseFailure("storage-daemon NDMP parser initialization ",
                                   loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon NDMP update ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  SetManagedPath(updated_managed_paths, repository_root,
                 context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("updated storage-daemon NDMP resource '" + std::string{ndmp_name}
           + "' in storage '" + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord> ServiceState::DeleteStorageNdmpResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view ndmp_name) const
{
  DebugLog("deleting storage-daemon NDMP resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', ndmp '" + std::string{ndmp_name}
           + "'");
  if (!IsSafePathSegment(storage_name) || !IsSafePathSegment(ndmp_name)) {
    return {.error = "storage and NDMP names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageNdmpWriteContext(*storage_config.value, ndmp_name,
                                             *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage '" + std::string{storage_name}
                     + "' does not define NDMP resource '"
                     + std::string{ndmp_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               storage_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Ndmp", ndmp_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared storage-daemon NDMP resource "
                "file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error
              = FormatParseFailure("storage-daemon NDMP parser initialization ",
                                   loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon NDMP delete ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  RemoveManagedPath(updated_managed_paths, repository_root,
                    context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("deleted storage-daemon NDMP resource '" + std::string{ndmp_name}
           + "' from storage '" + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertStorageAutochangerResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view autochanger_name,
    const StorageAutochangerResourceSpec& spec) const
{
  DebugLog("upserting storage-daemon autochanger resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', autochanger '"
           + std::string{autochanger_name} + "'");
  if (!IsSafePathSegment(storage_name)
      || !IsSafePathSegment(autochanger_name)) {
    return {.error
            = "storage and autochanger names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageAutochangerWriteContext(
      *storage_config.value, autochanger_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }

  const auto devices = spec.devices ? *spec.devices : context.value->devices;
  if (!devices || devices->empty()) {
    return {.error
            = "field 'devices' must contain at least one storage-daemon device "
              "resource name."};
  }
  const auto changer_device = spec.changer_device
                                  ? *spec.changer_device
                                  : context.value->changer_device;
  if (!changer_device || changer_device->empty()) {
    return {.error
            = "field 'changer_device' is required for storage-daemon "
              "autochanger resources."};
  }
  const auto changer_command = spec.changer_command
                                   ? *spec.changer_command
                                   : context.value->changer_command;
  if (!changer_command || changer_command->empty()) {
    return {.error
            = "field 'changer_command' is required for storage-daemon "
              "autochanger resources."};
  }
  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultStorageDaemonAutochangerDescription(
                                         autochanger_name, storage_name));

  const auto rendered = BuildStorageDaemonAutochangerResourceContent(
      autochanger_name, *devices, *changer_device, *changer_command,
      description);
  const auto resource_directory
      = storage_config.value->path / "bareos-sd.d" / "autochanger";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon autochanger directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Autochanger", autochanger_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write storage-daemon autochanger resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote storage-daemon autochanger file '"
           + context.value->file_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("storage-daemon autochanger update for '"
             + std::string{autochanger_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon autochanger parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon autochanger update ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  SetManagedPath(updated_managed_paths, repository_root,
                 context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("updated storage-daemon autochanger resource '"
           + std::string{autochanger_name} + "' in storage '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::DeleteStorageAutochangerResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view autochanger_name) const
{
  DebugLog("deleting storage-daemon autochanger resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "', autochanger '"
           + std::string{autochanger_name} + "'");
  if (!IsSafePathSegment(storage_name)
      || !IsSafePathSegment(autochanger_name)) {
    return {.error
            = "storage and autochanger names must be safe path segments."};
  }

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  const auto repository_root
      = RepositoryRootFromConfigPath(storage_config.value->path);
  auto managed_paths = LoadManagedPaths(repository_root);
  if (!managed_paths) { return {.error = managed_paths.error}; }

  auto context = LoadStorageAutochangerWriteContext(
      *storage_config.value, autochanger_name, *managed_paths.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "storage '" + std::string{storage_name}
                     + "' does not define autochanger '"
                     + std::string{autochanger_name} + "'."};
  }

  auto original_content = ReadFile(context.value->file_path);
  if (!original_content) { return {.error = original_content.error}; }
  if (context.value->is_standalone_file) {
    if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                               storage_config.value->path);
        error) {
      return {.error = *error};
    }
  } else {
    auto rewritten
        = RewriteNamedTopLevelResource(context.value->file_path, "Autochanger",
                                       autochanger_name, std::nullopt);
    if (!rewritten) { return {.error = rewritten.error}; }
    if (!WriteFile(context.value->file_path, *rewritten.value)) {
      return {.error
              = "failed to update shared storage-daemon autochanger resource "
                "file '"
                + context.value->file_path.string() + "'."};
    }
  }

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon autochanger parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon autochanger delete ",
                                        loaded.messages)};
  }

  auto updated_managed_paths = *managed_paths.value;
  RemoveManagedPath(updated_managed_paths, repository_root,
                    context.value->file_path);
  if (auto error = WriteManagedPaths(repository_root, updated_managed_paths);
      error) {
    auto rollback_error
        = RestoreDeletedFile(context.value->file_path, *original_content.value);
    if (rollback_error) { return {.error = *rollback_error}; }
    return {.error = *error};
  }

  DebugLog("deleted storage-daemon autochanger resource '"
           + std::string{autochanger_name} + "' from storage '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<DeploymentConfigRecord>
ServiceState::UpsertStorageDaemonResource(
    std::string_view deployment_id,
    std::string_view storage_name,
    const StorageDaemonResourceSpec& spec) const
{
  DebugLog("upserting storage-daemon storage resource for deployment '"
           + std::string{deployment_id} + "', storage '"
           + std::string{storage_name} + "'");
  if (!IsSafePathSegment(storage_name)) {
    return {.error = "storage name must be a safe path segment."};
  }
  if (spec.address && !IsSafeBareosToken(*spec.address)) {
    return {.error
            = "storage-daemon address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  if (spec.source_address && !IsSafeBareosToken(*spec.source_address)) {
    return {.error
            = "storage-daemon source_address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  if (spec.source_addresses) {
    for (const auto& value : *spec.source_addresses) {
      if (!IsSafeBareosToken(value)) {
        return {.error
                = "storage-daemon source_addresses entries must be bare "
                  "Bareos tokens without whitespace or quotes."};
      }
    }
  }
  if (spec.ndmp_address && !IsSafeBareosToken(*spec.ndmp_address)) {
    return {.error
            = "storage-daemon ndmp_address must be a bare Bareos token "
              "without whitespace or quotes."};
  }
  if (spec.port && *spec.port == 0) {
    return {.error = "storage-daemon port must be greater than zero."};
  }
  if (spec.ndmp_port && *spec.ndmp_port == 0) {
    return {.error = "storage-daemon ndmp_port must be greater than zero."};
  }
  auto storage_config = EnsureDeploymentConfigRoot(
      *this, deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) { return {.error = storage_config.error}; }

  auto context = LoadStorageDaemonWriteContext(*storage_config.value);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  if (spec.address) { content.address = spec.address; }
  if (spec.addresses) {
    content.addresses = *spec.addresses;
    content.address.reset();
    content.port.reset();
  }
  if (spec.source_addresses) {
    content.source_addresses = *spec.source_addresses;
    content.source_address.reset();
  } else if (spec.source_address) {
    content.source_address = spec.source_address;
    content.source_addresses.clear();
  }
  if (spec.port) {
    content.port = spec.port;
    content.addresses.clear();
  }
  if (spec.just_in_time_reservation) {
    content.just_in_time_reservation = spec.just_in_time_reservation;
  }
  if (spec.maximum_concurrent_jobs) {
    content.maximum_concurrent_jobs = spec.maximum_concurrent_jobs;
  }
  if (spec.absolute_job_timeout) {
    content.absolute_job_timeout = spec.absolute_job_timeout;
  }
  if (spec.allow_bandwidth_bursting) {
    content.allow_bandwidth_bursting = spec.allow_bandwidth_bursting;
  }
  if (spec.tls_authenticate) {
    content.tls_authenticate = spec.tls_authenticate;
  }
  if (spec.tls_enable) { content.tls_enable = spec.tls_enable; }
  if (spec.tls_require) { content.tls_require = spec.tls_require; }
  if (spec.tls_verify_peer) { content.tls_verify_peer = spec.tls_verify_peer; }
  if (spec.tls_cipher_list) { content.tls_cipher_list = spec.tls_cipher_list; }
  if (spec.tls_cipher_suites) {
    content.tls_cipher_suites = spec.tls_cipher_suites;
  }
  if (spec.tls_dh_file) { content.tls_dh_file = spec.tls_dh_file; }
  if (spec.tls_protocol) { content.tls_protocol = spec.tls_protocol; }
  if (spec.tls_ca_certificate_file) {
    content.tls_ca_certificate_file = spec.tls_ca_certificate_file;
  }
  if (spec.tls_ca_certificate_dir) {
    content.tls_ca_certificate_dir = spec.tls_ca_certificate_dir;
  }
  if (spec.tls_certificate_revocation_list) {
    content.tls_certificate_revocation_list
        = spec.tls_certificate_revocation_list;
  }
  if (spec.tls_certificate) { content.tls_certificate = spec.tls_certificate; }
  if (spec.tls_key) { content.tls_key = spec.tls_key; }
  if (spec.tls_allowed_cn) { content.tls_allowed_cn = *spec.tls_allowed_cn; }
  if (spec.ndmp_enable) { content.ndmp_enable = spec.ndmp_enable; }
  if (spec.ndmp_snooping) { content.ndmp_snooping = spec.ndmp_snooping; }
  if (spec.ndmp_log_level) { content.ndmp_log_level = spec.ndmp_log_level; }
  if (spec.ndmp_addresses) {
    content.ndmp_addresses = *spec.ndmp_addresses;
    content.ndmp_address.reset();
    content.ndmp_port.reset();
  } else if (spec.ndmp_address || spec.ndmp_port) {
    content.ndmp_addresses.clear();
  }
  if (spec.ndmp_address) { content.ndmp_address = spec.ndmp_address; }
  if (spec.ndmp_port) { content.ndmp_port = spec.ndmp_port; }
  if (spec.autoxflate_on_replication) {
    content.autoxflate_on_replication = spec.autoxflate_on_replication;
  }
  if (spec.collect_device_statistics) {
    content.collect_device_statistics = spec.collect_device_statistics;
  }
  if (spec.collect_job_statistics) {
    content.collect_job_statistics = spec.collect_job_statistics;
  }
  if (spec.statistics_collect_interval) {
    content.statistics_collect_interval = spec.statistics_collect_interval;
  }
  if (spec.device_reserve_by_media_type) {
    content.device_reserve_by_media_type = spec.device_reserve_by_media_type;
  }
  if (spec.file_device_concurrent_read) {
    content.file_device_concurrent_read = spec.file_device_concurrent_read;
  }
  if (spec.ver_id) { content.ver_id = spec.ver_id; }
  if (spec.log_timestamp_format) {
    content.log_timestamp_format = spec.log_timestamp_format;
  }
  if (spec.maximum_bandwidth_per_job) {
    content.maximum_bandwidth_per_job = spec.maximum_bandwidth_per_job;
  }
  if (spec.secure_erase_command) {
    content.secure_erase_command = spec.secure_erase_command;
  }
  if (spec.enable_ktls) { content.enable_ktls = spec.enable_ktls; }
  if (spec.sd_connect_timeout) {
    content.sd_connect_timeout = spec.sd_connect_timeout;
  }
  if (spec.fd_connect_timeout) {
    content.fd_connect_timeout = spec.fd_connect_timeout;
  }
  if (spec.heartbeat_interval) {
    content.heartbeat_interval = spec.heartbeat_interval;
  }
  if (spec.checkpoint_interval) {
    content.checkpoint_interval = spec.checkpoint_interval;
  }
  if (spec.client_connect_wait) {
    content.client_connect_wait = spec.client_connect_wait;
  }
  if (spec.maximum_network_buffer_size) {
    content.maximum_network_buffer_size = spec.maximum_network_buffer_size;
  }
  if (spec.description) { content.description = spec.description; }
  if (spec.working_directory) {
    content.working_directory = spec.working_directory;
  }
  if (spec.plugin_directory) {
    content.plugin_directory = spec.plugin_directory;
  }
  if (spec.plugin_names) { content.plugin_names = *spec.plugin_names; }
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  if (spec.backend_directories) {
    content.backend_directories = *spec.backend_directories;
  }
  if (content.backend_directories.empty()) {
    for (const auto& backend_directory : DefaultStorageBackendDirectories()) {
      content.backend_directories.emplace_back(backend_directory.string());
    }
  }
#endif
  if (spec.scripts_directory) {
    content.scripts_directory = spec.scripts_directory;
  }
  if (spec.messages) { content.messages = spec.messages; }

  std::string render_error;
  const auto rendered
      = BuildStorageDaemonResourceContent(storage_name, content, &render_error);
  if (!render_error.empty()) { return {.error = render_error}; }
  const auto resource_directory
      = storage_config.value->path / "bareos-sd.d" / "storage";
  const bool file_existed = std::filesystem::exists(context.value->file_path);
  std::string original_content;
  if (file_existed) {
    auto existing = ReadFile(context.value->file_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  std::error_code error_code;
  std::filesystem::create_directories(resource_directory, error_code);
  if (error_code) {
    return {.error = "failed to create storage-daemon storage directory '"
                     + resource_directory.string()
                     + "': " + error_code.message()};
  }
  std::string file_content = rendered;
  if (context.value->exists && !context.value->is_standalone_file) {
    auto rewritten = RewriteNamedTopLevelResource(
        context.value->file_path, "Storage", storage_name, rendered);
    if (!rewritten) { return {.error = rewritten.error}; }
    file_content = std::move(*rewritten.value);
  }
  if (!WriteFile(context.value->file_path, file_content)) {
    return {.error = "failed to write storage-daemon storage resource '"
                     + context.value->file_path.string() + "'."};
  }
  DebugLog("wrote storage-daemon storage file '"
           + context.value->file_path.string() + "'");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.value->path.string(), true);
  if (!loaded.parser || !loaded.parse_ok) {
    DebugLog("storage-daemon storage update for '" + std::string{storage_name}
             + "' failed validation and will be rolled back");
    std::optional<std::string> rollback_error;
    if (file_existed) {
      rollback_error
          = RestoreClientStubFile(context.value->file_path, original_content);
    } else {
      rollback_error = CleanupCreatedFile(context.value->file_path,
                                          storage_config.value->path);
    }
    if (rollback_error) { return {.error = *rollback_error}; }
    if (!loaded.parser) {
      return {.error = FormatParseFailure(
                  "storage-daemon storage parser initialization ",
                  loaded.messages)};
    }
    return {.error = FormatParseFailure("storage-daemon storage update ",
                                        loaded.messages)};
  }

  DebugLog("updated storage-daemon storage resource '"
           + std::string{storage_name} + "'");
  return {.value = *storage_config.value};
}

OperationResult<std::vector<DeploymentImportRecord>>
ServiceState::ListDeploymentImports(std::string_view deployment_id) const
{
  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  return LoadDeploymentImports(repository_path);
}

OperationResult<DeploymentGitStatusRecord> ServiceState::GetDeploymentGitStatus(
    std::string_view deployment_id) const
{
  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  return LoadDeploymentGitStatus(repository_path);
}

OperationResult<DeploymentDiffPreviewRecord>
ServiceState::GetDeploymentDiffPreview(std::string_view deployment_id) const
{
  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) { return {.error = "deployment not found."}; }
    repository_path = it->second.repository_path;
  }

  return LoadDeploymentDiffPreview(repository_path);
}

OperationResult<JobRecord> ServiceState::CreateJob(const JobSpec& spec)
{
  DebugLog("queueing job type '" + spec.type + "'");
  if (spec.type.empty()) { return {.error = "job type must not be empty."}; }

  std::lock_guard guard{mutex_};

  if (spec.deployment_id
      && deployments_.find(*spec.deployment_id) == deployments_.end()) {
    return {.error = "deployment id does not exist."};
  }

  const auto timestamp = MakeTimestamp();
  JobRecord record{.id = "job-" + std::to_string(next_job_id_++),
                   .type = spec.type,
                   .deployment_id = spec.deployment_id,
                   .source_component = spec.source_component,
                   .source_path = spec.source_path,
                   .commit_message = spec.commit_message,
                   .status = JobStatus::kQueued,
                   .created_at = timestamp,
                   .updated_at = timestamp};

  jobs_.emplace(record.id, record);
  if (auto error = SaveStateLocked(); error) {
    jobs_.erase(record.id);
    --next_job_id_;
    return {.error = *error};
  }
  DebugLog("queued job '" + record.id + "' of type '" + record.type + "'");
  worker_condition_.notify_one();
  return {.value = record};
}

std::vector<JobRecord> ServiceState::ListJobs() const
{
  std::lock_guard guard{mutex_};
  return SortedValues(jobs_);
}

std::optional<JobRecord> ServiceState::GetJob(std::string_view id) const
{
  std::lock_guard guard{mutex_};
  auto it = jobs_.find(std::string{id});
  if (it == jobs_.end()) { return std::nullopt; }
  return it->second;
}

const std::filesystem::path& ServiceState::GetStateDirectory() const
{
  return state_directory_;
}

bool ServiceState::HasPersistentState() const
{
  return !state_directory_.empty();
}

void ServiceState::WorkerLoop()
{
  DebugLog("worker loop started");
  for (;;) {
    std::optional<JobRecord> queued_job;

    {
      std::unique_lock lock{mutex_};
      worker_condition_.wait(lock, [this] {
        if (stop_worker_) { return true; }
        return std::any_of(jobs_.begin(), jobs_.end(), [](const auto& item) {
          return item.second.status == JobStatus::kQueued;
        });
      });

      if (stop_worker_) { return; }
      queued_job = GetNextQueuedJobLocked();
      if (!queued_job) { continue; }
      MarkJobRunning(queued_job->id, "job execution started");
      DebugLog("starting job '" + queued_job->id + "' of type '"
               + queued_job->type + "'");
      queued_job = jobs_.at(queued_job->id);
    }

    auto [status, logs] = ExecuteJob(*queued_job);
    std::optional<std::string> last_error{};
    if (status == JobStatus::kFailed && !logs.empty()) {
      last_error = logs.back();
    }

    std::string final_log = status == JobStatus::kSucceeded
                                ? "job execution finished"
                                : "job execution failed";
    if (!logs.empty()) {
      final_log += ": ";
      final_log += logs.back();
    }

    {
      std::lock_guard guard{mutex_};
      auto it = jobs_.find(queued_job->id);
      if (it == jobs_.end()) { continue; }
      it->second.logs.insert(it->second.logs.end(), logs.begin(), logs.end());
      MarkJobFinished(queued_job->id, status, last_error, final_log);
    }
    DebugLog("finished job '" + queued_job->id + "' with status '"
             + std::string{ToString(status)} + "'");
  }
}

std::optional<JobRecord> ServiceState::GetNextQueuedJobLocked()
{
  auto it = std::find_if(jobs_.begin(), jobs_.end(), [](const auto& item) {
    return item.second.status == JobStatus::kQueued;
  });
  if (it == jobs_.end()) { return std::nullopt; }
  return it->second;
}

void ServiceState::MarkJobRunning(const std::string& id,
                                  std::string log_message)
{
  auto it = jobs_.find(id);
  if (it == jobs_.end()) { return; }

  const auto timestamp = MakeTimestamp();
  it->second.status = JobStatus::kRunning;
  it->second.updated_at = timestamp;
  it->second.started_at = timestamp;
  it->second.finished_at.reset();
  it->second.last_error.reset();
  it->second.logs.emplace_back(std::move(log_message));
  static_cast<void>(SaveStateLocked());
}

void ServiceState::MarkJobFinished(const std::string& id,
                                   JobStatus status,
                                   std::optional<std::string> last_error,
                                   std::string log_message)
{
  auto it = jobs_.find(id);
  if (it == jobs_.end()) { return; }

  const auto timestamp = MakeTimestamp();
  it->second.status = status;
  it->second.updated_at = timestamp;
  it->second.finished_at = timestamp;
  it->second.last_error = std::move(last_error);
  it->second.logs.emplace_back(std::move(log_message));
  static_cast<void>(SaveStateLocked());
}

void ServiceState::RequeueRunningJobsLocked()
{
  for (auto& [_, job] : jobs_) {
    if (job.status == JobStatus::kRunning) {
      job.status = JobStatus::kQueued;
      job.updated_at = MakeTimestamp();
      job.finished_at.reset();
      job.last_error.reset();
      job.logs.emplace_back("job re-queued after service restart");
    }
  }
}

OperationResult<std::optional<std::filesystem::path>>
GetSmokeTestWorkingDirectory(const ServiceState& state,
                             std::string_view deployment_id,
                             const DeploymentConfigRecord& config)
{
  switch (config.component) {
    case bconfig::Component::kDirector: {
      auto spec
          = state.GetDirectorDaemonResourceSpec(deployment_id, config.name);
      if (!spec) { return {.error = spec.error}; }
      if (!spec.value->working_directory) { return {.value = std::nullopt}; }
      return {.value = std::filesystem::path{*spec.value->working_directory}};
    }
    case bconfig::Component::kStorage: {
      auto spec
          = state.GetStorageDaemonResourceSpec(deployment_id, config.name);
      if (!spec) { return {.error = spec.error}; }
      if (!spec.value->working_directory) { return {.value = std::nullopt}; }
      return {.value = std::filesystem::path{*spec.value->working_directory}};
    }
    case bconfig::Component::kClient: {
      auto spec = state.GetClientDaemonResourceSpec(deployment_id, config.name);
      if (!spec) { return {.error = spec.error}; }
      if (!spec.value->working_directory) { return {.value = std::nullopt}; }
      return {.value = std::filesystem::path{*spec.value->working_directory}};
    }
    default:
      return {.value = std::nullopt};
  }
}

OperationResult<std::filesystem::path> ResolveSmokeTestExecutable(
    bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return ResolveBareosExecutable(
          "BCONFIG_BAREOS_DIR_BINARY", "bareos-dir",
          std::filesystem::path{"dird"} / "bareos-dir");
    case bconfig::Component::kStorage:
      return ResolveBareosExecutable(
          "BCONFIG_BAREOS_SD_BINARY", "bareos-sd",
          std::filesystem::path{"stored"} / "bareos-sd");
    case bconfig::Component::kClient:
      return ResolveBareosExecutable(
          "BCONFIG_BAREOS_FD_BINARY", "bareos-fd",
          std::filesystem::path{"filed"} / "bareos-fd");
    default:
      return {.error = "unsupported config component for smoke test."};
  }
}

OperationResult<std::filesystem::path> ResolveSystemctlExecutable()
{
  return ResolveBareosExecutable("BCONFIG_SYSTEMCTL_BINARY", "systemctl",
                                 std::filesystem::path{"systemctl"});
}

std::optional<std::string> SystemdServiceNameForComponent(
    bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return std::string{"bareos-dir.service"};
    case bconfig::Component::kStorage:
      return std::string{"bareos-sd.service"};
    case bconfig::Component::kClient:
      return std::string{"bareos-fd.service"};
    default:
      return std::nullopt;
  }
}

OperationResult<std::monostate> StartSystemdService(
    const std::filesystem::path& systemctl,
    std::string_view service_name,
    std::vector<std::string>& logs)
{
  auto reset_result = RunCommand(BuildCommand(
      {systemctl.string(), "reset-failed", std::string{service_name}}));
  if (!reset_result) {
    const auto unit_not_loaded
        = "Unit " + std::string{service_name} + " not loaded";
    if (reset_result.error.find(unit_not_loaded) == std::string::npos) {
      return {.error = "failed to reset systemd service state for '"
                       + std::string{service_name}
                       + "': " + reset_result.error};
    }
    logs.emplace_back("systemd service '" + std::string{service_name}
                      + "' was not loaded while resetting failed state");
  }

  auto start_result = RunCommand(
      BuildCommand({systemctl.string(), "start", std::string{service_name}}));
  if (!start_result) {
    return {.error = "failed to start systemd service '"
                     + std::string{service_name} + "': " + start_result.error};
  }

  for (int attempt = 0; attempt < 20; ++attempt) {
    auto active_result
        = RunCommand(BuildCommand({systemctl.string(), "--quiet", "is-active",
                                   std::string{service_name}}));
    if (active_result) {
      logs.emplace_back("started " + std::string{service_name}
                        + " via systemctl");
      return {.value = std::monostate{}};
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  auto status_result
      = RunCommand(BuildCommand({systemctl.string(), "--no-pager", "--full",
                                 "status", std::string{service_name}}));
  if (status_result && !TrimAsciiWhitespace(*status_result.value).empty()) {
    for (const auto& line : SplitLines(*status_result.value)) {
      logs.emplace_back(std::string{service_name} + ": " + line);
    }
  }
  return {.error = "systemd service '" + std::string{service_name}
                   + "' did not become active."};
}

#if !HAVE_WIN32
OperationResult<std::vector<DeploymentConfigRecord>>
DiscoverOrderedDaemonConfigs(const std::filesystem::path& repository_path)
{
  std::vector<DeploymentConfigRecord> daemon_configs;
  for (const auto& config : DiscoverDeploymentConfigRoots(repository_path)) {
    if (config.component == bconfig::Component::kClient
        || config.component == bconfig::Component::kStorage
        || config.component == bconfig::Component::kDirector) {
      daemon_configs.emplace_back(config);
    }
  }
  if (daemon_configs.empty()) {
    return {.error = "no client, storage, or director configs found"};
  }

  const auto component_order = [](bconfig::Component component) {
    switch (component) {
      case bconfig::Component::kClient:
        return 0;
      case bconfig::Component::kStorage:
        return 1;
      case bconfig::Component::kDirector:
        return 2;
      default:
        return 3;
    }
  };
  std::sort(daemon_configs.begin(), daemon_configs.end(),
            [&](const auto& left, const auto& right) {
              return component_order(left.component)
                     < component_order(right.component);
            });
  return {.value = std::move(daemon_configs)};
}

OperationResult<std::monostate> EnsureDaemonWorkingDirectories(
    const ServiceState& state,
    std::string_view deployment_id,
    const std::vector<DeploymentConfigRecord>& daemon_configs,
    std::vector<std::string>& logs)
{
  for (const auto& config : daemon_configs) {
    auto working_directory
        = GetSmokeTestWorkingDirectory(state, deployment_id, config);
    if (!working_directory) { return {.error = working_directory.error}; }

    const auto& maybe_working_directory = *working_directory.value;
    if (!maybe_working_directory) { continue; }

    std::error_code error_code;
    std::filesystem::create_directories(*maybe_working_directory, error_code);
    if (error_code) {
      return {.error = "failed to create working directory '"
                       + maybe_working_directory->string()
                       + "': " + error_code.message()};
    }
    logs.emplace_back("ensured working directory '"
                      + maybe_working_directory->string() + "'");
  }

  return {.value = std::monostate{}};
}

OperationResult<std::monostate> EnsureStorageArchiveDirectories(
    const ServiceState& state,
    std::string_view deployment_id,
    const std::vector<DeploymentConfigRecord>& daemon_configs,
    std::vector<std::string>& logs)
{
  for (const auto& config : daemon_configs) {
    if (config.component != bconfig::Component::kStorage) { continue; }

    auto working_directory
        = GetSmokeTestWorkingDirectory(state, deployment_id, config);
    if (!working_directory) { return {.error = working_directory.error}; }
    const auto& maybe_working_directory = *working_directory.value;
    if (!maybe_working_directory) {
      return {.error = "storage config '" + config.name
                       + "' is missing a working directory."};
    }

    struct stat working_directory_stat{};
    if (stat(maybe_working_directory->c_str(), &working_directory_stat) != 0) {
      return {.error = "failed to stat working directory '"
                       + maybe_working_directory->string()
                       + "': " + std::strerror(errno)};
    }

    const auto device_directory = config.path / "bareos-sd.d" / "device";
    if (!std::filesystem::is_directory(device_directory)) { continue; }

    for (const auto& entry :
         std::filesystem::directory_iterator(device_directory)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".conf") {
        continue;
      }

      auto spec = state.GetStorageDeviceResourceSpec(
          deployment_id, config.name, entry.path().stem().string());
      if (!spec) { return {.error = spec.error}; }

      if (!spec.value->archive_device || spec.value->archive_device->empty()) {
        continue;
      }
      if (!spec.value->device_type || *spec.value->device_type != "File") {
        continue;
      }

      const auto archive_directory
          = std::filesystem::path{*spec.value->archive_device};
      std::error_code error_code;
      std::filesystem::create_directories(archive_directory, error_code);
      if (error_code) {
        return {.error = "failed to create archive directory '"
                         + archive_directory.string()
                         + "': " + error_code.message()};
      }
      if (chown(archive_directory.c_str(), working_directory_stat.st_uid,
                working_directory_stat.st_gid)
          != 0) {
        return {.error = "failed to assign ownership for archive directory '"
                         + archive_directory.string()
                         + "': " + std::strerror(errno)};
      }
      logs.emplace_back("ensured archive directory '"
                        + archive_directory.string() + "'");
      logs.emplace_back("assigned archive directory ownership '"
                        + archive_directory.string() + "'");
    }
  }

  return {.value = std::monostate{}};
}

OperationResult<std::monostate> PrepareConsoleMessageFiles(
    const ServiceState& state,
    std::string_view deployment_id,
    const std::vector<DeploymentConfigRecord>& daemon_configs,
    std::vector<std::string>& logs)
{
  for (const auto& config : daemon_configs) {
    auto working_directory
        = GetSmokeTestWorkingDirectory(state, deployment_id, config);
    if (!working_directory) { return {.error = working_directory.error}; }

    const auto& maybe_working_directory = *working_directory.value;
    if (!maybe_working_directory) { continue; }

    struct stat working_directory_stat{};
    if (stat(maybe_working_directory->c_str(), &working_directory_stat) != 0) {
      return {.error = "failed to stat working directory '"
                       + maybe_working_directory->string()
                       + "': " + std::strerror(errno)};
    }

    auto console_message_file
        = *maybe_working_directory / (config.name + ".conmsg");
    int file_descriptor = open(console_message_file.c_str(), O_CREAT | O_RDWR,
                               S_IRUSR | S_IWUSR);
    if (file_descriptor == -1) {
      return {.error = "failed to open console message file '"
                       + console_message_file.string()
                       + "': " + std::strerror(errno)};
    }

    if (fchown(file_descriptor, working_directory_stat.st_uid,
               working_directory_stat.st_gid)
        != 0) {
      close(file_descriptor);
      return {.error = "failed to assign ownership for console message file '"
                       + console_message_file.string()
                       + "': " + std::strerror(errno)};
    }

    if (fchmod(file_descriptor, S_IRUSR | S_IWUSR) != 0) {
      close(file_descriptor);
      return {.error = "failed to set permissions for console message file '"
                       + console_message_file.string()
                       + "': " + std::strerror(errno)};
    }

    close(file_descriptor);
    logs.emplace_back("prepared console message file '"
                      + console_message_file.string() + "'");
  }

  return {.value = std::monostate{}};
}

OperationResult<std::vector<StartedSmokeTestProcess>> StartDeploymentProcesses(
    const std::vector<DeploymentConfigRecord>& daemon_configs,
    std::vector<std::string>& logs)
{
  std::vector<StartedSmokeTestProcess> started_processes;
  started_processes.reserve(daemon_configs.size());
  for (const auto& config : daemon_configs) {
    auto executable = ResolveSmokeTestExecutable(config.component);
    if (!executable) {
      for (auto it = started_processes.rbegin(); it != started_processes.rend();
           ++it) {
        if (auto stop_error = StopSmokeTestProcess(*it, logs); stop_error) {
          logs.emplace_back(*stop_error);
        }
      }
      return {.error = executable.error};
    }

    auto process = StartSmokeTestProcess(
        std::string{bconfig::ComponentToString(config.component)} + " '"
            + config.name + "'",
        *executable.value, config.path);
    if (!process) {
      for (auto it = started_processes.rbegin(); it != started_processes.rend();
           ++it) {
        if (auto stop_error = StopSmokeTestProcess(*it, logs); stop_error) {
          logs.emplace_back(*stop_error);
        }
      }
      return {.error = process.error};
    }

    auto startup = WaitForSmokeTestStartup(*process.value, logs);
    if (!startup) {
      if (auto stop_error = StopSmokeTestProcess(*process.value, logs);
          stop_error) {
        logs.emplace_back(*stop_error);
      }
      for (auto it = started_processes.rbegin(); it != started_processes.rend();
           ++it) {
        if (auto stop_error = StopSmokeTestProcess(*it, logs); stop_error) {
          logs.emplace_back(*stop_error);
        }
      }
      return {.error = startup.error};
    }

    started_processes.emplace_back(std::move(*process.value));
  }

  return {.value = std::move(started_processes)};
}

OperationResult<std::vector<std::string>> DiscoverOrderedSystemdServices(
    const std::vector<DeploymentConfigRecord>& daemon_configs)
{
  std::vector<std::string> services;
  services.reserve(daemon_configs.size());

  for (const auto& config : daemon_configs) {
    auto service_name = SystemdServiceNameForComponent(config.component);
    if (!service_name) {
      return {.error = "unsupported config component for systemd startup."};
    }

    if (std::find(services.begin(), services.end(), *service_name)
        != services.end()) {
      return {.error
              = "systemd startup supports only one config root per "
                "daemon type."};
    }

    services.emplace_back(std::move(*service_name));
  }

  return {.value = std::move(services)};
}

OperationResult<std::monostate> SyncDeploymentIntoInstalledConfigRoot(
    const DeploymentRecord& deployment,
    std::vector<std::string>& logs)
{
  auto install_root = ResolveInstalledConfigRoot();
  if (!install_root) { return {.error = install_root.error}; }

  std::error_code error_code;
  std::filesystem::create_directories(*install_root.value, error_code);
  if (error_code) {
    return {.error = "failed to create installed config root '"
                     + install_root.value->string()
                     + "': " + error_code.message()};
  }

  std::optional<DeploymentConfigRecord> director_config;
  std::optional<DeploymentConfigRecord> storage_config;
  std::optional<DeploymentConfigRecord> client_config;
#  ifdef BCONFIG_HAVE_CONSOLE
  std::optional<DeploymentConfigRecord> console_config;
#  endif

  for (const auto& config :
       DiscoverDeploymentConfigRoots(deployment.repository_path)) {
    auto* slot = [&]() -> std::optional<DeploymentConfigRecord>* {
      switch (config.component) {
        case bconfig::Component::kDirector:
          return &director_config;
        case bconfig::Component::kStorage:
          return &storage_config;
        case bconfig::Component::kClient:
          return &client_config;
#  ifdef BCONFIG_HAVE_CONSOLE
        case bconfig::Component::kConsole:
          return &console_config;
#  endif
        default:
          return nullptr;
      }
    }();
    if (!slot) { continue; }
    if (slot->has_value()) {
      return {.error
              = "installed config sync supports only one "
                + std::string{bconfig::ComponentToString(config.component)}
                + " config root per deployment."};
    }
    *slot = config;
  }

  const auto sync_config = [&](const DeploymentConfigRecord& config,
                               const std::filesystem::path& source,
                               const std::filesystem::path& target)
      -> OperationResult<std::monostate> {
    auto copy_error = ReplacePathWithCopy(source, target);
    if (copy_error) { return {.error = *copy_error}; }
    logs.emplace_back(
        "synced " + std::string{bconfig::ComponentToString(config.component)}
        + " '" + config.name + "' to " + target.string());
    return {.value = std::monostate{}};
  };

  if (director_config) {
    auto synced
        = sync_config(*director_config, director_config->path / "bareos-dir.d",
                      *install_root.value / "bareos-dir.d");
    if (!synced) { return synced; }
  }
  if (storage_config) {
    auto synced
        = sync_config(*storage_config, storage_config->path / "bareos-sd.d",
                      *install_root.value / "bareos-sd.d");
    if (!synced) { return synced; }
  }
  if (client_config) {
    auto synced
        = sync_config(*client_config, client_config->path / "bareos-fd.d",
                      *install_root.value / "bareos-fd.d");
    if (!synced) { return synced; }
  }
#  ifdef BCONFIG_HAVE_CONSOLE
  if (console_config) {
    auto synced = sync_config(
        *console_config,
        console_config->path
            / ComponentDefaultConfigFilename(bconfig::Component::kConsole),
        *install_root.value
            / ComponentDefaultConfigFilename(bconfig::Component::kConsole));
    if (!synced) { return synced; }
  }
#  endif

  return {.value = std::monostate{}};
}

std::pair<JobStatus, std::vector<std::string>> ExecuteSmokeTestDeployment(
    const ServiceState& state,
    const JobRecord& job_snapshot)
{
  std::vector<std::string> logs;
  if (!job_snapshot.deployment_id) {
    logs.emplace_back("deployment_id is required");
    return {JobStatus::kFailed, logs};
  }

  auto deployment = state.GetDeployment(*job_snapshot.deployment_id);
  if (!deployment) {
    logs.emplace_back("deployment not found");
    return {JobStatus::kFailed, logs};
  }

  auto synced_config = SyncDeploymentIntoInstalledConfigRoot(*deployment, logs);
  if (!synced_config) {
    logs.emplace_back(synced_config.error);
    return {JobStatus::kFailed, logs};
  }

  auto daemon_configs
      = DiscoverOrderedDaemonConfigs(deployment->repository_path);
  if (!daemon_configs) {
    logs.emplace_back(daemon_configs.error);
    return {JobStatus::kFailed, logs};
  }

  auto working_directories = EnsureDaemonWorkingDirectories(
      state, *job_snapshot.deployment_id, *daemon_configs.value, logs);
  if (!working_directories) {
    logs.emplace_back(working_directories.error);
    return {JobStatus::kFailed, logs};
  }

  auto archive_directories = EnsureStorageArchiveDirectories(
      state, *job_snapshot.deployment_id, *daemon_configs.value, logs);
  if (!archive_directories) {
    logs.emplace_back(archive_directories.error);
    return {JobStatus::kFailed, logs};
  }

  auto started_processes
      = StartDeploymentProcesses(*daemon_configs.value, logs);
  if (!started_processes) {
    logs.emplace_back(started_processes.error);
    return {JobStatus::kFailed, logs};
  }

  for (auto it = started_processes.value->rbegin();
       it != started_processes.value->rend(); ++it) {
    if (auto stop_error = StopSmokeTestProcess(*it, logs); stop_error) {
      logs.emplace_back(*stop_error);
      return {JobStatus::kFailed, logs};
    }
  }

  logs.emplace_back("smoke-tested "
                    + std::to_string(daemon_configs.value->size())
                    + " daemon config root(s)");
  return {JobStatus::kSucceeded, logs};
}

std::pair<JobStatus, std::vector<std::string>> ExecuteStartDeploymentDaemons(
    const ServiceState& state,
    const JobRecord& job_snapshot)
{
  std::vector<std::string> logs;
  if (!job_snapshot.deployment_id) {
    logs.emplace_back("deployment_id is required");
    return {JobStatus::kFailed, logs};
  }

  auto deployment = state.GetDeployment(*job_snapshot.deployment_id);
  if (!deployment) {
    logs.emplace_back("deployment not found");
    return {JobStatus::kFailed, logs};
  }

  auto daemon_configs
      = DiscoverOrderedDaemonConfigs(deployment->repository_path);
  if (!daemon_configs) {
    logs.emplace_back(daemon_configs.error);
    return {JobStatus::kFailed, logs};
  }

  auto working_directories = EnsureDaemonWorkingDirectories(
      state, *job_snapshot.deployment_id, *daemon_configs.value, logs);
  if (!working_directories) {
    logs.emplace_back(working_directories.error);
    return {JobStatus::kFailed, logs};
  }

  auto console_message_files = PrepareConsoleMessageFiles(
      state, *job_snapshot.deployment_id, *daemon_configs.value, logs);
  if (!console_message_files) {
    logs.emplace_back(console_message_files.error);
    return {JobStatus::kFailed, logs};
  }

  auto services = DiscoverOrderedSystemdServices(*daemon_configs.value);
  if (!services) {
    logs.emplace_back(services.error);
    return {JobStatus::kFailed, logs};
  }

  auto systemctl = ResolveSystemctlExecutable();
  if (!systemctl) {
    logs.emplace_back(systemctl.error);
    return {JobStatus::kFailed, logs};
  }

  for (const auto& service_name : *services.value) {
    auto start_result
        = StartSystemdService(*systemctl.value, service_name, logs);
    if (!start_result) {
      logs.emplace_back(start_result.error);
      return {JobStatus::kFailed, logs};
    }
  }

  logs.emplace_back("started " + std::to_string(services.value->size())
                    + " deployment daemon(s) via systemctl");
  return {JobStatus::kSucceeded, logs};
}
#endif

std::pair<JobStatus, std::vector<std::string>> ServiceState::ExecuteJob(
    const JobRecord& job_snapshot) const
{
  std::vector<std::string> logs;

  if (job_snapshot.type == "validate_deployment_repo") {
    DebugLog("validating deployment repository for job '" + job_snapshot.id
             + "'");
    if (!job_snapshot.deployment_id) {
      logs.emplace_back("deployment_id is required");
      return {JobStatus::kFailed, logs};
    }

    std::optional<DeploymentRecord> deployment;
    {
      std::lock_guard guard{mutex_};
      deployment
          = FindDeploymentLocked(deployments_, *job_snapshot.deployment_id);
    }
    if (!deployment) {
      logs.emplace_back("deployment not found");
      return {JobStatus::kFailed, logs};
    }

    const auto& root = deployment->repository_path;
    if (!std::filesystem::exists(RepositoryLayout::ManifestPath(root))) {
      logs.emplace_back("manifest.json is missing");
      return {JobStatus::kFailed, logs};
    }
    for (const auto& directory : RepositoryLayout::Directories(root)) {
      if (!std::filesystem::is_directory(directory)) {
        logs.emplace_back("missing required directory: " + directory.string());
        return {JobStatus::kFailed, logs};
      }
    }

    const auto configs = DiscoverDeploymentConfigRoots(root);
    if (configs.empty()) {
      logs.emplace_back("repository layout is valid");
      logs.emplace_back("no imported config roots found in deployment repo");
      return {JobStatus::kSucceeded, logs};
    }

    for (const auto& config : configs) {
      auto loaded
          = bconfig::LoadConfig(config.component, config.path.string(), true);
      if (!loaded.parser) {
        AppendParseMessages(
            logs, loaded.messages,
            std::string{bconfig::ComponentToString(config.component)} + " '"
                + config.name + "' ");
        logs.emplace_back(
            "failed to initialize parser for "
            + std::string{bconfig::ComponentToString(config.component)} + " '"
            + config.name + "'");
        return {JobStatus::kFailed, logs};
      }

      AppendParseMessages(
          logs, loaded.messages,
          std::string{bconfig::ComponentToString(config.component)} + " '"
              + config.name + "' ");
      if (!loaded.parse_ok) {
        logs.emplace_back(
            "config root failed validation: "
            + std::string{bconfig::ComponentToString(config.component)} + " '"
            + config.name + "'");
        return {JobStatus::kFailed, logs};
      }

      logs.emplace_back(
          "validated "
          + std::string{bconfig::ComponentToString(config.component)} + " '"
          + config.name + "'");
    }

    logs.emplace_back("validated " + std::to_string(configs.size())
                      + " imported config root(s)");
    return {JobStatus::kSucceeded, logs};
  }

  if (job_snapshot.type == "import_configuration") {
    DebugLog("importing configuration for job '" + job_snapshot.id + "'");
    if (!job_snapshot.deployment_id) {
      logs.emplace_back("deployment_id is required");
      return {JobStatus::kFailed, logs};
    }
    if (!job_snapshot.source_path) {
      logs.emplace_back("source_path is required");
      return {JobStatus::kFailed, logs};
    }

    std::optional<DeploymentRecord> deployment;
    {
      std::lock_guard guard{mutex_};
      deployment
          = FindDeploymentLocked(deployments_, *job_snapshot.deployment_id);
    }
    if (!deployment) {
      logs.emplace_back("deployment not found");
      return {JobStatus::kFailed, logs};
    }

    std::string root_error;
    auto source_root = ResolveImportRoot(*job_snapshot.source_path, root_error);
    if (!source_root) {
      logs.emplace_back(std::move(root_error));
      return {JobStatus::kFailed, logs};
    }

    std::vector<ImportedComponent> imported_components;
    imported_components.reserve(SupportedImportComponents().size());
    for (const auto component : SupportedImportComponents()) {
      auto imported = ImportComponentIntoDeployment(
          component, *source_root, deployment->repository_path, logs);
      if (!imported.value && imported.error.empty()) { continue; }
      if (!imported) {
        for (const auto& finished_import : imported_components) {
          RemoveTreeQuietly(finished_import.destination_root);
        }
        logs.emplace_back(imported.error);
        return {JobStatus::kFailed, logs};
      }

      imported_components.emplace_back(std::move(*imported.value));
    }

    const auto imported_director = std::find_if(
        imported_components.begin(), imported_components.end(),
        [](const ImportedComponent& imported) {
          return imported.component == bconfig::Component::kDirector;
        });
    if (imported_director != imported_components.end()) {
      auto generated = GenerateDirectorClientStubs(
          *imported_director, deployment->repository_path, logs);
      if (!generated) {
        for (const auto& finished_import : imported_components) {
          RemoveTreeQuietly(finished_import.destination_root);
        }
        logs.emplace_back(generated.error);
        return {JobStatus::kFailed, logs};
      }

      imported_components.insert(imported_components.end(),
                                 generated.value->begin(),
                                 generated.value->end());
    }

    if (imported_components.empty()) {
      logs.emplace_back("no supported Bareos config trees found under "
                        + source_root->string());
      return {JobStatus::kFailed, logs};
    }

    JobRecord finished_job = job_snapshot;
    finished_job.updated_at = MakeTimestamp();
    if (auto error = UpdateImportState(deployment->repository_path,
                                       finished_job, imported_components);
        error) {
      for (const auto& finished_import : imported_components) {
        RemoveTreeQuietly(finished_import.destination_root);
      }
      logs.emplace_back(*error);
      return {JobStatus::kFailed, logs};
    }

    logs.emplace_back("imported " + std::to_string(imported_components.size())
                      + " component tree(s) from " + source_root->string());
    return {JobStatus::kSucceeded, logs};
  }

  if (job_snapshot.type == "commit_deployment_repo") {
    DebugLog("committing deployment repository for job '" + job_snapshot.id
             + "'");
    if (!job_snapshot.deployment_id) {
      logs.emplace_back("deployment_id is required");
      return {JobStatus::kFailed, logs};
    }
    if (!job_snapshot.commit_message || job_snapshot.commit_message->empty()) {
      logs.emplace_back("commit_message is required");
      return {JobStatus::kFailed, logs};
    }

    std::optional<DeploymentRecord> deployment;
    {
      std::lock_guard guard{mutex_};
      deployment
          = FindDeploymentLocked(deployments_, *job_snapshot.deployment_id);
    }
    if (!deployment) {
      logs.emplace_back("deployment not found");
      return {JobStatus::kFailed, logs};
    }

    auto add_result = RunGitCommand(deployment->repository_path, {"add", "-A"});
    if (!add_result) {
      logs.emplace_back(add_result.error);
      return {JobStatus::kFailed, logs};
    }

    auto staged_paths = RunGitCommand(deployment->repository_path,
                                      {"diff", "--cached", "--name-only"});
    if (!staged_paths) {
      logs.emplace_back(staged_paths.error);
      return {JobStatus::kFailed, logs};
    }
    auto staged_lines = SplitLines(*staged_paths.value);
    if (staged_lines.empty()) {
      logs.emplace_back("no changes to commit");
      return {JobStatus::kFailed, logs};
    }

    logs.emplace_back("staged " + std::to_string(staged_lines.size())
                      + " path(s) for commit");
    for (const auto& path : staged_lines) {
      logs.emplace_back("staged path: " + path);
    }

    auto commit_result = RunGitCommand(
        deployment->repository_path,
        {"commit", "--quiet", "-m", *job_snapshot.commit_message});
    if (!commit_result) {
      logs.emplace_back(commit_result.error);
      return {JobStatus::kFailed, logs};
    }

    auto commit_id
        = RunGitCommand(deployment->repository_path, {"rev-parse", "HEAD"});
    if (!commit_id) {
      logs.emplace_back(commit_id.error);
      return {JobStatus::kFailed, logs};
    }

    auto commit_sha = *commit_id.value;
    while (!commit_sha.empty()
           && (commit_sha.back() == '\n' || commit_sha.back() == '\r')) {
      commit_sha.pop_back();
    }
    logs.emplace_back("created commit " + commit_sha);
    return {JobStatus::kSucceeded, logs};
  }

  if (job_snapshot.type == "initialize_catalog_database") {
#if HAVE_WIN32
    logs.emplace_back("catalog initialization is not supported on Windows.");
    return {JobStatus::kFailed, logs};
#else
    DebugLog("initializing catalog database for job '" + job_snapshot.id + "'");
    return ExecuteInitializeCatalogDatabase(*this, job_snapshot);
#endif
  }

  if (job_snapshot.type == "smoke_test_deployment") {
#if HAVE_WIN32
    logs.emplace_back("daemon smoke tests are not supported on Windows.");
    return {JobStatus::kFailed, logs};
#else
    DebugLog("smoke-testing deployment for job '" + job_snapshot.id + "'");
    return ExecuteSmokeTestDeployment(*this, job_snapshot);
#endif
  }

  if (job_snapshot.type == "start_deployment_daemons") {
#if HAVE_WIN32
    logs.emplace_back(
        "starting deployment daemons is not supported on Windows.");
    return {JobStatus::kFailed, logs};
#else
    DebugLog("starting deployment daemons for job '" + job_snapshot.id + "'");
    return ExecuteStartDeploymentDaemons(*this, job_snapshot);
#endif
  }

  logs.emplace_back("unsupported job type: " + job_snapshot.type);
  return {JobStatus::kFailed, logs};
}

std::string ServiceState::MakeTimestamp()
{
  const auto now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm utc{};
#if HAVE_WIN32
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif

  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

bool ServiceState::IsValidIdentifier(std::string_view id)
{
  if (id.empty()) { return false; }

  return std::all_of(id.begin(), id.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '-' || ch == '_';
  });
}

std::string ServiceState::JsonEscape(std::string_view value)
{
  std::string escaped;
  escaped.reserve(value.size());

  for (unsigned char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (ch < 0x20) {
          std::ostringstream stream;
          stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(ch);
          escaped += stream.str();
        } else {
          escaped += static_cast<char>(ch);
        }
        break;
    }
  }

  return escaped;
}

std::string ServiceState::ManifestJson(const DeploymentRecord& record)
{
  std::ostringstream stream;
  stream << "{\n"
         << "  \"id\": \"" << JsonEscape(record.id) << "\",\n"
         << "  \"name\": \"" << JsonEscape(record.name) << "\",\n"
         << "  \"repository_path\": \""
         << JsonEscape(record.repository_path.generic_string()) << "\",\n"
         << "  \"workflow_mode\": \"" << ToString(record.workflow_mode)
         << "\",\n"
         << "  \"created_at\": \"" << JsonEscape(record.created_at) << "\"\n"
         << "}\n";
  return stream.str();
}

std::string ServiceState::EmptyObjectJson() { return "{}\n"; }

std::string ServiceState::SerializeState(
    uint64_t next_job_id,
    const std::vector<DeploymentRecord>& deployments,
    const std::vector<JobRecord>& jobs)
{
  std::ostringstream stream;
  stream << "{\n"
         << "  \"next_job_id\": " << next_job_id << ",\n"
         << "  \"deployments\": [\n";

  for (size_t i = 0; i < deployments.size(); ++i) {
    const auto& deployment = deployments[i];
    stream << "    {\n"
           << "      \"id\": \"" << JsonEscape(deployment.id) << "\",\n"
           << "      \"name\": \"" << JsonEscape(deployment.name) << "\",\n"
           << "      \"repository_path\": \""
           << JsonEscape(deployment.repository_path.generic_string()) << "\",\n"
           << "      \"workflow_mode\": \""
           << ToString(deployment.workflow_mode) << "\",\n"
           << "      \"created_at\": \"" << JsonEscape(deployment.created_at)
           << "\"\n"
           << "    }";
    if (i + 1 != deployments.size()) { stream << ","; }
    stream << "\n";
  }

  stream << "  ],\n"
         << "  \"jobs\": [\n";

  for (size_t i = 0; i < jobs.size(); ++i) {
    const auto& job = jobs[i];
    stream << "    {\n"
           << "      \"id\": \"" << JsonEscape(job.id) << "\",\n"
           << "      \"type\": \"" << JsonEscape(job.type) << "\",\n"
           << "      \"deployment_id\": ";
    if (job.deployment_id) {
      stream << "\"" << JsonEscape(*job.deployment_id) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"source_component\": ";
    if (job.source_component) {
      stream << "\"" << JsonEscape(*job.source_component) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"source_path\": ";
    if (job.source_path) {
      stream << "\"" << JsonEscape(*job.source_path) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"commit_message\": ";
    if (job.commit_message) {
      stream << "\"" << JsonEscape(*job.commit_message) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"status\": \"" << ToString(job.status) << "\",\n"
           << "      \"created_at\": \"" << JsonEscape(job.created_at)
           << "\",\n"
           << "      \"updated_at\": \"" << JsonEscape(job.updated_at)
           << "\",\n"
           << "      \"started_at\": ";
    if (job.started_at) {
      stream << "\"" << JsonEscape(*job.started_at) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"finished_at\": ";
    if (job.finished_at) {
      stream << "\"" << JsonEscape(*job.finished_at) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"last_error\": ";
    if (job.last_error) {
      stream << "\"" << JsonEscape(*job.last_error) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"logs\": [";
    for (size_t log_index = 0; log_index < job.logs.size(); ++log_index) {
      if (log_index > 0) { stream << ", "; }
      stream << "\"" << JsonEscape(job.logs[log_index]) << "\"";
    }
    stream << "]\n"
           << "    }";
    if (i + 1 != jobs.size()) { stream << ","; }
    stream << "\n";
  }

  stream << "  ]\n"
         << "}\n";
  return stream.str();
}

std::optional<std::string> ServiceState::InitializeRepositoryLayout(
    const DeploymentRecord& record)
{
  std::error_code error_code;
  for (const auto& directory :
       RepositoryLayout::Directories(record.repository_path)) {
    if (!std::filesystem::create_directories(directory, error_code)
        && error_code) {
      return "failed to create repository directory '" + directory.string()
             + "': " + error_code.message();
    }
  }

  const auto manifest_path
      = RepositoryLayout::ManifestPath(record.repository_path);
  if (std::filesystem::exists(manifest_path)) {
    return "repository manifest already exists at '" + manifest_path.string()
           + "'.";
  }

  if (!WriteFile(manifest_path, ManifestJson(record))) {
    return "failed to write manifest '" + manifest_path.string() + "'.";
  }

  const auto ownership_path
      = RepositoryLayout::OwnershipPath(record.repository_path);
  if (!std::filesystem::exists(ownership_path)
      && !WriteFile(ownership_path, EmptyObjectJson())) {
    return "failed to write ownership file '" + ownership_path.string() + "'.";
  }

  const auto import_state_path
      = RepositoryLayout::ImportStatePath(record.repository_path);
  if (!std::filesystem::exists(import_state_path)
      && !WriteFile(import_state_path, EmptyObjectJson())) {
    return "failed to write import state file '" + import_state_path.string()
           + "'.";
  }

  auto init_result
      = RunCommand(BuildCommand({"git", "-c", "init.defaultBranch=main", "init",
                                 "--quiet", record.repository_path.string()}));
  if (!init_result) { return init_result.error; }

  auto config_name = RunGitCommand(record.repository_path,
                                   {"config", "user.name", "bconfig-service"});
  if (!config_name) { return config_name.error; }
  auto config_email
      = RunGitCommand(record.repository_path,
                      {"config", "user.email", "bconfig-service@localhost"});
  if (!config_email) { return config_email.error; }
  auto add_result = RunGitCommand(
      record.repository_path, {"add", "manifest.json", "service/ownership.json",
                               "service/import-state.json"});
  if (!add_result) { return add_result.error; }
  auto commit_result = RunGitCommand(
      record.repository_path,
      {"commit", "--quiet", "-m", "Initialize deployment repository"});
  if (!commit_result) { return commit_result.error; }

  return std::nullopt;
}

std::optional<std::string> ServiceState::LoadState()
{
  const auto state_file = state_directory_ / "service-state.json";
  if (!std::filesystem::exists(state_file)) { return std::nullopt; }

  json_error_t json_error{};
  auto root = MakeJson(json_load_file(state_file.c_str(), 0, &json_error));
  if (!root) {
    return "failed to parse service state '" + state_file.string()
           + "': " + json_error.text;
  }

  auto* next_job_id = json_object_get(root.get(), "next_job_id");
  auto* deployments = json_object_get(root.get(), "deployments");
  auto* jobs = json_object_get(root.get(), "jobs");
  if (!json_is_integer(next_job_id) || !json_is_array(deployments)
      || !json_is_array(jobs)) {
    return "service state file '" + state_file.string()
           + "' is missing required fields.";
  }

  deployments_.clear();
  jobs_.clear();
  next_job_id_ = static_cast<uint64_t>(json_integer_value(next_job_id));

  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(deployments, index, entry)
  {
    auto* id = json_object_get(entry, "id");
    auto* name = json_object_get(entry, "name");
    auto* repository_path = json_object_get(entry, "repository_path");
    auto* workflow_mode = json_object_get(entry, "workflow_mode");
    auto* created_at = json_object_get(entry, "created_at");

    if (!json_is_string(id) || !json_is_string(name)
        || !json_is_string(repository_path) || !json_is_string(workflow_mode)
        || !json_is_string(created_at)) {
      return "service state file '" + state_file.string()
             + "' contains an invalid deployment entry.";
    }

    auto parsed_workflow = ParseWorkflowMode(json_string_value(workflow_mode));
    if (!parsed_workflow) {
      return "service state file '" + state_file.string()
             + "' contains an unknown workflow mode.";
    }

    DeploymentRecord record{
        .id = json_string_value(id),
        .name = json_string_value(name),
        .repository_path = json_string_value(repository_path),
        .workflow_mode = *parsed_workflow,
        .created_at = json_string_value(created_at)};
    deployments_.emplace(record.id, std::move(record));
  }

  json_array_foreach(jobs, index, entry)
  {
    auto* id = json_object_get(entry, "id");
    auto* type = json_object_get(entry, "type");
    auto* deployment_id = json_object_get(entry, "deployment_id");
    auto* source_component = json_object_get(entry, "source_component");
    auto* source_path = json_object_get(entry, "source_path");
    auto* commit_message = json_object_get(entry, "commit_message");
    auto* status = json_object_get(entry, "status");
    auto* created_at = json_object_get(entry, "created_at");
    auto* updated_at = json_object_get(entry, "updated_at");
    auto* started_at = json_object_get(entry, "started_at");
    auto* finished_at = json_object_get(entry, "finished_at");
    auto* last_error = json_object_get(entry, "last_error");
    auto* logs = json_object_get(entry, "logs");

    if (!json_is_string(id) || !json_is_string(type)
        || (deployment_id && !json_is_null(deployment_id)
            && !json_is_string(deployment_id))
        || (source_component && !json_is_null(source_component)
            && !json_is_string(source_component))
        || (source_path && !json_is_null(source_path)
            && !json_is_string(source_path))
        || (commit_message && !json_is_null(commit_message)
            && !json_is_string(commit_message))
        || !json_is_string(status) || !json_is_string(created_at)
        || !json_is_string(updated_at)
        || (started_at && !json_is_null(started_at)
            && !json_is_string(started_at))
        || (finished_at && !json_is_null(finished_at)
            && !json_is_string(finished_at))
        || (last_error && !json_is_null(last_error)
            && !json_is_string(last_error))
        || (logs && !json_is_array(logs))) {
      return "service state file '" + state_file.string()
             + "' contains an invalid job entry.";
    }

    auto parsed_status = ParseJobStatus(json_string_value(status));
    if (!parsed_status) {
      return "service state file '" + state_file.string()
             + "' contains an unknown job status.";
    }

    JobRecord record{.id = json_string_value(id),
                     .type = json_string_value(type),
                     .deployment_id = std::nullopt,
                     .source_component = std::nullopt,
                     .source_path = std::nullopt,
                     .commit_message = std::nullopt,
                     .status = *parsed_status,
                     .created_at = json_string_value(created_at),
                     .updated_at = json_string_value(updated_at)};
    if (deployment_id && json_is_string(deployment_id)) {
      record.deployment_id = std::string{json_string_value(deployment_id)};
    }
    if (source_component && json_is_string(source_component)) {
      record.source_component
          = std::string{json_string_value(source_component)};
    }
    if (source_path && json_is_string(source_path)) {
      record.source_path = std::string{json_string_value(source_path)};
    }
    if (commit_message && json_is_string(commit_message)) {
      record.commit_message = std::string{json_string_value(commit_message)};
    }
    if (started_at && json_is_string(started_at)) {
      record.started_at = std::string{json_string_value(started_at)};
    }
    if (finished_at && json_is_string(finished_at)) {
      record.finished_at = std::string{json_string_value(finished_at)};
    }
    if (last_error && json_is_string(last_error)) {
      record.last_error = std::string{json_string_value(last_error)};
    }
    if (logs) {
      size_t log_index = 0;
      json_t* log_entry = nullptr;
      json_array_foreach(logs, log_index, log_entry)
      {
        if (!json_is_string(log_entry)) {
          return "service state file '" + state_file.string()
                 + "' contains a non-string job log entry.";
        }
        record.logs.emplace_back(json_string_value(log_entry));
      }
    }

    jobs_.emplace(record.id, std::move(record));
  }

  RequeueRunningJobsLocked();
  if (next_job_id_ == 0) { next_job_id_ = 1; }
  return std::nullopt;
}

std::optional<std::string> ServiceState::SaveStateLocked() const
{
  if (state_directory_.empty()) { return std::nullopt; }

  const auto state_file = state_directory_ / "service-state.json";
  const auto content = SerializeState(next_job_id_, SortedValues(deployments_),
                                      SortedValues(jobs_));
  if (!WriteFile(state_file, content)) {
    return "failed to write service state '" + state_file.string() + "'.";
  }

  return std::nullopt;
}

}  // namespace bconfig::service
