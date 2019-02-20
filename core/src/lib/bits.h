/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/*
 * Some elementary bit manipulations
 * NOTE:  base 0
 */

#ifndef BAREOS_LIB_BITS_H_
#define BAREOS_LIB_BITS_H_

/*
 * Number of bytes to hold n bits
 */
#define NbytesForBits(n) ((((n)-1) >> 3) + 1)

/*
 * Test if bit is set
 */
#define BitIsSet(b, var) (((var)[(b) >> 3] & (1 << ((b)&0x7))) != 0)

/*
 * Set bit
 */
#define SetBit(b, var) ((var)[(b) >> 3] |= (1 << ((b)&0x7)))

/*
 * Clear bit
 */
#define ClearBit(b, var) ((var)[(b) >> 3] &= ~(1 << ((b)&0x7)))

/*
 * Clear all bits
 */
#define ClearAllBits(b, var) memset((var), 0, NbytesForBits((b)))

/*
 * Set range of bits
 */
#define SetBits(f, l, var)                                 \
  {                                                        \
    int bit;                                               \
    for (bit = (f); bit <= (l); bit++) SetBit(bit, (var)); \
  }

/*
 * Clear range of bits
 */
#define ClearBits(f, l, var)                                 \
  {                                                          \
    int bit;                                                 \
    for (bit = (f); bit <= (l); bit++) ClearBit(bit, (var)); \
  }

/*
 * Clone all set bits from var1 to var2
 */
#define CopySetBits(l, var1, var2)                    \
  {                                                   \
    int bit;                                          \
    for (bit = 0; bit <= (l); bit++)                  \
      if (BitIsSet(bit, (var1))) SetBit(bit, (var2)); \
  }

/*
 * Copy all bits from var1 to var2
 */
#define CopyBits(b, var1, var2) memcpy((var2), (var1), NbytesForBits((b)))

#endif /* BAREOS_LIB_BITS_H_ */
