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
 * Main program to run unittests for lib with the cmocka framework
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
#include "../lib/protos.h"
#include "protos.h"


int main(void) {
   const struct CMUnitTest tests[] = {
#ifdef HAVE_WIN32
      cmocka_unit_test(test_junction),
#endif
      cmocka_unit_test(test_rwlock),
      cmocka_unit_test(test_devlock),
      cmocka_unit_test(test_sellist),
      cmocka_unit_test(test_scan),
      cmocka_unit_test(test_edit),
      cmocka_unit_test(test_dlist),
      cmocka_unit_test(test_bsnprintf),
      cmocka_unit_test(test_alist),
      cmocka_unit_test(test_htable),
//      cmocka_unit_test(test_base64),
//      cmocka_unit_test(test_generate_crypto_passphrase),
//      cmocka_unit_test(test_fnmatch),
//      cmocka_unit_test(test_guid_to_name),
//      cmocka_unit_test(test_ini),
//      cmocka_unit_test(test_rblist),  // stops in malloc()
//      cmocka_unit_test(test_tree),
   };
   return cmocka_run_group_tests(tests, NULL, NULL);
}
