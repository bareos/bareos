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
 */

#define main unused_console_main
#define tcsetattr intercepted_tcsetattr
#define ConsoleOutput intercepted_console_output
#include "console/console.cc"
#undef ConsoleOutput
#undef tcsetattr
#undef main

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <gtest/gtest.h>

namespace {
enum class Injection
{
  None,
  Raw,
  Screen,
  RawFailure
};
Injection injection = Injection::None;
bool signal_from_worker = false;

void* RaiseFromWorker(void*)
{
  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGTERM);
  if (pthread_sigmask(SIG_UNBLOCK, &signals, nullptr) != 0) { _exit(3); }
  raise(SIGTERM);
  return nullptr;
}

void InjectSignal()
{
  if (!signal_from_worker) {
    raise(SIGTERM);
    return;
  }
  pthread_t worker;
  if (pthread_create(&worker, nullptr, RaiseFromWorker, nullptr) != 0
      || pthread_join(worker, nullptr) != 0) {
    _exit(3);
  }
}

void TerminateTestConsole(int)
{
  RestoreActiveSelectionTerminalState();
  _exit(42);
}
}  // namespace

extern "C" int intercepted_tcsetattr(int fd,
                                     int action,
                                     const termios* value) noexcept
{
  if (injection == Injection::RawFailure) {
    errno = EIO;
    return -1;
  }
  auto actual = reinterpret_cast<int (*)(int, int, const termios*)>(
      dlsym(RTLD_NEXT, "tcsetattr"));
  if (!actual) { _exit(10); }
  int result = actual(fd, action, value);
  if (result == 0 && injection == Injection::Raw) {
    injection = Injection::None;
    InjectSignal();
  }
  return result;
}

void intercepted_console_output(const char* text)
{
  (void)write(STDOUT_FILENO, text, strlen(text));
  if (injection == Injection::Screen && strstr(text, "\033[?1049h")) {
    injection = Injection::None;
    InjectSignal();
  }
}

namespace {
struct FileDescriptor {
  int fd;
  ~FileDescriptor()
  {
    if (fd >= 0) { close(fd); }
  }
};

void CheckSelectionCleanup(Injection point,
                           bool worker,
                           bool inject = true,
                           bool routing = true)
{
  FileDescriptor master_fd{posix_openpt(O_RDWR | O_NOCTTY)};
  int master = master_fd.fd;
  ASSERT_GE(master, 0);
  ASSERT_EQ(grantpt(master), 0);
  ASSERT_EQ(unlockpt(master), 0);
  const char* name = ptsname(master);
  ASSERT_NE(name, nullptr);
  FileDescriptor slave_fd{open(name, O_RDWR | O_NOCTTY)};
  int slave = slave_fd.fd;
  ASSERT_GE(slave, 0);
  termios before{};
  ASSERT_EQ(tcgetattr(slave, &before), 0);

  pid_t child = fork();
  ASSERT_GE(child, 0);
  if (child == 0) {
    if (dup2(slave, STDIN_FILENO) < 0 || dup2(slave, STDOUT_FILENO) < 0) {
      _exit(5);
    }
    signal(SIGTERM, TerminateTestConsole);
    if (routing && !InstallConsoleSignalRouting()) { _exit(4); }
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &blocked, nullptr) != 0) { _exit(6); }
    TerminalRawModeGuard failed(-1);
    if (failed.active || selection_raw_mode_active) { _exit(7); }
    injection = inject ? point : Injection::None;
    signal_from_worker = worker;
    if (point == Injection::Raw) {
      TerminalRawModeGuard raw(STDIN_FILENO);
      if (!raw.active) { _exit(8); }
    } else {
      TerminalSelectionScreenGuard screen;
      if (!screen.active) { _exit(8); }
    }
    sigset_t current;
    if (pthread_sigmask(SIG_SETMASK, nullptr, &current) != 0
        || sigismember(&current, SIGUSR1) != 1
        || sigismember(&current, SIGTERM) != 0) {
      _exit(9);
    }
    _exit(0);
  }
  int status = 0;
  pid_t waited = 0;
  for (int i = 0; i < 500 && waited == 0; ++i) {
    waited = waitpid(child, &status, WNOHANG);
    if (waited == 0) { usleep(10000); }
  }
  if (waited == 0) {
    kill(child, SIGKILL);
    waitpid(child, &status, 0);
    FAIL() << "Child did not exit after signal injection";
  }
  ASSERT_EQ(waited, child);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), inject ? 42 : 0);
  termios after{};
  ASSERT_EQ(tcgetattr(slave, &after), 0);
  EXPECT_EQ(after.c_iflag, before.c_iflag);
  EXPECT_EQ(after.c_lflag, before.c_lflag);
  EXPECT_EQ(memcmp(after.c_cc, before.c_cc, sizeof(before.c_cc)), 0);
  close(slave);
  slave_fd.fd = -1;
  std::string output;
  char buf[128];
  ssize_t size;
  while ((size = read(master, buf, sizeof(buf))) > 0) {
    output.append(buf, static_cast<size_t>(size));
  }
  if (point == Injection::Screen) {
    EXPECT_NE(output.find("\033[?1049h"), std::string::npos);
    EXPECT_NE(output.find("\033[?1049l"), std::string::npos);
  }
}
}  // namespace

TEST(ConsoleSignal, RestoresRawModeWhenSignalledDuringSetup)
{
  CheckSelectionCleanup(Injection::Raw, false);
  CheckSelectionCleanup(Injection::Raw, true);
}

TEST(ConsoleSignal, RestoresScreenWhenSignalledDuringSetup)
{
  CheckSelectionCleanup(Injection::Screen, false);
  CheckSelectionCleanup(Injection::Screen, true);
}

TEST(ConsoleSignal, RestoresNormallyAndPreservesSignalMask)
{
  CheckSelectionCleanup(Injection::Raw, false, false);
  CheckSelectionCleanup(Injection::Screen, false, false);
  CheckSelectionCleanup(Injection::Raw, false, false, false);
  CheckSelectionCleanup(Injection::Screen, false, false, false);
}

TEST(ConsoleSignal, FailedRawModeSetupDoesNotRegisterCleanup)
{
  FileDescriptor master{posix_openpt(O_RDWR | O_NOCTTY)};
  ASSERT_GE(master.fd, 0);
  ASSERT_EQ(grantpt(master.fd), 0);
  ASSERT_EQ(unlockpt(master.fd), 0);
  const char* name = ptsname(master.fd);
  ASSERT_NE(name, nullptr);
  FileDescriptor slave{open(name, O_RDWR | O_NOCTTY)};
  ASSERT_GE(slave.fd, 0);
  termios before{};
  ASSERT_EQ(tcgetattr(slave.fd, &before), 0);
  injection = Injection::RawFailure;
  {
    TerminalRawModeGuard raw(slave.fd);
    EXPECT_FALSE(raw.active);
    EXPECT_FALSE(selection_raw_mode_active);
  }
  injection = Injection::None;
  termios after{};
  ASSERT_EQ(tcgetattr(slave.fd, &after), 0);
  EXPECT_EQ(after.c_iflag, before.c_iflag);
  EXPECT_EQ(after.c_lflag, before.c_lflag);
}
