/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2006 Free Software Foundation Europe e.V.

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
 * BAREOS MD5 definitions
 *
 * Kern Sibbald, 2001
 */

#ifndef __BMD5_H
#define __BMD5_H

#define MD5HashSize 16

struct MD5Context {
   uint32_t buf[4];
   uint32_t bits[2];
   uint8_t  in[64];
};

typedef struct MD5Context MD5Context;

void MD5Init(struct MD5Context *ctx);
void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
void MD5Transform(uint32_t buf[4], uint32_t in[16]);

#endif /* !__BMD5_H */
