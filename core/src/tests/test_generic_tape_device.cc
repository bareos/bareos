/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "stored/backends/generic_tape_device.h"
#include "stored/stored.h"

namespace storagedaemon {

namespace {

class TestTapeDevice : public generic_tape_device {
 public:
  explicit TestTapeDevice(bool immediate_supported)
      : immediate_supported_(immediate_supported)
  {
    errmsg = GetMemory(256);
    prt_name = GetMemory(32);
    PmStrcpy(errmsg, "");
    PmStrcpy(prt_name, "test-tape");
    fd = 1;
    SetAppend();
  }

  ~TestTapeDevice() override { fd = -1; }

  int d_close(int) override { return 0; }

  int d_ioctl(int, ioctl_req_t request, char* op) override
  {
    EXPECT_EQ(request, static_cast<ioctl_req_t>(MTIOCTOP));
    auto* mt_com = reinterpret_cast<mtop*>(op);
    operations.push_back(mt_com->mt_op);
#if defined(MTWEOFI)
    if (mt_com->mt_op == MTWEOFI && !immediate_supported_) {
      errno = ENOTTY;
      return -1;
    }
#endif
    return 0;
  }

  std::vector<short> operations;

 private:
  bool immediate_supported_;
};

}  // namespace

TEST(GenericTapeDevice, weof_uses_immediate_ioctl_or_falls_back)
{
  TestTapeDevice dev{/*immediate_supported=*/false};

  ASSERT_TRUE(dev.weof(1));

#if defined(MTWEOFI)
  ASSERT_EQ(dev.operations.size(), 2U);
  EXPECT_EQ(dev.operations[0], MTWEOFI);
  EXPECT_EQ(dev.operations[1], MTWEOF);
#else
  ASSERT_EQ(dev.operations.size(), 1U);
  EXPECT_EQ(dev.operations[0], MTWEOF);
#endif
  EXPECT_EQ(dev.GetFile(), 1U);
  EXPECT_EQ(dev.GetBlockNum(), 0U);
}

TEST(GenericTapeDevice, weof_uses_immediate_ioctl_when_supported)
{
  TestTapeDevice dev{/*immediate_supported=*/true};

  ASSERT_TRUE(dev.weof(2));

#if defined(MTWEOFI)
  ASSERT_EQ(dev.operations.size(), 1U);
  EXPECT_EQ(dev.operations[0], MTWEOFI);
#else
  ASSERT_EQ(dev.operations.size(), 1U);
  EXPECT_EQ(dev.operations[0], MTWEOF);
#endif
  EXPECT_EQ(dev.GetFile(), 2U);
  EXPECT_EQ(dev.GetBlockNum(), 0U);
}

TEST(GenericTapeDevice, tape_flush_issues_mtnop)
{
  TestTapeDevice dev{/*immediate_supported=*/true};

  ASSERT_TRUE(dev.d_flush(nullptr));
  ASSERT_EQ(dev.operations.size(), 1U);
  EXPECT_EQ(dev.operations[0], MTNOP);
}

}  // namespace storagedaemon
