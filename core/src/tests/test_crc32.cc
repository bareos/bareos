/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "stored/crc32/Crc32.h"

const size_t len = 63 * 1024;

TEST(crc32, internal)
{
  static const char* buf = "The quick brown fox jumps over the lazy dog";
  EXPECT_EQ(0x414fa339, crc32_fast((uint8_t*)buf, strlen(buf)));
}

TEST(crc32, internal_spd)
{
  void* buf = malloc(len);
  int fd = open("/dev/urandom", O_RDONLY);
  read(fd, buf, len);
  close(fd);
  for(int i=0;i<10000;i++) {
    crc32_fast((uint8_t*)buf, len);
  }
  free(buf);
}
