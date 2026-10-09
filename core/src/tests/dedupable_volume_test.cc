/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2026 Bareos GmbH & Co. KG

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
#include "stored/backends/dedupable/volume.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace dedup;

// Helper function to test SafeCast (copied from volume.cc implementation)
std::uint32_t SafeCast(std::size_t size)
{
  constexpr std::size_t max = std::numeric_limits<std::uint32_t>::max();
  if (size > max) {
    throw std::invalid_argument(std::to_string(size)
                                + " is bigger than allowed ("
                                + std::to_string(max) + ").");
  }

  return size;
}

// Tests for SafeCast function
TEST(SafeCastTest, ZeroValue) { EXPECT_EQ(SafeCast(0), 0u); }

TEST(SafeCastTest, SmallValue) { EXPECT_EQ(SafeCast(100), 100u); }

TEST(SafeCastTest, MaxUint32Value)
{
  std::size_t max_uint32 = std::numeric_limits<std::uint32_t>::max();
  EXPECT_EQ(SafeCast(max_uint32), max_uint32);
}

TEST(SafeCastTest, ValueAboveMax)
{
  ASSERT_GT(sizeof(std::size_t), sizeof(std::uint32_t));
  if (sizeof(std::size_t) > sizeof(std::uint32_t)) {
    std::size_t too_large
        = static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
          + 1;
    EXPECT_THROW(SafeCast(too_large), std::invalid_argument);
  }
}

TEST(SafeCastTest, LargeValidValue)
{
  std::size_t large = std::numeric_limits<std::uint32_t>::max();
  EXPECT_EQ(SafeCast(large), large);
}

TEST(SafeCastTest, TypicalValue)
{
  EXPECT_EQ(SafeCast(1024), 1024u);
  EXPECT_EQ(SafeCast(65536), 65536u);
  EXPECT_EQ(SafeCast(1048576), 1048576u);
}

TEST(ConfigTest, MakeDefaultDifferentBlockSizes)
{
  config conf1 = config::make_default(512);
  EXPECT_EQ(conf1.dfiles[0].BlockSize, 512u);

  config conf2 = config::make_default(8192);
  EXPECT_EQ(conf2.dfiles[0].BlockSize, 8192u);

  config conf3 = config::make_default(65536);
  EXPECT_EQ(conf3.dfiles[0].BlockSize, 65536u);
}

// Tests for config serialization and deserialization
TEST(ConfigSerializationTest, SimpleRoundTrip)
{
  config original = config::make_default(4096);

  // Serialize
  std::vector<char> serialized = config::serialize(original);
  EXPECT_GT(serialized.size(), 0u);

  // Deserialize
  config restored = config::deserialize(serialized.data(), serialized.size());

  // Verify block files
  ASSERT_EQ(restored.bfiles.size(), original.bfiles.size());
  EXPECT_EQ(restored.bfiles[0].relpath, original.bfiles[0].relpath);
  EXPECT_EQ(restored.bfiles[0].Start, original.bfiles[0].Start);
  EXPECT_EQ(restored.bfiles[0].End, original.bfiles[0].End);
  EXPECT_EQ(restored.bfiles[0].Idx, original.bfiles[0].Idx);

  // Verify part files
  ASSERT_EQ(restored.pfiles.size(), original.pfiles.size());
  EXPECT_EQ(restored.pfiles[0].relpath, original.pfiles[0].relpath);
  EXPECT_EQ(restored.pfiles[0].Start, original.pfiles[0].Start);
  EXPECT_EQ(restored.pfiles[0].End, original.pfiles[0].End);
  EXPECT_EQ(restored.pfiles[0].Idx, original.pfiles[0].Idx);

  // Verify data files
  ASSERT_EQ(restored.dfiles.size(), original.dfiles.size());
  for (size_t i = 0; i < original.dfiles.size(); ++i) {
    EXPECT_EQ(restored.dfiles[i].relpath, original.dfiles[i].relpath);
    EXPECT_EQ(restored.dfiles[i].Size, original.dfiles[i].Size);
    EXPECT_EQ(restored.dfiles[i].BlockSize, original.dfiles[i].BlockSize);
    EXPECT_EQ(restored.dfiles[i].Idx, original.dfiles[i].Idx);
    EXPECT_EQ(restored.dfiles[i].ReadOnly, original.dfiles[i].ReadOnly);
  }
}

TEST(ConfigSerializationTest, DifferentBlockSizes)
{
  std::vector<std::uint64_t> blocksizes = {512, 1024, 4096, 8192, 65536};

  for (auto blocksize : blocksizes) {
    config original = config::make_default(blocksize);
    std::vector<char> serialized = config::serialize(original);
    config restored = config::deserialize(serialized.data(), serialized.size());

    EXPECT_EQ(restored.dfiles[0].BlockSize, blocksize)
        << "Failed for blocksize " << blocksize;
  }
}

TEST(ConfigSerializationTest, WithNonZeroValues)
{
  config original;

  original.bfiles.push_back({"blocks", 0, 100, 0});
  original.pfiles.push_back({"parts", 0, 500, 0});
  original.dfiles.push_back({"aligned.data", 4096000, 4096, 0, false});
  original.dfiles.push_back({"unaligned.data", 1024, 1, 1, false});

  std::vector<char> serialized = config::serialize(original);
  config restored = config::deserialize(serialized.data(), serialized.size());

  EXPECT_EQ(restored.bfiles[0].End, 100u);
  EXPECT_EQ(restored.pfiles[0].End, 500u);
  EXPECT_EQ(restored.dfiles[0].Size, 4096000u);
  EXPECT_EQ(restored.dfiles[1].Size, 1024u);
}

TEST(ConfigSerializationTest, ReadOnlyFlag)
{
  config original;

  original.bfiles.push_back({"blocks", 0, 0, 0});
  original.pfiles.push_back({"parts", 0, 0, 0});
  original.dfiles.push_back({"aligned.data", 0, 4096, 0, true});
  original.dfiles.push_back({"unaligned.data", 0, 1, 1, false});

  std::vector<char> serialized = config::serialize(original);
  config restored = config::deserialize(serialized.data(), serialized.size());

  EXPECT_TRUE(restored.dfiles[0].ReadOnly);
  EXPECT_FALSE(restored.dfiles[1].ReadOnly);
}

TEST(ConfigSerializationTest, LongFilenames)
{
  config original;

  original.bfiles.push_back(
      {"very_long_block_filename_for_testing_purposes.dat", 0, 0, 0});
  original.pfiles.push_back(
      {"very_long_part_filename_for_testing_purposes.dat", 0, 0, 0});
  original.dfiles.push_back(
      {"very_long_aligned_data_filename.dat", 0, 4096, 0, false});
  original.dfiles.push_back(
      {"very_long_unaligned_data_filename.dat", 0, 1, 1, false});

  std::vector<char> serialized = config::serialize(original);
  config restored = config::deserialize(serialized.data(), serialized.size());

  EXPECT_EQ(restored.bfiles[0].relpath, original.bfiles[0].relpath);
  EXPECT_EQ(restored.pfiles[0].relpath, original.pfiles[0].relpath);
  EXPECT_EQ(restored.dfiles[0].relpath, original.dfiles[0].relpath);
  EXPECT_EQ(restored.dfiles[1].relpath, original.dfiles[1].relpath);
}

TEST(ConfigSerializationTest, EmptyFilenames)
{
  config original;

  original.bfiles.push_back({"", 0, 0, 0});
  original.pfiles.push_back({"", 0, 0, 0});
  original.dfiles.push_back({"", 0, 4096, 0, false});
  original.dfiles.push_back({"", 0, 1, 1, false});

  std::vector<char> serialized = config::serialize(original);
  config restored = config::deserialize(serialized.data(), serialized.size());

  EXPECT_EQ(restored.bfiles[0].relpath, "");
  EXPECT_EQ(restored.pfiles[0].relpath, "");
  EXPECT_EQ(restored.dfiles[0].relpath, "");
  EXPECT_EQ(restored.dfiles[1].relpath, "");
}

TEST(ConfigSerializationTest, MaximumValues)
{
  config original;

  std::uint64_t max_u64 = std::numeric_limits<std::uint64_t>::max();
  std::uint32_t max_u32 = std::numeric_limits<std::uint32_t>::max();

  original.bfiles.push_back({"blocks", max_u64 - 1, max_u64, max_u32});
  original.pfiles.push_back({"parts", max_u64 - 1, max_u64, max_u32});
  original.dfiles.push_back({"aligned.data", max_u64, max_u64, max_u32, false});
  original.dfiles.push_back({"unaligned.data", max_u64, 1, max_u32 - 1, false});

  std::vector<char> serialized = config::serialize(original);
  config restored = config::deserialize(serialized.data(), serialized.size());

  EXPECT_EQ(restored.bfiles[0].Start, max_u64 - 1);
  EXPECT_EQ(restored.bfiles[0].End, max_u64);
  EXPECT_EQ(restored.bfiles[0].Idx, max_u32);

  EXPECT_EQ(restored.dfiles[0].Size, max_u64);
  EXPECT_EQ(restored.dfiles[0].BlockSize, max_u64);
}

// Negative tests
TEST(ConfigDeserializationTest, EmptyData)
{
  EXPECT_THROW(config::deserialize(nullptr, 0), std::runtime_error);
}

TEST(ConfigDeserializationTest, TooSmallData)
{
  char data[4] = {0, 0, 0, 0};
  EXPECT_THROW(config::deserialize(data, 4), std::runtime_error);
}

// Tests for urid (universal record id)
TEST(UridTest, Equality)
{
  urid id1{1, 2, 3, 4};
  urid id2{1, 2, 3, 4};
  urid id3{1, 2, 3, 5};

  EXPECT_TRUE(id1 == id2);
  EXPECT_FALSE(id1 == id3);
}

TEST(UridTest, Hash)
{
  urid id1{1, 2, 3, 4};
  urid id2{1, 2, 3, 4};
  urid id3{5, 6, 7, 8};

  urid_hash hasher;

  EXPECT_EQ(hasher(id1), hasher(id2));
  EXPECT_NE(hasher(id1), hasher(id3));
}

TEST(UridTest, HashDistribution)
{
  // Test that different urids produce different hashes
  urid_hash hasher;

  std::vector<urid> ids = {
      {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0},
      {0, 0, 0, 1}, {1, 1, 1, 1}, {2, 2, 2, 2},
  };

  std::unordered_map<std::size_t, int> hash_counts;
  for (const auto& id : ids) { hash_counts[hasher(id)]++; }

  // All hashes should be unique for these test cases
  EXPECT_EQ(hash_counts.size(), ids.size());
}

// Tests for block structure
TEST(BlockTest, SizeCalculation)
{
  block b;
  b.BlockSize = 1000;
  b.Count = 10;
  b.Begin = 5;

  EXPECT_EQ(b.BlockSize.load(), 1000u);
  EXPECT_EQ(b.Count.load(), 10u);
  EXPECT_EQ(b.Begin.load(), 5u);
}

// Tests for part structure
TEST(PartTest, BasicFields)
{
  part p;
  p.FileIdx = 1;
  p.Size = 2048;
  p.Begin = 4096;

  EXPECT_EQ(p.FileIdx.load(), 1u);
  EXPECT_EQ(p.Size.load(), 2048u);
  EXPECT_EQ(p.Begin.load(), 4096u);
}

// Tests for save_state
TEST(SaveStateTest, MoveSemantics)
{
  save_state s1;
  s1.block_size = 100;
  s1.part_size = 200;
  s1.data_sizes = {10, 20, 30};

  save_state s2(std::move(s1));
  EXPECT_EQ(s2.block_size, 100u);
  EXPECT_EQ(s2.part_size, 200u);
  ASSERT_EQ(s2.data_sizes.size(), 3u);
  EXPECT_EQ(s2.data_sizes[0], 10u);
  EXPECT_EQ(s2.data_sizes[1], 20u);
  EXPECT_EQ(s2.data_sizes[2], 30u);
}

TEST(SaveStateTest, MoveAssignment)
{
  save_state s1;
  s1.block_size = 100;
  s1.part_size = 200;
  s1.data_sizes = {10, 20, 30};

  save_state s2;
  s2 = std::move(s1);

  EXPECT_EQ(s2.block_size, 100u);
  EXPECT_EQ(s2.part_size, 200u);
  ASSERT_EQ(s2.data_sizes.size(), 3u);
}

namespace {
constexpr std::size_t kDeviceBlockSize = 1024 * 1024;
constexpr std::size_t kAlignedBlockSize = 128 * 1024;

block_header MakeBlockHeader(std::uint32_t block_number, std::size_t block_size)
{
  block_header header;
  header.CheckSum = 0;
  header.BlockSize = SafeCast(block_size);
  header.BlockNumber = block_number;
  header.ID[0] = 'B';
  header.ID[1] = 'B';
  header.ID[2] = '0';
  header.ID[3] = '2';
  header.VolSessionId = 1;
  header.VolSessionTime = 2;
  return header;
}

record_header MakeRecordHeader(std::size_t offset, std::size_t remaining)
{
  record_header record;
  record.FileIndex = 1;
  record.Stream = offset == 0 ? 2 : -2;
  record.DataSize = SafeCast(remaining);
  return record;
}

std::vector<char> MakeData(std::size_t size)
{
  std::vector<char> data(size);
  for (std::size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<char>(i * 17 + (i >> 7));
  }
  return data;
}

void WriteLargeRecord(volume& vol, const std::vector<char>& data)
{
  std::size_t offset = 0;
  std::uint32_t block_number = 0;
  while (offset < data.size()) {
    const auto remaining = data.size() - offset;
    const auto chunk_size
        = std::min(remaining, kDeviceBlockSize - sizeof(block_header)
                                  - sizeof(record_header));
    const auto block_size
        = sizeof(block_header) + sizeof(record_header) + chunk_size;
    auto save = vol.BeginBlock(MakeBlockHeader(block_number, block_size));
    auto record = MakeRecordHeader(offset, remaining);
    vol.PushRecord(record, data.data() + offset, chunk_size);
    vol.CommitBlock(std::move(save));
    offset += chunk_size;
    ++block_number;
  }
}

config ReadConfig(const std::filesystem::path& volume_path)
{
  const auto config_path = volume_path / "config";
  std::ifstream file(config_path, std::ios::binary | std::ios::ate);
  if (!file) { throw std::runtime_error("Could not open config file."); }

  const auto size = file.tellg();
  std::vector<char> data(size);
  file.seekg(0);
  if (!file.read(data.data(), size)) {
    throw std::runtime_error("Could not read config file.");
  }

  return config::deserialize(data.data(), data.size());
}
}  // namespace

TEST(VolumeTest, ReusesUnfinishedReservationForRepeatedContinuation)
{
  const auto volume_path
      = std::filesystem::temp_directory_path()
        / ("dedupable-volume-test-" + std::to_string(getpid()));
  std::filesystem::remove_all(volume_path);

  volume::create_new(0700, volume_path.c_str(), kAlignedBlockSize);

  const auto data = MakeData(3 * kDeviceBlockSize);
  {
    volume vol(volume::ReadWrite, volume_path.c_str());
    WriteLargeRecord(vol, data);
  }

  const auto conf = ReadConfig(volume_path);
  ASSERT_EQ(conf.dfiles.size(), 2u);
  EXPECT_EQ(conf.dfiles[0].relpath, "aligned.data");
  EXPECT_EQ(conf.dfiles[0].Size, data.size());

  std::filesystem::remove_all(volume_path);
}
