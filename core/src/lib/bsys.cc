/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer = PTHREAD_COND_INITIALIZER;
static const char *secure_erase_cmdline = NULL;

/*
 * This routine is a somewhat safer unlink in that it
 * allows you to run a regex on the filename before
 * excepting it. It also requires the file to be in
 * the working directory.
 */
int SaferUnlink(const char *pathname, const char *regx)
{
   int rc;
   regex_t preg1;
   char prbuf[500];
   int rtn;

   /*
    * Name must start with working directory
    */
   if (strncmp(pathname, working_directory, strlen(working_directory)) != 0) {
      Pmsg1(000, "Safe_unlink excluded: %s\n", pathname);
      return EROFS;
   }

   /*
    * Compile regex expression
    */
   rc = regcomp(&preg1, regx, REG_EXTENDED);
   if (rc != 0) {
      regerror(rc, &preg1, prbuf, sizeof(prbuf));
      Pmsg2(000,  _("safe_unlink could not compile regex pattern \"%s\" ERR=%s\n"),
           regx, prbuf);
      return ENOENT;
   }

   /*
    * Unlink files that match regexes
    */
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

/*
 * This routine will use an external secure erase program to delete a file.
 */
int SecureErase(JobControlRecord *jcr, const char *pathname)
{
   int retval = -1;

   if (secure_erase_cmdline) {
      int status;
      Bpipe *bpipe;
      PoolMem line(PM_NAME),
               cmdline(PM_MESSAGE);

      Mmsg(cmdline,"%s \"%s\"", secure_erase_cmdline, pathname);
      if (jcr) {
         Jmsg(jcr, M_INFO, 0, _("SecureErase: executing %s\n"), cmdline.c_str());
      }

      bpipe = open_bpipe(cmdline.c_str(), 0, "r");
      if (bpipe == NULL) {
         berrno be;

         if (jcr) {
            Jmsg(jcr, M_FATAL, 0, _("SecureErase: %s could not execute. ERR=%s\n"),
                 secure_erase_cmdline, be.bstrerror());
         }
         goto bail_out;
      }

      while (fgets(line.c_str(), line.size(), bpipe->rfd)) {
         StripTrailingJunk(line.c_str());
         if (jcr) {
            Jmsg(jcr, M_INFO, 0, _("SecureErase: %s\n"), line.c_str());
         }
      }

      status = CloseBpipe(bpipe);
      if (status != 0) {
         berrno be;

         if (jcr) {
            Jmsg(jcr, M_FATAL, 0, _("SecureErase: %s returned non-zero status=%d. ERR=%s\n"),
                 secure_erase_cmdline, be.code(status), be.bstrerror(status));
         }
         goto bail_out;
      }

      Dmsg0(100, "wpipe_command OK\n");
      retval = 0;
   } else {
      retval = unlink(pathname);
   }

   return retval;

bail_out:
   errno = EROFS;
   return retval;
}

void SetSecureEraseCmdline(const char *cmdline)
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
   struct timezone tz;
   int status;

   timeout.tv_sec = sec;
   timeout.tv_nsec = usec * 1000;

#ifdef HAVE_NANOSLEEP
   status = nanosleep(&timeout, NULL);
   if (!(status < 0 && errno == ENOSYS)) {
      return status;
   }
   /*
    * If we reach here it is because nanosleep is not supported by the OS
    */
#endif

   /*
    * Do it the old way
    */
   gettimeofday(&tv, &tz);
   timeout.tv_nsec += tv.tv_usec * 1000;
   timeout.tv_sec += tv.tv_sec;
   while (timeout.tv_nsec >= 1000000000) {
      timeout.tv_nsec -= 1000000000;
      timeout.tv_sec++;
   }

   Dmsg2(200, "pthread_cond_timedwait sec=%lld usec=%d\n", sec, usec);

   /*
    * Note, this unlocks mutex during the sleep
    */
   P(timer_mutex);
   status = pthread_cond_timedwait(&timer, &timer_mutex, &timeout);
   V(timer_mutex);

   return status;
}

/*
 * Copy a string inline from one point in the same string
 * to another.
 */
char *bstrinlinecpy(char *dest, const char *src)
{
   int len;

   /*
    * Sanity check. We can only inline copy if the src > dest
    * otherwise the resulting copy will overwrite the end of
    * the string.
    */
   if (src <= dest) {
      return NULL;
   }

   len = strlen(src);
#if HAVE_BCOPY
   /*
    * Cannot use strcpy or memcpy as those functions are not
    * allowed on overlapping data and this is inline replacement
    * for sure is. So we use bcopy which is allowed on overlapping
    * data.
    */
   bcopy(src, dest, len + 1);
#else
   /*
    * Cannot use strcpy or memcpy as those functions are not
    * allowed on overlapping data and this is inline replacement
    * for sure is. So we use memmove which is allowed on
    * overlapping data.
    */
   memmove(dest, src, len + 1);
#endif
   return dest;
}

/*
 * Guarantee that the string is properly terminated
 */
char *bstrncpy(char *dest, const char *src, int maxlen)
{
   strncpy(dest, src, maxlen - 1);
   dest[maxlen - 1] = 0;
   return dest;
}

/*
 * Guarantee that the string is properly terminated
 */
char *bstrncpy(char *dest, PoolMem &src, int maxlen)
{
   strncpy(dest, src.c_str(), maxlen - 1);
   dest[maxlen - 1] = 0;
   return dest;
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char *bstrncat(char *dest, const char *src, int maxlen)
{
   int len = strlen(dest);
   if (len < maxlen - 1) {
      strncpy(dest + len, src, maxlen - len - 1);
   }
   dest[maxlen - 1] = 0;
   return dest;
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char *bstrncat(char *dest, PoolMem &src, int maxlen)
{
   int len = strlen(dest);
   if (len < maxlen - 1) {
      strncpy(dest + len, src.c_str(), maxlen - (len + 1));
   }
   dest[maxlen - 1] = 0;
   return dest;
}

/*
 * Allows one or both pointers to be NULL
 */
bool bstrcmp(const char *s1, const char *s2)
{
   if (s1 == s2) return true;
   if (s1 == NULL || s2 == NULL) return false;
   return strcmp(s1, s2) == 0;
}

bool bstrncmp(const char *s1, const char *s2, int n)
{
   if (s1 == s2) return true;
   if (s1 == NULL || s2 == NULL) return false;
   return strncmp(s1, s2, n) == 0;
}

bool Bstrcasecmp(const char *s1, const char *s2)
{
   if (s1 == s2) return true;
   if (s1 == NULL || s2 == NULL) return false;
   return strcasecmp(s1, s2) == 0;
}

bool bstrncasecmp(const char *s1, const char *s2, int n)
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
 * U-04000000 - U-7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 */
int cstrlen(const char *str)
{
   uint8_t *p = (uint8_t *)str;
   int len = 0;
   if (str == NULL) {
      return 0;
   }
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
      p++;                      /* Shouln't get here but must advance */
   }
   return len;
}

#ifndef bmalloc
void *bmalloc(size_t size)
{
  void *buf;

#ifdef SMARTALLOC
  buf = sm_malloc(file, line, size);
#else
  buf = malloc(size);
#endif
  if (buf == NULL) {
     berrno be;
     Emsg1(M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
  }
  return buf;
}
#endif

void *b_malloc(const char *file, int line, size_t size)
{
  void *buf;

#ifdef SMARTALLOC
  buf = sm_malloc(file, line, size);
#else
  buf = malloc(size);
#endif
  if (buf == NULL) {
     berrno be;
     e_msg(file, line, M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
  }
  return buf;
}


void bfree(void *buf)
{
#ifdef SMARTALLOC
  sm_free(__FILE__, __LINE__, buf);
#else
  free(buf);
#endif
}

void *brealloc (void *buf, size_t size)
{
#ifdef SMARTALOC
   buf = sm_realloc(__FILE__, __LINE__, buf, size);
#else
   buf = realloc(buf, size);
#endif
   if (buf == NULL) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
   }
   return buf;
}

void *bcalloc(size_t size1, size_t size2)
{
  void *buf;

   buf = calloc(size1, size2);
   if (buf == NULL) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
   }
   return buf;
}

/* Code now in src/lib/bsnprintf.c */
#ifndef USE_BSNPRINTF

#define BIG_BUF 5000
/*
 * Implement snprintf
 */
int Bsnprintf(char *str, int32_t size, const char *fmt,  ...)
{
   va_list   arg_ptr;
   int len;

   va_start(arg_ptr, fmt);
   len = Bvsnprintf(str, size, fmt, arg_ptr);
   va_end(arg_ptr);
   return len;
}

/*
 * Implement vsnprintf()
 */
int Bvsnprintf(char *str, int32_t size, const char  *format, va_list ap)
{
#ifdef HAVE_VSNPRINTF
   int len;
   len = vsnprintf(str, size, format, ap);
   str[size - 1] = 0;
   return len;

#else

   int len, buflen;
   char *buf;
   buflen = size > BIG_BUF ? size : BIG_BUF;
   buf = GetMemory(buflen);
   len = vsprintf(buf, format, ap);
   if (len >= buflen) {
      Emsg0(M_ABORT, 0, _("Buffer overflow.\n"));
   }
   memcpy(str, buf, len);
   str[len] = 0;                /* len excludes the null */
   FreeMemory(buf);
   return len;
#endif
}
#endif /* USE_BSNPRINTF */

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timep, struct tm *tm)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    struct tm *ltm,

    P(mutex);
    ltm = localtime(timep);
    if (ltm) {
       memcpy(tm, ltm, sizeof(struct tm));
    }
    V(mutex);
    return ltm ? tm : NULL;
}
#endif /* HAVE_LOCALTIME_R */

#ifndef HAVE_READDIR_R
#ifndef HAVE_WIN32
#include <dirent.h>

int Readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    struct dirent *ndir;
    int status;

    P(mutex);
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
    V(mutex);
    return status;

}
#endif
#endif /* HAVE_READDIR_R */

int b_strerror(int errnum, char *buf, size_t bufsiz)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int status = 0;
    const char *msg;

    P(mutex);

    msg = strerror(errnum);
    if (!msg) {
       msg = _("Bad errno");
       status = -1;
    }
    bstrncpy(buf, msg, bufsiz);
    V(mutex);
    return status;
}

#ifdef DEBUG_MEMSET
/* These routines are not normally turned on */
#undef memset
void b_memset(const char *file, int line, void *mem, int val, size_t num)
{
   /* Testing for 2000 byte zero at beginning of Volume block */
   if (num > 1900 && num < 3000) {
      Pmsg3(000, _("Memset for %d bytes at %s:%d\n"), (int)num, file, line);
   }
   memset(mem, val, num);
}
#endif

#if !defined(HAVE_WIN32)
static bool del_pid_file_ok = false;
#endif

/*
 * Create a standard "Unix" pid file.
 */
void CreatePidFile(char *dir, const char *progname, int port)
{
#if !defined(HAVE_WIN32)
   int pidfd = -1;
   int len;
   int oldpid;
   char  pidbuf[20];
   POOLMEM *fname = GetPoolMemory(PM_FNAME);
   struct stat statp;

   Mmsg(fname, "%s/%s.%d.pid", dir, progname, port);
   if (stat(fname, &statp) == 0) {
      /* File exists, see what we have */
      *pidbuf = 0;
      if ((pidfd = open(fname, O_RDONLY|O_BINARY, 0)) < 0 ||
           read(pidfd, &pidbuf, sizeof(pidbuf)) < 0 ||
           sscanf(pidbuf, "%d", &oldpid) != 1) {
         berrno be;
         Emsg2(M_ERROR_TERM, 0, _("Cannot open pid file. %s ERR=%s\n"), fname,
               be.bstrerror());
      } else {
         /*
          * Some OSes (IRIX) don't bother to clean out the old pid files after a crash, and
          * since they use a deterministic algorithm for assigning PIDs, we can have
          * pid conflicts with the old PID file after a reboot.
          * The intent the following code is to check if the oldpid read from the pid
          * file is the same as the currently executing process's pid,
          * and if oldpid == getpid(), skip the attempt to
          * kill(oldpid,0), since the attempt is guaranteed to succeed,
          * but the success won't actually mean that there is an
          * another BAREOS process already running.
          * For more details see bug #797.
          */
          if ((oldpid != (int)getpid()) && (kill(oldpid, 0) != -1 || errno != ESRCH)) {
            Emsg3(M_ERROR_TERM, 0, _("%s is already running. pid=%d\nCheck file %s\n"),
                  progname, oldpid, fname);
         }
      }

      if (pidfd >= 0) {
         close(pidfd);
      }

      /*
       * He is not alive, so take over file ownership
       */
      unlink(fname);                  /* remove stale pid file */
   }

   /*
    * Create new pid file
    */
   if ((pidfd = open(fname, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0640)) >= 0) {
      len = sprintf(pidbuf, "%d\n", (int)getpid());
      write(pidfd, pidbuf, len);
      close(pidfd);
      del_pid_file_ok = true;         /* we created it so we can delete it */
   } else {
      berrno be;
      Emsg2(M_ERROR_TERM, 0, _("Could not open pid file. %s ERR=%s\n"), fname,
            be.bstrerror());
   }
   FreePoolMemory(fname);
#endif
}

/*
 * Delete the pid file if we created it
 */
int DeletePidFile(char *dir, const char *progname, int port)
{
#if !defined(HAVE_WIN32)
   POOLMEM *fname = GetPoolMemory(PM_FNAME);

   if (!del_pid_file_ok) {
      FreePoolMemory(fname);
      return 0;
   }
   del_pid_file_ok = false;
   Mmsg(fname, "%s/%s.%d.pid", dir, progname, port);
   unlink(fname);
   FreePoolMemory(fname);
#endif
   return 1;
}

struct s_state_hdr {
   char id[14];
   int32_t version;
   uint64_t last_jobs_addr;
   uint64_t reserved[20];
};

static struct s_state_hdr state_hdr = {
   "Bareos State\n",
   4,
   0
};

/*
 * Open and read the state file for the daemon
 */
void ReadStateFile(char *dir, const char *progname, int port)
{
   int sfd;
   ssize_t status;
   bool ok = false;
   POOLMEM *fname = GetPoolMemory(PM_FNAME);
   struct s_state_hdr hdr;
   int hdr_size = sizeof(hdr);

   Mmsg(fname, "%s/%s.%d.state", dir, progname, port);
   /*
    * If file exists, see what we have
    */
   if ((sfd = open(fname, O_RDONLY|O_BINARY)) < 0) {
      berrno be;
      Dmsg3(010, "Could not open state file. sfd=%d size=%d: ERR=%s\n",
            sfd, sizeof(hdr), be.bstrerror());
      goto bail_out;
   }
   if ((status = read(sfd, &hdr, hdr_size)) != hdr_size) {
      berrno be;
      Dmsg4(010, "Could not read state file. sfd=%d status=%d size=%d: ERR=%s\n",
            sfd, (int)status, hdr_size, be.bstrerror());
      goto bail_out;
   }
   if (hdr.version != state_hdr.version) {
      Dmsg2(010, "Bad hdr version. Wanted %d got %d\n",
            state_hdr.version, hdr.version);
      goto bail_out;
   }
   hdr.id[13] = 0;
   if (!bstrcmp(hdr.id, state_hdr.id)) {
      Dmsg0(000, "State file header id invalid.\n");
      goto bail_out;
   }

   if (!ReadLastJobsList(sfd, hdr.last_jobs_addr)) {
      goto bail_out;
   }
   ok = true;
bail_out:
   if (sfd >= 0) {
      close(sfd);
   }

   if (!ok) {
      SecureErase(NULL, fname);
   }

   FreePoolMemory(fname);
}

/*
 * Write the state file
 */
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

void WriteStateFile(char *dir, const char *progname, int port)
{
   int sfd;
   bool ok = false;
   POOLMEM *fname = GetPoolMemory(PM_FNAME);

   P(state_mutex);                    /* Only one job at a time can call here */
   Mmsg(fname, "%s/%s.%d.state", dir, progname, port);

   /*
    * Create new state file
    */
   SecureErase(NULL, fname);
   if ((sfd = open(fname, O_CREAT|O_WRONLY|O_BINARY, 0640)) < 0) {
      berrno be;
      Emsg2(M_ERROR, 0, _("Could not create state file. %s ERR=%s\n"), fname, be.bstrerror());
      goto bail_out;
   }

   if (write(sfd, &state_hdr, sizeof(state_hdr)) != sizeof(state_hdr)) {
      berrno be;
      Dmsg1(000, "Write hdr error: ERR=%s\n", be.bstrerror());
      goto bail_out;
   }

   state_hdr.last_jobs_addr = sizeof(state_hdr);
   state_hdr.reserved[0] = WriteLastJobsList(sfd, state_hdr.last_jobs_addr);
   if (lseek(sfd, 0, SEEK_SET) < 0) {
      berrno be;
      Dmsg1(000, "lseek error: ERR=%s\n", be.bstrerror());
      goto bail_out;
   }

   if (write(sfd, &state_hdr, sizeof(state_hdr)) != sizeof(state_hdr)) {
      berrno be;
      Pmsg1(000, _("Write final hdr error: ERR=%s\n"), be.bstrerror());
      goto bail_out;
   }
   ok = true;
bail_out:
   if (sfd >= 0) {
      close(sfd);
   }

   if (!ok) {
      SecureErase(NULL, fname);
   }
   V(state_mutex);
   FreePoolMemory(fname);
}

/* BSDI does not have this.  This is a *poor* simulation */
#ifndef HAVE_STRTOLL
long long int
strtoll(const char *ptr, char **endptr, int base)
{
   return (long long int)strtod(ptr, endptr);
}
#endif

/*
 * BAREOS's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD).
 */
#undef fgetc
char *bfgets(char *s, int size, FILE *fd)
{
   char *p = s;
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
         if (ch != '\n') { /* Mac (\r only) */
            (void)ungetc(ch, fd); /* Push next character back to fd */
         }
         p[-1] = '\n';
         break;
      }
      if (ch == '\n') {
         break;
      }
   }
   return s;
}

/*
 * BAREOS's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD) and it has a
 *   different calling sequence which implements input lines of
 *   up to a million characters.
 */
char *bfgets(POOLMEM *&s, FILE *fd)
{
   int ch;
   int soft_max;
   int i = 0;

   s[0] = 0;
   soft_max = SizeofPoolMemory(s) - 10;
   for ( ;; ) {
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
         if (soft_max > 1000000) {
            return s;
         }
         s = CheckPoolMemorySize(s, soft_max+10000);
         soft_max = SizeofPoolMemory(s) - 10;
      }
      s[i++] = ch;
      s[i] = 0;
      if (ch == '\r') { /* Support for Mac/Windows file format */
         ch = fgetc(fd);
         if (ch != '\n') { /* Mac (\r only) */
            (void)ungetc(ch, fd); /* Push next character back to fd */
         }
         s[i - 1] = '\n';
         break;
      }
      if (ch == '\n') {
         break;
      }
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
void MakeUniqueFilename(POOLMEM *&name, int Id, char *what)
{
   Mmsg(name, "%s/%s.%s.%d.tmp", working_directory, my_name, what, Id);
}

char *escape_filename(const char *file_path)
{
   if (file_path == NULL || strpbrk(file_path, "\"\\") == NULL) {
      return NULL;
   }

   char *escaped_path = (char *)bmalloc(2 * (strlen(file_path) + 1));
   char *cur_char = escaped_path;

   while (*file_path) {
      if (*file_path == '\\' || *file_path == '"') {
         *cur_char++ = '\\';
      }

      *cur_char++ = *file_path++;
   }

   *cur_char = '\0';

   return escaped_path;
}

bool PathExists(const char *path)
{
   struct stat statp;

   if (!path || !strlen(path)) {
      return false;
   }

   return (stat(path, &statp) == 0);
}

bool PathExists(PoolMem &path)
{
   return PathExists(path.c_str());
}

bool PathIsDirectory(const char *path)
{
   struct stat statp;

   if (!path || !strlen(path)) {
      return false;
   }

   if (stat(path, &statp) == 0) {
      return (S_ISDIR(statp.st_mode));
   } else {
      return false;
   }
}

bool PathIsDirectory(PoolMem &path)
{
   return PathIsDirectory(path.c_str());
}

bool PathIsAbsolute(const char *path)
{
   if (!path || !strlen(path)) {
      /*
       * No path: not an absolute path
       */
      return false;
   }

   /*
    * Is path absolute?
    */
   if (IsPathSeparator(path[0])) {
      return true;
   }

#ifdef HAVE_WIN32
   /*
    * Windows:
    * Does path begin with drive? if yes, it is absolute
    */
   if (strlen(path) >= 3) {
      if (isalpha(path[0]) && path[1] == ':' && IsPathSeparator(path[2])) {
         return true;
      }
   }
#endif

   return false;
}

bool PathIsAbsolute(PoolMem &path)
{
   return PathIsAbsolute(path.c_str());
}

bool PathContainsDirectory(const char *path)
{
   int i;

   if (!path) {
      return false;
   }

   i = strlen(path) - 1;

   while (i >= 0) {
      if (IsPathSeparator(path[i])) {
         return true;
      }
      i--;
   }

   return false;
}

bool PathContainsDirectory(PoolMem &path)
{
   return PathContainsDirectory(path.c_str());
}


/*
 * Get directory from path.
 */
bool PathGetDirectory(PoolMem &directory, PoolMem &path)
{
   char *dir = NULL;
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
      /*
       * Make sure, path ends with path separator
       */
      PathAppend(directory, "");
      return true;
   }

   return false;
}

bool PathAppend(char *path, const char *extra, unsigned int max_path)
{
   unsigned int path_len;
   unsigned int required_length;

   if (!path || !extra) {
      return true;
   }

   path_len = strlen(path);
   required_length = path_len + 1 + strlen(extra);
   if (required_length > max_path) {
      return false;
   }

   /*
    * Add path separator after original path if missing.
    */
   if (!IsPathSeparator(path[path_len - 1])) {
      path[path_len] = PathSeparator;
      path_len++;
   }

   memcpy(path + path_len, extra, strlen(extra) + 1);

   return true;
}

bool PathAppend(PoolMem &path, const char *extra)
{
   unsigned int required_length;

   if (!extra) {
      return true;
   }

   required_length = path.strlen() + 1 + strlen(extra);
   if (!path.check_size(required_length)) {
      return false;
   }

   return PathAppend(path.c_str(), extra, required_length);
}

/*
 * Append to paths together.
 */
bool PathAppend(PoolMem &path, PoolMem &extra)
{
   return PathAppend(path, extra.c_str());
}

/*
 * based on
 * src/findlib/mkpath.c:bool makedir(...)
 */
static bool PathMkdir(char *path, mode_t mode)
{
   if (PathExists(path)) {
      Dmsg1(500, "skipped, path %s already exists.\n", path);
      return PathIsDirectory(path);
   }

   if (mkdir(path, mode) != 0) {
      berrno be;
      Emsg2(M_ERROR, 0, "Falied to create directory %s: ERR=%s\n",
              path, be.bstrerror());
      return false;
   }

   return true;
}

/*
 * based on
 * src/findlib/mkpath.c:bool makepath(Attributes *attr, const char *apath, mode_t mode, mode_t parent_mode, ...
 */
bool PathCreate(const char *apath, mode_t mode)
{
   char *p;
   int len;
   bool ok = false;
   struct stat statp;
   char *path = NULL;

   if (stat(apath, &statp) == 0) {     /* Does dir exist? */
      if (!S_ISDIR(statp.st_mode)) {
         Emsg1(M_ERROR, 0, "%s exists but is not a directory.\n", path);
         return false;
      }
      return true;
   }

   len = strlen(apath);
   path = (char *)alloca(len + 1);
   bstrncpy(path, apath, len + 1);
   StripTrailingSlashes(path);

#if defined(HAVE_WIN32)
   /*
    * Validate drive letter
    */
   if (path[1] == ':') {
      char drive[4] = "X:\\";

      drive[0] = path[0];

      UINT drive_type = GetDriveType(drive);

      if (drive_type == DRIVE_UNKNOWN || drive_type == DRIVE_NO_ROOT_DIR) {
         Emsg1(M_ERROR, 0, "%c: is not a valid drive.\n", path[0]);
         goto bail_out;
      }

      if (path[2] == '\0') {          /* attempt to create a drive */
         ok = true;
         goto bail_out;               /* OK, it is already there */
      }

      p = &path[3];
   } else {
      p = path;
   }
#else
   p = path;
#endif

   /*
    * Skip leading slash(es)
    */
   while (IsPathSeparator(*p)) {
      p++;
   }
   while ((p = first_path_separator(p))) {
      char save_p;
      save_p = *p;
      *p = 0;
      if (!PathMkdir(path, mode)) {
         goto bail_out;
      }
      *p = save_p;
      while (IsPathSeparator(*p)) {
         p++;
      }
   }

   if (!PathMkdir(path, mode)) {
      goto bail_out;
   }

   ok = true;

bail_out:
   return ok;
}

bool PathCreate(PoolMem &path, mode_t mode)
{
   return PathCreate(path.c_str(), mode);
}

/*
 * Some Solaris specific support needed for Solaris 10 and lower.
 */
#if defined(HAVE_SUN_OS)

/*
 * If libc doesn't have Addrtosymstr emulate it.
 * Solaris 11 has Addrtosymstr in libc older
 * Solaris versions don't have this.
 */
#ifndef HAVE_ADDRTOSYMSTR

#include <dlfcn.h>

#ifdef _LP64
#define _ELF64
#endif
#include <sys/machelf.h>

static int Addrtosymstr(void *pc, char *buffer, int size)
{
   Dl_info info;
   Sym *sym;

   if (dladdr1(pc, &info, (void **)&sym, RTLD_DL_SYMENT) == 0) {
      return (Bsnprintf(buffer, size, "[0x%p]", pc));
   }

   if ((info.dli_fname != NULL && info.dli_sname != NULL) &&
      ((uintptr_t)pc - (uintptr_t)info.dli_saddr < sym->st_size)) {
      return (Bsnprintf(buffer, size, "%s'%s+0x%x [0x%p]",
                        info.dli_fname,
                        info.dli_sname,
                        (unsigned long)pc - (unsigned long)info.dli_saddr,
                        pc));
   } else {
      return (Bsnprintf(buffer, size, "%s'0x%p [0x%p]",
                        info.dli_fname,
                        (unsigned long)pc - (unsigned long)info.dli_fbase,
                        pc));
   }
}
#endif /* HAVE_ADDRTOSYMSTR */

/*
 * If libc doesn't have backtrace_symbols emulate it.
 * Solaris 11 has backtrace_symbols in libc older
 * Solaris versions don't have this.
 */
#ifndef HAVE_BACKTRACE_SYMBOLS
static char **backtrace_symbols(void *const *array, int size)
{
   int bufferlen, len;
   char **ret_buffer;
   char **ret = NULL;
   char linebuffer[512];
   int i;

   bufferlen = size * sizeof(char *);
   ret_buffer = (char **)actuallymalloc(bufferlen);
   if (ret_buffer) {
      for (i = 0; i < size; i++) {
         (void) Addrtosymstr(array[i], linebuffer, sizeof(linebuffer));
         ret_buffer[i] = (char *)actuallymalloc(len = strlen(linebuffer) + 1);
         strcpy(ret_buffer[i], linebuffer);
         bufferlen += len;
      }
      ret = (char **)actuallymalloc(bufferlen);
      if (ret) {
         for (len = i = 0; i < size; i++) {
            ret[i] = (char *)ret + size * sizeof(char *) + len;
            (void) strcpy(ret[i], ret_buffer[i]);
            len += strlen(ret_buffer[i]) + 1;
         }
      }
   }

   return (ret);
}

/*
 * Define that we now know backtrace_symbols()
 */
#define HAVE_BACKTRACE_SYMBOLS 1

#endif /* HAVE_BACKTRACE_SYMBOLS */

/*
 * If libc doesn't have backtrace call emulate it using getcontext(3)
 * Solaris 11 has backtrace in libc older Solaris versions don't have this.
 */
#ifndef HAVE_BACKTRACE

#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

typedef struct backtrace {
   void **bt_buffer;
   int bt_maxcount;
   int bt_actcount;
} backtrace_t;

static int callback(uintptr_t pc, int signo, void *arg)
{
   backtrace_t *bt = (backtrace_t *)arg;

   if (bt->bt_actcount >= bt->bt_maxcount)
      return (-1);

   bt->bt_buffer[bt->bt_actcount++] = (void *)pc;

   return (0);
}

static int backtrace(void **buffer, int count)
{
   backtrace_t bt;
   ucontext_t u;

   bt.bt_buffer = buffer;
   bt.bt_maxcount = count;
   bt.bt_actcount = 0;

   if (getcontext(&u) < 0)
      return (0);

   (void) walkcontext(&u, callback, &bt);

   return (bt.bt_actcount);
}

/*
 * Define that we now know backtrace()
 */
#define HAVE_BACKTRACE 1

#endif /* HAVE_BACKTRACE */
#endif /* HAVE_SUN_OS */

/*
 * Support strack_trace support on platforms that use GCC as compiler.
 */
#if defined(HAVE_BACKTRACE) && \
    defined(HAVE_BACKTRACE_SYMBOLS) && \
    defined(HAVE_GCC)

#ifdef HAVE_CXXABI_H
#include <cxxabi.h>
#endif

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

void stack_trace()
{
   int status;
   size_t stack_depth, sz, i;
   const size_t max_depth = 100;
   void *stack_addrs[max_depth];
   char **stack_strings, *begin, *end, *j, *function, *ret;

   stack_depth = backtrace(stack_addrs, max_depth);
   stack_strings = backtrace_symbols(stack_addrs, stack_depth);

   for (i = 3; i < stack_depth; i++) {
      sz = 200; /* Just a guess, template names will go much wider */
      function = (char *)actuallymalloc(sz);
      begin = end = 0;
      /*
       * Find the parentheses and address offset surrounding the mangled name
       */
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
         /*
          * Found our mangled name, now in [begin, end]
          */
         ret = abi::__cxa_demangle(begin, function, &sz, &status);
         if (ret) {
            /*
             * Return value may be a realloc() of the input
             */
            function = ret;
         } else {
            /*
             * Demangling failed, just pretend it's a C function with no args
             */
            strncpy(function, begin, sz - 3);
            strcat(function, "()");
            function[sz - 1] = '\0';
         }
         Pmsg2(000, "    %s:%s\n", stack_strings[i], function);

      } else {
         /* didn't find the mangled name, just print the whole line */
         Pmsg1(000, "    %s\n", stack_strings[i]);
      }
      Actuallyfree(function);
   }
   Actuallyfree(stack_strings); /* malloc()ed by backtrace_symbols */
}

/*
 * Support strack_trace support on Solaris when using the SUNPRO_CC compiler.
 */
#elif defined(HAVE_SUN_OS) && \
     !defined(HAVE_NON_WORKING_WALKCONTEXT) && \
      defined(HAVE_UCONTEXT_H) && \
      defined(HAVE_DEMANGLE_H) && \
      defined(HAVE_CPLUS_DEMANGLE) && \
      defined(__SUNPRO_CC)

#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

#if defined(HAVE_EXECINFO_H)
#include <execinfo.h>
#endif

#include <demangle.h>

void stack_trace()
{
   int ret, i;
   bool demangled_symbol;
   size_t stack_depth;
   size_t sz = 200; /* Just a guess, template names will go much wider */
   const size_t max_depth = 100;
   void *stack_addrs[100];
   char **stack_strings, *begin, *end, *j, *function;

   stack_depth = backtrace(stack_addrs, max_depth);
   stack_strings = backtrace_symbols(stack_addrs, stack_depth);

   for (i = 1; i < stack_depth; i++) {
      function = (char *)actuallymalloc(sz);
      begin = end = 0;
      /*
       * Find the single quote and address offset surrounding the mangled name
       */
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
         /*
          * Found our mangled name, now in [begin, end)
          */
         demangled_symbol = false;
         while (!demangled_symbol) {
            ret = cplus_demangle(begin, function, sz);
            switch (ret) {
            case DEMANGLE_ENAME:
               /*
                * Demangling failed, just pretend it's a C function with no args
                */
               strcat(function, "()");
               function[sz - 1] = '\0';
               demangled_symbol = true;
               break;
            case DEMANGLE_ESPACE:
               /*
                * Need more space for demangled function name.
                */
               Actuallyfree(function);
               sz = sz * 2;
               function = (char *)actuallymalloc(sz);
               continue;
            default:
               demangled_symbol = true;
               break;
            }
         }
         Pmsg2(000, "    %s:%s\n", stack_strings[i], function);
      } else {
         /*
          * Didn't find the mangled name, just print the whole line
          */
         Pmsg1(000, "    %s\n", stack_strings[i]);
      }
      Actuallyfree(function);
   }
   Actuallyfree(stack_strings); /* malloc()ed by backtrace_symbols */
}

#else

void stack_trace()
{
}

#endif
