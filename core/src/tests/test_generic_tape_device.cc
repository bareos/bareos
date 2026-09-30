/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "stored/backends/generic_tape_device.h"
#include "stored/stored.h"

namespace storagedaemon {

namespace {

class TestTapeDevice : public generic_tape_device {
 public:
  explicit TestTapeDevice(int write_error) : write_error_(write_error)
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
    if (write_error_) {
      errno = write_error_;
      return -1;
    }
    return 0;
  }

  std::vector<short> operations;

 private:
  int write_error_;
};

}  // namespace

TEST(GenericTapeDevice, weof_reports_failed_filemark_without_retry)
{
  for (int write_error : {EIO, ENOTTY}) {
    TestTapeDevice dev{write_error};

    EXPECT_FALSE(dev.weof(1));

    ASSERT_EQ(dev.operations.size(), 1U);
    EXPECT_EQ(dev.operations[0], MTWEOFI);
    EXPECT_EQ(dev.GetFile(), 0U);
    EXPECT_EQ(dev.GetBlockNum(), 0U);
  }
}

TEST(GenericTapeDevice, weof_writes_immediate_filemark)
{
  TestTapeDevice dev{/*write_error=*/0};

  ASSERT_TRUE(dev.weof(2));

  ASSERT_EQ(dev.operations.size(), 1U);
  EXPECT_EQ(dev.operations[0], MTWEOFI);
  EXPECT_EQ(dev.GetFile(), 2U);
  EXPECT_EQ(dev.GetBlockNum(), 0U);
}

}  // namespace storagedaemon
