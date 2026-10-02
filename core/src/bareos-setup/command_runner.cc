/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
#include "command_runner.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr int kExecFailureExitCode = 127;

enum class ToolRequirement
{
  Startup,
  PackageManager,
  PostInstall,
  Optional
};

constexpr std::uint8_t PackageManagerBit(PackageManager package_manager)
{
  return static_cast<std::uint8_t>(1U
                                   << static_cast<unsigned>(package_manager));
}

constexpr std::uint8_t kAptAndZypper
    = PackageManagerBit(PackageManager::Apt)
      | PackageManagerBit(PackageManager::Zypper);

struct ToolDefinition {
  SetupTool tool;
  std::string_view name;
  ToolRequirement requirement;
  PackageManager package_manager = PackageManager::Unknown;
  uint8_t post_install_package_managers = 0;
};

// This catalog also records external programs called by shell snippets:
// OpenSSL, chmod, and cat are used by SUSE HTTPS setup; getenforce and
// setsebool are guarded by the SELinux check.
constexpr std::array<ToolDefinition, static_cast<size_t>(SetupTool::Count)>
    kToolDefinitions{{
        {SetupTool::Bash, "bash", ToolRequirement::Startup},
        {SetupTool::Curl, "curl", ToolRequirement::Startup},
        {SetupTool::Install, "install", ToolRequirement::Startup},
        {SetupTool::Chown, "chown", ToolRequirement::Startup},
        {SetupTool::Systemctl, "systemctl", ToolRequirement::Startup},
        {SetupTool::Su, "su", ToolRequirement::Startup},
        {SetupTool::Sh, "sh", ToolRequirement::Startup},
        {SetupTool::AptGet, "apt-get", ToolRequirement::PackageManager,
         PackageManager::Apt},
        {SetupTool::Dnf, "dnf", ToolRequirement::PackageManager,
         PackageManager::Dnf},
        {SetupTool::Yum, "yum", ToolRequirement::PackageManager,
         PackageManager::Yum},
        {SetupTool::Zypper, "zypper", ToolRequirement::PackageManager,
         PackageManager::Zypper},
        {SetupTool::Rm, "rm", ToolRequirement::Startup},
        {SetupTool::PostgresqlSetup, "postgresql-setup",
         ToolRequirement::Optional},
        {SetupTool::A2enmod, "a2enmod", ToolRequirement::PostInstall,
         PackageManager::Unknown, kAptAndZypper},
        {SetupTool::A2ensite, "a2ensite", ToolRequirement::PostInstall,
         PackageManager::Unknown, PackageManagerBit(PackageManager::Apt)},
        {SetupTool::A2enflag, "a2enflag", ToolRequirement::PostInstall,
         PackageManager::Unknown, PackageManagerBit(PackageManager::Zypper)},
        {SetupTool::Echo, "echo", ToolRequirement::Optional},
        {SetupTool::Sudo, "sudo", ToolRequirement::Optional},
        {SetupTool::OpenSSL, "openssl", ToolRequirement::PostInstall,
         PackageManager::Unknown, PackageManagerBit(PackageManager::Zypper)},
        {SetupTool::Chmod, "chmod", ToolRequirement::PostInstall,
         PackageManager::Unknown, PackageManagerBit(PackageManager::Zypper)},
        {SetupTool::Cat, "cat", ToolRequirement::PostInstall,
         PackageManager::Unknown, PackageManagerBit(PackageManager::Zypper)},
        {SetupTool::Getenforce, "getenforce", ToolRequirement::Optional},
        {SetupTool::Setsebool, "setsebool", ToolRequirement::Optional},
        {SetupTool::XdgOpen, "xdg-open", ToolRequirement::Optional},
        {SetupTool::Open, "open", ToolRequirement::Optional},
        {SetupTool::SensibleBrowser, "sensible-browser",
         ToolRequirement::Optional},
    }};

static_assert([] {
  for (size_t i = 0; i < kToolDefinitions.size(); ++i) {
    if (static_cast<size_t>(kToolDefinitions[i].tool) != i) { return false; }
  }
  return true;
}());

const ToolDefinition& Definition(SetupTool tool)
{
  const auto index = static_cast<size_t>(tool);
  if (index >= kToolDefinitions.size()) {
    throw std::invalid_argument("Invalid bareos-setup command tool");
  }
  return kToolDefinitions[index];
}

std::optional<std::string> FindToolPath(SetupTool tool)
{
  const char* path_env = getenv("PATH");
  if (path_env == nullptr) { return std::nullopt; }

  std::istringstream stream(path_env);
  std::string directory;
  const std::string name(Definition(tool).name);
  while (std::getline(stream, directory, ':')) {
    if (directory.empty()) { continue; }
    const std::string candidate = directory + "/" + name;
    if (access(candidate.c_str(), X_OK) == 0) { return candidate; }
  }
  return std::nullopt;
}

SetupCommand MakeCommand(SetupTool tool, std::vector<std::string> arguments)
{
  (void)Definition(tool);
  return {tool, std::move(arguments)};
}

}  // namespace

std::vector<std::string> SetupCommand::Argv() const
{
  std::vector<std::string> argv;
  argv.reserve(arguments.size() + 1);
  argv.emplace_back(Definition(tool).name);
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  return argv;
}

#define DEFINE_SETUP_TOOL_WRAPPER(wrapper, tool)               \
  SetupCommand wrapper(std::vector<std::string> arguments)     \
  {                                                            \
    return MakeCommand(SetupTool::tool, std::move(arguments)); \
  }

DEFINE_SETUP_TOOL_WRAPPER(Bash, Bash)
DEFINE_SETUP_TOOL_WRAPPER(Curl, Curl)
DEFINE_SETUP_TOOL_WRAPPER(Install, Install)
DEFINE_SETUP_TOOL_WRAPPER(Chown, Chown)
DEFINE_SETUP_TOOL_WRAPPER(Systemctl, Systemctl)
DEFINE_SETUP_TOOL_WRAPPER(Su, Su)
DEFINE_SETUP_TOOL_WRAPPER(Sh, Sh)
DEFINE_SETUP_TOOL_WRAPPER(AptGet, AptGet)
DEFINE_SETUP_TOOL_WRAPPER(Dnf, Dnf)
DEFINE_SETUP_TOOL_WRAPPER(Yum, Yum)
DEFINE_SETUP_TOOL_WRAPPER(Zypper, Zypper)
DEFINE_SETUP_TOOL_WRAPPER(Rm, Rm)
DEFINE_SETUP_TOOL_WRAPPER(PostgresqlSetup, PostgresqlSetup)
DEFINE_SETUP_TOOL_WRAPPER(A2enmod, A2enmod)
DEFINE_SETUP_TOOL_WRAPPER(A2ensite, A2ensite)
DEFINE_SETUP_TOOL_WRAPPER(A2enflag, A2enflag)
DEFINE_SETUP_TOOL_WRAPPER(Echo, Echo)
DEFINE_SETUP_TOOL_WRAPPER(XdgOpen, XdgOpen)
DEFINE_SETUP_TOOL_WRAPPER(Open, Open)
DEFINE_SETUP_TOOL_WRAPPER(SensibleBrowser, SensibleBrowser)

#undef DEFINE_SETUP_TOOL_WRAPPER

bool IsRoot() { return geteuid() == 0; }

// Drain available bytes from fd into line buffer, calling cb on complete lines.
static bool DrainFd(int fd,
                    std::string& buf,
                    std::string_view stream,
                    OutputCallback& cb,
                    int& read_error)
{
  std::array<char, 4096> tmp{};
  // Read until the pipe is drained. A single read() would truncate output
  // whenever more than one buffer's worth is pending -- most importantly on
  // POLLHUP, after which the loop in RunCommandImpl() stops polling this fd
  // and the remaining bytes would be lost.
  for (;;) {
    const ssize_t n = read(fd, tmp.data(), tmp.size());
    if (n > 0) {
      buf.append(tmp.data(), static_cast<size_t>(n));
      continue;
    }
    if (n < 0 && errno == EINTR) continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    if (n < 0) read_error = errno;
    return false;
  }
  // Emit complete lines
  size_t pos;
  while ((pos = buf.find('\n')) != std::string::npos) {
    std::string_view line(buf.data(), pos);
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    cb(line, stream);
    buf.erase(0, pos + 1);
  }
  return true;
}

static int RunCommandImpl(
    const SetupCommand& command,
    const std::string* input,
    bool run_as_root,
    OutputCallback cb,
    const std::function<std::optional<std::string>(SetupTool)>&
        resolve_tool_path)
{
  const auto command_path = resolve_tool_path(command.tool);
  if (!command_path) {
    throw std::runtime_error("Command is not available in PATH: "
                             + std::string(Definition(command.tool).name));
  }

  // Use the same registered lookup for sudo as for ordinary commands.
  std::string executable_path = *command_path;
  std::vector<std::string> exec_argv;
  if (run_as_root && !IsRoot()) {
    const auto sudo_path = resolve_tool_path(SetupTool::Sudo);
    if (!sudo_path) {
      throw std::runtime_error("Command is not available in PATH: sudo");
    }
    executable_path = *sudo_path;
    exec_argv.emplace_back(Definition(SetupTool::Sudo).name);
    exec_argv.emplace_back(*command_path);
  } else {
    exec_argv.emplace_back(Definition(command.tool).name);
  }
  exec_argv.insert(exec_argv.end(), command.arguments.begin(),
                   command.arguments.end());

  // Build C-style argv
  std::vector<const char*> cargv;
  cargv.reserve(exec_argv.size() + 1);
  for (const auto& s : exec_argv) cargv.push_back(s.c_str());
  // execv() requires argv to end with a null pointer.
  cargv.push_back(nullptr);

  // Create stdout, stderr, and (when requested) stdin pipes.
  int pipe_out[2], pipe_err[2], pipe_in[2] = {-1, -1};
  // Input is written before output is drained; larger data could deadlock
  // if the child fills stdout or stderr before reading stdin.
  if (input != nullptr && input->size() > PIPE_BUF) {
    throw std::invalid_argument("command stdin exceeds PIPE_BUF");
  }
  if (pipe(pipe_out) != 0 || pipe(pipe_err) != 0
      || (input != nullptr && pipe(pipe_in) != 0))
    throw std::runtime_error(std::string("pipe: ") + strerror(errno));

  pid_t pid = fork();
  if (pid < 0)
    throw std::runtime_error(std::string("fork: ") + strerror(errno));

  if (pid == 0) {
    // Child
    close(pipe_out[0]);
    close(pipe_err[0]);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_err[1], STDERR_FILENO);
    close(pipe_out[1]);
    close(pipe_err[1]);
    if (input != nullptr) {
      close(pipe_in[1]);
      dup2(pipe_in[0], STDIN_FILENO);
      close(pipe_in[0]);
    }
    // Redirect stdin from /dev/null so sudo doesn't hang asking for password
    if (input == nullptr) {
      // The child inherits the current environment and can block on stdin
      // when a command asks for interactive input, so /dev/null keeps the
      // child non-interactive even if the parent is attached to a terminal.
      int devnull = open("/dev/null", O_RDONLY);
      if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        close(devnull);
      }
    }
    execv(executable_path.c_str(), const_cast<char* const*>(cargv.data()));
    // execv failed — write error to stderr and exit
    const char* msg = strerror(errno);
    [[maybe_unused]] auto _ = write(STDERR_FILENO, msg, strlen(msg));
    _exit(kExecFailureExitCode);
  }

  // Parent
  close(pipe_out[1]);
  close(pipe_err[1]);
  if (input != nullptr) {
    close(pipe_in[0]);
    size_t written = 0;
    while (written < input->size()) {
      const ssize_t n
          = write(pipe_in[1], input->data() + written, input->size() - written);
      if (n > 0) {
        written += static_cast<size_t>(n);
        continue;
      }
      if (n < 0 && errno == EINTR) continue;
      close(pipe_in[1]);
      throw std::runtime_error(std::string("write stdin: ") + strerror(errno));
    }
    close(pipe_in[1]);
  }

  // Make pipes non-blocking for poll loop
  fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
  fcntl(pipe_err[0], F_SETFL, O_NONBLOCK);

  std::string buf_out, buf_err;
  bool out_open = true, err_open = true;
  int output_error = 0;

  while (out_open || err_open) {
    struct pollfd fds[2] = {
        {out_open ? pipe_out[0] : -1, POLLIN, 0},
        {err_open ? pipe_err[0] : -1, POLLIN, 0},
    };
    int nfds = poll(fds, 2, 500);
    if (nfds < 0) {
      if (errno == EINTR) continue;
      output_error = errno;
      break;
    }
    if (out_open && (fds[0].revents & (POLLIN | POLLHUP))) {
      if (!DrainFd(pipe_out[0], buf_out, "stdout", cb, output_error)) {
        close(pipe_out[0]);
        out_open = false;
      }
    }
    if (err_open && (fds[1].revents & (POLLIN | POLLHUP))) {
      if (!DrainFd(pipe_err[0], buf_err, "stderr", cb, output_error)) {
        close(pipe_err[0]);
        err_open = false;
      }
    }
    if (out_open && (fds[0].revents & (POLLERR | POLLNVAL))) {
      if (output_error == 0)
        output_error = (fds[0].revents & POLLNVAL) ? EBADF : EIO;
      close(pipe_out[0]);
      out_open = false;
    }
    if (err_open && (fds[1].revents & (POLLERR | POLLNVAL))) {
      if (output_error == 0)
        output_error = (fds[1].revents & POLLNVAL) ? EBADF : EIO;
      close(pipe_err[0]);
      err_open = false;
    }
  }

  // Flush any remaining partial lines
  if (!buf_out.empty()) cb(buf_out, "stdout");
  if (!buf_err.empty()) cb(buf_err, "stderr");

  if (out_open) close(pipe_out[0]);
  if (err_open) close(pipe_err[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  if (output_error != 0) {
    throw std::runtime_error("Failed to read command output: "
                             + std::string(strerror(output_error)));
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

SetupContext::SetupContext(bool dry_run) : dry_run_(dry_run) {}

bool SetupContext::dry_run() const { return dry_run_; }

std::optional<std::string> SetupContext::ToolPath(SetupTool tool) const
{
  const auto index = static_cast<size_t>(tool);
  if (index >= tool_paths_.size()) {
    throw std::invalid_argument("Invalid bareos-setup command tool");
  }
  std::lock_guard lock(tool_paths_mutex_);
  auto& path = tool_paths_[index];
  if (path && access(path->c_str(), X_OK) == 0) return path;
  path = FindToolPath(tool);
  return path;
}

bool SetupContext::IsToolAvailable(SetupTool tool) const
{
  return ToolPath(tool).has_value();
}

std::vector<std::string> SetupContext::MissingRequiredTools(
    PackageManager pkg_mgr) const
{
  std::vector<std::string> missing;
  for (const auto& definition : kToolDefinitions) {
    const bool required
        = definition.requirement == ToolRequirement::Startup
          || (definition.requirement == ToolRequirement::PackageManager
              && definition.package_manager == pkg_mgr);
    if (required && !IsToolAvailable(definition.tool)) {
      missing.emplace_back(definition.name);
    }
  }
  return missing;
}

std::vector<std::string> SetupContext::MissingPostInstallTools(
    PackageManager pkg_mgr) const
{
  std::vector<std::string> missing;
  const auto package_manager_bit = PackageManagerBit(pkg_mgr);
  for (const auto& definition : kToolDefinitions) {
    const bool required
        = definition.requirement == ToolRequirement::PostInstall
          && (definition.post_install_package_managers & package_manager_bit);
    if (required && !IsToolAvailable(definition.tool)) {
      missing.emplace_back(definition.name);
    }
  }
  return missing;
}

int SetupContext::RunImpl(const SetupCommand& command,
                          const std::string* input,
                          bool run_as_root,
                          OutputCallback output,
                          CommandLogCallback log_command,
                          PreviewCallback preview) const
{
  if (log_command) log_command(command, dry_run_, input != nullptr);
  if (dry_run_) {
    if (preview) preview(input != nullptr);
    return 0;
  }
  return RunCommandImpl(command, input, run_as_root, std::move(output),
                        [this](SetupTool tool) { return ToolPath(tool); });
}

int SetupContext::Run(const SetupCommand& command,
                      bool run_as_root,
                      OutputCallback output,
                      CommandLogCallback log_command,
                      PreviewCallback preview) const
{
  return RunImpl(command, nullptr, run_as_root, std::move(output),
                 std::move(log_command), std::move(preview));
}

int SetupContext::RunWithInput(const SetupCommand& command,
                               const std::string& input,
                               bool run_as_root,
                               OutputCallback output,
                               CommandLogCallback log_command,
                               PreviewCallback preview) const
{
  return RunImpl(command, &input, run_as_root, std::move(output),
                 std::move(log_command), std::move(preview));
}

std::filesystem::path SetupContext::CreateTemporaryFile(
    std::string_view prefix) const
{
  if (dry_run_) {
    throw std::logic_error("Cannot create a temporary file in dry-run mode");
  }
  if (prefix.empty()
      || !std::all_of(prefix.begin(), prefix.end(), [](unsigned char c) {
           return std::isalnum(c) || c == '-' || c == '_';
         })) {
    throw std::invalid_argument("Invalid temporary file prefix");
  }

  struct stat directory_status{};
  if (stat("/tmp", &directory_status) != 0 || !S_ISDIR(directory_status.st_mode)
      || directory_status.st_uid != 0
      || (directory_status.st_mode & S_ISVTX) == 0) {
    throw std::runtime_error(
        "Temporary files require a root-owned sticky /tmp directory");
  }

  std::string directory_pattern = "/tmp/" + std::string(prefix) + "-XXXXXX";
  std::vector<char> directory_name(directory_pattern.begin(),
                                   directory_pattern.end());
  directory_name.push_back('\0');
  if (mkdtemp(directory_name.data()) == nullptr) {
    throw std::runtime_error("Unable to create a private setup directory: "
                             + std::string(strerror(errno)));
  }

  std::string file_pattern
      = std::string(directory_name.data()) + "/script-XXXXXX";
  std::vector<char> file_name(file_pattern.begin(), file_pattern.end());
  file_name.push_back('\0');
  const int fd = mkstemp(file_name.data());
  if (fd < 0) {
    const int error = errno;
    rmdir(directory_name.data());
    throw std::runtime_error("Unable to create a private setup file: "
                             + std::string(strerror(error)));
  }
  if (close(fd) != 0) {
    const int error = errno;
    unlink(file_name.data());
    rmdir(directory_name.data());
    throw std::runtime_error("Unable to close a private setup file: "
                             + std::string(strerror(error)));
  }
  const std::filesystem::path path{file_name.data()};
  {
    std::lock_guard lock(temporary_files_mutex_);
    temporary_file_directories_.emplace(path, directory_name.data());
  }
  return path;
}

void SetupContext::Remove(const std::filesystem::path& path) const
{
  if (dry_run_) return;
  std::filesystem::remove(path);
  std::lock_guard lock(temporary_files_mutex_);
  const auto directory = temporary_file_directories_.find(path);
  if (directory != temporary_file_directories_.end()) {
    std::filesystem::remove(directory->second);
    temporary_file_directories_.erase(directory);
  }
}
