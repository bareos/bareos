/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, October MMII
/**
 * @file
 * Encode and decode standard Unix attributes
 */

#include "include/bareos.h"
#include "lib/scan.h"
#include "lib/base64.h"

/**
 * Encode a stat structure into a base64 character string
 *   All systems must create such a structure.
 *   In addition, we tack on the LinkFI, which is non-zero in
 *   the case of a hard linked file that has no data.  This
 *   is a File Index pointing to the link that does have the
 *   data (always the first one encountered in a save).
 * You may piggyback attributes on this packet by encoding
 *   them in the encode_attribsEx() subroutine, but this is
 *   not recommended.
 */
void EncodeStat(char* buf,
                struct stat* statp,
                int stat_size,
                int32_t LinkFI,
                int data_stream)
{
  char* p = buf;

  /* We read the stat packet so make sure the caller's conception
   *  is the same as ours.  They can be different if LARGEFILE is not
   *  the same when compiling this library and the calling program. */
  ASSERT(stat_size == (int)sizeof(struct stat));

  /*  Encode a stat packet.  I should have done this more intelligently
   *   with a length so that it could be easily expanded. */
  p += ToBase64((int64_t)statp->st_dev, p);
  *p++ = ' '; /* separate fields with a space */
  p += ToBase64((int64_t)statp->st_ino, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_mode, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_nlink, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_uid, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_gid, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_rdev, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_size, p);
  *p++ = ' ';
#ifndef HAVE_MINGW
  p += ToBase64((int64_t)statp->st_blksize, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_blocks, p);
  *p++ = ' ';
#else
  p += ToBase64((int64_t)0, p); /* output place holder */
  *p++ = ' ';
  p += ToBase64((int64_t)0, p); /* output place holder */
  *p++ = ' ';
#endif
  p += ToBase64((int64_t)statp->st_atime, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_mtime, p);
  *p++ = ' ';
  p += ToBase64((int64_t)statp->st_ctime, p);
  *p++ = ' ';
  p += ToBase64((int64_t)LinkFI, p);
  *p++ = ' ';

#if defined(HAVE_CHFLAGS) && !defined(__stub_chflags)
  /* FreeBSD function */
  p += ToBase64((int64_t)statp->st_flags, p); /* output st_flags */
#else
  p += ToBase64((int64_t)0, p); /* output place holder */
#endif
  *p++ = ' ';
  p += ToBase64((int64_t)data_stream, p);
  *p = 0;
  return;
}

/* Do casting according to unknown type to keep compiler happy */
template <typename T> static void plug(T& st, uint64_t val)
{
  st = static_cast<T>(val);
}

// Decode a stat packet from base64 characters
int DecodeStat(char* buf, struct stat* statp, int stat_size, int32_t* LinkFI)
{
  char* p = buf;
  int64_t val;

  /* We store into the stat packet so make sure the caller's conception
   *  is the same as ours.  They can be different if LARGEFILE is not
   *  the same when compiling this library and the calling program. */
  ASSERT(stat_size == (int)sizeof(struct stat));
  memset(statp, 0, stat_size);

  p += FromBase64(&val, p);
  plug(statp->st_dev, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_ino, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_mode, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_nlink, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_uid, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_gid, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_rdev, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_size, val);
  p++;
#ifndef HAVE_MINGW
  p += FromBase64(&val, p);
  plug(statp->st_blksize, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_blocks, val);
  p++;
#else
  p += FromBase64(&val, p);
  //   plug(statp->st_blksize, val);
  p++;
  p += FromBase64(&val, p);
  //   plug(statp->st_blocks, val);
  p++;
#endif
  p += FromBase64(&val, p);
  plug(statp->st_atime, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_mtime, val);
  p++;
  p += FromBase64(&val, p);
  plug(statp->st_ctime, val);

  /* Optional FileIndex of hard linked file data */
  if (*p == ' ' || (*p != 0 && *(p + 1) == ' ')) {
    p++;
    p += FromBase64(&val, p);
    *LinkFI = (uint32_t)val;
  } else {
    *LinkFI = 0;
    return 0;
  }

  /* FreeBSD user flags */
  if (*p == ' ' || (*p != 0 && *(p + 1) == ' ')) {
    p++;
    p += FromBase64(&val, p);
#if defined(HAVE_CHFLAGS) && !defined(__stub_chflags)
    plug(statp->st_flags, val);
  } else {
    statp->st_flags = 0;
#endif
  }

  /* Look for data stream id */
  if (*p == ' ' || (*p != 0 && *(p + 1) == ' ')) {
    p++;
    p += FromBase64(&val, p);
  } else {
    val = 0;
  }
  return (int)val;
}
