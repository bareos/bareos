/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
 *   Generic base 64 input and output routines
 *
 *    Written by Kern E. Sibbald, March MM.
 *
 *   Version $Id$
 */


#include "bacula.h"


#ifdef TEST_MODE
#include <glob.h>
#endif


static uint8_t const base64_digits[64] =
{
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static int base64_inited = 0;
static uint8_t base64_map[256];


/* Initialize the Base 64 conversion routines */
void
base64_init(void)
{
   int i;
   memset(base64_map, 0, sizeof(base64_map));
   for (i=0; i<64; i++)
      base64_map[(uint8_t)base64_digits[i]] = i;
   base64_inited = 1;
}

/* Convert a value to base64 characters.
 * The result is stored in where, which
 * must be at least 8 characters long.
 *
 * Returns the number of characters
 * stored (not including the EOS).
 */
int
to_base64(int64_t value, char *where)
{
   uint64_t val;
   int i = 0;
   int n;

   /* Handle negative values */
   if (value < 0) {
      where[i++] = '-';
      value = -value;
   }

   /* Determine output size */
   val = value;
   do {
      val >>= 6;
      i++;
   } while (val);
   n = i;

   /* Output characters */
   val = value;
   where[i] = 0;
   do {
      where[--i] = base64_digits[val & (uint64_t)0x3F];
      val >>= 6;
   } while (val);
   return n;
}

/*
 * Convert the Base 64 characters in where to
 * a value. No checking is done on the validity
 * of the characters!!
 *
 * Returns the value.
 */
int
from_base64(int64_t *value, char *where)
{
   uint64_t val = 0;
   int i, neg;

   if (!base64_inited)
      base64_init();
   /* Check if it is negative */
   i = neg = 0;
   if (where[i] == '-') {
      i++;
      neg = 1;
   }
   /* Construct value */
   while (where[i] != 0 && where[i] != ' ') {
      val <<= 6;
      val += base64_map[(uint8_t)where[i++]];
   }

   *value = neg ? -(int64_t)val : (int64_t)val;
   return i;
}


/*
 * Encode binary data in bin of len bytes into
 * buf as base64 characters.
 *
 * If compatible is true, the bin_to_base64 routine will be compatible
 * with what the rest of the world uses.
 *
 *  Returns: the number of characters stored not
 *           including the EOS
 */
int
bin_to_base64(char *buf, int buflen, char *bin, int binlen, int compatible)
{
   uint32_t reg, save, mask;
   int rem, i;
   int j = 0;

   reg = 0;
   rem = 0;
   buflen--;                       /* allow for storing EOS */
   for (i=0; i < binlen; ) {
      if (rem < 6) {
         reg <<= 8;
         if (compatible) {
            reg |= (uint8_t)bin[i++];
         } else {
            reg |= (int8_t)bin[i++];
         }
         rem += 8;
      }
      save = reg;
      reg >>= (rem - 6);
      if (j < buflen) {
         buf[j++] = base64_digits[reg & 0x3F];
      }
      reg = save;
      rem -= 6;
   }
   if (rem && j < buflen) {
      mask = (1 << rem) - 1;
      if (compatible) {
         buf[j++] = base64_digits[(reg & mask) << (6 - rem)];
      } else {
         buf[j++] = base64_digits[reg & mask];
      }
   }
   buf[j] = 0;
   return j;
}

/*
 * Decode base64 data in bin of len bytes into
 * buf as binary characters.
 *
 * the base64_to_bin routine is compatible with what the rest of the world
 * uses.
 *
 *  Returns: the number of characters stored not
 *           including the EOS
 */
int base64_to_bin(char *dest, int dest_size, char *src, int srclen)
{
   int nprbytes;
   uint8_t *bufout;
   uint8_t *bufplain = (uint8_t*) dest;
   const uint8_t *bufin;

   if (!base64_inited)
      base64_init();

   if (dest_size < (((srclen + 3) / 4) * 3)) {
      /* dest buffer too small */
      *dest = 0;
      return 0;
   }

   bufin = (const uint8_t *) src;
   while ((*bufin != ' ') && (srclen != 0)) {
      bufin++;
      srclen--;
   }

   nprbytes = bufin - (const uint8_t *) src;
   bufin = (const uint8_t *) src;
   bufout = (uint8_t *) bufplain;

   while (nprbytes > 4)
   {
      *(bufout++) = (base64_map[bufin[0]] << 2 | base64_map[bufin[1]] >> 4);
      *(bufout++) = (base64_map[bufin[1]] << 4 | base64_map[bufin[2]] >> 2);
      *(bufout++) = (base64_map[bufin[2]] << 6 | base64_map[bufin[3]]);
      bufin += 4;
      nprbytes -= 4;
   }

   /* Bacula base64 strings are not always padded with = */
   if (nprbytes > 1) {
      *(bufout++) = (base64_map[bufin[0]] << 2 | base64_map[bufin[1]] >> 4);
   }
   if (nprbytes > 2) {
      *(bufout++) = (base64_map[bufin[1]] << 4 | base64_map[bufin[2]] >> 2);
   }
   if (nprbytes > 3) {
      *(bufout++) = (base64_map[bufin[2]] << 6 | base64_map[bufin[3]]);
   }
   *bufout = 0;
   
   return (bufout - (uint8_t *) dest);
}

#ifdef BIN_TEST
int main(int argc, char *argv[])
{
   int xx = 0;
   int len;
   char buf[100];
   char junk[100];
   int i;

#ifdef xxxx
   for (i=0; i < 1000; i++) {
      bin_to_base64(buf, sizeof(buf), (char *)&xx, 4, true);
      printf("xx=%s\n", buf);
      xx++;
   }
#endif
   junk[0] = 0xFF;
   for (i=1; i<100; i++) {
      junk[i] = junk[i-1]-1;
   }
   len = bin_to_base64(buf, sizeof(buf), junk, 16, true);
   printf("len=%d junk=%s\n", len, buf);

   strcpy(junk, "This is a sample stringa");
   len = bin_to_base64(buf, sizeof(buf), junk, strlen(junk), true);
   buf[len] = 0;
   base64_to_bin(junk, sizeof(junk), buf, len);
   printf("buf=<%s>\n", junk);
   return 0;
}
#endif

#ifdef TEST_MODE
static int errfunc(const char *epath, int eernoo)
{
  printf("in errfunc\n");
  return 1;
}


/*
 * Test the base64 routines by encoding and decoding
 * lstat() packets.
 */
int main(int argc, char *argv[])
{
   char where[500];
   int i;
   glob_t my_glob;
   char *fname;
   struct stat statp;
   struct stat statn;
   int debug_level = 0;
   char *p;
   int32_t j;
   time_t t = 1028712799;

   if (argc > 1 && strcmp(argv[1], "-v") == 0)
      debug_level++;

   base64_init();

   my_glob.gl_offs = 0;
   glob("/etc/grub.conf", GLOB_MARK, errfunc, &my_glob);

   for (i=0; my_glob.gl_pathv[i]; i++) {
      fname = my_glob.gl_pathv[i];
      if (lstat(fname, &statp) < 0) {
         berrno be;
         printf("Cannot stat %s: %s\n", fname, be.bstrerror(errno));
         continue;
      }
      encode_stat(where, &statp, sizeof(statp), 0, 0);

      printf("Encoded stat=%s\n", where);

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

      if (debug_level)
         printf("%s: len=%d val=%s\n", fname, strlen(where), where);

      decode_stat(where, &statn, sizeof(statn), &j);

      if (statp.st_dev != statn.st_dev ||
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
          statp.st_ctime != statn.st_ctime) {

         printf("%s: %s\n", fname, where);
         encode_stat(where, &statn, sizeof(statn), 0, 0);
         printf("%s: %s\n", fname, where);
         printf("NOT EQAL\n");
      }

   }
   globfree(&my_glob);

   printf("%d files examined\n", i);

   to_base64(UINT32_MAX, where);
   printf("UINT32_MAX=%s\n", where);

   return 0;
}
#endif
