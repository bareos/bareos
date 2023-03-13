/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2021 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_DIRD_DBCHECK_UTILS_H_
#define BAREOS_DIRD_DBCHECK_UTILS_H_

#include "include/bareos.h"
#include "cats/cats.h"
#include "lib/runscript.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include <algorithm>

using namespace directordaemon;

extern bool ParseDirConfig(const char* configfile, int exit_code);

typedef struct s_id_ctx {
  int64_t* Id; /* ids to be modified */
  int num_ids; /* ids stored */
  int max_ids; /* size of array */
  int num_del; /* number deleted */
  int tot_ids; /* total to process */
} ID_LIST;

typedef struct s_name_ctx {
  char** name; /* list of names */
  int num_ids; /* ids stored */
  int max_ids; /* size of array */
  int num_del; /* number deleted */
  int tot_ids; /* total to process */
} NameList;

#define MAX_ID_LIST_LEN 10000000

// Helper functions
int PrintNameHandler(void* ctx, int num_fields, char** row);
int GetNameHandler(void* ctx, int num_fields, char** row);
int PrintJobHandler(void* ctx, int num_fields, char** row);
int PrintJobmediaHandler(void* ctx, int num_fields, char** row);
int PrintFileHandler(void* ctx, int num_fields, char** row);
int PrintFilesetHandler(void* ctx, int num_fields, char** row);
int PrintClientHandler(void* ctx, int num_fields, char** row);
int IdListHandler(void* ctx, int num_fields, char** row);
int MakeIdList(BareosDb* db, const char* query, ID_LIST* id_list);
int NameListHandler(void* ctx, int num_fields, char** row);
int MakeNameList(BareosDb* db, const char* query, s_name_ctx* name_list);
void PrintNameList(s_name_ctx* name_list);
void FreeNameList(s_name_ctx* name_list);

std::vector<int> get_deletable_storageids(
    BareosDb* db,
    std::vector<std::string> orphaned_storage_names_list);
std::vector<std::string> get_orphaned_storages_names(BareosDb* db);
void delete_storages(BareosDb* db, std::vector<int> storages_to_be_deleted);


#endif  // BAREOS_DIRD_DBCHECK_UTILS_H_
