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

#include "dird/dird_conf.h"
#include "include/bareos.h"
#include "lib/bpipe.h"
#include "lib/message.h"
#include "lib/thread_specific_data.h"
#include "stored/stored_conf.h"

#include <jansson.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <set>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <variant>

namespace bconfig::service {
namespace {

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;
constexpr int kServiceDebugLevel = 10;

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

std::string MakeDebugTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_seconds = std::chrono::system_clock::to_time_t(now);
  const auto subseconds = now.time_since_epoch() % std::chrono::seconds(1);
  constexpr auto kSubsecondTicksPerSecond
      = std::chrono::system_clock::duration::period::den
        / std::chrono::system_clock::duration::period::num;
  constexpr int kSubsecondDigits = [] {
    auto ticks = kSubsecondTicksPerSecond;
    int digits = 0;
    while (ticks > 1) {
      ticks /= 10;
      ++digits;
    }
    return digits;
  }();
  std::tm utc{};
#if HAVE_WIN32
  gmtime_s(&utc, &now_seconds);
#else
  gmtime_r(&now_seconds, &utc);
#endif

  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
  if constexpr (kSubsecondDigits > 0) {
    stream << '.' << std::setw(kSubsecondDigits) << std::setfill('0')
           << subseconds.count();
  }
  stream << 'Z';
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
  return {bconfig::Component::kDirector, bconfig::Component::kStorage,
          bconfig::Component::kClient};
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
  }

  return normalized;
}

std::optional<std::filesystem::path> FindComponentConfigDirectory(
    bconfig::Component component,
    const std::filesystem::path& source_root)
{
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
    if (bucket.empty() || config_directory_name.empty()
        || !std::filesystem::is_directory(bucket)) {
      continue;
    }

    for (const auto& entry : std::filesystem::directory_iterator(bucket)) {
      if (!entry.is_directory()) { continue; }
      if (!std::filesystem::is_directory(entry.path()
                                         / config_directory_name)) {
        continue;
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

std::string BuildClientDirectorStubContent(std::string_view director_name,
                                           std::string_view password,
                                           std::string_view description)
{
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "}\n";
  return content.str();
}

constexpr uint16_t kDefaultFileDaemonPort = 9102;
constexpr uint16_t kDefaultStorageDaemonPort = 9103;
constexpr std::array<std::string_view, directordaemon::Num_ACL>
    kDirectorAclDirectiveNames
    = {"JobACL",   "ClientACL",       "StorageACL", "ScheduleACL",
       "PoolACL",  "CommandACL",      "FileSetACL", "CatalogACL",
       "WhereACL", "PluginOptionsACL"};

std::string BuildDirectorClientResourceContent(std::string_view client_name,
                                               std::string_view address,
                                               std::string_view password,
                                               uint16_t port,
                                               std::string_view description)
{
  std::ostringstream content;
  content << "Client {\n"
          << "  Name = " << QuoteBareosString(client_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Address = " << address << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "  Port = " << port << "\n"
          << "}\n";
  return content.str();
}

std::string BuildDirectorStorageResourceContent(std::string_view storage_name,
                                                std::string_view address,
                                                std::string_view password,
                                                std::string_view device,
                                                std::string_view media_type,
                                                uint16_t port,
                                                std::string_view description)
{
  std::ostringstream content;
  content << "Storage {\n"
          << "  Name = " << QuoteBareosString(storage_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Address = " << address << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "  Device = " << device << "\n"
          << "  Media Type = " << media_type << "\n"
          << "  Port = " << port << "\n"
          << "}\n";
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

std::string BuildDirectorConsoleResourceContent(
    std::string_view console_name,
    std::string_view password,
    std::string_view description,
    bool use_pam_authentication,
    const std::array<std::vector<std::string>, directordaemon::Num_ACL>& acl,
    const std::vector<std::string>& profiles)
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

std::string BuildStorageDaemonDirectorResourceContent(
    std::string_view director_name,
    std::string_view password,
    std::string_view description)
{
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n"
          << "}\n";
  return content.str();
}

std::string BuildStorageDaemonDeviceResourceContent(
    std::string_view device_name,
    std::string_view media_type,
    std::string_view archive_device,
    std::string_view device_type,
    std::string_view description)
{
  std::ostringstream content;
  content << "Device {\n"
          << "  Name = " << QuoteBareosString(device_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Media Type = " << media_type << "\n"
          << "  Device Type = " << QuoteBareosString(device_type) << "\n"
          << "  Archive Device = " << archive_device << "\n"
          << "}\n";
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

std::string DefaultStorageDaemonDirectorDescription(
    std::string_view director_name,
    std::string_view storage_name)
{
  return "Managed storage-daemon director resource for "
         + std::string{director_name} + " synced from storage "
         + std::string{storage_name};
}

std::string DefaultStorageDaemonDeviceDescription(std::string_view device_name,
                                                  std::string_view storage_name)
{
  return "Managed storage-daemon device resource for "
         + std::string{device_name} + " synced from storage "
         + std::string{storage_name};
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
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  bool exists{false};
  bool is_standalone_file{false};
};

struct DirectorStorageWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> device{};
  std::optional<std::string> media_type{};
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
  bool exists{false};
  bool is_standalone_file{false};
};

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

struct StorageDaemonDirectorWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  bool exists{false};
  bool is_standalone_file{false};
  bool is_managed{false};
};

struct StorageDaemonDeviceWriteContext {
  std::filesystem::path file_path{};
  std::optional<std::string> media_type{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
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
    if (client->FDport != 0) {
      context.port = static_cast<uint16_t>(client->FDport);
    }
    if (client->description_ && client->description_[0] != '\0') {
      context.description = std::string{client->description_};
    }
    auto rendered_password = RenderPasswordForConfig(
        client->password_,
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
    if (!context.is_standalone_file) {
      return {.error = "director client '" + std::string{client_name}
                       + "' is not stored in a standalone file yet."};
    }
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
    if (storage->SDport != 0) {
      context.port = static_cast<uint16_t>(storage->SDport);
    }
    if (storage->description_ && storage->description_[0] != '\0') {
      context.description = std::string{storage->description_};
    }
    if (storage->media_type && storage->media_type[0] != '\0') {
      context.media_type = std::string{storage->media_type};
    }
    if (storage->devices.size() != 1) {
      return {.error = "director storage '" + std::string{storage_name}
                       + "' must have exactly one Device to be managed."};
    }
    if (storage->devices.front().name.empty()) {
      return {.error = "director storage '" + std::string{storage_name}
                       + "' has an empty Device value."};
    }
    context.device = storage->devices.front().name;

    auto rendered_password = RenderPasswordForConfig(
        storage->password_, "director-side storage password for '"
                                + std::string{storage_name} + "'");
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
    if (!context.is_standalone_file) {
      return {.error = "director storage '" + std::string{storage_name}
                       + "' is not stored in a standalone file yet."};
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
    auto rendered_password = RenderPasswordForConfig(
        console->password_, "director-side console password for '"
                                + std::string{console_name} + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;

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
    if (!context.is_standalone_file) {
      return {.error = "director console '" + std::string{console_name}
                       + "' is not stored in a standalone file yet."};
    }
    return {.value = std::move(context)};
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
    if (!context.is_standalone_file) {
      return {.error = "director user '" + std::string{user_name}
                       + "' is not stored in a standalone file yet."};
    }
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
    if (!context.is_standalone_file) {
      return {.error = "director profile '" + std::string{profile_name}
                       + "' is not stored in a standalone file yet."};
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
    auto rendered_password = RenderPasswordForConfig(
        director->password_, "storage-daemon director password for '"
                                 + std::string{director_name} + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;

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
  const auto device_description = device_context.value->description.value_or(
      DefaultStorageDaemonDeviceDescription(device_name, storage_name));

  const auto director_content = BuildStorageDaemonDirectorResourceContent(
      director_name, password, director_description);
  const auto device_content = BuildStorageDaemonDeviceResourceContent(
      device_name, media_type, *archive_device, *device_type,
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

OperationResult<std::monostate> DeleteClientDirectorStub(
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
    return RenderPasswordForConfig(
        client->password_,
        "director-side client password for '" + std::string{client_name} + "'");
  }

  return {.error = "director '" + director_config.name
                   + "' does not define client '" + std::string{client_name}
                   + "'."};
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
  const auto temp_config_directory
      = temp_root / ComponentConfigDirectoryName(component);
  if (auto error
      = CopyDirectoryTree(*source_config_directory, temp_config_directory);
      error) {
    RemoveTreeQuietly(temp_root);
    return {.error = *error};
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

std::filesystem::path DefaultStorageBasePath()
{
  const auto home = HomeDirectory();
  if (!home.empty()) { return home / "tmp/bconfig"; }
  return std::filesystem::temp_directory_path() / "bconfig";
}

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
  const auto stub_directory = client_root / "bareos-fd.d" / "director";
  const auto stub_path
      = stub_directory / (std::string{director_name} + ".conf");
  const bool client_root_existed = std::filesystem::exists(client_root);
  const bool stub_existed = std::filesystem::exists(stub_path);

  std::string original_content;
  if (stub_existed) {
    auto existing = ReadFile(stub_path);
    if (!existing) { return {.error = existing.error}; }
    original_content = std::move(*existing.value);
  }

  const auto description
      = spec.description && !spec.description->empty()
            ? *spec.description
            : DefaultClientDirectorStubDescription(client_name, director_name);
  const auto stub_content = BuildClientDirectorStubContent(
      director_name, *password.value, description);

  std::error_code error_code;
  std::filesystem::create_directories(stub_directory, error_code);
  if (error_code) {
    return {.error = "failed to create stub directory '"
                     + stub_directory.string() + "': " + error_code.message()};
  }
  if (!WriteFile(stub_path, stub_content)) {
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

  const auto password
      = spec.password ? *spec.password : context.value->password;
  if (!password || password->empty()) {
    return {.error
            = "field 'password' is required when creating a director "
              "client resource."};
  }

  const auto port = spec.port.value_or(
      context.value->port.value_or(kDefaultFileDaemonPort));
  if (port == 0) {
    return {.error = "director client port must be greater than zero."};
  }

  const auto description
      = spec.description
            ? *spec.description
            : context.value->description.value_or(
                  DefaultDirectorClientDescription(client_name, director_name));
  const auto content = BuildDirectorClientResourceContent(
      client_name, *address, *password, port, description);

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
  if (!WriteFile(context.value->file_path, content)) {
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

  auto synced_stub
      = UpsertClientDirectorStub(deployment_id, client_name, director_name, {});
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

  const auto device
      = spec.device ? *spec.device
                    : context.value->device.value_or(std::string{storage_name});
  if (device.empty()) {
    return {.error
            = "field 'device' is required when creating a director "
              "storage resource."};
  }
  if (!IsSafeBareosToken(device)) {
    return {.error
            = "director storage device must be a bare Bareos token "
              "without whitespace or quotes."};
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

  const auto port = spec.port.value_or(
      context.value->port.value_or(kDefaultStorageDaemonPort));
  if (port == 0) {
    return {.error = "director storage port must be greater than zero."};
  }

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultDirectorStorageDescription(
                                         storage_name, director_name));
  const auto content = BuildDirectorStorageResourceContent(
      storage_name, *address, *password, device, *media_type, port,
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
  if (!WriteFile(context.value->file_path, content)) {
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

  auto synced_storage = SyncStorageDaemonConfig(
      *this, deployment_id, director_name, storage_name, *context.value,
      *password, device, *media_type, spec.archive_device, spec.device_type);
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
  if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                             director_config.value->path);
      error) {
    return {.error = *error};
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

  auto deleted_stub
      = DeleteClientDirectorStub(repository_path, client_name, director_name);
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
  if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                             director_config.value->path);
      error) {
    return {.error = *error};
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
  const auto use_pam_authentication = spec.use_pam_authentication.value_or(
      context.value->use_pam_authentication.value_or(false));
  const auto content = BuildDirectorConsoleResourceContent(
      console_name, *password, description, use_pam_authentication,
      context.value->acl, context.value->profiles);

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
  if (!WriteFile(context.value->file_path, content)) {
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
  if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                             director_config.value->path);
      error) {
    return {.error = *error};
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

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorUserWriteContext(*director_config.value, user_name);
  if (!context) { return {.error = context.error}; }

  const auto description
      = spec.description
            ? *spec.description
            : context.value->description.value_or(
                  DefaultDirectorUserDescription(user_name, director_name));
  const auto content = BuildDirectorUserResourceContent(
      user_name, description, context.value->acl, context.value->profiles);

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
  if (!WriteFile(context.value->file_path, content)) {
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

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

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
  if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                             director_config.value->path);
      error) {
    return {.error = *error};
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
  const auto content = BuildDirectorProfileResourceContent(
      profile_name, description, context.value->acl);

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
  if (!WriteFile(context.value->file_path, content)) {
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
  if (auto error = DeleteFileAndEmptyParents(context.value->file_path,
                                             director_config.value->path);
      error) {
    return {.error = *error};
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
