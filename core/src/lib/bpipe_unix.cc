/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
/*
 * bpipe.c bi-directional pipe
 *
 * Kern Sibbald, November MMII
 */

#include <algorithm>
#include <fcntl.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <vector>
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/bareos.h"
#include "jcr.h"
#include "lib/berrno.h"
#include "lib/bsys.h"
#include "lib/btimers.h"
#include "lib/util.h"
#include "lib/bpipe.h"

extern char** environ;

#if defined(__GLIBC__)
#  if __GLIBC_PREREQ(2, 34)
#    define HAVE_POSIX_SPAWN_CLOSEFROM_NP 1
#  endif
#endif

int execvp_errors[] = {EACCES,       ENOEXEC, EFAULT,  EINTR, E2BIG,
                       ENAMETOOLONG, ENOMEM,  ETXTBSY, ENOENT};
int num_execvp_errors = (int)(sizeof(execvp_errors) / sizeof(int));

#define MAX_ARGV 100

static void BuildArgcArgv(char* cmd, int* bargc, char* bargv[], int max_arg);

namespace {
std::vector<char*> BuildEnvironment(
    const std::unordered_map<std::string, std::string>& env_vars,
    std::vector<std::string>& environment)
{
  for (char** entry = environ; *entry; ++entry) {
    environment.emplace_back(*entry);
  }

  for (const auto& [var_name, var_value] : env_vars) {
    const std::string variable = var_name + "=";
    const auto entry = std::find_if(
        environment.begin(), environment.end(),
        [&](const std::string& value) { return value.starts_with(variable); });
    const std::string value = variable + var_value;
    if (entry == environment.end()) {
      environment.emplace_back(value);
    } else {
      *entry = value;
    }
  }

  std::vector<char*> environment_pointers;
  environment_pointers.reserve(environment.size() + 1);
  for (auto& entry : environment) {
    environment_pointers.push_back(entry.data());
  }
  environment_pointers.push_back(nullptr);
  return environment_pointers;
}

int MovePipeDescriptorsAboveStandardStreams(int pipe_descriptors[2])
{
  for (int i = 0; i < 2; ++i) {
    int& descriptor = pipe_descriptors[i];
    if (descriptor > STDERR_FILENO) { continue; }

    const int replacement = fcntl(descriptor, F_DUPFD, STDERR_FILENO + 1);
    if (replacement == -1) { return errno; }

    close(descriptor);
    descriptor = replacement;
  }
  return 0;
}
}  // namespace

/*
 * Run an external program. Optionally wait a specified number
 * of seconds. Program killed if wait exceeded. We open
 * a bi-directional pipe so that the user can read from and
 * write to the program.
 */
Bpipe* OpenBpipe(const char* prog,
                 int wait,
                 const char* mode,
                 bool dup_stderr,
                 const std::unordered_map<std::string, std::string>& env_vars)
{
  char* bargv[MAX_ARGV];
  int bargc;
  int readp[2], writep[2];
  POOLMEM* tprog;
  int mode_read, mode_write;
  Bpipe* bpipe;
  int save_errno;
  posix_spawn_file_actions_t file_actions;
  bool file_actions_initialized = false;
  std::vector<std::string> environment;
  std::vector<char*> environment_pointers;
  std::vector<char*> shell_arguments;

  bpipe = (Bpipe*)malloc(sizeof(Bpipe));
  memset(bpipe, 0, sizeof(Bpipe));
  mode_read = (mode[0] == 'r');
  mode_write = (mode[0] == 'w' || mode[1] == 'w');

  // Build arguments for running program.
  tprog = GetPoolMemory(PM_FNAME);
  PmStrcpy(tprog, prog);
  BuildArgcArgv(tprog, &bargc, bargv, MAX_ARGV);

  // Each pipe is one way, write one end, read the other, so we need two
  if (mode_write && pipe(writep) == -1) {
    save_errno = errno;
    free(bpipe);
    FreePoolMemory(tprog);
    errno = save_errno;
    return NULL;
  }
  if (mode_write
      && (save_errno = MovePipeDescriptorsAboveStandardStreams(writep)) != 0) {
    close(writep[0]);
    close(writep[1]);
    free(bpipe);
    FreePoolMemory(tprog);
    errno = save_errno;
    return NULL;
  }
  if (mode_read && pipe(readp) == -1) {
    save_errno = errno;
    if (mode_write) {
      close(writep[0]);
      close(writep[1]);
    }
    free(bpipe);
    FreePoolMemory(tprog);
    errno = save_errno;
    return NULL;
  }
  if (mode_read
      && (save_errno = MovePipeDescriptorsAboveStandardStreams(readp)) != 0) {
    close(readp[0]);
    close(readp[1]);
    if (mode_write) {
      close(writep[0]);
      close(writep[1]);
    }
    free(bpipe);
    FreePoolMemory(tprog);
    errno = save_errno;
    return NULL;
  }

  save_errno = posix_spawn_file_actions_init(&file_actions);
  file_actions_initialized = save_errno == 0;
  if (save_errno == 0 && mode_write) {
    save_errno = posix_spawn_file_actions_addclose(&file_actions, writep[1]);
  }
  if (save_errno == 0 && mode_write) {
    save_errno = posix_spawn_file_actions_adddup2(&file_actions, writep[0], 0);
  }
  if (save_errno == 0 && mode_write) {
    save_errno = posix_spawn_file_actions_addclose(&file_actions, writep[0]);
  }
  if (save_errno == 0 && mode_read) {
    save_errno = posix_spawn_file_actions_addclose(&file_actions, readp[0]);
  }
  if (save_errno == 0 && mode_read) {
    save_errno = posix_spawn_file_actions_adddup2(&file_actions, readp[1], 1);
  }
  if (save_errno == 0 && mode_read && dup_stderr) {
    save_errno = posix_spawn_file_actions_adddup2(&file_actions, readp[1], 2);
  }
  if (save_errno == 0 && mode_read) {
    save_errno = posix_spawn_file_actions_addclose(&file_actions, readp[1]);
  }
#if defined(HAVE_POSIX_SPAWN_CLOSEFROM_NP)
  if (save_errno == 0) {
    save_errno = posix_spawn_file_actions_addclosefrom_np(&file_actions, 3);
  }
#elif defined(HAVE_CLOSEFROM)
  const long open_max = sysconf(_SC_OPEN_MAX);
  for (int fd = 3; save_errno == 0 && fd < open_max; ++fd) {
    save_errno = posix_spawn_file_actions_addclose(&file_actions, fd);
  }
#else
  for (int fd = 3; save_errno == 0 && fd <= 32; ++fd) {
    save_errno = posix_spawn_file_actions_addclose(&file_actions, fd);
  }
#endif
  if (save_errno == 0) {
    environment_pointers = BuildEnvironment(env_vars, environment);
    save_errno = posix_spawnp(&bpipe->worker_pid, bargv[0], &file_actions,
                              nullptr, bargv, environment_pointers.data());
  }
  if (save_errno == ENOEXEC) {
    shell_arguments.reserve(bargc + 2);
    shell_arguments.push_back(const_cast<char*>("sh"));
    for (int i = 0; i < bargc; ++i) { shell_arguments.push_back(bargv[i]); }
    shell_arguments.push_back(nullptr);
    save_errno
        = posix_spawn(&bpipe->worker_pid, "/bin/sh", &file_actions, nullptr,
                      shell_arguments.data(), environment_pointers.data());
  }
  if (file_actions_initialized) {
    posix_spawn_file_actions_destroy(&file_actions);
  }
  FreePoolMemory(tprog);

  if (save_errno != 0) {
    if (mode_write) {
      close(writep[0]);
      close(writep[1]);
    }
    if (mode_read) {
      close(readp[0]);
      close(readp[1]);
    }
    free(bpipe);
    errno = save_errno;
    return NULL;
  }

  if (mode_read) {
    close(readp[1]);                    /* close unused parent fds */
    bpipe->rfd = fdopen(readp[0], "r"); /* open file descriptor */
  }

  if (mode_write) {
    close(writep[0]);
    bpipe->wfd = fdopen(writep[1], "w");
  }

  bpipe->worker_stime = time(NULL);
  bpipe->wait = wait;

  if (wait > 0) {
    bpipe->timer_id = StartChildTimer(NULL, bpipe->worker_pid, wait);
  }

  return bpipe;
}

// Close the write pipe only
int CloseWpipe(Bpipe* bpipe)
{
  int status = 1;

  if (bpipe->wfd) {
    fflush(bpipe->wfd);
    if (fclose(bpipe->wfd) != 0) { status = 0; }
    bpipe->wfd = NULL;
  }
  return status;
}

/*
 * Close both pipes and free resources
 *
 * Returns: 0 on success
 *          BErrNo on failure
 */
int CloseBpipe(Bpipe* bpipe)
{
  int chldstatus = 0;
  int status = 0;
  int wait_option;
  int remaining_wait;
  pid_t wpid = 0;


  // Close pipes
  if (bpipe->rfd) {
    fclose(bpipe->rfd);
    bpipe->rfd = NULL;
  }

  if (bpipe->wfd) {
    fclose(bpipe->wfd);
    bpipe->wfd = NULL;
  }

  if (bpipe->wait == 0) {
    wait_option = 0; /* wait indefinitely */
  } else {
    wait_option = WNOHANG; /* don't hang */
  }
  remaining_wait = bpipe->wait;

  // Wait for worker child to exit
  for (;;) {
    Dmsg2(800, "Wait for %d opt=%d\n", bpipe->worker_pid, wait_option);
    do {
      wpid = waitpid(bpipe->worker_pid, &chldstatus, wait_option);
    } while (wpid == -1 && (errno == EINTR || errno == EAGAIN));
    if (wpid == bpipe->worker_pid || wpid == -1) {
      BErrNo be;
      status = errno;
      Dmsg3(800, "Got break wpid=%d status=%d ERR=%s\n", wpid, chldstatus,
            wpid == -1 ? be.bstrerror() : "none");
      break;
    }
    Dmsg3(800, "Got wpid=%d status=%d ERR=%s\n", wpid, chldstatus,
          wpid == -1 ? strerror(errno) : "none");
    if (remaining_wait > 0) {
      Bmicrosleep(1, 0); /* wait one second */
      remaining_wait--;
    } else {
      status = ETIME; /* set error status */
      wpid = -1;
      break; /* don't wait any longer */
    }
  }

  if (wpid > 0) {
    if (WIFEXITED(chldstatus)) { /* process exit()ed */
      status = WEXITSTATUS(chldstatus);
      if (status != 0) {
        Dmsg1(800, "Non-zero status %d returned from child.\n", status);
        status |= b_errno_exit; /* exit status returned */
      }
      Dmsg1(800, "child status=%d\n", status & ~b_errno_exit);
    } else if (WIFSIGNALED(chldstatus)) { /* process died */
      status = WTERMSIG(chldstatus);
      Dmsg1(800, "Child died from signal %d\n", status);
      status |= b_errno_signal; /* exit signal returned */
    }
  }

  if (bpipe->timer_id) { StopChildTimer(bpipe->timer_id); }

  free(bpipe);
  Dmsg2(800, "returning status=%d,%d\n",
        status & ~(b_errno_exit | b_errno_signal), status);

  return status;
}

// Build argc and argv from a string
static void BuildArgcArgv(char* cmd, int* bargc, char* bargv[], int max_argv)
{
  int i;
  char *p, *q, quote;
  int argc = 0;

  argc = 0;
  for (i = 0; i < max_argv; i++) bargv[i] = NULL;

  p = cmd;
  quote = 0;
  while (*p && (*p == ' ' || *p == '\t')) p++;
  if (*p == '\"' || *p == '\'') {
    quote = *p;
    p++;
  }
  if (*p) {
    while (*p && argc < MAX_ARGV) {
      q = p;
      if (quote) {
        while (*q && *q != quote) q++;
        quote = 0;
      } else {
        while (*q && *q != ' ') q++;
      }
      if (*q) *(q++) = '\0';
      bargv[argc++] = p;
      p = q;
      while (*p && (*p == ' ' || *p == '\t')) p++;
      if (*p == '\"' || *p == '\'') {
        quote = *p;
        p++;
      }
    }
  }
  *bargc = argc;
}
