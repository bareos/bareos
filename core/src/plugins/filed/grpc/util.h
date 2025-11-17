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

#ifndef BAREOS_PLUGINS_FILED_GRPC_UTIL_H_
#define BAREOS_PLUGINS_FILED_GRPC_UTIL_H_

#include <sys/stat.h>
#include "common.pb.h"

void time_native_to_grpc(google::protobuf::Timestamp* time, struct timespec t);

void time_grpc_to_native(struct timespec* t,
                         const google::protobuf::Timestamp& time);

void stat_native_to_grpc(bareos::common::OsStat* grpc, const struct stat* os);

void stat_grpc_to_native(struct stat* os, const bareos::common::OsStat& grpc);


#endif  // BAREOS_PLUGINS_FILED_GRPC_UTIL_H_
