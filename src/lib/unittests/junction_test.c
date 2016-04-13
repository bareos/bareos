/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016      Bareos GmbH & Co. KG

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
 * Philipp Storz, March 2016
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>


extern "C" {
#include <cmocka.h>
}

#ifdef HAVE_WIN32
#include "bareos.h"
#include "winapi.h"

bool CreateJunction(const char *szJunction, const char *szPath);


/*
 * Test the CreateJunction functionality
 * - create target directory
 * - create a file in this dir
 * - create junction
 * - change dir into junction
 * - check if file is accessible
 * - remove both dir and junction
 */

#define TESTSTRING "THIS_IS_A_JUNCTION_TEST"
void test_junction(void **state) {

   (void) state; /* unused */

   FILE* f;

   POOLMEM *szJunctionW = get_pool_memory(PM_FNAME);
   POOLMEM *szTargetW = get_pool_memory(PM_FNAME);
   POOLMEM *szTestFileW = get_pool_memory(PM_FNAME);
   POOLMEM *s = get_pool_memory(PM_FNAME);

   const char *szJunction = "C:\\junction_with_öüäß";
   const char *szTarget = "C:\\öäüß";
   const char *szTestFile = "testfile";

   debug_level = 500;
   InitWinAPIWrapper();
   /*
    * convert names to wchar
    */
   if (!UTF8_2_wchar(szJunctionW, szJunction)) {
      printf("error converting szJunction:%s\n", errorString());
   }
   make_win32_path_UTF8_2_wchar(szTargetW, szTarget);
   make_win32_path_UTF8_2_wchar(szTestFileW, szTestFile);


   if (!p_CreateDirectoryW((LPCWSTR)szTargetW, NULL)) {
      printf("CreateDirectory Target Failed:%s\n", errorString());
   }

   BOOL WINAPI retval = SetCurrentDirectoryW( (LPCWSTR)szTargetW );
   assert_non_null(retval);

   f = _wfopen( (LPCWSTR)szTestFileW, L"w" );
   if (!f) printf("fopen Failed:%s\n", errorString());
   assert_non_null(f);
   fprintf(f, TESTSTRING);
   fclose(f);

   bool successful = CreateJunction(szJunction, szTarget);

   retval = SetCurrentDirectoryW( (LPCWSTR)szJunctionW );
   assert_non_null(retval);

   f = _wfopen( (LPCWSTR)szTestFileW, L"r" );
   if (!f) printf("fopen Failed:%s\n", errorString());

   assert_non_null(f);
   fscanf( f, "%s", s);
   assert_string_equal( TESTSTRING, s);
   fclose(f);

   int unlinkresult = _wunlink((LPCWSTR)szTestFileW);
   assert_int_equal(unlinkresult,0);

   retval = SetCurrentDirectoryW( L"C:\\" );
   assert_non_null(retval);

   if (!RemoveDirectoryW((LPCWSTR)szJunctionW)) {
      printf("RemoveDirectory Junction Failed:%s\n", errorString());
   }
   if (!RemoveDirectoryW((LPCWSTR)szTargetW)) {
      printf("RemoveDirectory Target Failed:%s\n", errorString());
   }
   sm_dump(false);
   assert_true(successful);
}
#endif
