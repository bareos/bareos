/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2024 Bareos GmbH & Co. KG

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
#if defined HAVE_WIN32
#  include "compat.h"
#endif
#include "gtest/gtest.h"
#include "lib/mem_pool.h"
#include "include/baconfig.h"

TEST(poolmem, alloc)
{
  POOLMEM* pm0 = GetPoolMemory(PM_NOPOOL);
  EXPECT_NE(pm0, nullptr);
  EXPECT_EQ(SizeofPoolMemory(pm0), 256);

  POOLMEM* pm1 = GetPoolMemory(PM_NAME);
  EXPECT_NE(pm1, nullptr);
  EXPECT_NE(pm1, pm0);
  EXPECT_EQ(SizeofPoolMemory(pm1), MAX_NAME_LENGTH + 2);

  POOLMEM* pm2 = GetPoolMemory(PM_NOPOOL);
  EXPECT_NE(pm2, nullptr);
  EXPECT_NE(pm2, pm0);
  EXPECT_NE(pm2, pm1);
  EXPECT_EQ(SizeofPoolMemory(pm2), 256);

  POOLMEM* pm3 = GetPoolMemory(PM_MESSAGE);
  EXPECT_NE(pm3, nullptr);
  EXPECT_NE(pm3, pm0);
  EXPECT_NE(pm3, pm1);
  EXPECT_NE(pm3, pm2);
  EXPECT_EQ(SizeofPoolMemory(pm3), 512);

  POOLMEM* pm4 = GetPoolMemory(PM_EMSG);
  EXPECT_NE(pm4, nullptr);
  EXPECT_NE(pm4, pm0);
  EXPECT_NE(pm4, pm1);
  EXPECT_NE(pm4, pm2);
  EXPECT_NE(pm4, pm3);
  EXPECT_EQ(SizeofPoolMemory(pm4), 1024);

  POOLMEM* pm5 = GetPoolMemory(PM_BSOCK);
  EXPECT_NE(pm5, nullptr);
  EXPECT_NE(pm5, pm0);
  EXPECT_NE(pm5, pm1);
  EXPECT_NE(pm5, pm2);
  EXPECT_NE(pm5, pm3);
  EXPECT_NE(pm5, pm4);
  EXPECT_EQ(SizeofPoolMemory(pm5), 4096);

  POOLMEM* pm6 = GetPoolMemory(PM_RECORD);
  EXPECT_NE(pm6, nullptr);
  EXPECT_NE(pm6, pm0);
  EXPECT_NE(pm6, pm1);
  EXPECT_NE(pm6, pm2);
  EXPECT_NE(pm6, pm3);
  EXPECT_NE(pm6, pm4);
  EXPECT_NE(pm6, pm5);
  EXPECT_EQ(SizeofPoolMemory(pm6), 128);

  FreePoolMemory(pm0);
  FreePoolMemory(pm1);
  FreePoolMemory(pm2);
  FreePoolMemory(pm3);
  FreePoolMemory(pm4);
  FreePoolMemory(pm5);
  FreePoolMemory(pm6);
}

TEST(poolmem, realloc)
{
  POOLMEM* pm = GetMemory(10);
  EXPECT_NE(pm, nullptr);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);

  POOLMEM* realloc = ReallocPoolMemory(pm, 20);
  EXPECT_NE(realloc, nullptr);
  EXPECT_EQ(SizeofPoolMemory(realloc), 20);

  FreeMemory(realloc);
}

TEST(poolmem, checkmemorysize)
{
  POOLMEM* pm = GetMemory(10);
  EXPECT_NE(pm, nullptr);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);

  POOLMEM* checked = CheckPoolMemorySize(pm, 10);
  EXPECT_EQ(pm, checked);
  EXPECT_EQ(SizeofPoolMemory(checked), 10);

  POOLMEM* realloc = CheckPoolMemorySize(pm, 20);
  EXPECT_NE(realloc, nullptr);
  EXPECT_EQ(SizeofPoolMemory(realloc), 20);

  FreeMemory(realloc);
}

TEST(poolmem, pmstrcat1)
{
  // POOLMEM*&, const char*
  POOLMEM* pm = GetMemory(10);
  POOLMEM* pm_ = pm;  // alias, as PmStrcat may change pm
  pm[0] = '\0';
  EXPECT_STREQ(pm, "");

  EXPECT_EQ(PmStrcat(pm, nullptr), 0);
  EXPECT_EQ(pm, pm_);
  EXPECT_STREQ(pm, "");

  EXPECT_EQ(PmStrcat(pm, ""), 0);
  EXPECT_EQ(pm, pm_);
  EXPECT_STREQ(pm, "");

  EXPECT_EQ(PmStrcat(pm, "12345"), 5);
  EXPECT_EQ(pm, pm_);
  EXPECT_STREQ(pm, "12345");

  EXPECT_EQ(PmStrcat(pm, "67890"), 10);
  EXPECT_STREQ(pm, "1234567890");
  EXPECT_EQ(SizeofPoolMemory(pm), 11);

  EXPECT_EQ(PmStrcat(pm, "abc"), 13);
  EXPECT_STREQ(pm, "1234567890abc");
  EXPECT_EQ(SizeofPoolMemory(pm), 14);

  FreeMemory(pm);
}


TEST(poolmem, pmstrcat2)
{
  // POOLMEM*&, PoolMem&
  POOLMEM* pm = GetMemory(10);
  POOLMEM* pm_ = pm;  // alias, as PmStrcat may change pm
  pm[0] = '\0';
  EXPECT_STREQ(pm, "");
  pm[0] = '\0';
  {
    PoolMem pm_nullstr{""};
    EXPECT_EQ(PmStrcat(pm, pm_nullstr), 0);
    EXPECT_EQ(pm, pm_);
    EXPECT_STREQ(pm, "");

    PoolMem pm_12345{"12345"};
    EXPECT_EQ(PmStrcat(pm, pm_12345), 5);
    EXPECT_EQ(pm, pm_);
    EXPECT_STREQ(pm, "12345");

    PoolMem pm_67890{"67890"};
    EXPECT_EQ(PmStrcat(pm, pm_67890), 10);
    EXPECT_STREQ(pm, "1234567890");
    EXPECT_EQ(SizeofPoolMemory(pm), 11);

    PoolMem pm_abc{"abc"};
    EXPECT_EQ(PmStrcat(pm, pm_abc), 13);
    EXPECT_STREQ(pm, "1234567890abc");
    EXPECT_EQ(SizeofPoolMemory(pm), 14);
  }
  FreeMemory(pm);
}

TEST(poolmem, pmstrcat3)
{
  // PoolMem& pm, const char* str

  PoolMem pm{""};
  auto pm_addr = pm.addr();

  EXPECT_STREQ(pm.c_str(), "");
  EXPECT_EQ(pm.size(), 130);

  EXPECT_EQ(PmStrcat(pm, nullptr), 0);
  EXPECT_EQ(pm.addr(), pm_addr);
  EXPECT_STREQ(pm.c_str(), "");

  EXPECT_EQ(PmStrcat(pm, ""), 0);
  EXPECT_EQ(pm.addr(), pm_addr);
  EXPECT_STREQ(pm.c_str(), "");

  EXPECT_EQ(PmStrcat(pm, "12345"), 5);
  EXPECT_EQ(pm.addr(), pm_addr);
  EXPECT_STREQ(pm.c_str(), "12345");

  EXPECT_EQ(PmStrcat(pm, "67890"), 10);
  EXPECT_STREQ(pm.c_str(), "1234567890");

  for (auto i = 0; i < 13; i++) { PmStrcat(pm, "1234567890"); }
  EXPECT_EQ(pm.size(), 141);
}

TEST(poolmem, pmstrcat4)
{
  // PoolMem*& pm, const char* str

  PoolMem pm{""};
  PoolMem* pm_ptr = &pm;
  auto pm_addr = pm.addr();

  EXPECT_STREQ(pm.c_str(), "");
  EXPECT_EQ(pm.size(), 130);

  EXPECT_EQ(PmStrcat(pm_ptr, nullptr), 0);
  EXPECT_EQ(pm.addr(), pm_addr);
  EXPECT_STREQ(pm.c_str(), "");

  EXPECT_EQ(PmStrcat(pm_ptr, ""), 0);
  EXPECT_EQ(pm.addr(), pm_addr);
  EXPECT_STREQ(pm.c_str(), "");

  EXPECT_EQ(PmStrcat(pm_ptr, "12345"), 5);
  EXPECT_EQ(pm.addr(), pm_addr);
  EXPECT_STREQ(pm.c_str(), "12345");

  EXPECT_EQ(PmStrcat(pm_ptr, "67890"), 10);
  EXPECT_STREQ(pm.c_str(), "1234567890");

  for (auto i = 0; i < 13; i++) { PmStrcat(pm_ptr, "1234567890"); }
  EXPECT_EQ(pm.size(), 141);
}

TEST(poolmem, pmstrcpy1)
{
  // POOLMEM*&, const char*
  POOLMEM* pm = GetMemory(10);

  EXPECT_EQ(PmStrcpy(pm, nullptr), 0);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);
  EXPECT_STREQ(pm, "");

  EXPECT_EQ(PmStrcpy(pm, ""), 0);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);
  EXPECT_STREQ(pm, "");


  EXPECT_EQ(PmStrcpy(pm, "12345"), 5);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);
  EXPECT_STREQ(pm, "12345");

  EXPECT_EQ(PmStrcpy(pm, "1234567890"), 10);
  EXPECT_EQ(SizeofPoolMemory(pm), 11);
  EXPECT_STREQ(pm, "1234567890");

  FreeMemory(pm);
}

TEST(poolmem, pmstrcpy2)
{
  // POOLMEM*&, PoolMem&
  POOLMEM* pm = GetMemory(10);

  PoolMem pm_nullstr{""};
  EXPECT_EQ(PmStrcpy(pm, pm_nullstr), 0);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);
  EXPECT_STREQ(pm, "");

  PoolMem pm_12345{"12345"};
  EXPECT_EQ(PmStrcpy(pm, pm_12345), 5);
  EXPECT_EQ(SizeofPoolMemory(pm), 10);
  EXPECT_STREQ(pm, "12345");

  PoolMem pm_1234567890{"1234567890"};
  EXPECT_EQ(PmStrcpy(pm, pm_1234567890), 10);
  EXPECT_EQ(SizeofPoolMemory(pm), 11);
  EXPECT_STREQ(pm, "1234567890");

  FreeMemory(pm);
}

TEST(poolmem, pmstrcpy3)
{
  // PoolMem&, const char*

  PoolMem pm{"sometext"};

  EXPECT_EQ(PmStrcpy(pm, nullptr), 0);
  EXPECT_STREQ(pm.c_str(), "");

  EXPECT_EQ(PmStrcpy(pm, ""), 0);
  EXPECT_STREQ(pm.c_str(), "");


  EXPECT_EQ(PmStrcpy(pm, "12345"), 5);
  EXPECT_STREQ(pm.c_str(), "12345");

  EXPECT_EQ(PmStrcpy(pm, "1234567890"), 10);
  EXPECT_STREQ(pm.c_str(), "1234567890");
}

TEST(poolmem, pmstrcpy4)
{
  // PoolMem*&, const char*

  PoolMem pm{"sometext"};
  PoolMem* pm_ptr = &pm;

  EXPECT_EQ(PmStrcpy(pm_ptr, nullptr), 0);
  EXPECT_STREQ(pm.c_str(), "");

  EXPECT_EQ(PmStrcpy(pm_ptr, ""), 0);
  EXPECT_STREQ(pm.c_str(), "");


  EXPECT_EQ(PmStrcpy(pm_ptr, "12345"), 5);
  EXPECT_STREQ(pm.c_str(), "12345");

  EXPECT_EQ(PmStrcpy(pm_ptr, "1234567890"), 10);
  EXPECT_STREQ(pm.c_str(), "1234567890");
}

TEST(poolmem, pmmemcpy1)
{
  // POOLMEM*&, const char*, int32_t
  POOLMEM* pm = GetMemory(10);

  pm[5] = 'x';
  EXPECT_EQ(PmMemcpy(pm, "1234567890", 5), 5);
  EXPECT_EQ(pm[5], 'x');
  pm[5] = '\0';
  EXPECT_STREQ(pm, "12345");

  FreeMemory(pm);
}

TEST(poolmem, pmmemcpy2)
{
  // POOLMEM*&, PoolMem&, int32_t
  POOLMEM* pm = GetMemory(10);
  PoolMem pm_1234567890{"1234567890"};

  pm[5] = 'x';
  EXPECT_EQ(PmMemcpy(pm, pm_1234567890, 5), 5);
  EXPECT_EQ(pm[5], 'x');
  pm[5] = '\0';
  EXPECT_STREQ(pm, "12345");

  FreeMemory(pm);
}

TEST(poolmem, pmmemcpy3)
{
  // PoolMem&, const char*, int32_t
  PoolMem pm{""};
  char* pm_str = pm.c_str();

  pm_str[5] = 'x';
  EXPECT_EQ(PmMemcpy(pm, "1234567890", 5), 5);
  EXPECT_EQ(pm_str[5], 'x');
  pm_str[5] = '\0';

  EXPECT_STREQ(pm.c_str(), "12345");
}

TEST(poolmem, pmmemcpy4)
{
  // PoolMem*&, const char*, int32_t
  PoolMem pm{""};
  PoolMem* pm_ptr = &pm;
  char* pm_str = pm.c_str();

  pm_str[5] = 'x';
  EXPECT_EQ(PmMemcpy(pm_ptr, "1234567890", 5), 5);
  EXPECT_EQ(pm_str[5], 'x');
  pm_str[5] = '\0';

  EXPECT_STREQ(pm.c_str(), "12345");
}

TEST(poolmem, bad_realloc)
{
  POOLMEM* mem = GetMemory(20);

  POOLMEM* result = ReallocPoolMemory(mem, -1);

  ASSERT_NE(result, nullptr);

  ASSERT_GE(SizeofPoolMemory(result), 0);

  FreeMemory(result);
}

TEST(poolmem, format_no_memory)
{
  POOLMEM* mem = GetMemory(0);

  EXPECT_EQ(PmFormat(mem, "%s", "Hello"), 5);
  EXPECT_STREQ(mem, "Hello");
  EXPECT_GT(SizeofPoolMemory(mem), 5);

  FreeMemory(mem);
}

TEST(poolmem, format)
{
  POOLMEM* string = GetMemory(0);

  EXPECT_EQ(PmFormat(string, "%s %s", "xyz", "123"), 7);
  EXPECT_EQ(strlen(string), 7);
  EXPECT_STREQ(string, "xyz 123");
  EXPECT_GT(SizeofPoolMemory(string), 7);

  EXPECT_EQ(PmFormat(string, "%150d", 150), 150);
  EXPECT_EQ(strlen(string), 150);
  EXPECT_GT(SizeofPoolMemory(string), 150);

  FreeMemory(string);
}

TEST(PoolMem, constructor)
{
  PoolMem pm_name;
  EXPECT_EQ(pm_name.MaxSize(), 130);
  EXPECT_STREQ(pm_name.c_str(), "");
  EXPECT_EQ(pm_name.strlen(), 0);

  PoolMem pm_pool{PM_BSOCK};
  EXPECT_EQ(pm_pool.MaxSize(), 4096);
  EXPECT_STREQ(pm_pool.c_str(), "");
  EXPECT_EQ(pm_pool.strlen(), 0);

  PoolMem pm_string{"abc"};
  EXPECT_EQ(pm_string.MaxSize(), 130);
  EXPECT_STREQ(pm_string.c_str(), "abc");
  EXPECT_EQ(pm_string.strlen(), 3);
}

TEST(PoolMem, realloc)
{
  PoolMem pm_name;
  PoolMem pm_pool{PM_BSOCK};

  pm_name.ReallocPm(150);
  EXPECT_EQ(pm_name.MaxSize(), 150);

  pm_pool.ReallocPm(150);
  EXPECT_EQ(pm_pool.MaxSize(), 150);
}

TEST(PoolMem, strcat)
{
  PoolMem pm_name;
  PoolMem pm_string{"abc"};

  EXPECT_EQ(pm_name.strcat(pm_string), 3);
  EXPECT_EQ(pm_name.strlen(), 3);
  EXPECT_STREQ(pm_name.c_str(), "abc");

  EXPECT_EQ(pm_name.strcat("DEF"), 6);
  EXPECT_EQ(pm_name.strlen(), 6);
  EXPECT_STREQ(pm_name.c_str(), "abcDEF");

  EXPECT_EQ(pm_name.strcat(nullptr), 6);
  EXPECT_EQ(pm_name.strlen(), 6);
  EXPECT_STREQ(pm_name.c_str(), "abcDEF");
}

TEST(PoolMem, tolower)
{
  PoolMem pm_name{"AbCDeF"};
  pm_name.toLower();
  EXPECT_EQ(pm_name.strlen(), 6);
  EXPECT_STREQ(pm_name.c_str(), "abcdef");
}

TEST(PoolMem, strcpy)
{
  PoolMem pm_name{"6chars"};
  PoolMem pm_pool{PM_BSOCK};
  EXPECT_EQ(pm_pool.strcpy(pm_name), 6);
  EXPECT_EQ(pm_pool.strlen(), 6);
  EXPECT_STREQ(pm_pool.c_str(), pm_name.c_str());

  PoolMem pm_string;
  EXPECT_EQ(pm_string.strcpy(nullptr), 0);
  EXPECT_EQ(pm_string.strlen(), 0);
  EXPECT_STREQ(pm_string.c_str(), "");
}

TEST(PoolMem, format)
{
  PoolMem pm_string;
  EXPECT_EQ(pm_string.bsprintf("%s %s", "xyz", "123"), 7);
  EXPECT_EQ(pm_string.strlen(), 7);
  EXPECT_STREQ(pm_string.c_str(), "xyz 123");

  EXPECT_EQ(pm_string.bsprintf("%150d", 0), 150);
  EXPECT_EQ(pm_string.strlen(), 150);
  EXPECT_GT(pm_string.MaxSize(), 150);
}
