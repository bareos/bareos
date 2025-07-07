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

struct data_dumper;

data_dumper* dumper_setup(GenericLogger* logger, bool dry);

std::size_t dumper_write(data_dumper* dumper, std::span<char> data);

void dumper_stop(data_dumper* dumper);

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_DUMP_H_
