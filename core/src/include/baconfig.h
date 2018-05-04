/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#undef  TRUE
#undef  FALSE
#define TRUE  1
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

#define MANUAL_AUTH_URL "http://doc.bareos.org/master/html/bareos-manual-main-reference.html#AuthorizationErrors"
#define MANUAL_CONFIG_DIR_URL "http://doc.bareos.org/master/html/bareos-manual-main-reference.html#ConfigurationIncludeDirectory"

#ifdef PROTOTYPES
# define __PROTO(p)     p
#else
# define __PROTO(p)     ()
#endif

#ifdef DEBUG
/**
 * In DEBUG mode an assert that is triggered generates a segmentation
 * fault so we can capture the debug info using btraceback.
 */
#define ASSERT(x) if (!(x)) { \
   char *fatal = NULL; \
   Emsg1(M_ERROR, 0, _("Failed ASSERT: %s\n"), #x); \
   Pmsg1(000, _("Failed ASSERT: %s\n"), #x); \
   fatal[0] = 0; }
#define ASSERT2(x,y) if (!(x)) { \
   assert_msg =  y; \
   Emsg1(M_ERROR, 0, _("Failed ASSERT: %s\n"), #x); \
   Pmsg1(000, _("Failed ASSERT: %s\n"), #x); \
   char *fatal = NULL; \
   fatal[0] = 0; }
#else
#define ASSERT(x)
#define ASSERT2(x, y)
#endif

/**
 * Allow printing of NULL pointers
 */
#define NPRT(x) (x)?(x):_("*None*")
#define NPRTB(x) (x)?(x):""

#if defined(HAVE_WIN32)
/**
 * Reduce compiler warnings from Windows vss code
 */
#define uuid(x)

void InitWinAPIWrapper();

#define  OSDependentInit()    InitWinAPIWrapper()

#define sbrk(x)  0

#define ClearThreadId(x) memset(&(x), 0, sizeof(x))

#if defined(BUILDING_DLL)
#define DLL_IMP_EXP _declspec(dllexport)
#define CATS_IMP_EXP _declspec(dllexport)
#define SD_IMP_EXP _declspec(dllexport)
#elif defined(USING_DLL)
#define DLL_IMP_EXP _declspec(dllimport)
#define CATS_IMP_EXP _declspec(dllimport)
#define SD_IMP_EXP _declspec(dllimport)
#else
#define DLL_IMP_EXP
#define CATS_IMP_EXP
#define SD_IMP_EXP
#endif

#else  /* HAVE_WIN32 */

#define ClearThreadId(x) x = 0

#define DLL_IMP_EXP
#define CATS_IMP_EXP
#define SD_IMP_EXP

#define  OSDependentInit()

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
#else /* !ENABLE_NLS */
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
#define B_ANSI_LABEL   1
#define B_IBM_LABEL    2

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
#define B_TIMES10(d) ((d<<3)+(d<<1))


typedef void (HANDLER)();
typedef int (INTHANDLER)();

#ifdef SETPGRP_VOID
# define SETPGRP_ARGS(x, y) /* No arguments */
#else
# define SETPGRP_ARGS(x, y) (x, y)
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFM) == S_IFLNK)
#endif

/** Added by KES to deal with Win32 systems */
#ifndef S_ISWIN32
#define S_ISWIN32 020000
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
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


enum {
API_MODE_OFF = 0,
API_MODE_ON = 1,
API_MODE_JSON = 2
};

#if defined(HAVE_WIN32)
typedef int64_t   boffset_t;
#define caddr_t  char *
#else
typedef off_t     boffset_t;
#endif

/**
* Create some simple types for now int16_t e.g. 65 K should be enough.
*/
typedef int16_t slot_number_t;
typedef int16_t drive_number_t;
typedef int16_t slot_flags_t;

/* These probably should be subroutines */
#define Pw(x) \
do { int errstat; if ((errstat=RwlWritelock(&(x)))) \
   e_msg(__FILE__, __LINE__, M_ABORT, 0, "Write lock lock failure. ERR=%s\n",\
        strerror(errstat)); \
} while(0)

#define Vw(x) \
do { int errstat; if ((errstat=RwlWriteunlock(&(x)))) \
      e_msg(__FILE__, __LINE__, M_ABORT, 0, "Write lock unlock failure. ERR=%s\n",\
        strerror(errstat)); \
} while(0)

#define LockRes()   b_LockRes(__FILE__, __LINE__)
#define UnlockRes() b_UnlockRes(__FILE__, __LINE__)

#ifdef DEBUG_MEMSET
#define memset(a, v, n) b_memset(__FILE__, __LINE__, a, v, n)
void b_memset(const char *file, int line, void *mem, int val, size_t num);
#endif

/**
* The digit following Dmsg and Emsg indicates the number of substitutions in
* the message string. We need to do this kludge because non-GNU compilers
* do not handle varargs #defines.
*/
/** Debug Messages that are printed */
#ifdef DEBUG
#define Dmsg0(lvl, msg) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg)
#define Dmsg1(lvl, msg, a1) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1)
#define Dmsg2(lvl, msg, a1, a2) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2)
#define Dmsg3(lvl, msg, a1, a2, a3) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define Dmsg4(lvl, msg, a1, a2, a3, a4) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4)
#define Dmsg5(lvl, msg, a1, a2, a3, a4, a5) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define Dmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Dmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Dmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) \
if ((lvl)<=debug_level) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Dmsg9(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
if ((lvl)<=debug_level) d_msg(__FILE__,__LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define Dmsg10(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
if ((lvl)<=debug_level) d_msg(__FILE__,__LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#define Dmsg11(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
if ((lvl)<=debug_level) d_msg(__FILE__,__LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define Dmsg12(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
if ((lvl)<=debug_level) d_msg(__FILE__,__LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
#define Dmsg13(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
if ((lvl)<=debug_level) d_msg(__FILE__,__LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
#else
#define Dmsg0(lvl, msg)
#define Dmsg1(lvl, msg, a1)
#define Dmsg2(lvl, msg, a1, a2)
#define Dmsg3(lvl, msg, a1, a2, a3)
#define Dmsg4(lvl, msg, a1, a2, a3, a4)
#define Dmsg5(lvl, msg, a1, a2, a3, a4, a5)
#define Dmsg6(lvl, msg, a1, a2, a3, a4, a5, a6)
#define Dmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Dmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Dmsg11(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define Dmsg12(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
#define Dmsg13(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
#endif /* DEBUG */

#ifdef TRACE_FILE
#define Tmsg0(lvl, msg) \
t_msg(__FILE__, __LINE__, lvl, msg)
#define Tmsg1(lvl, msg, a1) \
t_msg(__FILE__, __LINE__, lvl, msg, a1)
#define Tmsg2(lvl, msg, a1, a2) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2)
#define Tmsg3(lvl, msg, a1, a2, a3) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define Tmsg4(lvl, msg, a1, a2, a3, a4) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4)
#define Tmsg5(lvl, msg, a1, a2, a3, a4, a5) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define Tmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Tmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Tmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Tmsg9(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define Tmsg10(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#define Tmsg11(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define Tmsg12(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
#define Tmsg13(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
#else
#define Tmsg0(lvl, msg)
#define Tmsg1(lvl, msg, a1)
#define Tmsg2(lvl, msg, a1, a2)
#define Tmsg3(lvl, msg, a1, a2, a3)
#define Tmsg4(lvl, msg, a1, a2, a3, a4)
#define Tmsg5(lvl, msg, a1, a2, a3, a4, a5)
#define Tmsg6(lvl, msg, a1, a2, a3, a4, a5, a6)
#define Tmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Tmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Tmsg9(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define Tmsg10(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#define Tmsg11(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define Tmsg12(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
#define Tmsg13(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
#endif /* TRACE_FILE */

/** Messages that are printed (uses p_msg) */
#define Pmsg0(lvl, msg) \
p_msg(__FILE__, __LINE__, lvl, msg)
#define Pmsg1(lvl, msg, a1) \
p_msg(__FILE__, __LINE__, lvl, msg, a1)
#define Pmsg2(lvl, msg, a1, a2) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2)
#define Pmsg3(lvl, msg, a1, a2, a3) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define Pmsg4(lvl, msg, a1, a2, a3, a4) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4)
#define Pmsg5(lvl, msg, a1, a2, a3, a4, a5) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define Pmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Pmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Pmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Pmsg9(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define Pmsg10(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#define Pmsg11(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define Pmsg12(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
#define Pmsg13(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
#define Pmsg14(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) \
p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)

/** Messages that are printed using fixed size buffers (uses p_msg_fb) */
#define FPmsg0(lvl, msg) \
p_msg_fb(__FILE__, __LINE__, lvl, msg)
#define FPmsg1(lvl, msg, a1) \
p_msg_fb(__FILE__, __LINE__, lvl, msg, a1)
#define FPmsg2(lvl, msg, a1, a2) \
p_msg_fb(__FILE__, __LINE__, lvl, msg, a1, a2)
#define FPmsg3(lvl, msg, a1, a2, a3) \
p_msg_fb(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define FPmsg4(lvl, msg, a1, a2, a3, a4) \
p_msg_fb(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4)
#define FPmsg5(lvl, msg, a1, a2, a3, a4, a5) \
p_msg_fb(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define FPmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) \
p_msg_fb(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)

/** Daemon Error Messages that are delivered according to the message resource */
#define Emsg0(typ, lvl, msg) \
e_msg(__FILE__, __LINE__, typ, lvl, msg)
#define Emsg1(typ, lvl, msg, a1) \
e_msg(__FILE__, __LINE__, typ, lvl, msg, a1)
#define Emsg2(typ, lvl, msg, a1, a2) \
e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2)
#define Emsg3(typ, lvl, msg, a1, a2, a3) \
e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3)
#define Emsg4(typ, lvl, msg, a1, a2, a3, a4) \
e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3, a4)
#define Emsg5(typ, lvl, msg, a1, a2, a3, a4, a5) \
e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3, a4, a5)
#define Emsg6(typ, lvl, msg, a1, a2, a3, a4, a5, a6) \
e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3, a4, a5, a6)

/** Job Error Messages that are delivered according to the message resource */
#define Jmsg0(jcr, typ, lvl, msg) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg)
#define Jmsg1(jcr, typ, lvl, msg, a1) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1)
#define Jmsg2(jcr, typ, lvl, msg, a1, a2) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2)
#define Jmsg3(jcr, typ, lvl, msg, a1, a2, a3) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3)
#define Jmsg4(jcr, typ, lvl, msg, a1, a2, a3, a4) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4)
#define Jmsg5(jcr, typ, lvl, msg, a1, a2, a3, a4, a5) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5)
#define Jmsg6(jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Jmsg7(jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6, a7) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Jmsg8(jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) \
j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)

/** Queued Job Error Messages that are delivered according to the message resource */
#define Qmsg0(jcr, typ, lvl, msg) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg)
#define Qmsg1(jcr, typ, lvl, msg, a1) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1)
#define Qmsg2(jcr, typ, lvl, msg, a1, a2) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2)
#define Qmsg3(jcr, typ, lvl, msg, a1, a2, a3) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3)
#define Qmsg4(jcr, typ, lvl, msg, a1, a2, a3, a4) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4)
#define Qmsg5(jcr, typ, lvl, msg, a1, a2, a3, a4, a5) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5)
#define Qmsg6(jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6) \
q_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6)

/** Memory Messages that are edited into a Pool Memory buffer */
#define Mmsg0(buf, msg) \
msg_(__FILE__, __LINE__, buf, msg)
#define Mmsg1(buf, msg, a1) \
msg_(__FILE__, __LINE__, buf, msg, a1)
#define Mmsg2(buf, msg, a1, a2) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2)
#define Mmsg3(buf, msg, a1, a2, a3) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3)
#define Mmsg4(buf, msg, a1, a2, a3, a4) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4)
#define Mmsg5(buf, msg, a1, a2, a3, a4, a5) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5)
#define Mmsg6(buf, msg, a1, a2, a3, a4, a5, a6) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6)
#define Mmsg7(buf, msg, a1, a2, a3, a4, a5, a6, a7) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6)
#define Mmsg8(buf, msg, a1, a2, a3, a4, a5, a6, a7, a8) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6)
#define Mmsg11(buf, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define Mmsg15(buf, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
msg_(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15)

class PoolMem;

/* Edit message into Pool Memory buffer -- no __FILE__ and __LINE__ */
DLL_IMP_EXP int Mmsg(POOLMEM *&msgbuf, const char *fmt, ...);
DLL_IMP_EXP int Mmsg(PoolMem &msgbuf, const char *fmt, ...);
DLL_IMP_EXP int Mmsg(PoolMem *&msgbuf, const char *fmt, ...);

class JobControlRecord;
DLL_IMP_EXP void d_msg(const char *file, int line, int level, const char *fmt, ...);
DLL_IMP_EXP void p_msg(const char *file, int line, int level, const char *fmt, ...);
DLL_IMP_EXP void p_msg_fb(const char *file, int line, int level, const char *fmt,...);
DLL_IMP_EXP void e_msg(const char *file, int line, int type, int level, const char *fmt, ...);
DLL_IMP_EXP void j_msg(const char *file, int line, JobControlRecord *jcr, int type, utime_t mtime, const char *fmt, ...);
DLL_IMP_EXP void q_msg(const char *file, int line, JobControlRecord *jcr, int type, utime_t mtime, const char *fmt, ...);
DLL_IMP_EXP int msg_(const char *file, int line, POOLMEM *&pool_buf, const char *fmt, ...);

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

#ifdef DEBUG
#define bstrdup(str) strcpy((char *)b_malloc(__FILE__,__LINE__, strlen((str))+1), (str))
#else
#define bstrdup(str) strcpy((char *)bmalloc(strlen((str))+1),(str))
#endif

#ifdef DEBUG
#define bmalloc(size) b_malloc(__FILE__, __LINE__, (size))
#endif

/** Macro to simplify free/reset pointers */
#define BfreeAndNull(a) do{if(a){free(a); (a)=NULL;}} while(0)

/**
* Replace codes needed in both file routines and non-file routines
* Job replace codes -- in "replace"
*/
#define REPLACE_ALWAYS   'a'
#define REPLACE_IFNEWER  'w'
#define REPLACE_NEVER    'n'
#define REPLACE_IFOLDER  'o'

/** This probably should be done on a machine by machine basis, but it works */
/** This is critical for the smartalloc routines to properly align memory */
#define ALIGN_SIZE (sizeof(double))
#define BALIGN(x) (((x) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE -1))


/* =============================================================
*               OS Dependent defines
* =============================================================
*/
#ifndef HAVE_WIN32
#if defined (__digital__) && defined (__unix__)
/* Tru64 - it does have fseeko and ftello , but since ftell/fseek are also 64 bit */
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
#define SetThreadConcurrency(x)  ThrSetconcurrency(x)
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
/* Apparently someone forgot to wrap getdomainname as a C function */
#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */
int getdomainname(char *name, int len);
#ifdef  __cplusplus
}
#endif /* __cplusplus */
#endif /* HAVE_DARWIN_OS */

#define TRACEFILEDIRECTORY working_directory ? working_directory : "c:"

#if defined(HAVE_WIN32)
/**
 *   Windows
 */
#define DEFAULT_CONFIGDIR "C:\\Documents and Settings\\All Users\\Application Data\\Bareos"
#define PathSeparator '\\'

inline bool IsPathSeparator(int ch) { return ch == '/' || ch == '\\'; }
inline char *first_path_separator(char *path) { return strpbrk(path, "/\\"); }
inline const char *first_path_separator(const char *path) { return strpbrk(path, "/\\"); }

extern void PauseMsg(const char *file, const char *func, int line, const char *msg);
#define pause(msg) if (debug_level) PauseMsg(__FILE__, __func__, __LINE__, (msg))

#else
/**
 *   Unix/Linux
 */
#define PathSeparator '/'

/* Define Winsock functions if we aren't on Windows */
inline int WSA_Init() { return (0); } /* 0 = success */
inline int WSACleanup() { return (0); } /* 0 = success */

inline bool IsPathSeparator(int ch) { return ch == '/'; }
inline char *first_path_separator(char *path) { return strchr(path, '/'); }
inline const char *first_path_separator(const char *path) { return strchr(path, '/'); }
#define pause(msg)
#endif


/** HP-UX 11 specific workarounds */

#ifdef HAVE_HPUX_OS
# undef h_errno
extern int h_errno;
/** the {get,set}domainname() functions exist in HPUX's libc.
 * the configure script detects that correctly.
 * the problem is no system headers declares the prototypes for these functions
 * this is done below
 */
#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */
int getdomainname(char *name, int namelen);
int setdomainname(char *name, int namelen);
#ifdef  __cplusplus
}
#endif /* __cplusplus */
#endif /* HAVE_HPUX_OS */


#ifdef HAVE_OSF1_OS
#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */
int fchdir(int filedes);
long gethostid(void);
int getdomainname(char *name, int len);
#ifdef  __cplusplus
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
