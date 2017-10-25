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
#include "ndmp/ndmp9.h"
#include "protos.h"



int main(void) {
   // globals
   dbg_timestamp = true;
   debug_level= 100;
   working_directory = "/tmp";
   dbg_timestamp = true;

   init_msg(NULL, NULL);

   const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_ndmp_fhdb_lmdb),
      //cmocka_unit_test(test_ndmp_fhdb_mem),
   };
   return cmocka_run_group_tests(tests, NULL, NULL);
}
