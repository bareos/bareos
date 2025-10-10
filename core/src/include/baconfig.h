/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
/**
 * @file
 * General header file configurations that apply to
 * all daemons.  System dependent stuff goes here.
 *
 */


#ifndef BAREOS_INCLUDE_BACONFIG_H_
#define BAREOS_INCLUDE_BACONFIG_H_

#include "lib/message.h"
#include "include/compiler_macro.h"

/* Bareos common configuration defines */

#ifndef ETIME
#  define ETIME ETIMEDOUT
#endif

#ifdef HAVE_IOCTL_ULINT_REQUEST
#  define ioctl_req_t unsigned long int
#else
#  define ioctl_req_t int
#endif

/**
 * In DEBUG mode an assert that is triggered generates a segmentation
 * fault so we can capture the debug info using btraceback.
 */
#define ASSERT(x)                                     \
  if (!(x)) {                                         \
    Emsg1(M_ERROR, 0, T_("Failed ASSERT: %s\n"), #x); \
    Pmsg1(000, T_("Failed ASSERT: %s\n"), #x);        \
    abort();                                          \
  }

// Allow printing of NULL pointers
#define NPRT(x) (x) ? (x) : T_("*None*")
#define NSTDPRNT(x) x.empty() ? "*None*" : x.c_str()
#define NPRTB(x) (x) ? (x) : ""

#if defined(HAVE_WIN32)

void InitWinAPIWrapper();

#  define OSDependentInit() InitWinAPIWrapper()

#  define ClearThreadId(x) memset(&(x), 0, sizeof(x))

#else /* HAVE_WIN32 */

#  define ClearThreadId(x) x = 0
#  define OSDependentInit() ((void)0)

#endif /* HAVE_WIN32 */

#ifdef ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  ifndef T_
#    define T_(s) gettext((s))
#  endif /* T_ */
#else    /* !ENABLE_NLS */
#  undef T_
#  undef textdomain
#  undef bindtextdomain
#  undef setlocale

#  define T_(s) (s)
#  define textdomain(d)
#  define bindtextdomain(p, d)
#  define setlocale(p, d)
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
#  define MAXHOSTNAMELEN 256
#endif

/* Default length of passphrases used */
#define DEFAULT_PASSPHRASE_LENGTH 32

#ifdef DEV_BSIZE
#  define B_DEV_BSIZE DEV_BSIZE
#endif

#if !defined(B_DEV_BSIZE) & defined(BSIZE)
#  define B_DEV_BSIZE BSIZE
#endif

#ifndef B_DEV_BSIZE
#  define B_DEV_BSIZE 512
#endif

/**
 * Set to time limit for other end to respond to
 *  authentication.  Normally 10 minutes is *way*
 *  more than enough. The idea is to keep the Director
 *  from hanging because there is a dead connection on
 *  the other end.
 */
#define AUTH_TIMEOUT 60 * 10

// Default network buffer size
inline constexpr std::size_t DEFAULT_NETWORK_BUFFER_SIZE = 256 * 1024;

// Tape label types -- stored in catalog
#define B_BAREOS_LABEL 0
#define B_ANSI_LABEL 1
#define B_IBM_LABEL 2

// Actions on purge (bit mask)
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
inline constexpr bool b_isjunkchar(int c)
{
  return (c == '\n' || c == '\r' || c == ' ');
}

/** For multiplying by 10 with shift and addition */
#define B_TIMES10(d) ((d << 3) + (d << 1))


typedef void(HANDLER)();
typedef int(INTHANDLER)();

#ifndef S_ISLNK
#  define S_ISLNK(m) (((m) & S_IFM) == S_IFLNK)
#endif

/** Added by KES to deal with Win32 systems */
#ifndef S_ISWIN32
#  define S_ISWIN32 020000
#endif

#ifndef INADDR_NONE
#  define INADDR_NONE ((unsigned long)-1)
#endif

#include <sys/time.h>

#ifndef O_BINARY
#  define O_BINARY 0
#endif

#ifndef O_NOFOLLOW
#  define O_NOFOLLOW 0
#endif

#ifndef MODE_RW
#  define MODE_RW 0666
#endif

#if defined(HAVE_WIN32)
typedef int64_t boffset_t;
#  define caddr_t char*
#else
typedef off_t boffset_t;
#endif

typedef uint16_t slot_number_t;
typedef uint16_t drive_number_t;
typedef uint16_t slot_flags_t;

#include <limits>

inline constexpr slot_number_t kInvalidSlotNumber
    = std::numeric_limits<slot_number_t>::max();
inline constexpr slot_number_t kInvalidDriveNumber
    = std::numeric_limits<drive_number_t>::max();

inline constexpr int kInvalidFiledescriptor = -1;

inline bool IsSlotNumberValid(slot_number_t slot)
{
  return slot && slot != kInvalidSlotNumber;
}

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


/**
 * In modern versions of C/C++ we can use attributes to express intentions
 * as not all compilers are modern enough for the attribute we want to use
 * we add macros for the attributes that will be automatically disabled on
 * older compilers that don't understand the attribute yet
 */
#if !defined(FALLTHROUGH_INTENDED)
#  if defined(__clang__)
#    define FALLTHROUGH_INTENDED [[clang::fallthrough]]
#  elif defined(__GNUC__) && __GNUC__ >= 7
#    define FALLTHROUGH_INTENDED [[gnu::fallthrough]]
#  else
#    define FALLTHROUGH_INTENDED \
      do {                       \
      } while (0)
#  endif
#endif

/**
 * As of C++11 varargs macros are part of the standard.
 * We keep the kludgy versions for backwards compatibility for now
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
int Mmsg(POOLMEM*& msgbuf, const char* fmt, ...) PRINTF_LIKE(2, 3);
int Mmsg(PoolMem& msgbuf, const char* fmt, ...) PRINTF_LIKE(2, 3);

class JobControlRecord;
void d_msg(const char* file, int line, int level, const char* fmt, ...)
    PRINTF_LIKE(4, 5);
void p_msg(const char* file, int line, int level, const char* fmt, ...)
    PRINTF_LIKE(4, 5);
void p_msg_fb(const char* file, int line, int level, const char* fmt, ...)
    PRINTF_LIKE(4, 5);
void e_msg(const char* file,
           int line,
           int type,
           int level,
           const char* fmt,
           ...) PRINTF_LIKE(5, 6);
void j_msg(const char* file,
           int line,
           JobControlRecord* jcr,
           int type,
           utime_t mtime,
           const char* fmt,
           ...) PRINTF_LIKE(6, 7);
void q_msg(const char* file,
           int line,
           JobControlRecord* jcr,
           int type,
           utime_t mtime,
           const char* fmt,
           ...) PRINTF_LIKE(6, 7);
int msg_(const char* file, int line, POOLMEM*& pool_buf, const char* fmt, ...)
    PRINTF_LIKE(4, 5);

#include "lib/bsys.h"
#include "lib/scan.h"

/** Use our fgets which handles interrupts */
#undef fgets
#define fgets(x, y, z) bfgets((x), (y), (z))

/** Use our sscanf, which is safer and works with known sizes */
#define sscanf bsscanf

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
/** This is critical for the memory allocation routines to properly align memory
 */
#define ALIGN_SIZE (sizeof(double))
#define BALIGN(x) (((x) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1))


/* =============================================================
 *               OS Dependent defines
 * =============================================================
 */
#ifdef HAVE_DARWIN_OS
/* Apparently someone forgot to wrap Getdomainname as a C function */
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */
int Getdomainname(char* name, int len);
#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif   /* HAVE_DARWIN_OS */

#define TRACEFILEDIRECTORY working_directory ? working_directory : "c:"

#if defined(HAVE_WIN32)
//   Windows
#  define DEFAULT_CONFIGDIR "C:\\Documents and Settings\\All Users\\Bareos"
#  define PathSeparator '\\'

inline bool IsPathSeparator(int ch)
{
  // check that this works regardless of whether one uses
  // wchar_t or char
  static_assert(int{L'/'} == int{'/'});
  static_assert(int{L'\\'} == int{'\\'});
  return ch == '/' || ch == '\\';
}
inline char* first_path_separator(char* path) { return strpbrk(path, "/\\"); }
inline const char* first_path_separator(const char* path)
{
  return strpbrk(path, "/\\");
}
#else
//   Unix/Linux
#  define PathSeparator '/'

/* Define Winsock functions if we aren't on Windows */
inline int WSA_Init() { return (0); }   /* 0 = success */
inline int WSACleanup() { return (0); } /* 0 = success */

inline bool IsPathSeparator(int ch) { return ch == '/'; }
inline char* first_path_separator(char* path) { return strchr(path, '/'); }
inline const char* first_path_separator(const char* path)
{
  return strchr(path, '/');
}
#endif

#ifndef __GNUC__
#  define __PRETTY_FUNCTION__ __func__
#endif
#ifdef ENTER_LEAVE
#  define Enter(lvl) Dmsg1(lvl, "Enter: %s\n", __PRETTY_FUNCTION__)
#  define Leave(lvl) Dmsg1(lvl, "Leave: %s\n", __PRETTY_FUNCTION__)
#else
#  define Enter(lvl)
#  define Leave(lvl)
#endif

#if defined(HAVE_WIN32)
// mingw/windows does not understand "%zu/%zi" by default
#  define PRIuz PRIu64
#  define PRIiz PRIi64
#  define PRItime "lld"
#else
#  define PRIuz "zu"
#  define PRIiz "zi"
#  define PRItime "ld"
#endif

#endif  // BAREOS_INCLUDE_BACONFIG_H_
