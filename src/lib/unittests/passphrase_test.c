/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015      Bareos GmbH & Co.KG

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
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for unittest framework cmocka
 *
 * Philipp Storz, April 2015
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

extern "C" {
#include <cmocka.h>
}
#include "bareos.h"
#define PASSWORDLEN 10

void test_generate_crypto_passphrase(void **state) {
   (void) state;
   char *password;
   int pwlen;
   password = generate_crypto_passphrase(PASSWORDLEN);
   pwlen = strlen(password);
   free(password);
   assert_int_equal(pwlen,PASSWORDLEN);
}
