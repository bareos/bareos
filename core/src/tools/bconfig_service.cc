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
#include "include/auth_types.h"
#include "include/job_level.h"
#include "include/job_types.h"
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
    console_resources.emplace_back(
        RenderParsedResource(*resource, *loaded.parser));
  }

  for (auto* resource = loaded.parser->GetNextRes(console::R_DIRECTOR, nullptr);
       resource != nullptr;
       resource = loaded.parser->GetNextRes(console::R_DIRECTOR, resource)) {
    if (!resource->resource_name_) {
      return "console import encountered a Director resource without a name.";
    }
    director_resources.emplace_back(
        RenderParsedResource(*resource, *loaded.parser));
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
void AppendRepeatedQuotedDirective(std::ostringstream& content,
                                   std::string_view name,
                                   const std::vector<std::string>& values);

std::string BuildClientDirectorStubContent(
    std::string_view director_name,
    std::string_view password,
    std::string_view description,
    const std::optional<std::string>& address = std::nullopt,
    const std::optional<uint16_t>& port = std::nullopt,
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

constexpr std::pair<std::string_view, crypto_cipher_t> kPkiCipherNames[] = {
    {"blowfish", CRYPTO_CIPHER_BLOWFISH_CBC},
    {"3des", CRYPTO_CIPHER_3DES_CBC},
    {"aes128", CRYPTO_CIPHER_AES_128_CBC},
    {"aes192", CRYPTO_CIPHER_AES_192_CBC},
    {"aes256", CRYPTO_CIPHER_AES_256_CBC},
    {"camellia128", CRYPTO_CIPHER_CAMELLIA_128_CBC},
    {"camellia192", CRYPTO_CIPHER_CAMELLIA_192_CBC},
    {"camellia256", CRYPTO_CIPHER_CAMELLIA_256_CBC},
    {"aes128hmacsha1", CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1},
    {"aes256hmacsha1", CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1},
};

std::optional<std::string_view> RenderPkiCipherToken(crypto_cipher_t cipher)
{
  for (const auto& [name, value] : kPkiCipherNames) {
    if (value == cipher) { return name; }
  }
  return std::nullopt;
}

bool IsSupportedPkiCipherToken(std::string_view token)
{
  return std::any_of(
      std::begin(kPkiCipherNames), std::end(kPkiCipherNames),
      [token](const auto& entry) { return entry.first == token; });
}
constexpr std::array<std::string_view, directordaemon::Num_ACL>
    kDirectorAclDirectiveNames
    = {"JobACL",   "ClientACL",       "StorageACL", "ScheduleACL",
       "PoolACL",  "CommandACL",      "FileSetACL", "CatalogACL",
       "WhereACL", "PluginOptionsACL"};

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
  std::vector<std::string> entries{};
};

struct StorageDaemonContentSpec {
  std::optional<std::string> address{};
  std::vector<std::string> addresses{};
  std::optional<std::string> source_address{};
  std::optional<uint16_t> port{};
  std::optional<bool> just_in_time_reservation{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
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
  std::optional<uint64_t> checkpoint_interval{};
  std::optional<uint64_t> client_connect_wait{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<std::string> description{};
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
  std::optional<std::string> where{};
  std::optional<int32_t> priority{};
  std::optional<bool> enabled{};
  std::vector<std::string> passthrough_entries{};
};

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
    const std::optional<uint16_t>& port,
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
  AppendBareosDirective(content, "Where", spec.where);
  AppendIntegerDirective(content, "Priority", spec.priority);
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
  AppendBareosDirective(content, "Where", spec.where);
  AppendIntegerDirective(content, "Priority", spec.priority);
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
  AppendBareosDirective(content, "SourceAddress", spec.source_address);
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
  AppendBareosDirective(content, "SourceAddress", spec.source_address);
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
  AppendBareosDirective(content, "SourceAddress", spec.source_address);
  AppendIntegerDirective(content, "MaximumConcurrentJobs",
                         spec.maximum_concurrent_jobs);
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
  AppendQuotedDirective(content, "Description", spec.description);
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
    const std::optional<uint64_t>& maximum_bandwidth_per_job = std::nullopt)
{
  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << QuoteBareosString(director_name) << "\n"
          << "  Description = " << QuoteBareosString(description) << "\n"
          << "  Password = " << QuoteBareosString(password) << "\n";
  AppendBoolDirective(content, "Monitor", monitor);
  AppendIntegerDirective(content, "MaximumBandwidthPerJob",
                         maximum_bandwidth_per_job);
  content << "}\n";
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

std::string BuildStorageDaemonNdmpResourceContent(std::string_view ndmp_name,
                                                  std::string_view username,
                                                  std::string_view password,
                                                  std::string_view auth_type,
                                                  uint32_t log_level)
{
  std::ostringstream content;
  content << "Ndmp {\n"
          << "  Name = " << QuoteBareosString(ndmp_name) << "\n"
          << "  Username = " << QuoteBareosString(username) << "\n"
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

OperationResult<std::string> NormalizeStorageNdmpAuthType(
    std::string_view auth_type)
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

std::string RenderStorageNdmpAuthType(uint32_t auth_type)
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
    if (device_resource->multiplied_device_resource) {
      return {.error = "storage-daemon autochanger '"
                       + std::string{autochanger_name}
                       + "' uses counted devices, which are not managed yet."};
    }
    if (!device_resource->resource_name_
        || device_resource->resource_name_[0] == '\0') {
      return {.error = "storage-daemon autochanger '"
                       + std::string{autochanger_name}
                       + "' references a device without a name."};
    }
    std::string device_name{device_resource->resource_name_};
    if (!device_name.empty() && device_name[0] == '$') {
      return {.error = "storage-daemon autochanger '"
                       + std::string{autochanger_name}
                       + "' uses counted devices, which are not managed yet."};
    }
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
  std::optional<uint16_t> port{};
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
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
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

struct StorageNdmpWriteContext {
  std::filesystem::path file_path{};
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
          const auto name = NormalizeDirectiveName(
              TrimAsciiWhitespace(trimmed.substr(0, open_brace)));
          if (!controlled_directives.contains(name)) {
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
          const auto name = NormalizeDirectiveName(
              TrimAsciiWhitespace(trimmed.substr(0, equals)));
          if (!controlled_directives.contains(name)) {
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
          const auto name = NormalizeDirectiveName(
              TrimAsciiWhitespace(trimmed.substr(0, open_brace)));
          if (!controlled_directives.contains(name)) {
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
          const auto name = NormalizeDirectiveName(
              TrimAsciiWhitespace(trimmed.substr(0, equals)));
          if (!controlled_directives.contains(name)) {
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

OperationResult<std::string> RenderPoolLabelTypeForConfig(int32_t label_type)
{
  switch (label_type) {
    case B_BAREOS_LABEL:
      return {.value = std::string{}};
    case B_ANSI_LABEL:
      return {.value = std::string{"ansi"}};
    case B_IBM_LABEL:
      return {.value = std::string{"ibm"}};
    default:
      return {.error = "unsupported director pool LabelType value "
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
    context.director_resources.emplace_back(
        RenderParsedResource(*configured_director, *loaded.parser));
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
      auto rendered = RenderParsedResource(*configured_console, *loaded.parser);
      if (seen_target) {
        context.console_resources_after.emplace_back(std::move(rendered));
      } else {
        context.console_resources_before.emplace_back(std::move(rendered));
      }
      continue;
    }

    seen_target = true;
    context.exists = true;
    if (configured_console->description_
        && configured_console->description_[0] != '\0') {
      context.description = std::string{configured_console->description_};
    }
    auto rendered_password = RenderPasswordForConfig(
        configured_console->password_,
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
    context.console_resources.emplace_back(
        RenderParsedResource(*configured_console, *loaded.parser));
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
      auto rendered
          = RenderParsedResource(*configured_director, *loaded.parser);
      if (seen_target) {
        context.director_resources_after.emplace_back(std::move(rendered));
      } else {
        context.director_resources_before.emplace_back(std::move(rendered));
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
      const auto parsed_port = configured_director->DIRport;
      if (parsed_port > std::numeric_limits<uint16_t>::max()) {
        return {.error = "console director '" + std::string{director_name}
                         + "' has unsupported port value."};
      }
      context.port = static_cast<uint16_t>(parsed_port);
    }
    if (HasMemberSource(*configured_director, {"Password"})) {
      auto rendered_password = RenderPasswordForConfig(
          configured_director->password_,
          "console director password for '" + std::string{director_name} + "'");
      if (!rendered_password) { return {.error = rendered_password.error}; }
      if (!rendered_password.value->empty()) {
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

    auto label_type = RenderPoolLabelTypeForConfig(pool->LabelType);
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

    auto rendered_password = RenderPasswordForConfig(
        catalog->db_password, "director-side catalog password for '"
                                  + std::string{catalog_name} + "'");
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
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    director_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure(
                "director config parser initialization ", loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("director config ", loaded.messages)};
  }

  DirectorMessagesWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "messages"
                   / (std::string{messages_name} + ".conf")};
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

    auto source = messages->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "director messages '" + std::string{messages_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    static const std::set<std::string> kControlledMessagesDirectives{
        "Name", "Description"};
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
  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("client config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("client config ", loaded.messages)};
  }

  ClientMessagesWriteContext context{
      .file_path = client_config.path / "bareos-fd.d" / "messages"
                   / (std::string{messages_name} + ".conf")};
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

    auto source = messages->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "client messages '" + std::string{messages_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    static const std::set<std::string> kControlledMessagesDirectives{
        "Name", "Description"};
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
  auto loaded = bconfig::LoadConfig(bconfig::Component::kStorage,
                                    storage_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("storage config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("storage config ", loaded.messages)};
  }

  StorageMessagesWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "messages"
                   / (std::string{messages_name} + ".conf")};
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

    auto source = messages->GetDefinitionSource();
    if (!source || source->file.empty()) {
      return {.error = "storage-daemon messages '" + std::string{messages_name}
                       + "' has no definition source."};
    }
    context.file_path = source->file;
    auto per_file = resources_per_file.find(source->file);
    context.is_standalone_file
        = per_file != resources_per_file.end() && per_file->second == 1;

    static const std::set<std::string> kControlledMessagesDirectives{
        "Name", "Description"};
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
  auto loaded = bconfig::LoadConfig(bconfig::Component::kClient,
                                    client_config.path.string(), true);
  if (!loaded.parser) {
    return {.error = FormatParseFailure("client config parser initialization ",
                                        loaded.messages)};
  }
  if (!loaded.parse_ok) {
    return {.error = FormatParseFailure("client config ", loaded.messages)};
  }

  ClientDaemonWriteContext context{
      .file_path = client_config.path / "bareos-fd.d" / "client"
                   / (client_config.name + ".conf")};
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
    if (has_source_address_source && client->FDsrc_addr
        && client->FDsrc_addr->size() > 1) {
      return {
          .error
          = "client daemon resource '" + client_config.name
            + "' uses multiple source addresses, which are not managed yet."};
    }
    if (has_source_address_source) {
      context.content.source_address = CopyFirstAddress(client->FDsrc_addr);
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
      auto pki_cipher = RenderPkiCipherToken(client->pki_cipher);
      if (!pki_cipher) {
        return {.error = "client daemon resource '" + client_config.name
                         + "' uses an unsupported PKI cipher value."};
      }
      context.content.pki_cipher = std::string{*pki_cipher};
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
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<DirectorDaemonWriteContext> LoadDirectorDaemonWriteContext(
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

  DirectorDaemonWriteContext context{
      .file_path = director_config.path / "bareos-dir.d" / "director"
                   / (director_config.name + ".conf")};
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
    if (has_source_address_source && director->DIRsrc_addr
        && director->DIRsrc_addr->size() > 1) {
      return {.error = "director daemon resource '" + director_config.name
                       + "' uses multiple source addresses, which are not "
                         "managed yet."};
    }
    if (has_source_address_source) {
      context.content.source_address = CopyFirstAddress(director->DIRsrc_addr);
    }
    if (director->password_.value && director->password_.value[0] != '\0') {
      context.password = std::string{director->password_.value};
    }
    if (HasMemberSource(*director, {"MaximumConcurrentJobs"})) {
      context.content.maximum_concurrent_jobs = director->MaxConcurrentJobs;
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
    if (director->description_ && director->description_[0] != '\0') {
      context.content.description = std::string{director->description_};
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
    return {.value = std::move(context)};
  }

  return {.value = std::move(context)};
}

OperationResult<StorageDaemonWriteContext> LoadStorageDaemonWriteContext(
    const DeploymentConfigRecord& storage_config)
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

  StorageDaemonWriteContext context{
      .file_path = storage_config.path / "bareos-sd.d" / "storage"
                   / (storage_config.name + ".conf")};
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
    if (has_source_address_source && storage->SDsrc_addr
        && storage->SDsrc_addr->size() > 1) {
      return {
          .error
          = "storage-daemon storage resource '" + storage_config.name
            + "' uses multiple source addresses, which are not managed yet."};
    }
    if (has_source_address_source) {
      context.content.source_address = CopyFirstAddress(storage->SDsrc_addr);
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
    if (job->RestoreWhere && job->RestoreWhere[0] != '\0') {
      context.content.where = std::string{job->RestoreWhere};
    }
    if (job->Priority != 0) { context.content.priority = job->Priority; }
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

    static const std::set<std::string> kControlledJobDirectives{
        "Name",
        "Description",
        "Type",
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
        "Where",
        "Priority",
        "Enabled"};
    auto passthrough
        = context.is_standalone_file
              ? ExtractTopLevelResourceEntries(context.file_path, "Job",
                                               kControlledJobDirectives)
              : ExtractNamedTopLevelResourceEntries(context.file_path, "Job",
                                                    job_name,
                                                    kControlledJobDirectives);
    if (!passthrough) { return {.error = passthrough.error}; }
    context.content.passthrough_entries = std::move(*passthrough.value);
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
    if (jobdefs->RestoreWhere && jobdefs->RestoreWhere[0] != '\0') {
      context.content.where = std::string{jobdefs->RestoreWhere};
    }
    if (jobdefs->Priority != 0) {
      context.content.priority = jobdefs->Priority;
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

    static const std::set<std::string> kControlledJobDefsDirectives{
        "Name",
        "Description",
        "Type",
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
        "Where",
        "Priority",
        "Enabled"};
    auto passthrough
        = context.is_standalone_file
              ? ExtractTopLevelResourceEntries(context.file_path, "JobDefs",
                                               kControlledJobDefsDirectives)
              : ExtractNamedTopLevelResourceEntries(
                    context.file_path, "JobDefs", jobdefs_name,
                    kControlledJobDefsDirectives);
    if (!passthrough) { return {.error = passthrough.error}; }
    context.content.passthrough_entries = std::move(*passthrough.value);
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
    if (HasMemberSource(*director, {"Monitor"})) {
      context.monitor = director->monitor;
    }
    if (HasMemberSource(*director, {"MaximumBandwidthPerJob"})) {
      context.maximum_bandwidth_per_job = director->max_bandwidth_per_job;
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
    if (ndmp->username && ndmp->username[0] != '\0') {
      context.username = std::string{ndmp->username};
    }
    auto rendered_password = RenderPasswordForConfig(
        ndmp->password,
        "storage-daemon NDMP password for '" + std::string{ndmp_name} + "'");
    if (!rendered_password) { return {.error = rendered_password.error}; }
    context.password = *rendered_password.value;
    if (HasMemberSource(*ndmp, {"AuthType"})) {
      context.auth_type = RenderStorageNdmpAuthType(ndmp->AuthType);
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
  const auto director_monitor = director_context.value->monitor;
  const auto director_maximum_bandwidth_per_job
      = director_context.value->maximum_bandwidth_per_job;
  const auto device_description = device_context.value->description.value_or(
      DefaultStorageDaemonDeviceDescription(device_name, storage_name));

  const auto director_content = BuildStorageDaemonDirectorResourceContent(
      director_name, password, director_description, director_monitor,
      director_maximum_bandwidth_per_job);
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
    return RenderPasswordForConfig(
        client->password_,
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
    if (HasMemberSource(*director, {"Address"}) && director->address
        && director->address[0] != '\0') {
      context.address = std::string{director->address};
    }
    if (HasMemberSource(*director, {"Port"}) && director->port > 0
        && director->port <= 65535) {
      context.port = static_cast<uint16_t>(director->port);
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
  const auto address = spec.address ? spec.address : context.value->address;
  const auto port = spec.port ? spec.port : context.value->port;
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
      director_name, *password.value, description, address, port,
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

  auto client_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kClient, client_name);
  if (!client_config) {
    return {.error = "client config not found for '" + std::string{client_name}
                     + "'."};
  }

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
  if (spec.pki_cipher && !IsSupportedPkiCipherToken(*spec.pki_cipher)) {
    return {.error
            = "client daemon pki_cipher must be one of the supported filed "
              "PKI cipher tokens."};
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
  if (spec.source_address) { content.source_address = spec.source_address; }
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
  if (spec.port && *spec.port == 0) {
    return {.error = "director daemon port must be greater than zero."};
  }
  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context = LoadDirectorDaemonWriteContext(*director_config.value);
  if (!context) { return {.error = context.error}; }
  if (!context.value->exists) {
    return {.error = "director daemon resource '" + std::string{director_name}
                     + "' was not found."};
  }
  if (!context.value->password || context.value->password->empty()) {
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
  if (spec.source_address) { content.source_address = spec.source_address; }
  if (spec.port) {
    content.port = spec.port;
    content.addresses.clear();
  }
  if (spec.maximum_concurrent_jobs) {
    content.maximum_concurrent_jobs = spec.maximum_concurrent_jobs;
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
  if (spec.description) { content.description = spec.description; }
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
      director_name, *context.value->password, content, &render_error);
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
  if (!context.value->is_standalone_file) {
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

  auto console_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) {
    return {.error = "console config not found for '"
                     + std::string{console_config_name} + "'."};
  }

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

  auto console_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) {
    return {.error = "console config not found for '"
                     + std::string{console_config_name} + "'."};
  }

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

  auto console_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) {
    return {.error = "console config not found for '"
                     + std::string{console_config_name} + "'."};
  }

  auto context
      = LoadConsoleDirectorWriteContext(*console_config.value, director_name);
  if (!context) { return {.error = context.error}; }

  const auto address = spec.address ? *spec.address : context.value->address;
  if (!address || address->empty()) {
    return {.error
            = "field 'address' is required when creating a console director "
              "resource."};
  }

  const auto password = spec.password ? spec.password : context.value->password;
  const auto port = spec.port ? spec.port : context.value->port;
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

  auto console_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kConsole, console_config_name);
  if (!console_config) {
    return {.error = "console config not found for '"
                     + std::string{console_config_name} + "'."};
  }

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
  if (spec.maximum_volume_bytes) {
    content.maximum_volume_bytes = *spec.maximum_volume_bytes;
  }
  if (spec.volume_retention) {
    content.volume_retention = *spec.volume_retention;
  }
  if (spec.auto_prune) { content.auto_prune = *spec.auto_prune; }
  if (spec.recycle) { content.recycle = *spec.recycle; }

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

  auto director_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kDirector, director_name);
  if (!director_config) {
    return {.error = "director config not found for '"
                     + std::string{director_name} + "'."};
  }

  auto context
      = LoadDirectorMessagesWriteContext(*director_config.value, messages_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description
      = spec.description
            ? *spec.description
            : content.description.value_or(DefaultDirectorMessagesDescription(
                  messages_name, director_name));
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
  if (spec.where) { content.where = *spec.where; }
  if (spec.priority) { content.priority = *spec.priority; }
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
  if (spec.where) { content.where = *spec.where; }
  if (spec.priority) { content.priority = *spec.priority; }
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

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  auto context
      = LoadStorageMessagesWriteContext(*storage_config.value, messages_name);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  content.description = spec.description
                            ? *spec.description
                            : content.description.value_or(
                                  DefaultStorageDaemonMessagesDescription(
                                      messages_name, storage_name));
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

  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

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
  const auto rendered = BuildStorageDaemonDirectorResourceContent(
      director_name, *password, description, monitor,
      maximum_bandwidth_per_job);
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

  const auto description = spec.description
                               ? *spec.description
                               : context.value->description.value_or(
                                     DefaultStorageDaemonDeviceDescription(
                                         device_name, storage_name));
  const auto rendered = BuildStorageDaemonDeviceResourceContent(
      device_name, *media_type, *archive_device, *device_type, description);
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
  auto auth_type = NormalizeStorageNdmpAuthType(auth_type_raw);
  if (!auth_type) { return {.error = auth_type.error}; }
  const auto log_level
      = spec.log_level ? *spec.log_level : context.value->log_level.value_or(4);

  const auto rendered = BuildStorageDaemonNdmpResourceContent(
      ndmp_name, *username, *password, *auth_type.value, log_level);
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
  auto storage_config = GetDeploymentConfig(
      deployment_id, bconfig::Component::kStorage, storage_name);
  if (!storage_config) {
    return {.error = "storage config not found for '"
                     + std::string{storage_name} + "'."};
  }

  auto context = LoadStorageDaemonWriteContext(*storage_config.value);
  if (!context) { return {.error = context.error}; }

  auto content = context.value->content;
  if (spec.address) { content.address = spec.address; }
  if (spec.addresses) {
    content.addresses = *spec.addresses;
    content.address.reset();
    content.port.reset();
  }
  if (spec.source_address) { content.source_address = spec.source_address; }
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
