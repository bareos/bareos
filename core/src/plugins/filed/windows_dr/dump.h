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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_DUMP_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_DUMP_H_

#include "plugins/filed/windows_dr/parser.h"
#include <span>
#include <optional>
#include <vector>
#include <Windows.h>

struct data_dumper;

using insert_bytes = std::vector<char>;
struct insert_from {
  HANDLE hndl;
  std::size_t offset;
  std::size_t length;
};

using insert_step = std::variant<insert_bytes, insert_from>;

using insert_plan = std::vector<insert_step>;

struct dump_context;

dump_context* make_context(GenericLogger* logger);

void dump_context_save_unknown_disks(dump_context*, bool);
void dump_context_save_unknown_partitions(dump_context*, bool);
void dump_context_save_unknown_extents(dump_context*, bool);
void dump_context_ignore_all_data(dump_context*, bool);
insert_plan dump_context_create_plan(dump_context* ctx);
void destroy_context(dump_context* ctx);

std::size_t compute_plan_size(const insert_plan& plan);

data_dumper* dumper_setup(GenericLogger* logger, insert_plan&& plan);
std::size_t dumper_write(data_dumper* dumper, std::span<char> data);
void dumper_stop(data_dumper* dumper);

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_DUMP_H_
