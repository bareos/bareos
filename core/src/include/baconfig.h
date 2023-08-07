/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
#include "include/messages.h"

/* Bareos common configuration defines */

#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

#ifdef HAVE_TLS
#  define have_tls 1
#else
#  define have_tls 0
#endif

#ifndef ETIME
#  define ETIME ETIMEDOUT
#endif

#ifdef HAVE_IOCTL_ULINT_REQUEST
#  define ioctl_req_t unsigned long int
#else
#  define ioctl_req_t int
#endif

// Allow printing of NULL pointers
#define NPRT(x) (x) ? (x) : _("*None*")
#define NSTDPRNT(x) x.empty() ? "*None*" : x.c_str()
#define NPRTB(x) (x) ? (x) : ""

#if defined(HAVE_WIN32)
// Reduce compiler warnings from Windows vss code
#  define uuid(x)

void InitWinAPIWrapper();

#  define OSDependentInit() InitWinAPIWrapper()

#  define ClearThreadId(x) memset(&(x), 0, sizeof(x))

#else /* HAVE_WIN32 */

#  define ClearThreadId(x) x = 0
#  define OSDependentInit()

#endif /* HAVE_WIN32 */

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
#define DEFAULT_NETWORK_BUFFER_SIZE (64 * 1024)

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

/** For multiplying by 10 with shift and addition */
#define B_TIMES10(d) ((d << 3) + (d << 1))


typedef void(HANDLER)();
typedef int(INTHANDLER)();

#ifndef S_ISLNK
#  define S_ISLNK(m) (((m)&S_IFM) == S_IFLNK)
#endif

/** Added by KES to deal with Win32 systems */
#ifndef S_ISWIN32
#  define S_ISWIN32 020000
#endif

#ifndef INADDR_NONE
#  define INADDR_NONE ((unsigned long)-1)
#endif

#ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

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

constexpr slot_number_t kInvalidSlotNumber
    = std::numeric_limits<slot_number_t>::max();
constexpr slot_number_t kInvalidDriveNumber
    = std::numeric_limits<drive_number_t>::max();

constexpr int kInvalidFiledescriptor = -1;

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
#ifndef HAVE_WIN32
#  if defined(__digital__) && defined(__unix__)
/* Tru64 - it does have fseeko and ftello , but since ftell/fseek are also 64
 * bit */
/* take this 'shortcut' */
#    define fseeko fseek
#    define ftello ftell
#  else
#    ifndef HAVE_FSEEKO
/* Bad news. This OS cannot handle 64 bit fseeks and ftells */
#      define fseeko fseek
#      define ftello ftell
#    endif
#  endif /* __digital__ && __unix__ */
#endif   /* HAVE_WIN32 */

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

#endif  // BAREOS_INCLUDE_BACONFIG_H_
