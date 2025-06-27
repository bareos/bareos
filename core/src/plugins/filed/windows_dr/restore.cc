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
#include "common.h"

#include <iostream>
#include <string>
#include <format>
#include <stdexcept>
#include <span>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>
#include <zlib.h>
#include <optional>

#include "parser.h"
#include "partitioning.h"

#include <fmt/format.h>
#include <fmt/xchar.h>


void WriteOverlapped(HANDLE hndl,
                     std::size_t offset,
                     const char* buffer,
                     DWORD size,
                     DWORD* bytes_written)
{
  OVERLAPPED overlapped = {};
  overlapped.Offset = (DWORD)offset;
  overlapped.OffsetHigh = (DWORD)(offset >> 32);

  if (WriteFile(hndl, buffer, size, bytes_written, &overlapped)) { return; }

  auto err = GetLastError();

  if (err == ERROR_IO_PENDING) {
    if (!GetOverlappedResult(hndl, &overlapped, bytes_written, TRUE)) {
      err = GetLastError();
      fprintf(stderr, "WriteFile: could not write (%llu, %llu): %d\n",
              static_cast<long long unsigned>(offset),
              static_cast<long long unsigned>(size), err);

      throw win_error("WriteFileW", err);
    }
    return;
  }

  fprintf(stderr, "WriteFile: could not write (%llu, %llu): %d\n",
          static_cast<long long unsigned>(offset),
          static_cast<long long unsigned>(size), err);
  throw win_error("WriteFileW", err);
}

void write_buffer(HANDLE output,
                  std::size_t start,
                  std::span<const char> buffer)
{
  std::size_t offset = 0;

  fprintf(
      stderr,
      "%llu: %02X %02X %02X %02X %02X %02X ... %02X %02X %02X %02X %02X %02X\n",
      start, (unsigned char)buffer[0], (unsigned char)buffer[1],
      (unsigned char)buffer[2], (unsigned char)buffer[3],
      (unsigned char)buffer[4], (unsigned char)buffer[5],
      (unsigned char)buffer[buffer.size() - 6],
      (unsigned char)buffer[buffer.size() - 5],
      (unsigned char)buffer[buffer.size() - 4],
      (unsigned char)buffer[buffer.size() - 3],
      (unsigned char)buffer[buffer.size() - 2],
      (unsigned char)buffer[buffer.size() - 1]);

  while (offset != buffer.size()) {
    DWORD bytes_written = 0;

    WriteOverlapped(output, start + offset, buffer.data() + offset,
                    (DWORD)(buffer.size() - offset), &bytes_written);

    offset += bytes_written;
  }
}

class HandleOutput : public Output {
 public:
  HandleOutput(HANDLE hndl, std::size_t size) : hndl_{hndl}, size_{size} {}

  void append(std::span<const char> bytes) override
  {
    if (current_offset_ + bytes.size() > size_) {
      throw std::logic_error{"can not write past the end of the file"};
    }

    write_buffer(hndl_, current_offset_, bytes);

    current_offset_ += bytes.size();
  }
  void skip_forwards(std::size_t offset) override
  {
    if (offset < current_offset_) {
      throw std::logic_error{
          fmt::format("Trying to skip to offset {}, when already at offset {}",
                      offset, current_offset_)};
    }

    if (offset > size_) {
      throw std::logic_error{"can not seek past the end of the file"};
    }

    current_offset_ = offset;
  }

  std::size_t current_offset() override { return current_offset_; }

 private:
  HANDLE hndl_;
  std::size_t current_offset_ = 0;
  std::size_t size_ = 0;
};

class OutputHandleGenerator {
 public:
  virtual HANDLE Create(disk_info info, disk_geometry geo) = 0;
  virtual void Close(HANDLE hndl) = 0;

  virtual ~OutputHandleGenerator() {}
};

class RawFileGenerator : public OutputHandleGenerator {
 public:
  HANDLE Create(disk_info info, disk_geometry) override
  {
    auto disk_path = std::format(L"disk-{}.raw", ++disk_idx_);

    HANDLE output
        = CreateFileW(disk_path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (output == INVALID_HANDLE_VALUE) {
      throw std::runtime_error{fmt::format(
          "could not open disk-{}.raw: Err={}\n", disk_idx_, GetLastError())};
    }

    // TODO: set file size
    (void)info;

    return output;
  }
  void Close(HANDLE hndl) override { CloseHandle(hndl); }

 private:
  std::size_t disk_idx_ = 0;
};

class VhdxGenerator : public OutputHandleGenerator {
 public:
  HANDLE Create(disk_info info, disk_geometry geo) override
  {
    auto disk_path = std::format(L"disk-{}.vhdx", ++disk_idx_);

    VIRTUAL_STORAGE_TYPE vst
        = {.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
           .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

    CREATE_VIRTUAL_DISK_PARAMETERS params = {
      .Version = CREATE_VIRTUAL_DISK_VERSION_2,
      .Version2 = {
        .UniqueId = {},
        .MaximumSize = info.disk_size,
        .BlockSizeInBytes = {},
        .SectorSizeInBytes = gsl::narrow<ULONG>(geo.bytes_per_sector),
        .PhysicalSectorSizeInBytes = {},
        .ParentPath = nullptr,
        .SourcePath = nullptr,
      },
    };

    fprintf(stderr, "creating disk\n");
    HANDLE output = INVALID_HANDLE_VALUE;

    DWORD hres = CreateVirtualDisk(
        &vst, disk_path.c_str(), VIRTUAL_DISK_ACCESS_NONE, NULL,
        CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &params, NULL, &output);

    if (hres != ERROR_SUCCESS) {
      throw std::runtime_error{fmt::format(
          "CreateVirtualDisk(disk-{}.vhdx) returned {}", disk_idx_, hres)};
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
        = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
    auto attach_res = AttachVirtualDisk(output, NULL,
                                        ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST,
                                        0, &attach_params, 0);

    if (attach_res != ERROR_SUCCESS) {
      CloseHandle(output);
      throw std::runtime_error(
          fmt::format("AttachVirtualDisk(disk-{}.vhdx) returned {}\n",
                      disk_idx_, attach_res));
    }

    return output;
  }
  void Close(HANDLE hndl) override
  {
    DetachVirtualDisk(hndl, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
    CloseHandle(hndl);
  }

 private:
  std::size_t disk_idx_ = 0;
};

class RestoreToHandles : public GenericHandler {
 public:
  RestoreToHandles(OutputHandleGenerator* Generator) : Generator_{Generator} {}

  void BeginRestore(std::size_t num_disks [[maybe_unused]]) override {}
  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    if (disk_) {
      throw std::logic_error{"cannot begin disk after one was created"};
    }

    fmt::println(stderr, "begin disk {{ size {}, count {} }}", info.disk_size,
                 info.extent_count);
    auto geo = geometry_for_size(info.disk_size);
    HANDLE hndl = Generator_->Create(info, geo);
    disk_.emplace(hndl, geo, info.disk_size);
  }
  void EndDisk() override
  {
    fmt::println(stderr, "disk done");
    HANDLE hndl = disk_->hndl;
    disk_.reset();
    Generator_->Close(hndl);
  }

  void BeginMbrTable(const partition_info_mbr& mbr) override
  {
    disk().BeginMbrTable(mbr);
  }

  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    disk().BeginGptTable(gpt);
  }

  void BeginRawTable(const partition_info_raw& raw) override
  {
    disk().BeginRawTable(raw);
  }

  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    disk().MbrEntry(entry, data);
  }

  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    disk().GptEntry(entry, data);
  }

  void EndPartTable() override { disk().EndPartTable(); }

  void BeginExtent(extent_header header) override
  {
    fmt::println(stderr, "begin extent {{ size: {}, offset: {} }}",
                 header.length, header.offset);
    disk().BeginExtent(header);
  }
  void ExtentData(std::span<const char> data) override
  {
    fmt::println(stderr, "extent data {{ size: {} }}", data.size());
    disk().ExtentData(data);
  }
  void EndExtent() override
  {
    fmt::println(stderr, "extent end");
    disk().EndExtent();
  }

  ~RestoreToHandles()
  {
    if (disk_) {
      HANDLE hndl = disk_->hndl;
      disk_.reset();
      Generator_->Close(hndl);
    }
  }

 private:
  disk::Parser& disk()
  {
    if (!disk_) {
      throw std::logic_error{"cannot access disk if no disk is started"};
    }
    return disk_->parser;
  }

  OutputHandleGenerator* Generator_ = nullptr;
  struct open_disk {
    HANDLE hndl;
    HandleOutput output;
    disk::Parser parser;

    open_disk(HANDLE hndl_, disk_geometry geo, std::size_t disk_size)
        : hndl{hndl_}, output{hndl_, disk_size}, parser{geo, disk_size, &output}
    {
    }
  };
  std::optional<open_disk> disk_;
};

struct NoLogger : public GenericLogger {
  void Begin(std::size_t) override {}
  void Progressed(std::size_t) {}
  void End() {}

  virtual ~NoLogger() {}
};

void restore_data(std::istream& stream, bool raw_file)
{
  auto output_generator = [&]() -> std::unique_ptr<OutputHandleGenerator> {
    if (raw_file) {
      return std::make_unique<RawFileGenerator>();
    } else {
      return std::make_unique<VhdxGenerator>();
    }
  }();
  RestoreToHandles alg{output_generator.get()};


  NoLogger log;

  parse_file_format(&log, stream, &alg);
}
