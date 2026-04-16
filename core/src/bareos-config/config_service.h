/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_BAREOS_CONFIG_CONFIG_SERVICE_H_
#define BAREOS_BAREOS_CONFIG_CONFIG_SERVICE_H_

#include <filesystem>
#include <vector>

#include "http_server.h"

struct ConfigServiceOptions {
  std::vector<std::filesystem::path> config_roots;
};

HttpResponse HandleConfigServiceRequest(const ConfigServiceOptions& options,
                                        const HttpRequest& request);

#endif  // BAREOS_BAREOS_CONFIG_CONFIG_SERVICE_H_
