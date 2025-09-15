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
#include <istream>
#include <fstream>

#include <vector>

#include "logger.h"
#include "dump.h"
#include "com.h"
#include "restore.h"

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

std::atomic<bool> quit = false;

void restore_data(std::istream& stream,
                  GenericHandler* handler,
                  GenericLogger* logger)
{
  logger->Trace("starting restore");
  logger->PushIndent();
  logger->Trace("starting parser");
  auto* parser = parse_begin(handler, logger);

  std::vector<char> buffer_storage;
  buffer_storage.resize(64 << 10);
  std::span<char> buffer = std::span{buffer_storage};

  logger->SetStatus("restoring");
  for (;;) {
    if (quit.load()) {
      logger->Info("early stop was requested ...");
      break;
    }

    stream.read(buffer.data(), buffer.size());

    if (stream.bad() || stream.exceptions()) {
      fprintf(stderr,
              "[ERROR] could not read from stream: bad = %s, exception = %s\n",
              stream.bad() ? "yes" : "no", stream.exceptions() ? "yes" : "no");
      break;
    }

    auto bytes_read = stream.gcount();
    if (bytes_read != buffer.size()) {
      logger->Trace("read {}/{} bytes", bytes_read, buffer.size());
    }
    parse_data(parser, buffer.subspan(0, bytes_read));

    if (stream.eof()) { break; }
  }
  logger->PopIndent();

  logger->Trace("stopping parser");
  parse_end(parser);
  logger->Trace("restore done");
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

  logger->Info("writing backup (expecting {} bytes of output)",
               human_readable(output_size));
  logger->Trace(" => exactly {} bytes of output", output_size);
  if (!dry) {
    auto dumper = dumper_setup(logger, std::move(plan));

    try {
      for (;;) {
        if (quit.load()) {
          logger->Info("early stop was requested ...");
          break;
        }

        auto count = dumper_write(dumper, buffer);
        if (!count) { break; }

        stream.write(buffer.data(), count);

        if (stream.eof() || stream.fail() || stream.bad()) {
          logger->Info(
              "stream did not accept all data (eof = {} | fail = {} | bad = "
              "{})",
              stream.eof(), stream.fail(), stream.bad());
          break;
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

BOOL WINAPI SignalHandler(DWORD dwCtrlType)
{
  // handle Ctrl+C and so on
  switch (dwCtrlType) {
    case CTRL_C_EVENT:
      [[fallthrough]];
    case CTRL_BREAK_EVENT:
      [[fallthrough]];
    case CTRL_CLOSE_EVENT: {
      // signal to loops to exit
      quit = true;
      return TRUE;
    } break;

      // cannot do anything useful here
    case CTRL_LOGOFF_EVENT: {
    } break;
    case CTRL_SHUTDOWN_EVENT: {
    } break;
  }

  return FALSE;
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

  auto* location = restore->add_option_group(
      "output", "select where the data will be restored to");
  std::wstring vhdx_dir;
  auto* vhdx_dir_target
      = location
            ->add_option("--vhdx-directory", vhdx_dir,
                         "create one vhdx image per restored drive in "
                         "the given directory")
            ->check(CLI::ExistingDirectory);
  std::wstring file_dir;
  auto* raw_dir_target
      = location
            ->add_option("--raw-directory", file_dir,
                         "create one raw image file per restored "
                         "drive in the given directory")
            ->check(CLI::ExistingDirectory);
  std::vector<std::size_t> disks;
  auto* disks_target
      = location->add_option("--disks", disks, "restore to the given disk ids");

  location->require_option(1);

  auto* version = app.add_subcommand("version", "output the version");

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  auto logger = progressbar::get(trace);

  if (!SetConsoleCtrlHandler(&SignalHandler, TRUE)) {
    logger->Info("could not setup ctrl-c handler: Err={}", GetLastError());
  } else {
    logger->Trace("ctrl-c handler was setup");
  }


  try {
    if (*save) {
      _setmode(_fileno(stdout), _O_BINARY);
      dump_data(std::cout, logger);
    } else if (*restore) {
      std::ifstream infile;
      std::istream* input = std::addressof(std::cin);

      if (!filename.empty()) {
        logger->Info("using {} as input", filename.c_str());
        infile = std::ifstream{filename,
                               std::ios_base::in | std::ios_base::binary};
        input = &infile;
      } else {
        _setmode(_fileno(stdin), _O_BINARY);
      }

      logger->Trace("source chosen");

      std::unique_ptr<GenericHandler> handler;
      {
        using namespace barri::restore;
        if (*vhdx_dir_target) {
          handler = GetHandler(logger, vhdx_directory{vhdx_dir});
        } else if (*raw_dir_target) {
          handler = GetHandler(logger, raw_directory{file_dir});
        } else if (*disks_target) {
          handler = GetHandler(logger, disks);
        } else {
          logger->Info("this should not happen; no target is set.");
          return 1;
        }
      }

      logger->Trace("handler chosen");
      restore_data(*input, handler.get(), logger);
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
    std::cerr << "[ERROR] " << ex.what() << std::endl;

    return 1;
  }
}
