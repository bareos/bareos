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

#include <array>
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

bool IsRoot() { return geteuid() == 0; }

bool PrimeSudoTicket()
{
  if (IsRoot()) return true;
  // Run "sudo -v" with the controlling terminal inherited (unlike the
  // /dev/null-redirected commands below), so the user can be prompted for
  // their password interactively exactly once. A successful run caches a
  // sudo timestamp ticket that later non-interactive "sudo" calls reuse.
  const pid_t pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    execlp("sudo", "sudo", "-v", nullptr);
    const char* msg = strerror(errno);
    [[maybe_unused]] auto _ = write(STDERR_FILENO, msg, strlen(msg));
    _exit(127);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void StartSudoKeepAlive()
{
  if (IsRoot()) return;
  std::thread([] {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::seconds(60));
      const pid_t pid = fork();
      if (pid < 0) continue;
      if (pid == 0) {
        const int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
          dup2(devnull, STDIN_FILENO);
          dup2(devnull, STDOUT_FILENO);
          dup2(devnull, STDERR_FILENO);
          close(devnull);
        }
        execlp("sudo", "sudo", "-v", nullptr);
        _exit(127);
      }
      int status = 0;
      waitpid(pid, &status, 0);
    }
  }).detach();
}

// Drain available bytes from fd into line buffer, calling cb on complete lines.
static void DrainFd(int fd,
                    std::string& buf,
                    const std::string& stream,
                    OutputCallback& cb)
{
  std::array<char, 4096> tmp{};
  // Read until the pipe is drained. A single read() would truncate output
  // whenever more than one buffer's worth is pending -- most importantly on
  // POLLHUP, after which the loop in RunCommandImpl() stops polling this fd
  // and the remaining bytes would be lost.
  for (;;) {
    const ssize_t n = read(fd, tmp.data(), tmp.size());
    if (n <= 0) break;
    buf.append(tmp.data(), static_cast<size_t>(n));
  }
  // Emit complete lines
  size_t pos;
  while ((pos = buf.find('\n')) != std::string::npos) {
    std::string line = buf.substr(0, pos);
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r') line.pop_back();
    cb(line, stream);
    buf.erase(0, pos + 1);
  }
}

static int RunCommandImpl(const std::vector<std::string>& argv,
                          const std::string* input,
                          bool use_sudo,
                          OutputCallback cb)
{
  // Build final argv with optional sudo prefix. Already running as root
  // makes sudo pointless (and would add an avoidable dependency on the
  // sudo binary being installed), so it is skipped in that case.
  std::vector<std::string> full_argv;
  if (use_sudo && !IsRoot()) full_argv.push_back("sudo");
  full_argv.insert(full_argv.end(), argv.begin(), argv.end());

  // Build C-style argv
  std::vector<const char*> cargv;
  cargv.reserve(full_argv.size() + 1);
  for (const auto& s : full_argv) cargv.push_back(s.c_str());
  cargv.push_back(nullptr);

  // Create stdout, stderr, and (when requested) stdin pipes.
  int pipe_out[2], pipe_err[2], pipe_in[2] = {-1, -1};
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
      int devnull = open("/dev/null", O_RDONLY);
      if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        close(devnull);
      }
    }
    execvp(cargv[0], const_cast<char* const*>(cargv.data()));
    // execvp failed — write error to stderr and exit
    const char* msg = strerror(errno);
    [[maybe_unused]] auto _ = write(STDERR_FILENO, msg, strlen(msg));
    _exit(127);
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
      if (n <= 0) break;
      written += static_cast<size_t>(n);
    }
    close(pipe_in[1]);
  }

  // Make pipes non-blocking for poll loop
  fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
  fcntl(pipe_err[0], F_SETFL, O_NONBLOCK);

  std::string buf_out, buf_err;
  bool out_open = true, err_open = true;

  while (out_open || err_open) {
    struct pollfd fds[2] = {
        {pipe_out[0], POLLIN, 0},
        {pipe_err[0], POLLIN, 0},
    };
    int nfds = poll(fds, 2, 500);
    if (nfds < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (fds[0].revents & POLLIN) DrainFd(pipe_out[0], buf_out, "stdout", cb);
    if (fds[1].revents & POLLIN) DrainFd(pipe_err[0], buf_err, "stderr", cb);
    if (fds[0].revents & POLLHUP) {
      DrainFd(pipe_out[0], buf_out, "stdout", cb);
      out_open = false;
    }
    if (fds[1].revents & POLLHUP) {
      DrainFd(pipe_err[0], buf_err, "stderr", cb);
      err_open = false;
    }
  }

  // Flush any remaining partial lines
  if (!buf_out.empty()) cb(buf_out, "stdout");
  if (!buf_err.empty()) cb(buf_err, "stderr");

  close(pipe_out[0]);
  close(pipe_err[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int RunCommand(const std::vector<std::string>& argv,
               bool use_sudo,
               OutputCallback cb)
{
  return RunCommandImpl(argv, nullptr, use_sudo, std::move(cb));
}

int RunCommandWithInput(const std::vector<std::string>& argv,
                        const std::string& input,
                        bool use_sudo,
                        OutputCallback cb)
{
  return RunCommandImpl(argv, &input, use_sudo, std::move(cb));
}

bool IsToolInPath(const std::string& name)
{
  if (name.empty()) return false;
  // An absolute or relative path is checked directly rather than
  // searched for in PATH.
  if (name.find('/') != std::string::npos) {
    struct stat st{};
    return stat(name.c_str(), &st) == 0 && (st.st_mode & S_IXUSR);
  }
  const char* path_env = getenv("PATH");
  const std::string path = path_env != nullptr ? path_env : "/usr/bin:/bin";
  std::istringstream stream(path);
  std::string dir;
  while (std::getline(stream, dir, ':')) {
    if (dir.empty()) continue;
    const std::string candidate = dir + "/" + name;
    struct stat st{};
    if (stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode)
        && (st.st_mode & S_IXUSR)) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> MissingRequiredTools(const std::string& pkg_mgr)
{
  std::vector<std::string> required
      = {"curl", "bash", "install", "chown", "systemctl", "su", "sh"};
  if (!pkg_mgr.empty() && pkg_mgr != "unknown") required.push_back(pkg_mgr);
  if (pkg_mgr == "apt") required.push_back("apt-get");

  std::vector<std::string> missing;
  for (const auto& tool : required) {
    if (!IsToolInPath(tool)) missing.push_back(tool);
  }
  return missing;
}
