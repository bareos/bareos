/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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

#define STORAGE_DAEMON

#include "include/bareos.h"
#include "lib/crypto.h"
#include "stored/butil.h"
#include "stored/device_control_record.h"
#include "include/jcr.h"
#include "stored/stored_jcr_impl.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"
#include "stored/sd_plugins.h"
#include "stored/bsr.h"
#include "lib/parse_conf.h"

#include "lib/edit.h"
#include "stored/acquire.h"
#include "lib/compression.h"

#include "lib/cli.h"
#include "lib/version.h"

#include <cassert>
#include <array>
#include <iostream>
#include <unordered_set>

struct dedup_unit {
  std::size_t data_size;
  std::array<uint64_t, 4> digest;

  dedup_unit(std::vector<std::uint8_t> data)
      : dedup_unit(data.data(), data.size())
  {
  }
  dedup_unit(const std::uint8_t* data, std::size_t dsize) : data_size{dsize}
  {
    DIGEST* digester = crypto_digest_new(nullptr, CRYPTO_DIGEST_SHA256);

    uint32_t size = sizeof(digest);
    assert(digester->Update(data, dsize));
    assert(digester->Finalize((uint8_t*)digest.data(), &size));

    if (size != sizeof(digest)) { throw "Bad size"; }

    CryptoDigestFree(digester);
  }

  friend bool operator==(const dedup_unit& l, const dedup_unit& r)
  {
    return l.data_size == r.data_size && l.digest == r.digest;
  }
};

template <> struct std::hash<dedup_unit> {
  std::size_t operator()(const dedup_unit& b) const
  {
    std::size_t val = b.digest[0] + b.digest[1] + b.digest[2] + b.digest[3];
    return val;
  }
};

namespace {
std::string device_name;
storagedaemon::DirectorResource* dir = nullptr;
std::size_t block_size{4 * 1024};
std::size_t total_size{0};
std::size_t real_size{0};
std::size_t num_records{0};
std::unordered_set<dedup_unit> dedup_units;
bool verbose_status{false};
bool enable_decompression{false};

void OutputBlockSize(std::size_t blocksize)
{
  std::cout << "Using block size " << SizeAsSiPrefixFormat(blocksize) << "\n";
}

void OutputStatus()
{
  char before[edit::min_buffer_size];
  char after[edit::min_buffer_size];
  char delta[edit::min_buffer_size];

  char multiplier[100];

  snprintf(multiplier, sizeof(multiplier), "%.2lf",
           (double)total_size / (double)real_size);

  std::cout << "Records consumed: " << num_records << "\n  Total record size: "
            << edit_uint64_with_suffix(total_size, before) << "B"
            << "\n  Size after dedup: "
            << edit_uint64_with_suffix(real_size, after) << "B"
            << "\n  Possible reduction: "
            << edit_uint64_with_suffix(total_size - real_size, delta) << "B"
            << "\n  Dedup factor: " << multiplier << "x"
            << "\n";
}

bool RecordCallback(storagedaemon::DeviceControlRecord* dcr,
                    storagedaemon::DeviceRecord* record)
{
  num_records += 1;

  auto* buf = record->data;
  auto size = record->data_len;

  if (!enable_decompression
      || !DecompressData(dcr->jcr, "unknown", record->maskedStream, &buf, &size,
                         false)) {
    buf = record->data;
    size = record->data_len;
  }

  total_size += size;
  auto num_units = size / block_size;

  for (std::size_t unit = 0; unit < num_units; ++unit) {
    const std::uint8_t* start
        = reinterpret_cast<const std::uint8_t*>(buf) + unit * block_size;
    if (dedup_units.emplace(start, block_size).second) {
      real_size += block_size;
    }
  }

  real_size += size % block_size;

  if (verbose_status && (num_records % 100000 == 0)) { OutputStatus(); }
  return true;
}

class SimpleVolumesBsr {
 public:
  SimpleVolumesBsr(const std::vector<std::string>& volumenames)
  {
    using namespace storagedaemon;
    for (auto& name : volumenames) {
      auto* bsrvolume = (BsrVolume*)calloc(1, sizeof(BsrVolume));
      bstrncpy(bsrvolume->VolumeName, name.c_str(),
               sizeof(bsrvolume->VolumeName));

      BootStrapRecord* bsr = (BootStrapRecord*)calloc(1, sizeof(*bsr));
      bsr->volume = bsrvolume;
      if (root) {
        bsr->next = root;
        root->prev = bsr;
      }
      root = bsr;
    }

    for (auto* current = root; current; current = current->next) {
      current->root = root;
    }
  }

  storagedaemon::BootStrapRecord* get() { return root; }

  ~SimpleVolumesBsr()
  {
    while (root) {
      auto* next = root->next;
      free(root->volume);
      free(root);
      root = next;
    }
  }

 private:
  storagedaemon::BootStrapRecord* root{nullptr};
};
};  // namespace

bool read_records(const std::vector<std::string>& volumenames)
{
  using namespace storagedaemon;
  std::string volumename;
  SimpleVolumesBsr bsr(volumenames);
  DeviceControlRecord* dcr = new DeviceControlRecord;
  auto* jcr = storagedaemon::SetupJcr("bdedupestimate", device_name.data(),
                                      bsr.get(), dir, dcr, volumename, true);

  LoadSdPlugins(me->plugin_directory, me->plugin_names);
  if (!jcr) { exit(1); }
  auto* dev = jcr->sd_impl->read_dcr->dev;
  if (!dev) { exit(1); }
  dcr = jcr->sd_impl->read_dcr;

  // Let SD plugins setup the record translation
  if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation, dcr) != bRC_OK) {
    Jmsg(jcr, M_FATAL, 0, T_("bSdEventSetupRecordTranslation call failed!\n"));
  }

  uint32_t decompress_buf_size;
  SetupDecompressionBuffers(jcr, &decompress_buf_size);
  if (decompress_buf_size > 0) {
    // See if we need to create a new compression buffer or make sure the
    // existing is big enough.
    if (!jcr->compress.inflate_buffer) {
      jcr->compress.inflate_buffer = GetMemory(decompress_buf_size);
      jcr->compress.inflate_buffer_size = decompress_buf_size;
    } else {
      if (decompress_buf_size > jcr->compress.inflate_buffer_size) {
        jcr->compress.inflate_buffer = ReallocPoolMemory(
            jcr->compress.inflate_buffer, decompress_buf_size);
        jcr->compress.inflate_buffer_size = decompress_buf_size;
      }
    }
  }

  ReadRecords(dcr, RecordCallback, storagedaemon::MountNextReadVolume);

  CleanDevice(jcr->sd_impl->dcr);

  delete dev;

  FreeDeviceControlRecord(dcr);

  CleanupCompression(jcr);
  FreePlugins(jcr);
  FreeJcr(jcr);
  UnloadSdPlugins();
  return true;
}

int main(int argc, const char* argv[])
{
  CLI::App app;
  std::string desc(1024, '\0');
  kBareosVersionStrings.FormatCopyright(desc.data(), desc.size(), 2023);
  desc.resize(strlen(desc.c_str()));
  desc += "The Bareos Deduplication Estimation Tool";
  InitCLIApp(app, desc, 0);
  AddDebugOptions(app);
  MyNameIs(argc, argv, "bdedupestimate");

  std::vector<std::string> volumes;
  app.add_option("-V,--volumes", volumes, "List of volumes to be analyzed.")
      ->type_name("<volume>")
      ->required();
  std::string config;
  app.add_option("-c,--config", config,
                 "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  std::string director{};
  app.add_option("-D,--director", director,
                 "Specify a director name specified in the storage.\n"
                 "Configuration file for the Key Encryption Key selection.")
      ->type_name("<director>");

  app.add_flag("-u,--decompress", enable_decompression,
               "Specify whether compressed records should get decompressed.\n"
               "Compressed records are very unlikely to be dedupable.")
      ->type_name("<bool>");

  app.add_option("--devicename,devicename", device_name,
                 "Specify the input device name (either as name of a Bareos "
                 "Storage Daemon Device resource or identical to the Archive "
                 "Device in a Bareos Storage Daemon Device resource).")
      ->required();

  bool k_is_1000 = false;
  app.add_option("-b,--blocksize", block_size)
      ->transform(CLI::AsSizeValue{k_is_1000})
      ->check(CLI::PositiveNumber);

  app.add_flag("-v,--verbose", verbose_status);

  CLI11_PARSE(app, argc, argv);

  storagedaemon::my_config
      = storagedaemon::InitSdConfig(config.c_str(), M_ERROR_TERM);
  storagedaemon::ParseSdConfig(config.c_str(), M_ERROR_TERM);

  if (!director.empty()) {
    {
      using namespace storagedaemon;
      foreach_res (dir, storagedaemon::R_DIRECTOR) {
        if (bstrcmp(dir->resource_name_, director.c_str())) { break; }
      }
    }
    if (!dir) {
      Emsg2(
          M_ERROR_TERM, 0,
          T_("No Director resource named %s defined in %s. Cannot continue.\n"),
          director.c_str(), config.c_str());
    }
  }

  OutputBlockSize(block_size);
  read_records(volumes);

  OutputStatus();

  return 0;
}
