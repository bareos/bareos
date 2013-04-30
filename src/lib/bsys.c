/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Miscellaneous Bacula memory and thread safe routines
 *   Generally, these are interfaces to system or standard
 *   library routines.
 *
 *  Bacula utility functions are in util.c
 *
 */

#include "bacula.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer = PTHREAD_COND_INITIALIZER;

/*
 * This routine is a somewhat safer unlink in that it
 *   allows you to run a regex on the filename before 
 *   excepting it. It also requires the file to be in 
 *   the working directory.
 */
int safer_unlink(const char *pathname, const char *regx)
{
   int rc;
   regex_t preg1;
   char prbuf[500];
   const int nmatch = 30;
   regmatch_t pmatch[nmatch];
   int rtn;

   /* Name must start with working directory */
   if (strncmp(pathname, working_directory, strlen(working_directory)) != 0) {
      Pmsg1(000, "Safe_unlink excluded: %s\n", pathname);
      return EROFS;
   }

   /* Compile regex expression */
   rc = regcomp(&preg1, regx, REG_EXTENDED);
   if (rc != 0) {
      regerror(rc, &preg1, prbuf, sizeof(prbuf));
      Pmsg2(000,  _("safe_unlink could not compile regex pattern \"%s\" ERR=%s\n"),
           regx, prbuf);
      return ENOENT;
   }

   /* Unlink files that match regexes */
   if (regexec(&preg1, pathname, nmatch, pmatch,  0) == 0) {
      Dmsg1(100, "safe_unlink unlinking: %s\n", pathname);
      rtn = unlink(pathname);
   } else {
      Pmsg2(000, "safe_unlink regex failed: regex=%s file=%s\n", regx, pathname);
      rtn = EROFS;
   }
   regfree(&preg1);
   return rtn;
}

/*
 * This routine will sleep (sec, microsec).  Note, however, that if a
 *   signal occurs, it will return early.  It is up to the caller
 *   to recall this routine if he/she REALLY wants to sleep the
 *   requested time.
 */
int bmicrosleep(int32_t sec, int32_t usec)
{
   struct timespec timeout;
   struct timeval tv;
   struct timezone tz;
   int stat;

   timeout.tv_sec = sec;
   timeout.tv_nsec = usec * 1000;

#ifdef HAVE_NANOSLEEP
   stat = nanosleep(&timeout, NULL);
   if (!(stat < 0 && errno == ENOSYS)) {
      return stat;
   }
   /* If we reach here it is because nanosleep is not supported by the OS */
#endif

   /* Do it the old way */
   gettimeofday(&tv, &tz);
   timeout.tv_nsec += tv.tv_usec * 1000;
   timeout.tv_sec += tv.tv_sec;
   while (timeout.tv_nsec >= 1000000000) {
      timeout.tv_nsec -= 1000000000;
      timeout.tv_sec++;
   }

   Dmsg2(200, "pthread_cond_timedwait sec=%lld usec=%d\n", sec, usec);
   /* Note, this unlocks mutex during the sleep */
   P(timer_mutex);
   stat = pthread_cond_timedwait(&timer, &timer_mutex, &timeout);
   if (stat != 0) {
      berrno be;
      Dmsg2(200, "pthread_cond_timedwait stat=%d ERR=%s\n", stat,
         be.bstrerror(stat));
   }
   V(timer_mutex);
   return stat;
}

/*
 * Guarantee that the string is properly terminated */
char *bstrncpy(char *dest, const char *src, int maxlen)
{
   strncpy(dest, src, maxlen-1);
   dest[maxlen-1] = 0;
   return dest;
}

/*
 * Guarantee that the string is properly terminated */
char *bstrncpy(char *dest, POOL_MEM &src, int maxlen)
{
   strncpy(dest, src.c_str(), maxlen-1);
   dest[maxlen-1] = 0;
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
   if (len < maxlen-1) {
      strncpy(dest+len, src, maxlen-len-1);
   }
   dest[maxlen-1] = 0;
   return dest;
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char *bstrncat(char *dest, POOL_MEM &src, int maxlen)
{
   int len = strlen(dest);
   if (len < maxlen-1) {
      strncpy(dest+len, src.c_str(), maxlen-len-1);
   }
   dest[maxlen-1] = 0;
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
int bsnprintf(char *str, int32_t size, const char *fmt,  ...)
{
   va_list   arg_ptr;
   int len;

   va_start(arg_ptr, fmt);
   len = bvsnprintf(str, size, fmt, arg_ptr);
   va_end(arg_ptr);
   return len;
}

/*
 * Implement vsnprintf()
 */
int bvsnprintf(char *str, int32_t size, const char  *format, va_list ap)
{
#ifdef HAVE_VSNPRINTF
   int len;
   len = vsnprintf(str, size, format, ap);
   str[size-1] = 0;
   return len;

#else

   int len, buflen;
   char *buf;
   buflen = size > BIG_BUF ? size : BIG_BUF;
   buf = get_memory(buflen);
   len = vsprintf(buf, format, ap);
   if (len >= buflen) {
      Emsg0(M_ABORT, 0, _("Buffer overflow.\n"));
   }
   memcpy(str, buf, len);
   str[len] = 0;                /* len excludes the null */
   free_memory(buf);
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

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    struct dirent *ndir;
    int stat;

    P(mutex);
    errno = 0;
    ndir = readdir(dirp);
    stat = errno;
    if (ndir) {
       memcpy(entry, ndir, sizeof(struct dirent));
       strcpy(entry->d_name, ndir->d_name);
       *result = entry;
    } else {
       *result = NULL;
    }
    V(mutex);
    return stat;

}
#endif
#endif /* HAVE_READDIR_R */


int b_strerror(int errnum, char *buf, size_t bufsiz)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int stat = 0;
    const char *msg;

    P(mutex);

    msg = strerror(errnum);
    if (!msg) {
       msg = _("Bad errno");
       stat = -1;
    }
    bstrncpy(buf, msg, bufsiz);
    V(mutex);
    return stat;
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
static int del_pid_file_ok = FALSE;
#endif

/*
 * Create a standard "Unix" pid file.
 */
void create_pid_file(char *dir, const char *progname, int port)
{
#if !defined(HAVE_WIN32)
   int pidfd, len;
   int oldpid;
   char  pidbuf[20];
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   struct stat statp;

   Mmsg(&fname, "%s/%s.%d.pid", dir, progname, port);
   if (stat(fname, &statp) == 0) {
      /* File exists, see what we have */
      *pidbuf = 0;
      if ((pidfd = open(fname, O_RDONLY|O_BINARY, 0)) < 0 ||
           read(pidfd, &pidbuf, sizeof(pidbuf)) < 0 ||
           sscanf(pidbuf, "%d", &oldpid) != 1) {
         berrno be;
         Emsg2(M_ERROR_TERM, 0, _("Cannot open pid file. %s ERR=%s\n"), fname, 
               be.bstrerror());
      }
      /* Some OSes (IRIX) don't bother to clean out the old pid files after a crash, and
       * since they use a deterministic algorithm for assigning PIDs, we can have
       * pid conflicts with the old PID file after a reboot.
       * The intent the following code is to check if the oldpid read from the pid
       * file is the same as the currently executing process's pid,
       * and if oldpid == getpid(), skip the attempt to
       * kill(oldpid,0), since the attempt is guaranteed to succeed,
       * but the success won't actually mean that there is an
       * another Bacula process already running.
       * For more details see bug #797.
       */
       if ((oldpid != (int)getpid()) && (kill(oldpid, 0) != -1 || errno != ESRCH)) {
         Emsg3(M_ERROR_TERM, 0, _("%s is already running. pid=%d\nCheck file %s\n"),
               progname, oldpid, fname);
      }
      /* He is not alive, so take over file ownership */
      unlink(fname);                  /* remove stale pid file */
   }
   /* Create new pid file */
   if ((pidfd = open(fname, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0640)) >= 0) {
      len = sprintf(pidbuf, "%d\n", (int)getpid());
      write(pidfd, pidbuf, len);
      close(pidfd);
      del_pid_file_ok = TRUE;         /* we created it so we can delete it */
   } else {
      berrno be;
      Emsg2(M_ERROR_TERM, 0, _("Could not open pid file. %s ERR=%s\n"), fname, 
            be.bstrerror());
   }
   free_pool_memory(fname);
#endif
}


/*
 * Delete the pid file if we created it
 */
int delete_pid_file(char *dir, const char *progname, int port)
{
#if !defined(HAVE_WIN32)
   POOLMEM *fname = get_pool_memory(PM_FNAME);

   if (!del_pid_file_ok) {
      free_pool_memory(fname);
      return 0;
   }
   del_pid_file_ok = FALSE;
   Mmsg(&fname, "%s/%s.%d.pid", dir, progname, port);
   unlink(fname);
   free_pool_memory(fname);
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
   "Bacula State\n",
   4,
   0
};

/*
 * Open and read the state file for the daemon
 */
void read_state_file(char *dir, const char *progname, int port)
{
   int sfd;
   ssize_t stat;
   bool ok = false;
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   struct s_state_hdr hdr;
   int hdr_size = sizeof(hdr);

   Mmsg(&fname, "%s/%s.%d.state", dir, progname, port);
   /* If file exists, see what we have */
// Dmsg1(10, "O_BINARY=%d\n", O_BINARY);
   if ((sfd = open(fname, O_RDONLY|O_BINARY)) < 0) {
      berrno be;
      Dmsg3(010, "Could not open state file. sfd=%d size=%d: ERR=%s\n",
                    sfd, sizeof(hdr), be.bstrerror());
      goto bail_out;
   }
   if ((stat=read(sfd, &hdr, hdr_size)) != hdr_size) {
      berrno be;
      Dmsg4(010, "Could not read state file. sfd=%d stat=%d size=%d: ERR=%s\n",
                    sfd, (int)stat, hdr_size, be.bstrerror());
      goto bail_out;
   }
   if (hdr.version != state_hdr.version) {
      Dmsg2(010, "Bad hdr version. Wanted %d got %d\n",
         state_hdr.version, hdr.version);
      goto bail_out;
   }
   hdr.id[13] = 0;
   if (strcmp(hdr.id, state_hdr.id) != 0) {
      Dmsg0(000, "State file header id invalid.\n");
      goto bail_out;
   }
// Dmsg1(010, "Read header of %d bytes.\n", sizeof(hdr));
   if (!read_last_jobs_list(sfd, hdr.last_jobs_addr)) {
      goto bail_out;
   }
   ok = true;
bail_out:
   if (sfd >= 0) {
      close(sfd);
   }
   if (!ok) {
      unlink(fname);
    }
   free_pool_memory(fname);
}

/*
 * Write the state file
 */
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

void write_state_file(char *dir, const char *progname, int port)
{
   int sfd;
   bool ok = false;
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   
   P(state_mutex);                    /* Only one job at a time can call here */
   Mmsg(&fname, "%s/%s.%d.state", dir, progname, port);
   /* Create new state file */
   unlink(fname);
   if ((sfd = open(fname, O_CREAT|O_WRONLY|O_BINARY, 0640)) < 0) {
      berrno be;
      Dmsg2(000, "Could not create state file. %s ERR=%s\n", fname, be.bstrerror());
      Emsg2(M_ERROR, 0, _("Could not create state file. %s ERR=%s\n"), fname, be.bstrerror());
      goto bail_out;
   }
   if (write(sfd, &state_hdr, sizeof(state_hdr)) != sizeof(state_hdr)) {
      berrno be;
      Dmsg1(000, "Write hdr error: ERR=%s\n", be.bstrerror());
      goto bail_out;
   }
// Dmsg1(010, "Wrote header of %d bytes\n", sizeof(state_hdr));
   state_hdr.last_jobs_addr = sizeof(state_hdr);
   state_hdr.reserved[0] = write_last_jobs_list(sfd, state_hdr.last_jobs_addr);
// Dmsg1(010, "write last job end = %d\n", (int)state_hdr.reserved[0]);
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
// Dmsg1(010, "rewrote header = %d\n", sizeof(state_hdr));
bail_out:
   if (sfd >= 0) {
      close(sfd);
   }
   if (!ok) {
      unlink(fname);
   }
   V(state_mutex);
   free_pool_memory(fname);
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
 * Bacula's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD).
 */
#undef fgetc
char *bfgets(char *s, int size, FILE *fd)
{
   char *p = s;
   int ch;
   *p = 0;
   for (int i=0; i < size-1; i++) {
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
 * Bacula's implementation of fgets(). The difference is that it handles
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
   soft_max = sizeof_pool_memory(s) - 10;
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
         s = check_pool_memory_size(s, soft_max+10000);
         soft_max = sizeof_pool_memory(s) - 10;
      }
      s[i++] = ch;
      s[i] = 0;
      if (ch == '\r') { /* Support for Mac/Windows file format */
         ch = fgetc(fd);
         if (ch != '\n') { /* Mac (\r only) */
            (void)ungetc(ch, fd); /* Push next character back to fd */
         }
         s[i-1] = '\n';
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
void make_unique_filename(POOLMEM **name, int Id, char *what)
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

#if HAVE_BACKTRACE && HAVE_GCC
#include <cxxabi.h>
#include <execinfo.h>
void stack_trace()
{
   const size_t max_depth = 100;
   size_t stack_depth;
   void *stack_addrs[max_depth];
   char **stack_strings;
   
   stack_depth = backtrace(stack_addrs, max_depth);
   stack_strings = backtrace_symbols(stack_addrs, stack_depth);
   
   for (size_t i = 3; i < stack_depth; i++) {
      size_t sz = 200; /* just a guess, template names will go much wider */
      char *function = (char *)actuallymalloc(sz);
      char *begin = 0, *end = 0;
      /* find the parentheses and address offset surrounding the mangled name */
      for (char *j = stack_strings[i]; *j; ++j) {
         if (*j == '(') {
            begin = j;
         } else if (*j == '+') {
            end = j;
         }
      }
      if (begin && end) {
         *begin++ = '\0';
         *end = '\0';
         /* found our mangled name, now in [begin, end] */
         
         int status;
         char *ret = abi::__cxa_demangle(begin, function, &sz, &status);
         if (ret) {
            /* return value may be a realloc() of the input */
            function = ret;
         } else {
            /* demangling failed, just pretend it's a C function with no args */
            strncpy(function, begin, sz);
            strncat(function, "()", sz);
            function[sz-1] = '\0';
         }
         Pmsg2(000, "    %s:%s\n", stack_strings[i], function);

      } else {
         /* didn't find the mangled name, just print the whole line */
         Pmsg1(000, "    %s\n", stack_strings[i]);
      }
      actuallyfree(function);
   }
   actuallyfree(stack_strings); /* malloc()ed by backtrace_symbols */
}
#else /* HAVE_BACKTRACE && HAVE_GCC */
void stack_trace() {}
#endif /* HAVE_BACKTRACE && HAVE_GCC */
