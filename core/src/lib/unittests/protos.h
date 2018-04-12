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

void test_alist(void **state);
void test_dlist(void **state);
void test_htable(void **state);
void test_rblist(void **state);
void test_edit(void **state);
void test_generate_crypto_passphrase(void **state);
void test_bsnprintf(void **state);
void test_sellist(void **state);
void test_scan(void **state);
void test_base64(void **state);
void test_rwlock(void **state);
void test_devlock(void **state);
#ifdef HAVE_WIN32
void test_junction(void **state);
#endif
