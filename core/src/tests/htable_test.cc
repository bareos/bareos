/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2019 Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, July MMIII */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */

#define _S_IFDIR S_IFDIR
#define _stat stat
#include "minwindef.h"

#include "include/bareos.h"
#include "lib/htable.h"
#include "gtest/gtest.h"

struct HTABLEJCR {
#ifndef TEST_NON_CHAR
  char* key;
#else
  uint32_t key;
#endif
  hlink link;
};

#ifndef TEST_SMALL_HTABLE
#define NITEMS 5000000
#else
#define NITEMS 5000
#endif
TEST(htable, htable)
{
  char mkey[30];
  htable* jcrtbl;
  HTABLEJCR *save_jcr = NULL, *item;
  HTABLEJCR* jcr = NULL;
  int count = 0;

  jcrtbl = (htable*)malloc(sizeof(htable));

#ifndef TEST_SMALL_HTABLE
  jcrtbl->init(jcr, &jcr->link, NITEMS);
#else
  jcrtbl->init(jcr, &jcr->link, NITEMS, 128);
#endif
  for (int i = 0; i < NITEMS; i++) {
#ifndef TEST_NON_CHAR
    int len;
    len = sprintf(mkey, "%d", i) + 1;

    jcr = (HTABLEJCR*)jcrtbl->hash_malloc(sizeof(HTABLEJCR));
    jcr->key = (char*)jcrtbl->hash_malloc(len);
    memcpy(jcr->key, mkey, len);
#else
    jcr = (HTABLEJCR*)jcrtbl->hash_malloc(sizeof(HTABLEJCR));
    jcr->key = i;
#endif

    jcrtbl->insert(jcr->key, jcr);
    if (i == 10) { save_jcr = jcr; }
  }
  EXPECT_TRUE(item = (HTABLEJCR*)jcrtbl->lookup(save_jcr->key));
  foreach_htable (jcr, jcrtbl) {
    count++;
  }

  jcrtbl->destroy();
  EXPECT_EQ(count, NITEMS);
  free(jcrtbl);
}

struct RbListJobControlRecord {
  char* buf;
};

