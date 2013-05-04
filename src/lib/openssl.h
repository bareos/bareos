/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2007 Free Software Foundation Europe e.V.

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
 * openssl.h OpenSSL support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 */

#ifndef __OPENSSL_H_
#define __OPENSSL_H_

#ifdef HAVE_OPENSSL
void             openssl_post_errors     (int code, const char *errstring);
void             openssl_post_errors     (JCR *jcr, int code, const char *errstring);
int              openssl_init_threads    (void);
void             openssl_cleanup_threads (void);
int              openssl_seed_prng       (void);
int              openssl_save_prng       (void);
#endif /* HAVE_OPENSSL */

#endif /* __OPENSSL_H_ */
