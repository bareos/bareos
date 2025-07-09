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


void restore_data(std::istream& stream, bool raw_file)
{
  do_restore(stream, progressbar::get(), raw_file);
}

void dump_data(std::ostream& stream, bool dry)
{
  COM_CALL(CoInitializeEx(NULL, COINIT_MULTITHREADED));

  struct CoUninitializer {
    ~CoUninitializer() { CoUninitialize(); }
  };

  CoUninitializer _{};

  std::vector<char> buffer;
  buffer.resize(1 << 20);

  dump_context* ctx = make_context();
  insert_plan plan = create_insert_plan(ctx, dry);

  auto dumper = dumper_setup(progressbar::get(), std::move(plan));

  for (;;) {
    auto count = dumper_write(dumper, buffer);
    if (!count) { break; }
    stream.write(buffer.data(), count);
  }

  dumper_stop(dumper);
  destroy_context(ctx);
}

int main(int argc, char* argv[])
{
  CLI::App app;

  auto* save = app.add_subcommand("save");
  bool dry = false;
  save->add_flag("--dry", dry, "do not read/write actual disk data");
  auto* restore = app.add_subcommand("restore");
  std::string filename;
  restore->add_option("--from", filename,
                      "read from this file instead of stdin");
  bool raw_file;
  restore->add_flag("--raw", raw_file);

  auto* version = app.add_subcommand("version");

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);
  try {
    if (*save) {
      _setmode(_fileno(stdout), _O_BINARY);
      dump_data(std::cout, dry);
    } else if (*restore) {
      if (filename.empty()) {
        _setmode(_fileno(stdin), _O_BINARY);
        restore_data(std::cin, raw_file);
      } else {
        fprintf(stderr, "using %s as input\n", filename.c_str());
        std::ifstream infile{filename,
                             std::ios_base::in | std::ios_base::binary};
        restore_data(infile, raw_file);
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
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;

    return 1;
  }
}
