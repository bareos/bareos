/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
/**
 * @file
 * General header file configurations that apply to
 * all daemons.  System dependent stuff goes here.
 *
 */


#ifndef BAREOS_INCLUDE_BACONFIG_H_
#define BAREOS_INCLUDE_BACONFIG_H_ 1


/* Bareos common configuration defines */

#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

#ifdef HAVE_TLS
#define have_tls 1
#else
#define have_tls 0
#endif

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

#ifdef HAVE_IOCTL_ULINT_REQUEST
#define ioctl_req_t unsigned long int
#else
#define ioctl_req_t int
#endif


#ifdef PROTOTYPES
#define __PROTO(p) p
#else
#define __PROTO(p) ()
#endif

/**
 * In DEBUG mode an assert that is triggered generates a segmentation
 * fault so we can capture the debug info using btraceback.
 */
#define ASSERT(x)                                    \
  if (!(x)) {                                        \
    char* fatal = NULL;                              \
    Emsg1(M_ERROR, 0, _("Failed ASSERT: %s\n"), #x); \
    Pmsg1(000, _("Failed ASSERT: %s\n"), #x);        \
    fatal[0] = 0;                                    \
  }
#define ASSERT2(x, y)                                \
  if (!(x)) {                                        \
    assert_msg = y;                                  \
    Emsg1(M_ERROR, 0, _("Failed ASSERT: %s\n"), #x); \
    Pmsg1(000, _("Failed ASSERT: %s\n"), #x);        \
    char* fatal = NULL;                              \
    fatal[0] = 0;                                    \
  }

/**
 * Allow printing of NULL pointers
 */
#define NPRT(x) (x) ? (x) : _("*None*")
#define NPRTB(x) (x) ? (x) : ""

#if defined(HAVE_WIN32)
/**
 * Reduce compiler warnings from Windows vss code
 */
#define uuid(x)

void InitWinAPIWrapper();

#define OSDependentInit() InitWinAPIWrapper()

#define sbrk(x) 0

#define ClearThreadId(x) memset(&(x), 0, sizeof(x))

#else /* HAVE_WIN32 */

#define ClearThreadId(x) x = 0
#define OSDependentInit()

#endif /* HAVE_WIN32 */

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#ifndef _
#define _(s) gettext((s))
#endif /* _ */
#ifndef N_
#define N_(s) (s)
#endif /* N_ */
#else  /* !ENABLE_NLS */
#undef _
#undef N_
#undef textdomain
#undef bindtextdomain
#undef setlocale

#ifndef _
#define _(s) (s)
#endif
#ifndef N_
#define N_(s) (s)
#endif
#ifndef textdomain
#define textdomain(d)
#endif
#ifndef bindtextdomain
#define bindtextdomain(p, d)
#endif
#ifndef setlocale
#define setlocale(p, d)
#endif
#endif /* ENABLE_NLS */


/* Use the following for strings not to be translated */
#define NT_(s) (s)

/* Maximum length to edit time/date */
#define MAX_TIME_LENGTH 50

/* Maximum Name length including EOS */
#define MAX_NAME_LENGTH 128

/* Maximum escaped Name length including EOS 2*MAX_NAME_LENGTH+1 */
#define MAX_ESCAPE_NAME_LENGTH 257

/* Maximum number of user entered command args */
#define MAX_CMD_ARGS 30

/* All tape operations MUST be a multiple of this */
#define TAPE_BSIZE 1024

/* Maximum length of a hostname */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

/* Default length of passphrases used */
#define DEFAULT_PASSPHRASE_LENGTH 32

#ifdef DEV_BSIZE
#define B_DEV_BSIZE DEV_BSIZE
#endif

#if !defined(B_DEV_BSIZE) & defined(BSIZE)
#define B_DEV_BSIZE BSIZE
#endif

#ifndef B_DEV_BSIZE
#define B_DEV_BSIZE 512
#endif

/**
 * Set to time limit for other end to respond to
 *  authentication.  Normally 10 minutes is *way*
 *  more than enough. The idea is to keep the Director
 *  from hanging because there is a dead connection on
 *  the other end.
 */
#define AUTH_TIMEOUT 60 * 10

/**
 * Default network buffer size
 */
#define DEFAULT_NETWORK_BUFFER_SIZE (64 * 1024)

/**
 * Tape label types -- stored in catalog
 */
#define B_BAREOS_LABEL 0
#define B_ANSI_LABEL 1
#define B_IBM_LABEL 2

/**
 * Actions on purge (bit mask)
 */
#define ON_PURGE_NONE 0
#define ON_PURGE_TRUNCATE 1

/* Size of File Address stored in STREAM_SPARSE_DATA. Do NOT change! */
#define OFFSET_FADDR_SIZE (sizeof(uint64_t))

/* Size of crypto length stored at head of crypto buffer. Do NOT change! */
#define CRYPTO_LEN_SIZE ((int)sizeof(uint32_t))


/**
 * This is for dumb compilers/libraries like Solaris. Linux GCC
 * does it correctly, so it might be worthwhile
 * to remove the isascii(c) with ifdefs on such
 * "smart" systems.
 */
#define B_ISSPACE(c) (isascii((int)(c)) && isspace((int)(c)))
#define B_ISALPHA(c) (isascii((int)(c)) && isalpha((int)(c)))
#define B_ISUPPER(c) (isascii((int)(c)) && isupper((int)(c)))
#define B_ISDIGIT(c) (isascii((int)(c)) && isdigit((int)(c)))

/** For multiplying by 10 with shift and addition */
#define B_TIMES10(d) ((d << 3) + (d << 1))


typedef void(HANDLER)();
typedef int(INTHANDLER)();

#ifdef SETPGRP_VOID
#define SETPGRP_ARGS(x, y) /* No arguments */
#else
#define SETPGRP_ARGS(x, y) (x, y)
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (((m)&S_IFM) == S_IFLNK)
#endif

/** Added by KES to deal with Win32 systems */
#ifndef S_ISWIN32
#define S_ISWIN32 020000
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long)-1)
#endif

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#ifndef MODE_RW
#define MODE_RW 0666
#endif


enum
{
  API_MODE_OFF = 0,
  API_MODE_ON = 1,
  API_MODE_JSON = 2
};

#if defined(HAVE_WIN32)
typedef int64_t boffset_t;
#define caddr_t char*
#else
typedef off_t boffset_t;
#endif

/**
 * Create some simple types for now int16_t e.g. 65 K should be enough.
 */
typedef int16_t slot_number_t;
typedef int16_t drive_number_t;
typedef int16_t slot_flags_t;

/* These probably should be subroutines */
#define Pw(x)                                                        \
  do {                                                               \
    int errstat;                                                     \
    if ((errstat = RwlWritelock(&(x))))                              \
      e_msg(__FILE__, __LINE__, M_ABORT, 0,                          \
            "Write lock lock failure. ERR=%s\n", strerror(errstat)); \
  } while (0)

#define Vw(x)                                                          \
  do {                                                                 \
    int errstat;                                                       \
    if ((errstat = RwlWriteunlock(&(x))))                              \
      e_msg(__FILE__, __LINE__, M_ABORT, 0,                            \
            "Write lock unlock failure. ERR=%s\n", strerror(errstat)); \
  } while (0)

#define LockRes(x) (x)->b_LockRes(__FILE__, __LINE__)
#define UnlockRes(x) (x)->b_UnlockRes(__FILE__, __LINE__)

/**
 * As of C++11 varargs macros are part of the standard.
 * We keep the kludgy versions for backwards compatability for now
 * but they're going to go away soon.
 */
/** Debug Messages that are printed */
#define Dmsg0(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg1(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg2(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg3(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg4(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg5(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg6(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg7(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg8(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg9(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg10(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg11(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg12(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Dmsg13(lvl, ...) \
  if ((lvl) <= debug_level) d_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)

/** Messages that are printed (uses p_msg) */
#define Pmsg0(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg1(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg2(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg3(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg4(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg5(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg6(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg7(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg8(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg9(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg10(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg11(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg12(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg13(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg14(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define Pmsg15(lvl, ...) p_msg(__FILE__, __LINE__, lvl, __VA_ARGS__)

/** Messages that are printed using fixed size buffers (uses p_msg_fb) */
#define FPmsg0(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define FPmsg1(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define FPmsg2(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define FPmsg3(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define FPmsg4(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define FPmsg5(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)
#define FPmsg6(lvl, ...) p_msg_fb(__FILE__, __LINE__, lvl, __VA_ARGS__)

/** Daemon Error Messages that are delivered according to the message resource
 */
#define Emsg0(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)
#define Emsg1(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)
#define Emsg2(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)
#define Emsg3(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)
#define Emsg4(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)
#define Emsg5(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)
#define Emsg6(typ, lvl, ...) e_msg(__FILE__, __LINE__, typ, lvl, __VA_ARGS__)

/** Job Error Messages that are delivered according to the message resource */
#define Jmsg0(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg1(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg2(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg3(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg4(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg5(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg6(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg7(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Jmsg8(jcr, typ, lvl, ...) \
  j_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)

/** Queued Job Error Messages that are delivered according to the message
 * resource */
#define Qmsg0(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Qmsg1(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Qmsg2(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Qmsg3(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Qmsg4(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Qmsg5(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)
#define Qmsg6(jcr, typ, lvl, ...) \
  q_msg(__FILE__, __LINE__, jcr, typ, lvl, __VA_ARGS__)

/** Memory Messages that are edited into a Pool Memory buffer */
#define Mmsg0(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg1(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg2(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg3(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg4(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg5(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg6(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg7(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg8(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg9(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg10(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg11(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg12(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg13(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg14(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)
#define Mmsg15(buf, ...) msg_(__FILE__, __LINE__, buf, __VA_ARGS__)

class PoolMem;

/* Edit message into Pool Memory buffer -- no __FILE__ and __LINE__ */
int Mmsg(POOLMEM*& msgbuf, const char* fmt, ...);
int Mmsg(PoolMem& msgbuf, const char* fmt, ...);
int Mmsg(PoolMem*& msgbuf, const char* fmt, ...);

class JobControlRecord;
void d_msg(const char* file, int line, int level, const char* fmt, ...);
void p_msg(const char* file, int line, int level, const char* fmt, ...);
void p_msg_fb(const char* file, int line, int level, const char* fmt, ...);
void e_msg(const char* file,
           int line,
           int type,
           int level,
           const char* fmt,
           ...);
void j_msg(const char* file,
           int line,
           JobControlRecord* jcr,
           int type,
           utime_t mtime,
           const char* fmt,
           ...);
void q_msg(const char* file,
           int line,
           JobControlRecord* jcr,
           int type,
           utime_t mtime,
           const char* fmt,
           ...);
int msg_(const char* file, int line, POOLMEM*& pool_buf, const char* fmt, ...);

#include "lib/bsys.h"
#include "lib/scan.h"

/** Use our strdup with smartalloc */
#undef strdup
#define strdup(buf) BadCallOnStrdupUseBstrdup(buf)

/** Use our fgets which handles interrupts */
#undef fgets
#define fgets(x, y, z) bfgets((x), (y), (z))

/** Use our sscanf, which is safer and works with known sizes */
#define sscanf bsscanf

#define bstrdup(str) \
  strcpy((char*)b_malloc(__FILE__, __LINE__, strlen((str)) + 1), (str))

#define actuallystrdup(str) \
  strcpy((char*)actuallymalloc(strlen((str)) + 1), (str))

#define bmalloc(size) b_malloc(__FILE__, __LINE__, (size))

/** Macro to simplify free/reset pointers */
#define BfreeAndNull(a) \
  do {                  \
    if (a) {            \
      free(a);          \
      (a) = NULL;       \
    }                   \
  } while (0)

/**
 * Replace codes needed in both file routines and non-file routines
 * Job replace codes -- in "replace"
 */
#define REPLACE_ALWAYS 'a'
#define REPLACE_IFNEWER 'w'
#define REPLACE_NEVER 'n'
#define REPLACE_IFOLDER 'o'

/** This probably should be done on a machine by machine basis, but it works */
/** This is critical for the smartalloc routines to properly align memory */
#define ALIGN_SIZE (sizeof(double))
#define BALIGN(x) (((x) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1))


/* =============================================================
 *               OS Dependent defines
 * =============================================================
 */
#ifndef HAVE_WIN32
#if defined(__digital__) && defined(__unix__)
/* Tru64 - it does have fseeko and ftello , but since ftell/fseek are also 64
 * bit */
/* take this 'shortcut' */
#define fseeko fseek
#define ftello ftell
#else
#ifndef HAVE_FSEEKO
/* Bad news. This OS cannot handle 64 bit fseeks and ftells */
#define fseeko fseek
#define ftello ftell
#endif
#endif /* __digital__ && __unix__ */
#endif /* HAVE_WIN32 */

#ifdef HAVE_SUN_OS
/**
 * On Solaris 2.5/2.6/7 and 8, threads are not timesliced by default,
 * so we need to explictly increase the concurrency level.
 */
#ifdef USE_THR_SETCONCURRENCY
#include <thread.h>
#define SetThreadConcurrency(x) ThrSetconcurrency(x)
extern int ThrSetconcurrency(int);
#define SunOS 1
#else
#define SetThreadConcurrency(x)
#endif

#else
/**
 * Not needed on most systems
 */
#define SetThreadConcurrency(x)

#endif

#ifdef HAVE_DARWIN_OS
/* Apparently someone forgot to wrap Getdomainname as a C function */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int Getdomainname(char* name, int len);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* HAVE_DARWIN_OS */

#define TRACEFILEDIRECTORY working_directory ? working_directory : "c:"

#if defined(HAVE_WIN32)
/**
 *   Windows
 */
#define DEFAULT_CONFIGDIR \
  "C:\\Documents and Settings\\All Users\\Application Data\\Bareos"
#define PathSeparator '\\'

inline bool IsPathSeparator(int ch) { return ch == '/' || ch == '\\'; }
inline char* first_path_separator(char* path) { return strpbrk(path, "/\\"); }
inline const char* first_path_separator(const char* path)
{
  return strpbrk(path, "/\\");
}

extern void PauseMsg(const char* file,
                     const char* func,
                     int line,
                     const char* msg);
#define pause(msg) \
  if (debug_level) PauseMsg(__FILE__, __func__, __LINE__, (msg))

#else
/**
 *   Unix/Linux
 */
#define PathSeparator '/'

/* Define Winsock functions if we aren't on Windows */
inline int WSA_Init() { return (0); }   /* 0 = success */
inline int WSACleanup() { return (0); } /* 0 = success */

inline bool IsPathSeparator(int ch) { return ch == '/'; }
inline char* first_path_separator(char* path) { return strchr(path, '/'); }
inline const char* first_path_separator(const char* path)
{
  return strchr(path, '/');
}
#define pause(msg)
#endif


/** HP-UX 11 specific workarounds */

#ifdef HAVE_HPUX_OS
#undef h_errno
extern int h_errno;
/** the {get,set}domainname() functions exist in HPUX's libc.
 * the configure script detects that correctly.
 * the problem is no system headers declares the prototypes for these functions
 * this is done below
 */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int Getdomainname(char* name, int namelen);
int Setdomainname(char* name, int namelen);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* HAVE_HPUX_OS */


#ifdef HAVE_OSF1_OS
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int fchdir(int filedes);
long gethostid(void);
int Getdomainname(char* name, int len);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* HAVE_OSF1_OS */


/** Disabled because it breaks internationalisation...
#undef HAVE_SETLOCALE
#ifdef HAVE_SETLOCALE
#include <locale.h>
#else
#define setlocale(x, y) ("ANSI_X3.4-1968")
#endif
#ifdef HAVE_NL_LANGINFO
#include <langinfo.h>
#else
#define nl_langinfo(x) ("ANSI_X3.4-1968")
#endif
*/

/** Determine endianes */
static inline bool bigendian() { return htonl(1) == 1L; }

#ifndef __GNUC__
#define __PRETTY_FUNCTION__ __func__
#endif
#ifdef ENTER_LEAVE
#define Enter(lvl) Dmsg1(lvl, "Enter: %s\n", __PRETTY_FUNCTION__)
#define Leave(lvl) Dmsg1(lvl, "Leave: %s\n", __PRETTY_FUNCTION__)
#else
#define Enter(lvl)
#define Leave(lvl)
#endif

#endif /* BAREOS_INCLUDE_BACONFIG_H_ */
