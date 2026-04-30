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
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

// Drain available bytes from fd into line buffer, calling cb on complete lines.
static void DrainFd(int fd,
                    std::string& buf,
                    const std::string& stream,
                    OutputCallback& cb)
{
  std::array<char, 4096> tmp{};
  ssize_t n = read(fd, tmp.data(), tmp.size());
  if (n <= 0) return;
  buf.append(tmp.data(), static_cast<size_t>(n));
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

int RunCommand(const std::vector<std::string>& argv,
               bool use_sudo,
               OutputCallback cb)
{
  // Build final argv with optional sudo prefix
  std::vector<std::string> full_argv;
  if (use_sudo) full_argv.push_back("sudo");
  full_argv.insert(full_argv.end(), argv.begin(), argv.end());

  // Build C-style argv
  std::vector<const char*> cargv;
  cargv.reserve(full_argv.size() + 1);
  for (const auto& s : full_argv) cargv.push_back(s.c_str());
  cargv.push_back(nullptr);

  // Create stdout and stderr pipes
  int pipe_out[2], pipe_err[2], pipe_exec[2];
  if (pipe(pipe_out) != 0 || pipe(pipe_err) != 0 || pipe(pipe_exec) != 0)
    throw std::runtime_error(std::string("pipe: ") + strerror(errno));
  fcntl(pipe_exec[1], F_SETFD, FD_CLOEXEC);

  pid_t pid = fork();
  if (pid < 0)
    throw std::runtime_error(std::string("fork: ") + strerror(errno));

  if (pid == 0) {
    // Child
    close(pipe_out[0]);
    close(pipe_err[0]);
    close(pipe_exec[0]);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_err[1], STDERR_FILENO);
    close(pipe_out[1]);
    close(pipe_err[1]);
    // Redirect stdin from /dev/null so sudo doesn't hang asking for password
    int devnull = open("/dev/null", O_RDONLY);
    if (devnull >= 0) {
      dup2(devnull, STDIN_FILENO);
      close(devnull);
    }
    execvp(cargv[0], const_cast<char* const*>(cargv.data()));
    const int exec_errno = errno;
    [[maybe_unused]] auto _
        = write(pipe_exec[1], &exec_errno, sizeof(exec_errno));
    close(pipe_exec[1]);
    _exit(127);
  }

  // Parent
  close(pipe_out[1]);
  close(pipe_err[1]);
  close(pipe_exec[1]);

  int exec_errno = 0;
  const ssize_t exec_bytes = read(pipe_exec[0], &exec_errno, sizeof(exec_errno));
  close(pipe_exec[0]);
  if (exec_bytes > 0) {
    close(pipe_out[0]);
    close(pipe_err[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    throw std::runtime_error(std::string("execvp: ") + strerror(exec_errno));
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
