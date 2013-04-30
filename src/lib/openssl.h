/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2007 Free Software Foundation Europe e.V.

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
 * openssl.h OpenSSL support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 *
 * Version $Id$
 *
 * This file was contributed to the Bacula project by Landon Fuller.
 *
 * Landon Fuller has been granted a perpetual, worldwide, non-exclusive,
 * no-charge, royalty-free, irrevocable copyright * license to reproduce,
 * prepare derivative works of, publicly display, publicly perform,
 * sublicense, and distribute the original work contributed by Landon Fuller
 * to the Bacula project in source or object form.
 *
 * If you wish to license these contributions under an alternate open source
 * license please contact Landon Fuller <landonf@opendarwin.org>.
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
