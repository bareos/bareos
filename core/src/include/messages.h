/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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
 * Header file for all kinds of messaging (dmsg, emsg, ...).
 * Includes ASSERT macro
 *
 */

#ifndef BAREOS_INCLUDE_MESSAGES_H_
#define BAREOS_INCLUDE_MESSAGES_H_

#include "include/bc_types.h"
#include "lib/message_severity.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  include <locale.h>
#  ifndef _
#    define _(s) gettext((s))
#  endif /* _ */
#  ifndef N_
#    define N_(s) (s)
#  endif /* N_ */
#else    /* !ENABLE_NLS */
#  undef _
#  undef N_
#  undef textdomain
#  undef bindtextdomain
#  undef setlocale

#  ifndef _
#    define _(s) (s)
#  endif
#  ifndef N_
#    define N_(s) (s)
#  endif
#  ifndef textdomain
#    define textdomain(d)
#  endif
#  ifndef bindtextdomain
#    define bindtextdomain(p, d)
#  endif
#  ifndef setlocale
#    define setlocale(p, d)
#  endif
#endif /* ENABLE_NLS */


/* Use the following for strings not to be translated */
#define NT_(s) (s)


/**
 * In DEBUG mode an assert that is triggered generates a segmentation
 * fault so we can capture the debug info using btraceback.
 */
#define ASSERT(x)                                    \
  if (!(x)) {                                        \
    Emsg1(M_ERROR, 0, _("Failed ASSERT: %s\n"), #x); \
    Pmsg1(000, _("Failed ASSERT: %s\n"), #x);        \
    abort();                                         \
  }

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
int Mmsg(std::vector<char>& msgbuf, const char* fmt, ...);

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

#endif  // BAREOS_INCLUDE_MESSAGES_H_
