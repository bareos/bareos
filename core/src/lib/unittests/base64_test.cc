/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2017 Bareos GmbH & Co. KG

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
/* Written by Kern E. Sibbald, March MM. */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */




#include "bareos.h"
#include "gtest/gtest.h"

#ifdef HAVE_WIN32
#define BINARYNAME "test_lib.exe"
#else
#define BINARYNAME "test_lib"
#endif

//#define xxxx

static int errfunc(const char *epath, int eernoo)
{
  printf("in errfunc\n");
  return 1;
}



TEST(base64, base64) {
   /*
    * Test the base64 routines by encoding and decoding
    * lstat() packets.
    */

   char where[500];
   int i;
   char *fname;
   struct stat statp;
   struct stat statn;
   int debug_level = 0;
   char *p;
   int32_t j;
   time_t t = 1028712799;


   fname = (char*)BINARYNAME;
   base64_init();
   if (lstat(fname, &statp) < 0) {
      berrno be;
      printf("Cannot stat %s: %s\n", fname, be.bstrerror(errno));
   }
   encode_stat(where, &statp, sizeof(statp), 0, 0);

   //printf("Encoded stat=%s\n", where);

#ifdef xxx
   p = where;
   p += to_base64((int64_t)(statp.st_atime), p);
   *p++ = ' ';
   p += to_base64((int64_t)t, p);
   printf("%s %s\n", fname, where);

   printf("%s %lld\n", "st_dev", (int64_t)statp.st_dev);
   printf("%s %lld\n", "st_ino", (int64_t)statp.st_ino);
   printf("%s %lld\n", "st_mode", (int64_t)statp.st_mode);
   printf("%s %lld\n", "st_nlink", (int64_t)statp.st_nlink);
   printf("%s %lld\n", "st_uid", (int64_t)statp.st_uid);
   printf("%s %lld\n", "st_gid", (int64_t)statp.st_gid);
   printf("%s %lld\n", "st_rdev", (int64_t)statp.st_rdev);
   printf("%s %lld\n", "st_size", (int64_t)statp.st_size);
   printf("%s %lld\n", "st_blksize", (int64_t)statp.st_blksize);
   printf("%s %lld\n", "st_blocks", (int64_t)statp.st_blocks);
   printf("%s %lld\n", "st_atime", (int64_t)statp.st_atime);
   printf("%s %lld\n", "st_mtime", (int64_t)statp.st_mtime);
   printf("%s %lld\n", "st_ctime", (int64_t)statp.st_ctime);
#endif

   //printf("%s: len=%d val=%s\n", fname, strlen(where), where);

   decode_stat(where, &statn, sizeof(statn), &j);

   EXPECT_FALSE(
         statp.st_dev != statn.st_dev ||
         statp.st_ino != statn.st_ino ||
         statp.st_mode != statn.st_mode ||
         statp.st_nlink != statn.st_nlink ||
         statp.st_uid != statn.st_uid ||
         statp.st_gid != statn.st_gid ||
         statp.st_rdev != statn.st_rdev ||
         statp.st_size != statn.st_size ||
         statp.st_blksize != statn.st_blksize ||
         statp.st_blocks != statn.st_blocks ||
         statp.st_atime != statn.st_atime ||
         statp.st_mtime != statn.st_mtime ||
         statp.st_ctime != statn.st_ctime
         );
   to_base64(UINT32_MAX, where);
}
