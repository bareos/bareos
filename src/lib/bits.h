/* Some elementary bit manipulations
 *
 *   Kern Sibbald, MM
 *
 *  NOTE:  base 0
 *
 *   Version $Id$
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

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

#ifndef __BITS_H_
#define __BITS_H_

/* number of bytes to hold n bits */
#define nbytes_for_bits(n) ((((n)-1)>>3)+1)

/* test if bit is set */
#define bit_is_set(b, var) (((var)[(b)>>3] & (1<<((b)&0x7))) != 0)

/* set bit */
#define set_bit(b, var) ((var)[(b)>>3] |= (1<<((b)&0x7)))

/* clear bit */
#define clear_bit(b, var) ((var)[(b)>>3] &= ~(1<<((b)&0x7)))

/* clear all bits */
#define clear_all_bits(b, var) memset(var, 0, nbytes_for_bits(b))

/* set range of bits */
#define set_bits(f, l, var) { \
   int i; \
   for (i=f; i<=l; i++)  \
      set_bit(i, var); \
}

/* clear range of bits */
#define clear_bits(f, l, var) { \
   int i; \
   for (i=f; i<=l; i++)  \
      clear_bit(i, var); \
}

#endif /* __BITS_H_ */
