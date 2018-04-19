/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, October MMII
 */
/**
 * @file
 * Encode and decode standard Unix attributes
 */

#include "include/bareos.h"
#include "lib/scan.h"

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
void encode_stat(char *buf, struct stat *statp, int stat_size, int32_t LinkFI, int data_stream)
{
   char *p = buf;

   /*
    * We read the stat packet so make sure the caller's conception
    *  is the same as ours.  They can be different if LARGEFILE is not
    *  the same when compiling this library and the calling program.
    */
   ASSERT(stat_size == (int)sizeof(struct stat));

   /**
    *  Encode a stat packet.  I should have done this more intelligently
    *   with a length so that it could be easily expanded.
    */
   p += to_base64((int64_t)statp->st_dev, p);
   *p++ = ' ';                        /* separate fields with a space */
   p += to_base64((int64_t)statp->st_ino, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_mode, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_nlink, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_uid, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_gid, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_rdev, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_size, p);
   *p++ = ' ';
#ifndef HAVE_MINGW
   p += to_base64((int64_t)statp->st_blksize, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_blocks, p);
   *p++ = ' ';
#else
   p += to_base64((int64_t)0, p); /* output place holder */
   *p++ = ' ';
   p += to_base64((int64_t)0, p); /* output place holder */
   *p++ = ' ';
#endif
   p += to_base64((int64_t)statp->st_atime, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_mtime, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_ctime, p);
   *p++ = ' ';
   p += to_base64((int64_t)LinkFI, p);
   *p++ = ' ';

#ifdef HAVE_CHFLAGS
   /* FreeBSD function */
   p += to_base64((int64_t)statp->st_flags, p);  /* output st_flags */
#else
   p += to_base64((int64_t)0, p);     /* output place holder */
#endif
   *p++ = ' ';
   p += to_base64((int64_t)data_stream, p);
   *p = 0;
   return;
}

/* Do casting according to unknown type to keep compiler happy */
#ifdef HAVE_TYPEOF
  #define plug(st, val) st = (typeof st)val
#else
  #if !HAVE_GCC & HAVE_SUN_OS
    /* Sun compiler does not handle templates correctly */
    #define plug(st, val) st = val
  #elif __sgi
    #define plug(st, val) st = val
  #else
    /* Use templates to do the casting */
    template <class T> void plug(T &st, uint64_t val)
      { st = static_cast<T>(val); }
  #endif
#endif

/**
 * Decode a stat packet from base64 characters
 */
int decode_stat(char *buf, struct stat *statp, int stat_size, int32_t *LinkFI)
{
   char *p = buf;
   int64_t val;

   /*
    * We store into the stat packet so make sure the caller's conception
    *  is the same as ours.  They can be different if LARGEFILE is not
    *  the same when compiling this library and the calling program.
    */
   ASSERT(stat_size == (int)sizeof(struct stat));
   memset(statp, 0, stat_size);

   p += from_base64(&val, p);
   plug(statp->st_dev, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_ino, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_mode, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_nlink, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_uid, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_gid, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_rdev, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_size, val);
   p++;
#ifndef HAVE_MINGW
   p += from_base64(&val, p);
   plug(statp->st_blksize, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_blocks, val);
   p++;
#else
   p += from_base64(&val, p);
//   plug(statp->st_blksize, val);
   p++;
   p += from_base64(&val, p);
//   plug(statp->st_blocks, val);
   p++;
#endif
   p += from_base64(&val, p);
   plug(statp->st_atime, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_mtime, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_ctime, val);

   /* Optional FileIndex of hard linked file data */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
      *LinkFI = (uint32_t)val;
   } else {
      *LinkFI = 0;
      return 0;
   }

   /* FreeBSD user flags */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
#ifdef HAVE_CHFLAGS
      plug(statp->st_flags, val);
   } else {
      statp->st_flags  = 0;
#endif
   }

   /* Look for data stream id */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
   } else {
      val = 0;
   }
   return (int)val;
}

/**
 * Decode a LinkFI field of encoded stat packet
 */
int32_t decode_LinkFI(char *buf, struct stat *statp, int stat_size)
{
   char *p = buf;
   int64_t val;
   /*
    * We store into the stat packet so make sure the caller's conception
    *  is the same as ours.  They can be different if LARGEFILE is not
    *  the same when compiling this library and the calling program.
    */
   ASSERT(stat_size == (int)sizeof(struct stat));

   skip_nonspaces(&p);                /* st_dev */
   p++;                               /* skip space */
   skip_nonspaces(&p);                /* st_ino */
   p++;
   p += from_base64(&val, p);
   plug(statp->st_mode, val);         /* st_mode */
   p++;
   skip_nonspaces(&p);                /* st_nlink */
   p++;
   skip_nonspaces(&p);                /* st_uid */
   p++;
   skip_nonspaces(&p);                /* st_gid */
   p++;
   skip_nonspaces(&p);                /* st_rdev */
   p++;
   skip_nonspaces(&p);                /* st_size */
   p++;
   skip_nonspaces(&p);                /* st_blksize */
   p++;
   skip_nonspaces(&p);                /* st_blocks */
   p++;
   skip_nonspaces(&p);                /* st_atime */
   p++;
   skip_nonspaces(&p);                /* st_mtime */
   p++;
   skip_nonspaces(&p);                /* st_ctime */

   /* Optional FileIndex of hard linked file data */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
      return (int32_t)val;
   }
   return 0;
}
