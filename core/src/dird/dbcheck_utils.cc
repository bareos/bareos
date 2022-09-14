/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2022 Bareos GmbH & Co. KG

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

#include "dbcheck_utils.h"

using namespace directordaemon;

int PrintNameHandler([[maybe_unused]] void* ctx,
                     [[maybe_unused]] int num_fields,
                     char** row)
{
  if (row[0]) { printf("%s\n", row[0]); }
  return 0;
}

int GetNameHandler(void* ctx, [[maybe_unused]] int num_fields, char** row)
{
  POOLMEM* name = (POOLMEM*)ctx;

  if (row[0]) { PmStrcpy(name, row[0]); }
  return 0;
}

int PrintJobHandler([[maybe_unused]] void* ctx,
                    [[maybe_unused]] int num_fields,
                    char** row)
{
  printf(_("JobId=%s Name=\"%s\" StartTime=%s\n"), NPRT(row[0]), NPRT(row[1]),
         NPRT(row[2]));
  return 0;
}

int PrintJobmediaHandler([[maybe_unused]] void* ctx,
                         [[maybe_unused]] int num_fields,
                         char** row)
{
  printf(_("Orphaned JobMediaId=%s JobId=%s Volume=\"%s\"\n"), NPRT(row[0]),
         NPRT(row[1]), NPRT(row[2]));
  return 0;
}

int PrintFileHandler([[maybe_unused]] void* ctx,
                     [[maybe_unused]] int num_fields,
                     char** row)
{
  printf(_("Orphaned FileId=%s JobId=%s Volume=\"%s\"\n"), NPRT(row[0]),
         NPRT(row[1]), NPRT(row[2]));
  return 0;
}

int PrintFilesetHandler([[maybe_unused]] void* ctx,
                        [[maybe_unused]] int num_fields,
                        char** row)
{
  printf(_("Orphaned FileSetId=%s FileSet=\"%s\" MD5=%s\n"), NPRT(row[0]),
         NPRT(row[1]), NPRT(row[2]));
  return 0;
}

int PrintClientHandler([[maybe_unused]] void* ctx,
                       [[maybe_unused]] int num_fields,
                       char** row)
{
  printf(_("Orphaned ClientId=%s Name=\"%s\"\n"), NPRT(row[0]), NPRT(row[1]));
  return 0;
}

// Called here with each id to be added to the list
int IdListHandler(void* ctx, [[maybe_unused]] int num_fields, char** row)
{
  ID_LIST* lst = (ID_LIST*)ctx;

  if (lst->num_ids == MAX_ID_LIST_LEN) { return 1; }
  if (lst->num_ids == lst->max_ids) {
    if (lst->max_ids == 0) {
      lst->max_ids = 10000;
      lst->Id = (int64_t*)malloc(sizeof(int64_t) * lst->max_ids);
    } else {
      lst->max_ids = (lst->max_ids * 3) / 2;
      lst->Id = (int64_t*)realloc(lst->Id, sizeof(int64_t) * lst->max_ids);
    }
  }
  lst->Id[lst->num_ids++] = str_to_int64(row[0]);
  return 0;
}

// Construct record id list
int MakeIdList(BareosDb* db, const char* query, ID_LIST* id_list)
{
  id_list->num_ids = 0;
  id_list->num_del = 0;
  id_list->tot_ids = 0;

  if (!db->SqlQuery(query, IdListHandler, (void*)id_list)) {
    printf("%s", db->strerror());
    return 0;
  }
  return 1;
}

// Called here with each name to be added to the list
int NameListHandler(void* ctx, [[maybe_unused]] int num_fields, char** row)
{
  NameList* name = (NameList*)ctx;

  if (name->num_ids == MAX_ID_LIST_LEN) { return 1; }
  if (name->num_ids == name->max_ids) {
    if (name->max_ids == 0) {
      name->max_ids = 10000;
      name->name = (char**)malloc(sizeof(char*) * name->max_ids);
    } else {
      name->max_ids = (name->max_ids * 3) / 2;
      name->name = (char**)realloc(name->name, sizeof(char*) * name->max_ids);
    }
  }
  name->name[name->num_ids++] = strdup(row[0]);
  return 0;
}

// Construct name list
int MakeNameList(BareosDb* db, const char* query, NameList* name_list)
{
  name_list->num_ids = 0;
  name_list->num_del = 0;
  name_list->tot_ids = 0;

  if (!db->SqlQuery(query, NameListHandler, (void*)name_list)) {
    printf("%s", db->strerror());
    return 0;
  }
  return 1;
}

// Print names in the list
void PrintNameList(NameList* name_list)
{
  for (int i = 0; i < name_list->num_ids; i++) {
    printf("%s\n", name_list->name[i]);
  }
}

// Free names in the list
void FreeNameList(NameList* name_list)
{
  for (int i = 0; i < name_list->num_ids; i++) { free(name_list->name[i]); }
  name_list->num_ids = 0;
  name_list->max_ids = 0;
  free(name_list->name);
}

std::vector<int> get_deletable_storageids(
    BareosDb* db,
    std::vector<std::string> orphaned_storage_names_list)
{
  std::string query = "SELECT storageid FROM storage WHERE Name in (";
  for (const auto& orphaned_element : orphaned_storage_names_list) {
    query += "'" + orphaned_element + "',";
  }
  query.pop_back();
  query += ")";

  ID_LIST orphaned_storage_ids_list{};
  if (!MakeIdList(db, query.c_str(), &orphaned_storage_ids_list)) { exit(1); }

  std::vector<int> storage_ids_to_delete;
  NameList volume_names = {};

  for (int orphaned_storage_id = 0;
       orphaned_storage_id < orphaned_storage_ids_list.num_ids;
       ++orphaned_storage_id) {
    std::string media_query
        = "SELECT volumename FROM media WHERE storageid="
          + std::to_string(orphaned_storage_ids_list.Id[orphaned_storage_id]);

    if (!MakeNameList(db, media_query.c_str(), &volume_names)) { exit(1); }

    if (volume_names.num_ids > 0) {
      for (int volumename = 0; volumename < volume_names.num_ids;
           ++volumename) {
        Emsg3(M_WARNING, 0,
              _("Orphaned storage '%s' is being used by volume '%s'. "
                "An orphaned storage will only be removed when it is "
                "no longer referenced.\n"),
              orphaned_storage_names_list[orphaned_storage_id].c_str(),
              volume_names.name[volumename]);
      }
    }

    NameList device_names{};
    std::string device_query
        = "SELECT name FROM device WHERE storageid="
          + std::to_string(orphaned_storage_ids_list.Id[orphaned_storage_id]);

    if (!MakeNameList(db, device_query.c_str(), &device_names)) { exit(1); }

    if (device_names.num_ids > 0) {
      for (int devicename = 0; devicename < device_names.num_ids;
           ++devicename) {
        Emsg3(M_WARNING, 0,
              _("Orphaned storage '%s' is being used by device '%s'. "
                "An orphaned storage will only be removed when it is "
                "no longer referenced.\n"),
              orphaned_storage_names_list[orphaned_storage_id].c_str(),
              device_names.name[devicename]);
      }
    }

    if (volume_names.num_ids == 0 && device_names.num_ids == 0) {
      storage_ids_to_delete.push_back(
          orphaned_storage_ids_list.Id[orphaned_storage_id]);
    }
  }
  return storage_ids_to_delete;
}

static std::vector<std::string> get_configuration_storages()
{
  std::vector<std::string> config_storage_names_list;
  BareosResource* storage_ressource{nullptr};

  foreach_res (storage_ressource, R_STORAGE) {
    config_storage_names_list.push_back(storage_ressource->resource_name_);
  }

  return config_storage_names_list;
}

std::vector<std::string> get_orphaned_storages_names(BareosDb* db)
{
  std::vector<std::string> config_storage_names_list
      = get_configuration_storages();

  std::string query = "SELECT name FROM storage";

  NameList database_storage_names_list{};
  if (!MakeNameList(db, query.c_str(), &database_storage_names_list)) {
    exit(1);
  }

  std::vector<std::string> orphaned_storage_names_list;

  for (int i = 0; i < database_storage_names_list.num_ids; ++i) {
    if (std::find(config_storage_names_list.begin(),
                  config_storage_names_list.end(),
                  std::string(database_storage_names_list.name[i]))
        == config_storage_names_list.end()) {
      orphaned_storage_names_list.push_back(
          database_storage_names_list.name[i]);
    }
  }

  FreeNameList(&database_storage_names_list);
  return orphaned_storage_names_list;
}

void delete_storages(BareosDb* db, std::vector<int> storages_to_be_deleted)
{
  for (auto const& storageid : storages_to_be_deleted) {
    std::string delete_query
        = "DELETE FROM storage WHERE storageid=" + std::to_string(storageid);

    db->SqlQuery(delete_query.c_str(), nullptr, nullptr);
  }
}
