/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
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
 * Written by Kern E. Sibbald, March MM.
 */
/**
 * @file
 * Generic base 64 input and output routines
 */

#include "include/bareos.h"

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
void base64_init(void)
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
int to_base64(int64_t value, char *where)
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

/**
 * Convert the Base 64 characters in where to
 * a value. No checking is done on the validity
 * of the characters!!
 *
 * Returns the value.
 */
int from_base64(int64_t *value, char *where)
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


/**
 * Encode binary data in bin of len bytes into
 * buf as base64 characters.
 *
 * If compatible is true, the bin_to_base64 routine will be compatible
 * with what the rest of the world uses.
 *
 *  Returns: the number of characters stored not
 *           including the EOS
 */
int bin_to_base64(char *buf, int buflen, char *bin, int binlen, bool compatible)
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

/**
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

   /* BAREOS base64 strings are not always padded with = */
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
