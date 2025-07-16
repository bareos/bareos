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

#include "logger.h"
#include "dump.h"
#include "com.h"
#include "restore.h"

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"


void restore_data(std::istream& stream, bool raw_file, GenericLogger* logger)
{
  do_restore(stream, logger, raw_file);
}

bool dry = false;
bool save_unreferenced_extents{false};
bool save_unreferenced_partitions{false};
bool save_unreferenced_disks{false};

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
  dump_context_ignore_all_data(ctx, dry);
  dump_context_save_unknown_disks(ctx, save_unreferenced_disks);
  dump_context_save_unknown_partitions(ctx, save_unreferenced_partitions);
  dump_context_save_unknown_extents(ctx, save_unreferenced_extents);
  logger->Info("gathering meta data");
  insert_plan plan = dump_context_create_plan(ctx);
  logger->Info("... done!");

  auto dumper = dumper_setup(logger, std::move(plan));

  logger->Info("writing backup");
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
  destroy_context(ctx);
}

int main(int argc, char* argv[])
{
  CLI::App app;
  bool trace = false;
  app.add_flag("--trace", trace, "enable debug tracing");

  auto* save = app.add_subcommand("save");
  save->add_flag("--dry", dry, "do not read/write actual disk data");

  save->add_flag("--unreferenced-extents", save_unreferenced_extents,
                 "save even unsnapshotted data from partitions that contain "
                 "snapshotted data");
  save->add_flag("--unreferenced-partitions", save_unreferenced_partitions,
                 "save even unsnapshotted partitions from disks that contain "
                 "snapshotted partitions");
  // save->add_flag("--unreferenced-disks", save_unreferenced_disks,
  //                "save completey unsnapshotted disks");


  auto* restore = app.add_subcommand("restore");
  std::string filename;
  restore->add_option("--from", filename,
                      "read from this file instead of stdin");
  bool raw_file{false};
  restore->add_flag("--raw", raw_file);

  auto* version = app.add_subcommand("version");

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  auto logger = progressbar::get(trace);
  try {
    if (*save) {
      _setmode(_fileno(stdout), _O_BINARY);
      dump_data(std::cout, logger);
    } else if (*restore) {
      if (filename.empty()) {
        _setmode(_fileno(stdin), _O_BINARY);
        restore_data(std::cin, raw_file, logger);
      } else {
        fprintf(stderr, "using %s as input\n", filename.c_str());
        std::ifstream infile{filename,
                             std::ios_base::in | std::ios_base::binary};
        restore_data(infile, raw_file, logger);
      }
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
