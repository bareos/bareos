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
#include "util.h"

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#include "barri_cli.h"

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

  logger->Info("writing backup (expecting {} of output)",
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
              "{}); tried writing {} bytes",
              stream.eof(), stream.fail(), stream.bad(), count);
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
  app.description(version_text() + "\n");

  auto name = argc > 0 ? argv[0] : "barri-cli";

  auto* save
      = app.add_subcommand("save", "create a disaster recovery image")
            ->formatter(std::make_shared<SubcommandFormatter>(libbareos::format(
                R"(This command creates a barri backup of the currently running system
and outputs it to standard out.

As powershell can not reliably handle large, binary output,
we recommend executing {0} in cmd instead.

The standard output stream contains the whole backup image, so you need to save
it out to somewhere in case you need to restore it.

If you specify --dry, then the full process of preparing a backup will be
executed, but no backup will be written.

As {0} relies on volume shadow copies, it is important to make sure
that the service is running before you start the backup.

When executed in a terminal, the progress will be displayed in a progress bar.)",
                name)))
            ->fallthrough()
            ->footer(libbareos::format(R"(Examples:
  # create a backup, compress it and then send it to an offsite storage via ssh
  {0} save | zstd --stdout | ssh somewhere@else:/storage/backup.barri

  # make sure a backup is possible
  {0} save --dry

  # make a backup to an external usb drive and store a debug trace
  {0} save --trace 2>%TEMP%\debug.trace > F:\backup.barri)",
                                       name));
  save->add_flag("--dry", dry, "do not read/write actual disk data");

  save->add_flag("--save-unreferenced-disks", save_unreferenced_disks,
                 "save completey unsnapshotted disks");
  save->add_flag("--save-unreferenced-partitions", save_unreferenced_partitions,
                 "save even unsnapshotted partitions from disks that contain "
                 "snapshotted partitions");
  save->add_flag("--save-unreferenced-extents", save_unreferenced_extents,
                 "save even unsnapshotted data from partitions that contain "
                 "snapshotted data");

  save->add_option("--ignore-disks", ignored_disks,
                   "ids of disks to ignore (0, 1, 2, ...)");

  auto* restore
      = app.add_subcommand("restore",
                           "restore a barri backup to the given output(s)")
            ->formatter(std::make_shared<SubcommandFormatter>(libbareos::format(
                R"(This command restores a barri backup to some output.
The backup is read from stdin; alternatively you can specify a file to read from via the --file option.

If you can restore the disks either
 * as vhdx files, via the --to-vhdx-directory option,
 * as raw files, via the --to-raw-directory option, or
 * directly to disk(s), via the --to-disks option.

In the first two cases, {0} will create files called disk-X.[vhdx|raw] in the
given directory.

The --to-disks option expects as argument a space delimited list of disk ids.
I.e. if you want to restore the first disk to disk 2 and the second disk to
disk 1, then you have to specify "--to-disks 2 1".

You can find out the id of the disks via the disk manager in the gui,
"diskpart" in the cli or via the "Get-Disk" cmdlet in powershell.

When output to a terminal, the progress will be displayed in a progress bar.)",
                name)))
            ->fallthrough()
            ->footer(libbareos::format(R"(Examples:
  # restore the first disk to Disk 1, the second to Disk 3 and the third to Disk 2
  {0} restore --from backup.barri --to-disks 1 3 2

  # restore the disks as vhdx files into some directory
  # here `get-backup` is any command that outputs the barri image to stdout
  get-backup | {0} restore --to-vhdx-directory "C:\Users\Public\Documents\Hyper-V\Virtual Hard Disks\"

  # restore the disks as raw files in the TEMP directory
  {0} restore --to-raw-directory %TEMP% < backup.barri)",
                                       name));
  std::string filename;
  restore->add_option("--from", filename,
                      "read from the given file instead of stdin");

  auto* location = restore->add_option_group(
      "output", "select where the data will be restored to");
  std::wstring vhdx_dir;
  auto* vhdx_dir_target
      = location
            ->add_option("--to-vhdx-directory", vhdx_dir,
                         "create one vhdx image per restored drive in "
                         "the given directory")
            ->check(CLI::ExistingDirectory);
  std::wstring file_dir;
  auto* raw_dir_target
      = location
            ->add_option("--to-raw-directory", file_dir,
                         "create one raw image file per restored "
                         "drive in the given directory")
            ->check(CLI::ExistingDirectory);
  std::vector<std::size_t> disks;
  auto* disks_target = location->add_option("--to-disks", disks,
                                            "restore to the given disk ids");

  location->require_option(1);

  auto* version = app.add_subcommand("version", "output the version");

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  auto logger = progressbar::get(trace);

  try {
    if (*save) {
      logger->Info("{}", version_text());

      if (!SetConsoleCtrlHandler(&SignalHandler, TRUE)) {
        logger->Info("could not setup ctrl-c handler: Err={}", GetLastError());
      } else {
        logger->Trace("ctrl-c handler was setup");
      }

      _setmode(_fileno(stdout), _O_BINARY);
      dump_data(std::cout, logger);
    } else if (*restore) {
      logger->Info("{}", version_text());

      if (!SetConsoleCtrlHandler(&SignalHandler, TRUE)) {
        logger->Info("could not setup ctrl-c handler: Err={}", GetLastError());
      } else {
        logger->Trace("ctrl-c handler was setup");
      }

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
          logger->Trace("restore as vhdx files into {}", FromUtf16(vhdx_dir));
          handler = GetHandler(logger, vhdx_directory{vhdx_dir});
        } else if (*raw_dir_target) {
          logger->Trace("restore as raw files into {}", FromUtf16(file_dir));
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
      std::cout << version_text() << std::endl;
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[ERROR] " << ex.what() << std::endl;

    return 1;
  }
}
