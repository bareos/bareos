/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2026 Bareos GmbH & Co. KG

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
#include "gtest/gtest.h"
#include "include/bareos.h"

#include "lib/htable.h"

struct HTABLEJCR {
#ifndef TEST_NON_CHAR
  char* key;
#else
  uint32_t key;
#endif
  hlink link;
};

#ifndef TEST_SMALL_HTABLE
#  define NITEMS 5000000
#else
#  define NITEMS 5000
#endif
TEST(htable, htable)
{
#ifdef TEST_SMALL_HTABLE
  using JcrTable = htable<decltype(HTABLEJCR::key), HTABLEJCR>;
#else
  using JcrTable = htable<decltype(HTABLEJCR::key), HTABLEJCR,
                          MonotonicBuffer::Size::Medium>;
#endif
  char mkey[30];
  JcrTable* jcrtbl;
  HTABLEJCR *save_jcr = NULL, *item;
  HTABLEJCR* jcr = NULL;
  int count = 0;

  jcrtbl = new JcrTable(NITEMS);
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
  foreach_htable (jcr, jcrtbl) { count++; }

  delete jcrtbl;
  EXPECT_EQ(count, NITEMS);
}

struct RbListJobControlRecord {
  char* buf;
};

TEST(htable_impl, Growth)
{
  struct value {
    /* we chose a structure that does not have link as its first member
     * so that the offset is not 0.  This is done so we can test that
     * growth does _not_ accidentially reset the offset to 0. */
    char x{};
    hlink link{};
  };


  htableImpl impl{offsetof(value, link)};

  std::vector<value> pre_growth_links;
  pre_growth_links.resize(impl.capacity());

  for (uint64_t i = 0; i < pre_growth_links.size(); ++i) {
    ASSERT_TRUE(impl.insert(i, &pre_growth_links[i]));
    ASSERT_EQ(impl.capacity(), pre_growth_links.size())
        << "Growth at insert " << i + 1 << "/" << pre_growth_links.size();
  }

  value threshold;
  ASSERT_TRUE(impl.insert(pre_growth_links.size(), &threshold));


  EXPECT_GT(impl.capacity(), pre_growth_links.size());

  // we want to make sure that inserting still works after the growth
  value next;
  EXPECT_TRUE(impl.insert(pre_growth_links.size() + 1, &next));
}
