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

#include "include/bareos.h"
#include "include/fcntl_def.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "lib/cli.h"
#include "lib/parse_conf.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* kDefaultSetupStagingDir = PATH_BAREOS_WORKINGDIR "/setup";

struct Options {
  std::string config_path = ConfigurationParser::GetDefaultConfigDir();
  fs::path staging_dir{kDefaultSetupStagingDir};
  std::string client_name;
  bool reload = false;
  std::string reload_systemd_unit = "bareos-dir";
};

struct CommandResult {
  int exit_code{-1};
  std::string output;
};

[[noreturn]] void Throw(const std::string& message)
{
  throw std::runtime_error(message);
}

void ValidateName(std::string_view field, std::string_view name)
{
  if (name.empty()) { Throw(std::string{field} + " must not be empty."); }
  if (std::isspace(static_cast<unsigned char>(name.front()))
      || std::isspace(static_cast<unsigned char>(name.back()))) {
    Throw(std::string{field} + " must not start or end with whitespace.");
  }
  if (name.size() > 127) { Throw(std::string{field} + " is too long."); }
  if (name.find('/') != std::string_view::npos
      || name.find('\\') != std::string_view::npos) {
    Throw(std::string{field} + " must not contain path separators.");
  }
}

fs::path DetermineStagePath(const Options& options)
{
  ValidateName("client", options.client_name);
  return options.staging_dir / "bareos-dir.d" / "client"
         / (options.client_name + ".conf");
}

std::unique_ptr<ConfigurationParser> LoadDirectorConfig(const std::string& config_path)
{
  auto parser = std::unique_ptr<ConfigurationParser>(
      directordaemon::InitDirConfig(config_path.c_str(), M_CONFIG_ERROR));
  if (!parser) { Throw("Failed to initialize the Director config parser."); }

  directordaemon::my_config = parser.get();
  if (!parser->ParseConfig()) {
    Throw("Failed to parse the Director config from \"" + config_path + "\".");
  }

  directordaemon::me
      = static_cast<directordaemon::DirectorResource*>(
          parser->GetNextRes(directordaemon::R_DIRECTOR, nullptr));
  directordaemon::my_config->own_resource_ = directordaemon::me;
  return parser;
}

size_t CountResources(const ConfigurationParser& parser, int type)
{
  size_t count = 0;
  for (auto* resource = parser.GetNextRes(type, nullptr); resource != nullptr;
       resource = parser.GetNextRes(type, resource)) {
    ++count;
  }
  return count;
}

std::array<size_t, directordaemon::R_NUM> CountAllResources(
    const ConfigurationParser& parser)
{
  std::array<size_t, directordaemon::R_NUM> counts{};
  for (int type = 0; type < directordaemon::R_NUM; ++type) {
    counts[static_cast<size_t>(type)] = CountResources(parser, type);
  }
  return counts;
}

void WriteFully(int fd, const char* data, size_t length)
{
  size_t written = 0;
  while (written < length) {
    const auto chunk = write(fd, data + written, length - written);
    if (chunk < 0) {
      if (errno == EINTR) { continue; }
      Throw("Failed to write the temporary activated config file.");
    }
    written += static_cast<size_t>(chunk);
  }
}

void FsyncDirectory(const fs::path& directory)
{
  const int fd = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd < 0) { return; }
  fsync(fd);
  close(fd);
}

CommandResult RunCommand(const std::vector<std::string>& arguments)
{
  std::array<int, 2> pipefd{};
  if (pipe(pipefd.data()) != 0) {
    Throw("Failed to create a pipe for the reload command.");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    Throw("Failed to start the reload command.");
  }

  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv.front(), argv.data());
    _exit(127);
  }

  close(pipefd[1]);

  std::string output;
  std::array<char, 4096> buffer{};
  ssize_t bytes_read = 0;
  while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
    output.append(buffer.data(), static_cast<size_t>(bytes_read));
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  CommandResult result;
  result.output = std::move(output);
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }
  return result;
}

void CopyStagedFileToTemporaryPath(const fs::path& source, const fs::path& temporary_path)
{
  std::ifstream input(source, std::ios::binary);
  if (!input) {
    Throw("Failed to open the staged client config \"" + source.string() + "\".");
  }

  const int fd = open(temporary_path.c_str(),
                      O_CREAT | O_WRONLY | O_TRUNC | O_EXCL | O_BINARY, 0640);
  if (fd < 0) {
    Throw("Failed to create the temporary activated config file \""
          + temporary_path.string() + "\".");
  }

  try {
    std::array<char, 8192> buffer{};
    while (input) {
      input.read(buffer.data(), buffer.size());
      const auto bytes_read = input.gcount();
      if (bytes_read > 0) {
        WriteFully(fd, buffer.data(), static_cast<size_t>(bytes_read));
      }
    }
    if (!input.eof()) {
      Throw("Failed to read the staged client config \"" + source.string() + "\".");
    }
    if (fsync(fd) != 0) {
      Throw("Failed to flush the temporary activated config file.");
    }
    close(fd);
  } catch (...) {
    close(fd);
    std::error_code ec;
    fs::remove(temporary_path, ec);
    throw;
  }
}

void RemoveTemporaryPathIfPresent(const fs::path& path)
{
  std::error_code ec;
  fs::remove(path, ec);
}

void EnsureStageFileIsRegular(const fs::path& path)
{
  std::error_code ec;
  const auto status = fs::symlink_status(path, ec);
  if (ec) {
    Throw("Failed to inspect the staged client config \"" + path.string()
          + "\": " + ec.message());
  }
  if (!fs::exists(status)) {
    Throw("No staged client config found at \"" + path.string() + "\".");
  }
  if (!fs::is_regular_file(status)) {
    Throw("The staged client config \"" + path.string()
          + "\" is not a regular file.");
  }
}

void ValidateActivatedResource(ConfigurationParser& parser,
                               const std::string& client_name,
                               const std::array<size_t, directordaemon::R_NUM>& before_counts)
{
  const auto after_counts = CountAllResources(parser);
  for (int type = 0; type < directordaemon::R_NUM; ++type) {
    const auto before = before_counts[static_cast<size_t>(type)];
    const auto after = after_counts[static_cast<size_t>(type)];
    if (type == directordaemon::R_CLIENT) {
      if (after != before + 1) {
        Throw("The staged config must add exactly one Client resource.");
      }
    } else if (after != before) {
      Throw("The staged config must not add or modify non-Client resources.");
    }
  }

  auto* resource = parser.GetResWithName(directordaemon::R_CLIENT, client_name.c_str());
  if (resource == nullptr) {
    Throw("The staged config did not define Client \"" + client_name + "\".");
  }

  const auto* res_table = parser.GetResourceTable("client");
  if (res_table == nullptr) {
    Throw("Failed to look up the Director Client resource definition.");
  }
  if (!directordaemon::ValidateResource(directordaemon::R_CLIENT, res_table->items,
                                        resource)) {
    Throw("The staged config for Client \"" + client_name + "\" failed"
          " validation.");
  }
}

void ReloadDirector(const Options& options)
{
  const auto result = RunCommand(
      {"systemctl", "reload", options.reload_systemd_unit});
  if (result.exit_code != 0) {
    std::string message
        = "The staged client config was activated on disk, but reloading the"
          " Director via systemd unit \""
          + options.reload_systemd_unit + "\" failed.";
    if (!result.output.empty()) {
      message += " systemctl output:\n" + result.output;
    }
    Throw(message);
  }

  std::cout << "Reloaded Director via systemd unit \""
            << options.reload_systemd_unit << "\".\n";
}

fs::path ActivateStagedClient(const Options& options)
{
  auto parser = LoadDirectorConfig(options.config_path);
  if (parser->GetResWithName(directordaemon::R_CLIENT,
                             options.client_name.c_str())
      != nullptr) {
    Throw("Client \"" + options.client_name
          + "\" already exists in the Director config.");
  }

  const auto stage_path = DetermineStagePath(options);
  EnsureStageFileIsRegular(stage_path);

  PoolMem destination_path(PM_FNAME);
  PoolMem temporary_path(PM_FNAME);
  if (!parser->GetPathOfNewResource(destination_path, temporary_path, nullptr,
                                    "client", options.client_name.c_str(), true,
                                    true)) {
    Throw(temporary_path.c_str());
  }

  const auto tmp_path = fs::path{temporary_path.c_str()};
  RemoveTemporaryPathIfPresent(tmp_path);
  CopyStagedFileToTemporaryPath(stage_path, tmp_path);

  const auto before_counts = CountAllResources(*parser);
  if (!parser->ParseConfigFile(tmp_path.c_str(), nullptr, nullptr, nullptr)) {
    RemoveTemporaryPathIfPresent(tmp_path);
    Throw("The staged client config \"" + stage_path.string()
          + "\" is not valid Director config.");
  }
  ValidateActivatedResource(*parser, options.client_name, before_counts);

  std::error_code ec;
  fs::rename(tmp_path, destination_path.c_str(), ec);
  if (ec) {
    RemoveTemporaryPathIfPresent(tmp_path);
    Throw("Failed to activate the staged client config into \""
          + std::string(destination_path.c_str()) + "\": " + ec.message());
  }
  FsyncDirectory(fs::path(destination_path.c_str()).parent_path());

  ec.clear();
  fs::remove(stage_path, ec);
  if (ec) {
    std::cerr << "Activated staged client config, but failed to remove \""
              << stage_path.string() << "\": " << ec.message() << "\n";
  }

  return fs::path{destination_path.c_str()};
}

}  // namespace

int main(int argc, char** argv)
{
  OSDependentInit();

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App app;
  try {
    Options options;

    InitCLIApp(app, "Activate a staged Director Client resource.", 2000);

    app.add_option("-c,--config", options.config_path,
                   "Use <path> as Director configuration file or directory.")
        ->check(CLI::ExistingPath)
        ->type_name("<path>");
    app.add_option("--staging-dir", options.staging_dir,
                   "Directory holding staged setup-service output.")
        ->capture_default_str();
    app.add_option("--client", options.client_name,
                   "Client resource name to activate from staging.")
        ->required();
    app.add_flag("--reload", options.reload,
                 "Reload the running Director after successful activation.");
    app.add_option("--reload-systemd-unit", options.reload_systemd_unit,
                   "systemd unit to reload when --reload is used.")
        ->capture_default_str();

    ParseBareosApp(app, argc, argv);
    const auto activated_path = ActivateStagedClient(options);

    std::cout << "Activated staged client \"" << options.client_name << "\" at "
              << activated_path.string() << ".\n";
    if (options.reload) {
      ReloadDirector(options);
    } else {
      std::cout << "Reload or restart the Director to apply the new client.\n";
    }

    directordaemon::my_config = nullptr;
    directordaemon::me = nullptr;
    return 0;
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  } catch (const std::exception& e) {
    std::cerr << "Failed to activate staged client: " << e.what() << "\n";
    directordaemon::my_config = nullptr;
    directordaemon::me = nullptr;
    return 1;
  }
}
