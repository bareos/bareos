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

#include <vector>

namespace storagedaemon {
namespace {

class WeofiTapeDevice : public generic_tape_device {
 public:
  WeofiTapeDevice()
  {
    errmsg = GetMemory(256);
    prt_name = GetMemory(32);
    PmStrcpy(errmsg, "");
    PmStrcpy(prt_name, "test-tape");
    fd = 1;
    SetAppend();
    SetCap(CAP_EOF);
    SetCap(CAP_WEOFI);
  }

  ~WeofiTapeDevice() override { fd = -1; }

  int d_close(int) override { return 0; }

  int d_ioctl(int, ioctl_req_t request, char* op) override
  {
    EXPECT_EQ(request, static_cast<ioctl_req_t>(MTIOCTOP));
    auto* mt_com = reinterpret_cast<mtop*>(op);
    operations.push_back(mt_com->mt_op);
    int error = mt_com->mt_op == MTWEOFI ? immediate_error : normal_error;
    if (error) {
      errno = error;
      return -1;
    }
    return 0;
  }

  void clrerror(int op) override { last_error_operation = op; }

  int immediate_error = 0;
  int normal_error = 0;
  int last_error_operation = 0;
  std::vector<short> operations;
};

// An enabled WEOFI capability writes an immediate filemark.
TEST(TapeWeofi, ImmediateFilemarksByDefault)
{
  WeofiTapeDevice dev;
  ASSERT_TRUE(dev.weof(2));
  EXPECT_EQ(dev.operations, std::vector<short>{MTWEOFI});
  EXPECT_TRUE(dev.HasCap(CAP_WEOFI));
  EXPECT_EQ(dev.GetFile(), 2U);
}

// Disabling WEOFI selects a regular filemark without trying the immediate one.
TEST(TapeWeofi, DisabledCapabilitySkipsImmediateFilemarks)
{
  WeofiTapeDevice dev;
  dev.ClearCap(CAP_WEOFI);
  ASSERT_TRUE(dev.weof(1));
  EXPECT_EQ(dev.operations, std::vector<short>{MTWEOF});
  EXPECT_EQ(dev.GetFile(), 1U);
}

// Unsupported immediate filemarks fall back and are skipped on later writes.
TEST(TapeWeofi, UnsupportedImmediateFilemarksDisableCapability)
{
  for (int error : {ENOTTY, ENOSYS}) {
    WeofiTapeDevice dev;
    dev.immediate_error = error;
    ASSERT_TRUE(dev.weof(1));
    EXPECT_EQ(dev.operations, (std::vector<short>{MTWEOFI, MTWEOF}));
    EXPECT_FALSE(dev.HasCap(CAP_WEOFI));
    EXPECT_TRUE(dev.HasCap(CAP_EOF));
    ASSERT_TRUE(dev.weof(1));
    EXPECT_EQ(dev.operations, (std::vector<short>{MTWEOFI, MTWEOF, MTWEOF}));
    EXPECT_EQ(dev.GetFile(), 2U);
  }
}

// Other immediate-filemark errors fall back without disabling WEOFI.
TEST(TapeWeofi, OtherImmediateErrorsStillRetryNextTime)
{
  WeofiTapeDevice dev;
  dev.immediate_error = EIO;
  ASSERT_TRUE(dev.weof(1));
  EXPECT_TRUE(dev.HasCap(CAP_WEOFI));
  ASSERT_TRUE(dev.weof(1));
  EXPECT_EQ(dev.operations,
            (std::vector<short>{MTWEOFI, MTWEOF, MTWEOFI, MTWEOF}));
}

// A failed regular fallback reports failure without advancing the file.
TEST(TapeWeofi, FailingFallbackReportsNormalFilemarkError)
{
  WeofiTapeDevice dev;
  dev.immediate_error = ENOTTY;
  dev.normal_error = EIO;
  EXPECT_FALSE(dev.weof(1));
  EXPECT_EQ(dev.operations, (std::vector<short>{MTWEOFI, MTWEOF}));
  EXPECT_EQ(dev.last_error_operation, MTWEOF);
  EXPECT_EQ(dev.GetFile(), 0U);
  EXPECT_FALSE(dev.HasCap(CAP_WEOFI));
  EXPECT_TRUE(dev.HasCap(CAP_EOF));
}

// A failed regular filemark is not retried when WEOFI is disabled.
TEST(TapeWeofi, DisabledImmediateCapabilityDoesNotRetryNormalFailure)
{
  WeofiTapeDevice dev;
  dev.ClearCap(CAP_WEOFI);
  dev.normal_error = EIO;
  EXPECT_FALSE(dev.weof(1));
  EXPECT_EQ(dev.operations, std::vector<short>{MTWEOF});
  EXPECT_EQ(dev.last_error_operation, MTWEOF);
  EXPECT_EQ(dev.GetFile(), 0U);
}

}  // namespace
}  // namespace storagedaemon
