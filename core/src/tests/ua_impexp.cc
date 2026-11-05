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

#define get_storage_resource TestGetStorage
#define SetWstorage TestSetStorage
#define GetNumSlots TestGetNumSlots
#define get_vol_list_from_storage TestGetInventory
#define transfer_volume TestTransfer
#define StorageReleaseVolList TestReleaseInventory
#define CloseSdBsock TestCloseStorage
#define UpdateSlotsFromVolList TestUpdateSlots
#define UpdateInchangerForExport TestUpdateExport
#include "dird/ua_impexp.cc"
#undef get_storage_resource
#undef SetWstorage
#undef GetNumSlots
#undef get_vol_list_from_storage
#undef transfer_volume
#undef StorageReleaseVolList
#undef CloseSdBsock
#undef UpdateSlotsFromVolList
#undef UpdateInchangerForExport

#include "gtest/gtest.h"
#include "lib/output_formatter.h"
#include "lib/parse_conf.h"

namespace directordaemon {
namespace {
StorageResource storage;
changer_vol_list_t inventory;
std::vector<std::pair<slot_number_t, slot_number_t>> transfers;
int updated_slots;

bool CaptureOutput(void* ctx, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  PoolMem message;
  message.Bvsprintf(fmt, args);
  va_end(args);
  *static_cast<std::string*>(ctx) += message.c_str();
  return true;
}
}  // namespace

StorageResource* TestGetStorage(UaContext*, bool, bool) { return &storage; }
void TestSetStorage(JobControlRecord*, UnifiedStorageResource*) {}
slot_number_t TestGetNumSlots(UaContext*, StorageResource*) { return 4; }
changer_vol_list_t* TestGetInventory(UaContext*,
                                     StorageResource*,
                                     bool,
                                     bool,
                                     bool)
{
  return &inventory;
}
bool TestTransfer(UaContext*,
                  StorageResource*,
                  slot_number_t src,
                  slot_number_t dst)
{
  transfers.emplace_back(src, dst);
  return true;
}
void TestReleaseInventory(StorageResource*, changer_vol_list_t*) {}
void TestCloseStorage(UaContext*) {}
void TestUpdateSlots(UaContext*,
                     StorageResource*,
                     changer_vol_list_t*,
                     char* slots)
{
  updated_slots = count_enabled_slots(slots, 4);
}
void TestUpdateExport(UaContext*, StorageResource*, changer_vol_list_t*, char*)
{
}

class ImportExportSelection : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override
  {
    inventory.contents = new dlist<vol_list_t>;
    for (slot_number_t slot = 1; slot <= 4; ++slot) {
      auto* entry = static_cast<vol_list_t*>(calloc(1, sizeof(vol_list_t)));
      entry->bareos_slot_number = slot;
      entry->slot_type = (slot <= 2) != GetParam()
                             ? slot_type_t::kSlotTypeImport
                             : slot_type_t::kSlotTypeStorage;
      entry->slot_status = slot <= 2 ? slot_status_t::kSlotStatusFull
                                     : slot_status_t::kSlotStatusEmpty;
      if (slot <= 2) { entry->VolName = strdup(slot == 1 ? "A" : "B"); }
      inventory.contents->append(entry);
    }
    transfers.clear();
    updated_slots = 0;
  }

  void TearDown() override
  {
    vol_list_t* entry;
    foreach_dlist (entry, inventory.contents) { free(entry->VolName); }
    delete inventory.contents;
    inventory.contents = nullptr;
  }

  bool Run(const char* args)
  {
    JobControlRecord jcr;
    UaContext ua(&jcr);
    ua.send = std::make_unique<OutputFormatter>(CaptureOutput, &output, nullptr,
                                                nullptr);
    PmStrcpy(ua.cmd, args);
    ParseArgs(ua.cmd, ua.args, &ua.argc, ua.argk, ua.argv, MAX_CMD_ARGS);
    return GetParam() ? ExportCmd(&ua, "") : ImportCmd(&ua, "");
  }

  std::string output;
};

TEST_P(ImportExportSelection, DuplicateNamesFitOneDestination)
{
  ASSERT_TRUE(Run("move volume=A|A dstslots=3"));
  EXPECT_EQ(transfers,
            (std::vector<std::pair<slot_number_t, slot_number_t>>{{1, 3}}));
  EXPECT_EQ(updated_slots, 2);
  EXPECT_TRUE(output.empty()) << output;
}

TEST_P(ImportExportSelection, RepeatedArgumentsFitOneDestination)
{
  ASSERT_TRUE(Run("move volume=A volume=A dstslots=3"));
  ASSERT_EQ(transfers.size(), 1);
  EXPECT_TRUE(output.empty()) << output;
}

TEST_P(ImportExportSelection, DistinctNamesNeedTwoDestinations)
{
  EXPECT_FALSE(Run("move volume=A|B dstslots=3"));
  EXPECT_TRUE(transfers.empty());
  EXPECT_NE(output.find("doesn't fit"), std::string::npos) << output;
}

TEST_P(ImportExportSelection, DistinctNamesMoveOnceEach)
{
  ASSERT_TRUE(Run("move volume=A|B dstslots=3-4"));
  EXPECT_EQ(transfers, (std::vector<std::pair<slot_number_t, slot_number_t>>{
                           {1, 3}, {2, 4}}));
  EXPECT_EQ(updated_slots, 4);
  EXPECT_TRUE(output.empty()) << output;
}

TEST_P(ImportExportSelection, RestrictionsAndMissingNamesKeepWarnings)
{
  ASSERT_TRUE(Run("move volume=A|A|B|Missing srcslots=1 dstslots=3"));
  ASSERT_EQ(transfers.size(), 1);
  EXPECT_EQ(transfers.front().first, 1);
  EXPECT_EQ(output.find("No volume named A"), std::string::npos) << output;
  EXPECT_NE(output.find("No volume named B"), std::string::npos) << output;
  EXPECT_NE(output.find("No volume named Missing"), std::string::npos)
      << output;
}

INSTANTIATE_TEST_SUITE_P(ImportAndExport,
                         ImportExportSelection,
                         testing::Bool());
}  // namespace directordaemon
