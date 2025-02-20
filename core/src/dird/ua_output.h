/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_UA_OUTPUT_H_
#define BAREOS_DIRD_UA_OUTPUT_H_

#include "lib/output_formatter.h"
#include "dird/dird_conf.h"

#include <unordered_map>

namespace directordaemon {

static std::vector<std::pair<const char*, int>> show_cmd_available_resources = {
    {"directors", R_DIRECTOR}, {"clients", R_CLIENT},   {"jobs", R_JOB},
    {"jobdefs", R_JOBDEFS},    {"storages", R_STORAGE}, {"catalogs", R_CATALOG},
    {"schedules", R_SCHEDULE}, {"filesets", R_FILESET}, {"pools", R_POOL},
    {"messages", R_MSGS},      {"counters", R_COUNTER}, {"profiles", R_PROFILE},
    {"consoles", R_CONSOLE},   {"users", R_USER}};


class RunResource;

bool bsendmsg(void* ua_ctx, const char* fmt, ...);
of_filter_state filterit(void* ctx, void* data, of_filter_tuple* tuple);
bool printit(void* ctx, const char* msg);
bool sprintit(void* ctx, const char* fmt, ...);
bool CompleteJcrForJob(JobControlRecord* jcr,
                       JobResource* job,
                       PoolResource* pool);
} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_OUTPUT_H_
