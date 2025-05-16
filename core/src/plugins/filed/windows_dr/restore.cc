/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "file_format.h"
#include "error.h"

#include <iostream>
#include <string>
#include <format>
#include <span>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>

std::uint32_t ReadHeader(std::istream& stream)
{
  file_header header;
  header.read(stream);
  return header.disk_count;
}

struct disk_info {
  std::uint64_t disk_size;
  std::uint32_t extent_count;
};

disk_info ReadDiskHeader(std::istream& stream)
{
  disk_header header;
  header.read(stream);

  std::uint64_t disk_size = header.disk_size;

  return {disk_size, header.extent_count};
}


partition_layout ReadDiskPartTable(std::istream& stream)
{
  partition_layout layout;

  fprintf(stderr, "read part header ... \n");
  part_table_header header;
  header.read(stream);
  fprintf(stderr, "read part header ... done\n");

  switch (header.part_table_type) {
    case part_type::Raw: {
      layout.info = partition_info_raw{};
    } break;
    case part_type::Mbr: {
      partition_info_mbr mbr{.CheckSum = header.Datum0,
                             .Signature = (uint32_t)header.Datum1};
      memcpy(mbr.bootstrap, header.Data2, sizeof(header.Data2));
      layout.info = mbr;
    } break;
    case part_type::Gpt: {
      partition_info_gpt gpt{
          .DiskId = std::bit_cast<GUID>(header.Data),
          .StartingUsableOffset = header.Datum1,
          .UsableLength = header.Datum2,
          .MaxPartitionCount = header.Datum0,
      };

      memcpy(gpt.bootstrap, header.Data2, sizeof(header.Data2));
      layout.info = gpt;

    } break;
  }

  fprintf(stderr, "found %u partitions\n", header.partition_count);

  layout.partition_infos.resize(header.partition_count);

  for (auto& info : layout.partition_infos) {
    fprintf(stderr, "read entry ...\n");
    part_table_entry entry;
    entry.read(stream);
    fprintf(stderr, "read entry ... done\n");

    info.PartitionStyle = (PARTITION_STYLE)entry.partition_style;
    info.StartingOffset.QuadPart = entry.partition_offset;
    info.PartitionLength.QuadPart = entry.partition_length;
    info.PartitionNumber = entry.partition_number;

    switch (entry.partition_style) {
      case PARTITION_STYLE_MBR: {
        fprintf(stderr, "read mbr ...\n");
        part_table_entry_mbr_data data;
        data.read(stream);
        fprintf(stderr, "read mbr ... done\n");

        info.Mbr.PartitionType = data.partition_type;
        info.Mbr.BootIndicator = data.bootable ? TRUE : FALSE;
        info.Mbr.RecognizedPartition = data.recognized ? TRUE : FALSE;
        info.Mbr.HiddenSectors = data.num_hidden_sectors;
        info.Mbr.PartitionId = data.partition_id;
      } break;
      case PARTITION_STYLE_GPT: {
        fprintf(stderr, "read gpt ...\n");
        part_table_entry_gpt_data data;
        data.read(stream);
        fprintf(stderr, "read gpt ... done\n");

        fprintf(stderr, "gpt name = '%.36ls'\n", data.name);

        info.Gpt.PartitionType = data.partition_type;
        info.Gpt.PartitionId = data.partition_id;
        info.Gpt.Attributes = data.attributes;
        static_assert(sizeof(data.name) == sizeof(info.Gpt.Name));
        memcpy(info.Gpt.Name, data.name, sizeof(data.name));
      } break;
    }
  }

  return layout;
}

void write_buffer(HANDLE output, std::span<const char> buffer)
{
  std::size_t offset = 0;

  while (offset != buffer.size()) {
    DWORD bytes_written;

    if (!WriteFile(output, buffer.data() + offset,
                   (DWORD)(buffer.size() - offset), &bytes_written, NULL)) {
      throw win_error("WriteFileW", GetLastError());
    }

    offset += bytes_written;
  }
}

#pragma pack(push, 1)
struct gpt_header {
  char signature[8] = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'};
  uint32_t revision = 1 << 16;
  uint32_t header_size = sizeof(gpt_header);
  uint32_t crc32_sum = 0;
  uint32_t reserved_0 = 0;

  uint64_t lba_location;
  uint64_t backup_lba_location;
  uint64_t first_usable_lba;
  uint64_t last_usable_lba;

  GUID disk_guid;

  uint64_t table_lba_location;

  uint32_t partition_count;
  uint32_t entry_size;
  uint32_t table_crc32_sum;
};
#pragma pack(pop)

static_assert(sizeof(gpt_header) == 92);

struct gpt_entry {
  GUID partition_type;
  GUID id;
  uint64_t start_lba;
  uint64_t end_lba;  // inclusive!
  uint64_t attributes;
  wchar_t name[36];
};

static_assert(sizeof(gpt_entry) == 128);

struct ccitt_crc {
  ccitt_crc& update(std::span<const char> data)
  {
#define M2 0xffffff00

    for (std::size_t i = 0; i < data.size(); ++i) {
      uint32_t shifted = (crc << 8) & M2;
      uint32_t carry = (crc >> 24) & 0xFF;
      uint8_t elem = std::bit_cast<uint8_t>(data[data.size() - 1 - i]);
      uint32_t update = crc_table[carry ^ elem];

      crc = shifted ^ update;
    }

    return *this;
  }


  std::uint32_t get() const { return crc; }

 private:
  std::uint32_t crc = std::numeric_limits<uint32_t>::max();


  // see: https://www.mrob.com/pub/comp/crc-all.html (ccitt32)
  static constexpr uint32_t crc_table[256] = {
      0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
      0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
      0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
      0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
      0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
      0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
      0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
      0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
      0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
      0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
      0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
      0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
      0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
      0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
      0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
      0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
      0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
      0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
      0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
      0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
      0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
      0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
      0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
      0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
      0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
      0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
      0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
      0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
      0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
      0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
      0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
      0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
      0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
      0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
      0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
      0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
      0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
      0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
      0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
      0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
      0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
      0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
      0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4,
  };
};


template <typename T> std::span<const char> as_span(const T& val)
{
  return {reinterpret_cast<const char*>(&val), sizeof(T)};
}

gpt_header MakeGptHeader(const partition_info_gpt& info,
                         ccitt_crc table_crc,
                         uint64_t disk_size)
{
  gpt_header header;

  header.lba_location = 1;
  std::uint64_t lba_count = disk_size / 512;
  header.backup_lba_location = lba_count - 1;

  {
    wchar_t guid_storage[64] = {};

    StringFromGUID2(info.DiskId, guid_storage, sizeof(guid_storage));
    fprintf(stderr, "disk id: %ls\n", guid_storage);
  }

  fprintf(stderr, "disk size = %llu, lbas = %llu, loc = %llx\n", disk_size,
          lba_count, header.backup_lba_location);

  fprintf(stderr, "usable start = %llu, usable length = %llu\n",
          info.StartingUsableOffset, info.UsableLength);
  header.first_usable_lba = info.StartingUsableOffset / 512;
  header.last_usable_lba
      = header.first_usable_lba + info.UsableLength / 512 - 1;

  header.disk_guid = info.DiskId;
  header.table_lba_location = 2;

  header.partition_count = info.MaxPartitionCount;
  header.entry_size = sizeof(gpt_entry);
  header.table_crc32_sum = table_crc.get();

  header.crc32_sum = ccitt_crc{}.update(as_span(header)).get();

  return header;
}

gpt_entry MakeGptEntry(const PARTITION_INFORMATION_EX& info)
{
  gpt_entry entry;
  entry.partition_type = info.Gpt.PartitionType;
  entry.id = info.Gpt.PartitionId;
  entry.start_lba = info.StartingOffset.QuadPart / 512;
  entry.end_lba = entry.start_lba + info.PartitionLength.QuadPart / 512 - 1;
  entry.attributes = info.Gpt.Attributes;
  std::memcpy(entry.name, info.Gpt.Name, sizeof(entry.name));
  return entry;
}

#pragma pack(push, 1)
struct chs {
  // chs = cylinder, head, sector
  uint8_t data[3]{};

  chs() = default;
  chs(std::uint16_t cylinder,  // 10 bits
      std::uint8_t head,       // 8 bits
      std::uint8_t sector      // 6 bits
  )
  {
    data[0] = head;
    data[1] = sector | (cylinder << 6) & 0b11;
    data[2] = (cylinder & 0b11111111);
  }

  static chs from_lba(std::uint64_t lba, DISK_GEOMETRY geo)
  {
    // lba = ((cylinder * geo.TracksPerCylinder) + head) * geo.SectorsPerTrack
    //             + (sector - 1)

    std::uint64_t sector = lba % geo.SectorsPerTrack + 1;
    auto rest = lba / geo.SectorsPerTrack;

    std::uint64_t head = rest % geo.TracksPerCylinder;
    std::uint64_t cylinder = rest / geo.TracksPerCylinder;


    if (sector > 63 || head > 255 || cylinder > 1023) {
      // offset is too large, so just return the largest one
      return chs((std::uint16_t)(geo.Cylinders.QuadPart - 1),
                 (std::uint8_t)(geo.TracksPerCylinder - 1),
                 (std::uint8_t)(geo.SectorsPerTrack - 1));
    }

    return chs((std::uint16_t)cylinder, (std::uint8_t)head,
               (std::uint8_t)sector);
  }
};

static_assert(sizeof(chs) == 3);

struct mbr_entry {
  uint8_t bootable;
  chs start_chs;
  uint8_t os_type;
  chs end_chs;
  uint32_t start_lba;
  uint32_t end_lba;
};

static_assert(sizeof(mbr_entry) == 16);

struct mbr {
  char bootstrap[446];
  mbr_entry entries[4];
  uint8_t check[2];
};

static_assert(sizeof(mbr) == 512);

#pragma pack(pop)

void WriteProtectionMBR(HANDLE output,
                        std::uint64_t end_lba,
                        std::span<const char> bootstrap)
{
  static constexpr uint8_t gpt_type = 0xEE;

  DISK_GEOMETRY geo = {.Cylinders = {5221},
                       .MediaType = {},
                       .TracksPerCylinder = 255,
                       .SectorsPerTrack = 64,
                       .BytesPerSector = 512};

  mbr_entry gpt_entry = {
      .bootable = 0,  // if you dont understand gpt, dont boot this
      .start_chs = chs::from_lba(1, geo),
      .os_type = gpt_type,
      .end_chs = chs::from_lba(end_lba, geo),
      .start_lba = 1,
      .end_lba = (std::uint32_t)std::max(
          end_lba, (std::uint64_t)std::numeric_limits<std::uint32_t>::max()),
  };

  mbr protective_mbr = {};
  protective_mbr.entries[0] = gpt_entry;

  assert(bootstrap.size() <= std::size(protective_mbr.bootstrap));
  memcpy(protective_mbr.bootstrap, bootstrap.data(), bootstrap.size());

  protective_mbr.check[0] = 0x55;
  protective_mbr.check[1] = 0xAA;

  static_assert(sizeof(mbr) == 512);
  {
    DWORD off_low = 0;
    LONG off_high = 0;

    SetFilePointer(output, off_low, &off_high, FILE_BEGIN);
  }
  write_buffer(output, as_span(protective_mbr));
}

void WritePartitionTable(HANDLE output,
                         const partition_layout& table,
                         std::size_t disk_size)
{
  if (auto* Gpt = std::get_if<partition_info_gpt>(&table.info)) {
    WriteProtectionMBR(output, disk_size / 512 - 1, Gpt->bootstrap);

    {
      DWORD off_low = 1024;
      LONG off_high = 0;

      SetFilePointer(output, off_low, &off_high, FILE_BEGIN);
    }

    ccitt_crc table_crc;
    for (auto& part : table.partition_infos) {
      auto entry = MakeGptEntry(part);

      table_crc.update(as_span(entry));
      write_buffer(output, as_span(entry));
    }

    for (size_t i = 0;
         i < Gpt->MaxPartitionCount - table.partition_infos.size(); ++i) {
      gpt_entry entry;
      memset(&entry, 0, sizeof(entry));
      table_crc.update(as_span(entry));
      write_buffer(output, as_span(entry));
    }

    auto header = MakeGptHeader(*Gpt, table_crc, disk_size);

    char sector[512] = {};

    std::memcpy(sector, &header, sizeof(header));

    {
      DWORD off_low = 512;
      LONG off_high = 0;

      SetFilePointer(output, off_low, &off_high, FILE_BEGIN);
    }

    write_buffer(output, sector);
  } else if (auto* Mbr = std::get_if<partition_info_mbr>(&table.info)) {
    // todo
    fprintf(stderr, "cant restore mbr yet\n");
  } else {
    // todo
    fprintf(stderr, "cant restore raw yet\n");
  }
}

void copy_output(std::istream& stream,
                 std::size_t offset,
                 std::size_t length,
                 HANDLE output)
{
#if !defined(DO_DRY)
  std::vector<char> buffer;
  buffer.resize(4 * 1024 * 1024);

  DWORD off_low = offset & 0xFFFFFFFF;
  LONG off_high = (offset >> 32) & 0xFFFFFFFF;
  SetFilePointer(output, off_low, &off_high, FILE_BEGIN);

  std::size_t bytes_to_read = length;

  while (bytes_to_read > 0) {
    stream.read(buffer.data(), buffer.size());

    assert(stream.gcount() == (std::streamsize)buffer.size());

    write_buffer(output, buffer);
  }
#endif
}

void ReadExtent(std::istream& stream, HANDLE output)
{
  extent_header header;
  header.read(stream);

  fprintf(stderr, "Extent: Offset=%llu Length=%llu\n", header.offset,
          header.length);

  copy_output(stream, header.offset, header.length, output);
}

void restore_data(std::istream& stream, bool raw_file)
{
  fprintf(stderr, "reading header\n");
  auto disk_count = ReadHeader(stream);

  fprintf(stderr, "disk count = %lu\n", disk_count);
  for (std::size_t i = 0; i < disk_count; ++i) {
    fprintf(stderr, "reading disk header\n");
    auto disk_header = ReadDiskHeader(stream);
    fprintf(stderr, "reading disk header ... done\n");

    HANDLE output = INVALID_HANDLE_VALUE;

    if (raw_file) {
      auto disk_path = std::format(L"disk-{}.raw", i);

      output = CreateFileW(disk_path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE,
                           NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

      if (output == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "could not open %ls: Err=%d\n", disk_path.c_str(),
                GetLastError());

        return;
      }
    } else {
      auto disk_path = std::format(L"disk-{}.vhdx", i);

      VIRTUAL_STORAGE_TYPE vst
          = {.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
             .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

      CREATE_VIRTUAL_DISK_PARAMETERS params = {
        .Version = CREATE_VIRTUAL_DISK_VERSION_2,
        .Version2 = {
          .UniqueId = {},
          .MaximumSize = disk_header.disk_size,
          .BlockSizeInBytes = {},
          .SectorSizeInBytes = {},
          .PhysicalSectorSizeInBytes = {},
          .ParentPath = nullptr,
          .SourcePath = nullptr,
        },
      };

      fprintf(stderr, "creating disk\n");
      DWORD hres = CreateVirtualDisk(
          &vst, disk_path.c_str(), VIRTUAL_DISK_ACCESS_NONE, NULL,
          CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &params, NULL, &output);

      if (hres != ERROR_SUCCESS) {
        fprintf(stderr, "CreateVirtualDisk(%ls) returned %d\n",
                disk_path.c_str(), hres);
        return;
      }

      ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
          = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
      auto attach_res = AttachVirtualDisk(
          output, NULL, ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST, 0,
          &attach_params, 0);

      if (attach_res != ERROR_SUCCESS) {
        fprintf(stderr, "AttachVirtualDisk(%ls) returned %d\n",
                disk_path.c_str(), attach_res);
        return;
      }
    }


    fprintf(stderr, "reading part table ...\n");
    auto part_table = ReadDiskPartTable(stream);
    fprintf(stderr, "reading part table ... done\n");
    for (size_t extent = 0; extent < disk_header.extent_count; ++extent) {
      fprintf(stderr, "reading extent ...\n");
      ReadExtent(stream, output);
      fprintf(stderr, "reading extent ... done\n");
    }

    WritePartitionTable(output, part_table, disk_header.disk_size);

    CloseHandle(output);
  }
}
