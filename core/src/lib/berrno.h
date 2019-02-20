/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, July MMIV
 */
/**
 * @file
 * BErrNo header file
 */

/**
 * Extra bits set to interpret errno value differently from errno
 */
#ifdef HAVE_WIN32
#define b_errno_win32 (1 << 29) /* user reserved bit */
#else
#define b_errno_win32 0 /* On Unix/Linix system */
#endif
#define b_errno_exit (1 << 28)   /* child exited, exit code returned */
#define b_errno_signal (1 << 27) /* child died, signal code returned */

/**
 * A more generalized way of handling errno that works with Unix, Windows,
 *  and with BAREOS bpipes.
 *
 * It works by picking up errno and creating a memory pool buffer
 *  for editing the message. strerror() does the actual editing, and
 *  it is thread safe.
 *
 * If bit 29 in berrno_ is set then it is a Win32 error, and we
 *  must do a GetLastError() to get the error code for formatting.
 * If bit 29 in berrno_ is not set, then it is a Unix errno.
 *
 */
class BErrNo : public SmartAlloc {
  POOLMEM* buf_;
  int berrno_;
  void FormatWin32Message();

 public:
  BErrNo(int pool = PM_EMSG);
  ~BErrNo();
  const char* bstrerror();
  const char* bstrerror(int errnum);
  void SetErrno(int errnum);
  int code() { return berrno_ & ~(b_errno_exit | b_errno_signal); }
  int code(int stat) { return stat & ~(b_errno_exit | b_errno_signal); }
};

/* Constructor */
inline BErrNo::BErrNo(int pool)
{
  berrno_ = errno;
  buf_ = GetPoolMemory(pool);
  *buf_ = 0;
  errno = berrno_;
}

inline BErrNo::~BErrNo() { FreePoolMemory(buf_); }

inline const char* BErrNo::bstrerror(int errnum)
{
  berrno_ = errnum;
  return BErrNo::bstrerror();
}


inline void BErrNo::SetErrno(int errnum) { berrno_ = errnum; }
