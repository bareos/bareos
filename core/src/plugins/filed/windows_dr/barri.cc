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

#include <fcntl.h>
#include <io.h>
#include <fstream>

#include <vector>

#include "logger.h"
#include "dump.h"
#include "com.h"
#include "restore.h"

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"


void restore_data(std::ifstream& stream, restore_options options)
{
  auto* writer = writer_begin(std::move(options));

  std::vector<char> buffer_storage;
  buffer_storage.resize(64 << 10);
  std::span<char> buffer = std::span{buffer_storage};

  for (;;) {
    stream.read(buffer.data(), buffer.size());

    if (stream.bad() || stream.exceptions()) {
      err_msg("could not read from stream: bad = {}, exception = {}",
              stream.bad() ? "yes" : "no", stream.exceptions() ? "yes" : "no");
      break;
    }

    writer_write(writer, buffer.subspan(stream.gcount()));

    if (stream.eof()) { break; }
  }

  writer_end(writer);
}

bool dry = false;
bool save_unreferenced_extents{false};
bool save_unreferenced_partitions{false};
bool save_unreferenced_disks{false};
std::vector<std::size_t> ignored_disks;

void dump_data(std::ostream& stream, GenericLogger* logger)
{
  COM_CALL(CoInitializeEx(NULL, COINIT_MULTITHREADED));

  struct CoUninitializer {
    ~CoUninitializer() { CoUninitialize(); }
  };

  CoUninitializer _{};

  std::vector<char> buffer;
  buffer.resize(4 << 20);

  dump_context* ctx = make_context(logger);
  dump_context_save_unknown_disks(ctx, save_unreferenced_disks);
  dump_context_save_unknown_partitions(ctx, save_unreferenced_partitions);
  dump_context_save_unknown_extents(ctx, save_unreferenced_extents);
  for (auto disk_id : ignored_disks) { dump_context_ignore_disk(ctx, disk_id); }
  logger->Info("gathering meta data");
  logger->PushIndent();
  insert_plan plan = dump_context_create_plan(ctx);
  logger->PopIndent();
  logger->Info("... done!");

  auto output_size = compute_plan_size(plan);

  logger->Info("writing backup (expecting {} bytes of output)", output_size);
  if (!dry) {
    auto dumper = dumper_setup(logger, std::move(plan));

    try {
      for (;;) {
        auto count = dumper_write(dumper, buffer);
        if (!count) { break; }
        stream.write(buffer.data(), count);

        if (stream.eof() || stream.fail() || stream.bad()) {
          logger->Info(
              "stream did accept all data (eof = {} | fail = {} | bad = {})",
              stream.eof(), stream.fail(), stream.bad());
        }
      }
    } catch (const std::exception& ex) {
      fprintf(stderr, "[EXCEPTION]: %s\n", ex.what());
    } catch (...) {
      fprintf(stderr, "[EXCEPTION]: %s\n", "unknown");
    }

    dumper_stop(dumper);
  }
  destroy_context(ctx);
}

int main(int argc, char* argv[])
{
  CLI::App app;
  bool trace = false;
  app.add_flag("--trace", trace, "enable debug tracing");

  auto* save = app.add_subcommand("save", "create a disaster recovery image");
  save->add_flag("--dry", dry, "do not read/write actual disk data");

  save->add_flag("--unreferenced-extents", save_unreferenced_extents,
                 "save even unsnapshotted data from partitions that contain "
                 "snapshotted data");
  save->add_flag("--unreferenced-partitions", save_unreferenced_partitions,
                 "save even unsnapshotted partitions from disks that contain "
                 "snapshotted partitions");
  save->add_flag("--unreferenced-disks", save_unreferenced_disks,
                 "save completey unsnapshotted disks");

  save->add_option("--ignore-disks", ignored_disks,
                   "ids of disks to ignore (0, 1, 2, ...)");

  auto* restore = app.add_subcommand("restore",
                                     R"(
restore the disks to as vhdx files, or as regular files if --raw is set,
to the current directory.

The files will be called disk-0.vhdx, disk-1.vhdx, ..., and
disk-0.raw, disk-1.raw, ... respectively.

By default barri reads the dump from stdin, but you can also specify a path
via the --from option explicitly.
)");
  std::string filename;
  restore->add_option("--from", filename,
                      "read from this file instead of stdin");

  auto* location = location->add_option_group(
      "output", "select where the data will be restored to");
  std::string vhdx_dir;
  auto* vhdx = location->add_option(
      "--vhdx-directory", vhdx_dir,
      "create one vhdx image per restored drive in the given directory");
  std::string file_dir;
  auto* directory = location->add_option(
      "--raw-directory", file_dir,
      "create one raw image file per restored drive in the given directory");
  std::vector<std::string> targets;
  auto* outputs = location
                      ->add_option("--targets", targets,
                                   "write the disks into the given paths")
                      ->check(CLI::ExistingFile);

  location->require_option(1);

  auto* version = app.add_subcommand("version", "output the version");

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  auto logger = progressbar::get(trace);

  try {
    if (*save) {
      _setmode(_fileno(stdout), _O_BINARY);
      dump_data(std::cout, logger);
    } else if (*restore) {
      std::ifstream infile;
      std::ifstream* input = &std::cin;

      if (!filename.empty()) {
        fprintf(stderr, "using %s as input\n", filename.c_str());
        infile = std::ifstream{filename,
                               std::ios_base::in | std::ios_base::binary};
        input = &infile;
      } else {
        _setmode(_fileno(stdin), _O_BINARY);
      }


      restore_options options{};
      options.logger(logger);
      std::unique_ptr<OutputHandleGenerator> handle_generator;
      if (*vhdx) {
        options.target(restore_options::vhdx_directory{vhdx_dir});
      } else if (*directory) {
        options.target(restore_options::file_directory{file_dir});
      } else if (*targets) {
        options.target(restore_options::files{std::move(targets)});
      } else {
        fprintf(stderr, "this should not happen; no target is set.\n");
      }

      restore_data(*input, std::move(options));
    } else if (*version) {
#if !defined(BARRI_VERSION)
#  warning "no barri version defined"
#  define BARRI_VERSION "unknown"
#endif
#if !defined(BARRI_DATE)
#  warning "no barri date defined"
#  define BARRI_DATE "unknown"
#endif
      std::cout << BARRI_VERSION " (" BARRI_DATE ")" << std::endl;
      std::cout << "Copyright (C) 2025-2025 Bareos GmbH & Co. KG" << std::endl;
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;

    return 1;
  }
}
