/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
 * Miscellaneous BAREOS memory and thread safe routines
 * Generally, these are interfaces to system or standard
 * library routines.
 *
 * BAREOS utility functions are in util.c
 */


#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "lib/berrno.h"
#include "lib/recent_job_results_list.h"
#if __has_include(<regex.h>)
#  include <regex.h>
#else
#  include "lib/bregex.h"
#endif
#include "lib/bpipe.h"

#include <fstream>
#include <type_traits>
#include <sys/file.h>

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer = PTHREAD_COND_INITIALIZER;
static const char* secure_erase_cmdline = NULL;

/*
 * This routine is a somewhat safer unlink in that it
 * allows you to run a regex on the filename before
 * excepting it. It also requires the file to be in
 * the working directory.
 */
int SaferUnlink(const char* pathname, const char* regx)
{
  int rc;
  regex_t preg1{};
  char prbuf[500];
  int rtn;

  // Name must start with working directory
  if (strncmp(pathname, working_directory, strlen(working_directory)) != 0) {
    Pmsg1(000, "Safe_unlink excluded: %s\n", pathname);
    return EROFS;
  }

  // Compile regex expression
  rc = regcomp(&preg1, regx, REG_EXTENDED);
  if (rc != 0) {
    regerror(rc, &preg1, prbuf, sizeof(prbuf));
    Pmsg2(000,
          T_("safe_unlink could not compile regex pattern \"%s\" ERR=%s\n"),
          regx, prbuf);
    return ENOENT;
  }

  // Unlink files that match regexes
  if (regexec(&preg1, pathname, 0, NULL, 0) == 0) {
    Dmsg1(100, "safe_unlink unlinking: %s\n", pathname);
    rtn = SecureErase(NULL, pathname);
  } else {
    Pmsg2(000, "safe_unlink regex failed: regex=%s file=%s\n", regx, pathname);
    rtn = EROFS;
  }
  regfree(&preg1);

  return rtn;
}

// This routine will use an external secure erase program to delete a file.
int SecureErase(JobControlRecord* jcr, const char* pathname)
{
  int retval = -1;

  if (secure_erase_cmdline) {
    int status;
    Bpipe* bpipe;
    PoolMem line(PM_NAME), cmdline(PM_MESSAGE);

    Mmsg(cmdline, "%s \"%s\"", secure_erase_cmdline, pathname);
    if (jcr) {
      Jmsg(jcr, M_INFO, 0, T_("SecureErase: executing %s\n"), cmdline.c_str());
    }

    bpipe = OpenBpipe(cmdline.c_str(), 0, "r");
    if (bpipe == NULL) {
      BErrNo be;

      if (jcr) {
        Jmsg(jcr, M_FATAL, 0, T_("SecureErase: %s could not execute. ERR=%s\n"),
             secure_erase_cmdline, be.bstrerror());
      }
      goto bail_out;
    }

    while (fgets(line.c_str(), line.size(), bpipe->rfd)) {
      StripTrailingJunk(line.c_str());
      if (jcr) { Jmsg(jcr, M_INFO, 0, T_("SecureErase: %s\n"), line.c_str()); }
    }

    status = CloseBpipe(bpipe);
    if (status != 0) {
      BErrNo be;

      if (jcr) {
        Jmsg(jcr, M_FATAL, 0,
             T_("SecureErase: %s returned non-zero status=%d. ERR=%s\n"),
             secure_erase_cmdline, be.code(status), be.bstrerror(status));
      }
      goto bail_out;
    }

    Dmsg0(100, "wpipe_command OK\n");
    retval = 0;
  } else {
#ifdef HAVE_MSVC
    retval = _unlink(pathname);
#else
    retval = unlink(pathname);
#endif
  }

  return retval;

bail_out:
  errno = EROFS;
  return retval;
}

void SetSecureEraseCmdline(const char* cmdline)
{
  secure_erase_cmdline = cmdline;
}

/*
 * This routine will sleep (sec, microsec).  Note, however, that if a
 * signal occurs, it will return early.  It is up to the caller
 * to recall this routine if he/she REALLY wants to sleep the
 * requested time.
 */
int Bmicrosleep(int32_t sec, int32_t usec)
{
  struct timespec timeout;
  struct timeval tv;
  int status;

  timeout.tv_sec = sec;
  timeout.tv_nsec = static_cast<decltype(timeout.tv_nsec)>(usec) * 1000l;

#if !defined(HAVE_WIN32)
  status = nanosleep(&timeout, NULL);
  if (!(status < 0 && errno == ENOSYS)) { return status; }
  // If we reach here it is because nanosleep is not supported by the OS
#endif

  // Do it the old way
  gettimeofday(&tv, NULL);
  timeout.tv_nsec += static_cast<decltype(timeout.tv_nsec)>(tv.tv_usec) * 1000l;
  timeout.tv_sec += tv.tv_sec;
  while (timeout.tv_nsec >= 1000000000) {
    timeout.tv_nsec -= 1000000000;
    timeout.tv_sec++;
  }

  Dmsg2(200, "pthread_cond_timedwait sec=%" PRId32 " usec=%" PRId32 "\n", sec,
        usec);

  // Note, this unlocks mutex during the sleep
  lock_mutex(timer_mutex);
  status = pthread_cond_timedwait(&timer, &timer_mutex, &timeout);
  unlock_mutex(timer_mutex);

  return status;
}

/*
 * Copy a string inline from one point in the same string
 * to another.
 */
char* bstrinlinecpy(char* dest, const char* src)
{
  int len;

  /* Sanity check. We can only inline copy if the src > dest
   * otherwise the resulting copy will overwrite the end of
   * the string. */
  if (src <= dest) { return NULL; }

  len = strlen(src);
  /* Cannot use strcpy or memcpy as those functions are not
   * allowed on overlapping data and this is inline replacement
   * for sure is. So we use memmove which is allowed on
   * overlapping data. */
  memmove(dest, src, len + 1);

  return dest;
}

/*
 * Guarantee that the string is properly terminated.
 * maxlen is the maximum of the string pointed to by src,
 * including the terminating null byte ('\0').
 */
char* bstrncpy(char* dest, const char* src, int maxlen)
{
  std::string tmp;

  if ((src == nullptr) || (maxlen <= 1)) {
    dest[0] = 0;
    return dest;
  }

  if ((dest <= src) && ((dest + (maxlen - 1) * sizeof(char)) >= src)) {
    Dmsg0(100, "Overlapping strings found, using copy.\n");
    tmp.assign(src);
    src = tmp.c_str();
  }

  strncpy(dest, src, maxlen - 1);
  dest[maxlen - 1] = 0;
  return dest;
}

// Guarantee that the string is properly terminated
char* bstrncpy(char* dest, PoolMem& src, int maxlen)
{
  return bstrncpy(dest, src.c_str(), maxlen);
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char* bstrncat(char* dest, const char* src, int maxlen)
{
  std::string tmp;
  int len = strlen(dest);

  if ((dest <= src) && ((dest + (maxlen - 1) * sizeof(char)) >= src)) {
    Dmsg0(100, "Overlapping strings found, using copy.\n");
    tmp.assign(src);
    src = tmp.c_str();
  }

  if (len < maxlen - 1) { bstrncpy(dest + len, src, maxlen - len); }
  return dest;
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char* bstrncat(char* dest, PoolMem& src, int maxlen)
{
  return bstrncat(dest, src.c_str(), maxlen);
}

// Allows one or both pointers to be NULL
bool bstrcmp(const char* s1, const char* s2)
{
  if (s1 == s2) return true;
  if (s1 == NULL || s2 == NULL) return false;
  return strcmp(s1, s2) == 0;
}

bool bstrncmp(const char* s1, const char* s2, int n)
{
  if (s1 == s2) return true;
  if (s1 == NULL || s2 == NULL) return false;
  return strncmp(s1, s2, n) == 0;
}

bool Bstrcasecmp(const char* s1, const char* s2)
{
  if (s1 == s2) return true;
  if (s1 == NULL || s2 == NULL) return false;
  return strcasecmp(s1, s2) == 0;
}

bool bstrncasecmp(const char* s1, const char* s2, int n)
{
  if (s1 == s2) return true;
  if (s1 == NULL || s2 == NULL) return false;
  return strncasecmp(s1, s2, n) == 0;
}

/*
 * Get character length of UTF-8 string
 *
 * Valid UTF-8 codes
 * U-00000000 - U-0000007F: 0xxxxxxx
 * U-00000080 - U-000007FF: 110xxxxx 10xxxxxx
 * U-00000800 - U-0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
 * U-00010000 - U-001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U-00200000 - U-03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U-04000000 - U-7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * 10xxxxxx
 */
int cstrlen(const char* str)
{
  uint8_t* p = (uint8_t*)str;
  int len = 0;
  if (str == NULL) { return 0; }
  while (*p) {
    if ((*p & 0xC0) != 0xC0) {
      p++;
      len++;
      continue;
    }
    if ((*p & 0xD0) == 0xC0) {
      p += 2;
      len++;
      continue;
    }
    if ((*p & 0xF0) == 0xD0) {
      p += 3;
      len++;
      continue;
    }
    if ((*p & 0xF8) == 0xF0) {
      p += 4;
      len++;
      continue;
    }
    if ((*p & 0xFC) == 0xF8) {
      p += 5;
      len++;
      continue;
    }
    if ((*p & 0xFE) == 0xFC) {
      p += 6;
      len++;
      continue;
    }
    p++; /* Shouln't get here but must advance */
  }
  return len;
}

#ifndef HAVE_READDIR_R
#  ifndef HAVE_WIN32
#    include <dirent.h>

int Readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result)
{
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  struct dirent* ndir;
  int status;

  lock_mutex(mutex);
  errno = 0;
  ndir = readdir(dirp);
  status = errno;
  if (ndir) {
    memcpy(entry, ndir, sizeof(struct dirent));
    strcpy(entry->d_name, ndir->d_name);
    *result = entry;
  } else {
    *result = NULL;
  }
  unlock_mutex(mutex);
  return status;
}
#  endif
#endif /* HAVE_READDIR_R */

int b_strerror(int errnum, char* buf, size_t bufsiz)
{
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  int status = 0;
  const char* msg;

  lock_mutex(mutex);

  msg = strerror(errnum);
  if (!msg) {
    msg = T_("Bad errno");
    status = -1;
  }
  bstrncpy(buf, msg, bufsiz);
  unlock_mutex(mutex);
  return status;
}

#if !defined(HAVE_WIN32)
static void LockPidFile(const char* progname,
                        int pidfile_fd,
                        const char* pidfile_path)
{
  struct flock fl;

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  if (fcntl(pidfile_fd, F_SETLK, &fl)) {
    if (errno == EAGAIN || errno == EACCES) {
      BErrNo be;
      Emsg2(M_ERROR_TERM, 0,
            T_("PID file '%s' is locked; probably '%s' is already running. "),
            pidfile_path, progname);

    } else {
      BErrNo be;
      Emsg2(M_ERROR_TERM, 0, T_("Unable to lock PID file '%s'. ERR=%s\n"),
            pidfile_path, be.bstrerror());
    }
  }
}

static void UnlockPidFile(int pidfile_fd, const char* pidfile_path)
{
  struct flock fl;

  fl.l_type = F_UNLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  if (fcntl(pidfile_fd, F_SETLK, &fl)) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Unable to unlock PID file '%s'. ERR=%s\n"),
          pidfile_path, be.bstrerror());
  }
}
#endif  // HAVE_WIN32
/*
   The content of this function (CreatePidFile) was inspired and modified to fit
   current needs from filelock/create_pid_file.c (Listing 55-4, page 1143), an
   example to accompany the book, The Linux Programming Interface.

   The original source code file of filelock/create_pid_file.c is copyright
   2010, Michael Kerrisk, and is licensed under the GNU Lesser General Public
   License, version 3.
*/

#define CPF_CLOEXEC 1

#if !defined(HAVE_WIN32)
int CreatePidFile(const char* progname, const char* pidfile_path)
{
  int pidfd;

  pidfd = open(pidfile_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (pidfd == -1) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Cannot open pid file. %s ERR=%s\n"),
          pidfile_path, be.bstrerror());
  }

  int flags;
  if (CPF_CLOEXEC) {
    /* Set the close-on-exec file descriptor flag */

    /* Instead of the following steps, we could (on Linux) have opened the
       file with O_CLOEXEC flag. However, not all systems support open()
       O_CLOEXEC (which was standardized only in SUSv4), so instead we use
       fcntl() to set the close-on-exec flag after opening the file */

    flags = fcntl(pidfd, F_GETFD); /* Fetch flags */
    if (flags == -1) {
      BErrNo be;
      Emsg2(M_ERROR_TERM, 0,
            T_("Could not get flags for PID file %s. ERR=%s\n"), pidfile_path,
            be.bstrerror());
    }

    flags |= FD_CLOEXEC; /* Turn on FD_CLOEXEC */

    if (fcntl(pidfd, F_SETFD, flags) == -1) /* Update flags */ {
      BErrNo be;
      Emsg2(M_ERROR_TERM, 0,
            T_("Could not get flags for PID file %s. ERR=%s\n"), pidfile_path,
            be.bstrerror());
    }
  }

  // Locking and unlocking only to check if there is already an instance of
  // bareos running
  LockPidFile(progname, pidfd, pidfile_path);
  UnlockPidFile(pidfd, pidfile_path);
  return pidfd;
}
#endif

/*
   The content of this function (WritePidFile) was inspired and modified to fit
   current needs from filelock/create_pid_file.c (Listing 55-4, page 1143), an
   example to accompany the book, The Linux Programming Interface.

   The original source code file of filelock/create_pid_file.c is copyright
   2010, Michael Kerrisk, and is licensed under the GNU Lesser General Public
   License, version 3.
*/
#if !defined(HAVE_WIN32)
void WritePidFile(int pidfile_fd,
                  const char* pidfile_path,
                  const char* progname)
{
  const int buf_size = 100;
  char buf[buf_size];

  LockPidFile(progname, pidfile_fd, pidfile_path);

  if (ftruncate(pidfile_fd, 0) == -1) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Could not truncate PID file '%s'. ERR=%s\n"),
          pidfile_path, be.bstrerror());
  }

  snprintf(buf, buf_size, "%d\n", getpid());
  if (write(pidfile_fd, buf, strlen(buf))
      != static_cast<ssize_t>(strlen(buf))) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0, T_("Writing to PID file '%s'. ERR=%s\n"),
          pidfile_path, be.bstrerror());
  }
}
#endif

#if defined(HAVE_WIN32)
int DeletePidFile(std::string) { return 1; }
#else
int DeletePidFile(std::string pidfile_path)
{
  if (!pidfile_path.empty()) { unlink(pidfile_path.c_str()); }
  return 1;
}
#endif

static struct StateFileHeader state_hdr = {{"Bareos State\n"}, 4, 0, 0, {0}};

static bool CheckHeader(const StateFileHeader& hdr)
{
  if (hdr.version != state_hdr.version) {
    Dmsg2(100, "Bad hdr version. Wanted %d got %d\n", state_hdr.version,
          hdr.version);
    return false;
  }

  if (strncmp(hdr.id, state_hdr.id, sizeof(hdr.id))) {
    Dmsg0(100, "State file header id invalid.\n");
    return false;
  }
  return true;
}

class SecureEraseGuard {
  std::string filename;
  bool cleanup = true;

 public:
  SecureEraseGuard(const std::string& fname_in) : filename(fname_in) {}
  ~SecureEraseGuard()
  {
    if (cleanup) { SecureErase(nullptr, filename.c_str()); }
  }
  void Release() { cleanup = false; }
};

static std::string CreateFileNameFrom(const char* dir,
                                      const char* progname,
                                      int port)
{
  int amount = snprintf(nullptr, 0, "%s/%s.%d.state", dir, progname, port) + 1;
  std::vector<char> filename(amount);
  snprintf(filename.data(), amount, "%s/%s.%d.state", dir, progname, port);
  return std::string(filename.data());
}

void ReadStateFile(const char* dir, const char* progname, int port)
{
  std::string filename = CreateFileNameFrom(dir, progname, port);
  SecureEraseGuard secure_erase_guard(filename.data());

#if defined HAVE_IS_TRIVIALLY_COPYABLE
  static_assert(std::is_trivially_copyable<StateFileHeader>::value,
                "StateFileHeader must be trivially copyable");
#endif

  struct StateFileHeader hdr{{0}, 0, 0, 0, {0}};

  std::ifstream file;
  file.exceptions(file.exceptions() | std::ios::failbit | std::ios::badbit);

  try {
    file.open(filename, std::ios::binary);
    file.read(reinterpret_cast<char*>(&hdr), sizeof(StateFileHeader));
    if (!CheckHeader(hdr)) { return; }
    if (hdr.last_jobs_addr) {
      Dmsg1(100, "ReadStateFile seek to %d\n", (int)hdr.last_jobs_addr);
      file.seekg(hdr.last_jobs_addr);
    }
  } catch (const std::system_error& e) {
    BErrNo be;
    Dmsg3(100,
          "Could not open and read state file. size=%" PRIuz ": ERR=%s - %s\n",
          sizeof(StateFileHeader), be.bstrerror(), e.code().message().c_str());
    return;
  } catch (const std::exception& e) {
    Dmsg0(100, "Could not open or read file. Some error occurred: %s\n",
          e.what());
  }

  if (!RecentJobResultsList::ImportFromFile(file)) { return; }

  secure_erase_guard.Release();
}

void WriteStateFile(const char* dir, const char* progname, int port)
{
  std::string filename = CreateFileNameFrom(dir, progname, port);

#if defined HAVE_IS_TRIVIALLY_COPYABLE
  static_assert(std::is_trivially_copyable<StateFileHeader>::value,
                "StateFileHeader must be trivially copyable");
#endif

  SecureErase(NULL, filename.c_str());

  SecureEraseGuard erase_on_scope_exit(filename);
  static std::mutex exclusive_write_access_mutex;
  std::lock_guard<std::mutex> m(exclusive_write_access_mutex);

  std::ofstream file;
  file.exceptions(file.exceptions() | std::ios::failbit | std::ios::badbit);

  try {
    file.open(filename, std::ios::binary);
    file.write(reinterpret_cast<char*>(&state_hdr), sizeof(StateFileHeader));

    state_hdr.last_jobs_addr = sizeof(StateFileHeader);

    Dmsg1(100, "write_last_jobs seek to %d\n", (int)state_hdr.last_jobs_addr);
    file.seekp(state_hdr.last_jobs_addr);

    RecentJobResultsList::ExportToFile(file);

    file.seekp(0);
    file.write(reinterpret_cast<char*>(&state_hdr), sizeof(StateFileHeader));
  } catch (const std::system_error& e) {
    BErrNo be;
    Dmsg3(100, "Could not seek filepointer. ERR=%s - %s\n", be.bstrerror(),
          e.code().message().c_str());
    return;
  } catch (const std::exception& e) {
    Dmsg0(100, "Could not seek filepointer. Some error occurred: %s\n",
          e.what());
    return;
  }

  erase_on_scope_exit.Release();
}

/*
 * BAREOS's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD).
 */
#undef fgetc
char* bfgets(char* s, int size, FILE* fd)
{
  char* p = s;
  int ch;
  *p = 0;
  for (int i = 0; i < size - 1; i++) {
    do {
      errno = 0;
      ch = fgetc(fd);
    } while (ch == EOF && ferror(fd) && (errno == EINTR || errno == EAGAIN));
    if (ch == EOF) {
      if (i == 0) {
        return NULL;
      } else {
        return s;
      }
    }
    *p++ = ch;
    *p = 0;
    if (ch == '\r') { /* Support for Mac/Windows file format */
      ch = fgetc(fd);
      if (ch != '\n') {       /* Mac (\r only) */
        (void)ungetc(ch, fd); /* Push next character back to fd */
      }
      p[-1] = '\n';
      break;
    }
    if (ch == '\n') { break; }
  }
  return s;
}

/*
 * BAREOS's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD) and it has a
 *   different calling sequence which implements input lines of
 *   up to a million characters.
 */
char* bfgets(POOLMEM*& s, FILE* fd)
{
  int ch;
  int soft_max;
  int i = 0;

  s[0] = 0;
  soft_max = SizeofPoolMemory(s) - 10;
  for (;;) {
    do {
      errno = 0;
      ch = fgetc(fd);
    } while (ch == EOF && ferror(fd) && (errno == EINTR || errno == EAGAIN));
    if (ch == EOF) {
      if (i == 0) {
        return NULL;
      } else {
        return s;
      }
    }
    if (i > soft_max) {
      /* Insanity check */
      if (soft_max > 1000000) { return s; }
      s = CheckPoolMemorySize(s, soft_max + 10000);
      soft_max = SizeofPoolMemory(s) - 10;
    }
    s[i++] = ch;
    s[i] = 0;
    if (ch == '\r') { /* Support for Mac/Windows file format */
      ch = fgetc(fd);
      if (ch != '\n') {       /* Mac (\r only) */
        (void)ungetc(ch, fd); /* Push next character back to fd */
      }
      s[i - 1] = '\n';
      break;
    }
    if (ch == '\n') { break; }
  }
  return s;
}

/*
 * Make a "unique" filename.  It is important that if
 *   called again with the same "what" that the result
 *   will be identical. This allows us to use the file
 *   without saving its name, and re-generate the name
 *   so that it can be deleted.
 */
void MakeUniqueFilename(POOLMEM*& name, int Id, char* what)
{
  Mmsg(name, "%s/%s.%s.%d.tmp", working_directory, my_name, what, Id);
}

char* escape_filename(const char* file_path)
{
  if (file_path == NULL || strpbrk(file_path, "\"\\") == NULL) { return NULL; }

  char* escaped_path = (char*)malloc(2 * (strlen(file_path) + 1));
  char* cur_char = escaped_path;

  while (*file_path) {
    if (*file_path == '\\' || *file_path == '"') { *cur_char++ = '\\'; }

    *cur_char++ = *file_path++;
  }

  *cur_char = '\0';

  return escaped_path;
}

bool PathExists(const char* path)
{
  struct stat statp;

  if (!path || !strlen(path)) { return false; }

  return (stat(path, &statp) == 0);
}

bool PathExists(PoolMem& path) { return PathExists(path.c_str()); }

bool PathIsDirectory(const char* path)
{
  struct stat statp;

  if (!path || !strlen(path)) { return false; }

  if (stat(path, &statp) == 0) {
    return (S_ISDIR(statp.st_mode));
  } else {
    return false;
  }
}

bool PathIsDirectory(PoolMem& path) { return PathIsDirectory(path.c_str()); }

bool PathIsAbsolute(const char* path)
{
  if (!path || !strlen(path)) {
    // No path: not an absolute path
    return false;
  }

  // Is path absolute?
  if (IsPathSeparator(path[0])) { return true; }

#ifdef HAVE_WIN32
  /* Windows:
   * Does path begin with drive? if yes, it is absolute */
  if (strlen(path) >= 3) {
    if (isalpha(path[0]) && path[1] == ':' && IsPathSeparator(path[2])) {
      return true;
    }
  }
#endif

  return false;
}

bool PathIsAbsolute(PoolMem& path) { return PathIsAbsolute(path.c_str()); }

bool PathContainsDirectory(const char* path)
{
  int i;

  if (!path) { return false; }

  i = strlen(path) - 1;

  while (i >= 0) {
    if (IsPathSeparator(path[i])) { return true; }
    i--;
  }

  return false;
}

bool PathContainsDirectory(PoolMem& path)
{
  return PathContainsDirectory(path.c_str());
}


// Get directory from path.
bool PathGetDirectory(PoolMem& directory, PoolMem& path)
{
  char* dir = NULL;
  int i = path.strlen();

  directory.strcpy(path);
  if (!PathIsDirectory(directory)) {
    dir = directory.addr();
    while ((!IsPathSeparator(dir[i])) && (i > 0)) {
      dir[i] = 0;
      i--;
    }
  }

  if (PathIsDirectory(directory)) {
    // Make sure, path ends with path separator
    PathAppend(directory, "");
    return true;
  }

  return false;
}

bool PathAppend(char* path, const char* extra, unsigned int max_path)
{
  unsigned int path_len;
  unsigned int required_length;

  if (!path || !extra) { return true; }

  path_len = strlen(path);
  required_length = path_len + 1 + strlen(extra);
  if (required_length > max_path) { return false; }

  // Add path separator after original path if missing.
  if (!IsPathSeparator(path[path_len - 1])) {
    path[path_len] = PathSeparator;
    path_len++;
  }

  memcpy(path + path_len, extra, strlen(extra) + 1);

  return true;
}

bool PathAppend(PoolMem& path, const char* extra)
{
  unsigned int required_length;

  if (!extra) { return true; }

  required_length = path.strlen() + 2 + strlen(extra);
  if (!path.check_size(required_length)) { return false; }

  return PathAppend(path.c_str(), extra, required_length);
}

// Append to paths together.
bool PathAppend(PoolMem& path, PoolMem& extra)
{
  return PathAppend(path, extra.c_str());
}

/*
 * based on
 * src/findlib/mkpath.c:bool makedir(...)
 */
static bool PathMkdir(char* path, [[maybe_unused]] mode_t mode)
{
  if (PathExists(path)) {
    Dmsg1(500, "skipped, path %s already exists.\n", path);
    return PathIsDirectory(path);
  }

  if (mkdir(path, mode) != 0) {
    BErrNo be;
    Emsg2(M_ERROR, 0, "Falied to create directory %s: ERR=%s\n", path,
          be.bstrerror());
    return false;
  }

  return true;
}

/*
 * based on
 * src/findlib/mkpath.c:bool makepath(Attributes *attr, const char *apath,
 * mode_t mode, mode_t parent_mode, ...
 */
bool PathCreate(const char* apath, mode_t mode)
{
  char* p;
  int len;
  bool ok = false;
  struct stat statp;
  char* path = NULL;

  if (stat(apath, &statp) == 0) { /* Does dir exist? */
    if (!S_ISDIR(statp.st_mode)) {
      Emsg1(M_ERROR, 0, "%s exists but is not a directory.\n", apath);
      return false;
    }
    return true;
  }

  len = strlen(apath);
  path = (char*)alloca(len + 1);
  bstrncpy(path, apath, len + 1);
  StripTrailingSlashes(path);

#if defined(HAVE_WIN32)
  // Validate drive letter
  if (path[1] == ':') {
    char drive[4] = "X:\\";

    drive[0] = path[0];

    UINT drive_type = GetDriveType(drive);

    if (drive_type == DRIVE_UNKNOWN || drive_type == DRIVE_NO_ROOT_DIR) {
      Emsg1(M_ERROR, 0, "%c: is not a valid drive.\n", path[0]);
      goto bail_out;
    }

    if (path[2] == '\0') { /* attempt to create a drive */
      ok = true;
      goto bail_out; /* OK, it is already there */
    }

    p = &path[3];
  } else {
    p = path;
  }
#else
  p = path;
#endif

  // Skip leading slash(es)
  while (IsPathSeparator(*p)) { p++; }
  while ((p = first_path_separator(p))) {
    char save_p;
    save_p = *p;
    *p = 0;
    if (!PathMkdir(path, mode)) { goto bail_out; }
    *p = save_p;
    while (IsPathSeparator(*p)) { p++; }
  }

  if (!PathMkdir(path, mode)) { goto bail_out; }

  ok = true;

bail_out:
  return ok;
}

bool PathCreate(PoolMem& path, mode_t mode)
{
  return PathCreate(path.c_str(), mode);
}

// Some Solaris specific support needed for Solaris 10 and lower.
#if defined(HAVE_SUN_OS)

/*
 * If libc doesn't have backtrace_symbols emulate it.
 * Solaris 11 has backtrace_symbols in libc older
 * Solaris versions don't have this.
 */
#  ifndef HAVE_BACKTRACE_SYMBOLS
static char** backtrace_symbols(void* const* array, int size)
{
  int bufferlen, len;
  char** ret_buffer;
  char** ret = NULL;
  char linebuffer[512];
  int i;

  bufferlen = size * sizeof(char*);
  ret_buffer = (char**)malloc(bufferlen);
  if (ret_buffer) {
    for (i = 0; i < size; i++) {
      (void)Addrtosymstr(array[i], linebuffer, sizeof(linebuffer));
      ret_buffer[i] = (char*)malloc(len = strlen(linebuffer) + 1);
      strcpy(ret_buffer[i], linebuffer);
      bufferlen += len;
    }
    ret = (char**)malloc(bufferlen);
    if (ret) {
      for (len = i = 0; i < size; i++) {
        ret[i] = (char*)ret + size * sizeof(char*) + len;
        (void)strcpy(ret[i], ret_buffer[i]);
        len += strlen(ret_buffer[i]) + 1;
      }
    }
  }

  return (ret);
}

// Define that we now know backtrace_symbols()
#    define HAVE_BACKTRACE_SYMBOLS 1

#  endif /* HAVE_BACKTRACE_SYMBOLS */

/*
 * If libc doesn't have backtrace call emulate it using getcontext(3)
 * Solaris 11 has backtrace in libc older Solaris versions don't have this.
 */
#  ifndef HAVE_BACKTRACE

#    if __has_include(<ucontext.h>)
#      include <ucontext.h>
#    endif

typedef struct backtrace {
  void** bt_buffer;
  int bt_maxcount;
  int bt_actcount;
} backtrace_t;

static int callback(uintptr_t pc, int signo, void* arg)
{
  backtrace_t* bt = (backtrace_t*)arg;

  if (bt->bt_actcount >= bt->bt_maxcount) return (-1);

  bt->bt_buffer[bt->bt_actcount++] = (void*)pc;

  return (0);
}

static int backtrace(void** buffer, int count)
{
  backtrace_t bt;
  ucontext_t u;

  bt.bt_buffer = buffer;
  bt.bt_maxcount = count;
  bt.bt_actcount = 0;

  if (getcontext(&u) < 0) return (0);

  (void)walkcontext(&u, callback, &bt);

  return (bt.bt_actcount);
}

// Define that we now know backtrace()
#    define HAVE_BACKTRACE 1

#  endif /* HAVE_BACKTRACE */
#endif   /* HAVE_SUN_OS */

// Support strack_trace support on platforms that use GCC as compiler.
#if defined(HAVE_BACKTRACE) && defined(HAVE_BACKTRACE_SYMBOLS) \
    && defined(HAVE_GCC)

#  if __has_include(<cxxabi.h>)
#    include <cxxabi.h>
#  endif

#  if __has_include(<execinfo.h>)
#    include <execinfo.h>
#  endif

void stack_trace()
{
  int status;
  size_t stack_depth, sz, i;
  const size_t max_depth = 100;
  void* stack_addrs[max_depth];
  char **stack_strings, *begin, *end, *j, *function, *ret;

  stack_depth = backtrace(stack_addrs, max_depth);
  stack_strings = backtrace_symbols(stack_addrs, stack_depth);

  for (i = 3; i < stack_depth; i++) {
    sz = 200; /* Just a guess, template names will go much wider */
    function = (char*)malloc(sz);
    begin = end = 0;
    // Find the parentheses and address offset surrounding the mangled name
    for (j = stack_strings[i]; *j; ++j) {
      if (*j == '(') {
        begin = j;
      } else if (*j == '+') {
        end = j;
      }
    }
    if (begin && end) {
      *begin++ = '\0';
      *end = '\0';
      // Found our mangled name, now in [begin, end]
      ret = abi::__cxa_demangle(begin, function, &sz, &status);
      if (ret) {
        // Return value may be a realloc() of the input
        function = ret;
      } else {
        // Demangling failed, just pretend it's a C function with no args
        strncpy(function, begin, sz - 3);
        strcat(function, "()");
        function[sz - 1] = '\0';
      }
      Pmsg2(000, "    %s:%s\n", stack_strings[i], function);

    } else {
      /* didn't find the mangled name, just print the whole line */
      Pmsg1(000, "    %s\n", stack_strings[i]);
    }
    free(function);
  }
  free(stack_strings); /* malloc()ed by backtrace_symbols */
}

// Support strack_trace support on Solaris when using the SUNPRO_CC compiler.
#elif defined(HAVE_SUN_OS) && !defined(HAVE_NON_WORKING_WALKCONTEXT) \
    && __has_include(<ucontext.h>) && __has_include(<demangle.h>)    \
    && defined(__SUNPRO_CC)

#  include <ucontext.h>

#  if has_include(<execinfo.h>)
#    include <execinfo.h>
#  endif

#  include <demangle.h>

void stack_trace()
{
  int ret, i;
  bool demangled_symbol;
  size_t stack_depth;
  size_t sz = 200; /* Just a guess, template names will go much wider */
  const size_t max_depth = 100;
  void* stack_addrs[100];
  char **stack_strings, *begin, *end, *j, *function;

  stack_depth = backtrace(stack_addrs, max_depth);
  stack_strings = backtrace_symbols(stack_addrs, stack_depth);

  for (i = 1; i < stack_depth; i++) {
    function = (char*)malloc(sz);
    begin = end = 0;
    // Find the single quote and address offset surrounding the mangled name
    for (j = stack_strings[i]; *j; ++j) {
      if (*j == '\'') {
        begin = j;
      } else if (*j == '+') {
        end = j;
      }
    }
    if (begin && end) {
      *begin++ = '\0';
      *end = '\0';
      // Found our mangled name, now in [begin, end)
      demangled_symbol = false;
      while (!demangled_symbol) {
        ret = cplus_demangle(begin, function, sz);
        switch (ret) {
          case DEMANGLE_ENAME:
            // Demangling failed, just pretend it's a C function with no args
            strcat(function, "()");
            function[sz - 1] = '\0';
            demangled_symbol = true;
            break;
          case DEMANGLE_ESPACE:
            // Need more space for demangled function name.
            free(function);
            sz = sz * 2;
            function = (char*)malloc(sz);
            continue;
          default:
            demangled_symbol = true;
            break;
        }
      }
      Pmsg2(000, "    %s:%s\n", stack_strings[i], function);
    } else {
      // Didn't find the mangled name, just print the whole line
      Pmsg1(000, "    %s\n", stack_strings[i]);
    }
    free(function);
  }
  free(stack_strings); /* malloc()ed by backtrace_symbols */
}

#else

void stack_trace() {}

#endif
