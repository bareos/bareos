/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
// Kern E. Sibbald, August 2002
// Program to check a BAREOS database for consistency and to make repairs

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "cats/cats.h"
#include "lib/runscript.h"
#include "lib/cli.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include "dbcheck_utils.h"

using namespace directordaemon;

extern bool ParseDirConfig(const char* configfile, int exit_code);

// Global variables
static bool fix = false;
static bool batch = false;
static BareosDb* db;
static ID_LIST id_list;
static NameList name_list;
static char buf[20000];
static bool quit = false;
static const char* idx_tmp_name;

// Forward referenced functions
static void set_quit();
static void toggle_modify();
static void toggle_verbose();
static void eliminate_duplicate_paths();
static void eliminate_orphaned_jobmedia_records();
static void eliminate_orphaned_file_records();
static void eliminate_orphaned_path_records();
static void eliminate_orphaned_fileset_records();
static void eliminate_orphaned_client_records();
static void eliminate_orphaned_job_records();
static void eliminate_admin_records();
static void eliminate_orphaned_storage_records();
static void eliminate_restore_records();
static void repair_bad_paths();
static void repair_bad_filenames();
static void run_all_commands();

struct dbcheck_cmdstruct {
  void (*func)();           /**< Handler */
  const char* description;  /**< Main purpose */
  const bool baserepaircmd; /**< command that modifies the database */
};

static struct dbcheck_cmdstruct commands[] = {
    {set_quit, "Quit", false},
    {toggle_modify, "Toggle modify database flag", false},
    {toggle_verbose, "Toggle verbose flag", false},
    {repair_bad_filenames, "Check for bad Filename records", true},
    {repair_bad_paths, "Check for bad Path records", true},
    {eliminate_duplicate_paths, "Check for duplicate Path records", true},
    {eliminate_orphaned_jobmedia_records, "Check for orphaned Jobmedia records",
     true},
    {eliminate_orphaned_file_records, "Check for orphaned File records", true},
    {eliminate_orphaned_path_records, "Check for orphaned Path records", true},
    {eliminate_orphaned_fileset_records, "Check for orphaned FileSet records",
     true},
    {eliminate_orphaned_client_records, "Check for orphaned Client records",
     true},
    {eliminate_orphaned_job_records, "Check for orphaned Job records", true},
    {eliminate_orphaned_storage_records, "Check for orphaned storage records",
     true},
    {eliminate_admin_records, "Check for all Admin records", true},
    {eliminate_restore_records, "Check for all Restore records", true},
    {run_all_commands, "Run ALL checks", false},
};

const int number_commands
    = (sizeof(commands) / sizeof(struct dbcheck_cmdstruct));

// helper functions

// Gen next input command from the terminal
static char* GetCmd(const char* prompt)
{
  static char cmd[1000];

  printf("%s", prompt);
  fflush(stdout);
  if (fgets(cmd, sizeof(cmd), stdin) == nullptr) {
    printf("\n");
    quit = true;
    return nullptr;
  }
  StripTrailingJunk(cmd);
  return cmd;
}

static bool yes_no(const char* prompt, bool batchvalue = true)
{
  char* cmd;
  // return the batchvalue if batch operation is set
  if (batch) { return batchvalue; }
  cmd = GetCmd(prompt);
  if (!cmd) {
    quit = true;
    return false;
  }
  return (Bstrcasecmp(cmd, "yes")) || (Bstrcasecmp(cmd, T_("yes")));
}

static void set_quit() { quit = true; }

static void toggle_modify()
{
  fix = !fix;
  if (fix)
    printf(T_("Database will be modified.\n"));
  else
    printf(T_("Database will NOT be modified.\n"));
}

static void toggle_verbose()
{
  verbose = verbose ? 0 : 1;
  if (verbose)
    printf(T_(" Verbose is on.\n"));
  else
    printf(T_(" Verbose is off.\n"));
}


static void PrintCatalogDetails(CatalogResource* catalog)
{
  POOLMEM* catalog_details = GetPoolMemory(PM_MESSAGE);

  // Instantiate a BareosDb class and see what db_type gets assigned to it.
  db = db_init_database(nullptr, catalog->db_driver, catalog->db_name,
                        catalog->db_user, catalog->db_password.value,
                        catalog->db_address, catalog->db_port,
                        catalog->db_socket, catalog->mult_db_connections,
                        catalog->disable_batch_insert, catalog->try_reconnect,
                        catalog->exit_on_fatal);
  if (db) {
    printf("%sdb_type=%s\nworking_dir=%s\n", catalog->display(catalog_details),
           db->GetType(), working_directory);
    db->CloseDatabase(nullptr);
  }
  FreePoolMemory(catalog_details);
}
/**
 * database index handling functions
 *
 * The code below to add indexes is needed only for MySQL, and
 *  that to improve the performance.
 */

#define MAXIDX 100
typedef struct s_idx_list {
  char* key_name;
  int count_key; /* how many times the index meets *key_name */
  int CountCol;  /* how many times meets the desired column name */
} IDX_LIST;

// Drop temporary index
static bool DropTmpIdx(const char* idx_name, const char* table_name)
{
  if (idx_tmp_name != nullptr) {
    printf(T_("Drop temporary index.\n"));
    fflush(stdout);
    Bsnprintf(buf, sizeof(buf), "DROP INDEX %s ON %s", idx_name, table_name);
    if (verbose) { printf("%s\n", buf); }
    if (!db->SqlQuery(buf, nullptr, nullptr)) {
      printf("%s\n", db->strerror());
      return false;
    } else {
      if (verbose) {
        printf(T_("Temporary index %s deleted.\n"), idx_tmp_name);
      }
    }
    fflush(stdout);
  }
  idx_tmp_name = nullptr;
  return true;
}

// Delete all entries in the list
static int DeleteIdList(const char* query, ID_LIST* id_list)
{
  char ed1[50];

  for (int i = 0; i < id_list->num_ids; i++) {
    Bsnprintf(buf, sizeof(buf), query, edit_int64(id_list->Id[i], ed1));
    if (verbose) { printf(T_("Deleting: %s\n"), buf); }
    db->SqlQuery(buf, nullptr, nullptr);
  }
  return 1;
}

static void eliminate_duplicate_paths()
{
  const char* query;
  char esc_name[5000];

  printf(T_("Checking for duplicate Path entries.\n"));
  fflush(stdout);

  // Make list of duplicated names
  query
      = "SELECT Path, count(Path) as Count FROM Path "
        "GROUP BY Path HAVING count(Path) > 1";

  if (!MakeNameList(db, query, &name_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d duplicate Path records.\n"), name_list.num_ids);
  fflush(stdout);
  if (name_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    PrintNameList(&name_list);
  }
  if (quit) { return; }
  if (fix) {
    // Loop through list of duplicate names
    for (int i = 0; i < name_list.num_ids; i++) {
      // Get all the Ids of each name
      db->EscapeString(nullptr, esc_name, name_list.name[i],
                       strlen(name_list.name[i]));
      Bsnprintf(buf, sizeof(buf), "SELECT PathId FROM Path WHERE Path='%s'",
                esc_name);
      if (verbose > 1) { printf("%s\n", buf); }
      if (!MakeIdList(db, buf, &id_list)) { exit(BEXIT_FAILURE); }
      if (verbose) {
        printf(T_("Found %d for: %s\n"), id_list.num_ids, name_list.name[i]);
      }
      // Force all records to use the first id then delete the other ids
      for (int j = 1; j < id_list.num_ids; j++) {
        char ed1[50], ed2[50];
        Bsnprintf(buf, sizeof(buf), "UPDATE File SET PathId=%s WHERE PathId=%s",
                  edit_int64(id_list.Id[0], ed1),
                  edit_int64(id_list.Id[j], ed2));
        if (verbose > 1) { printf("%s\n", buf); }
        db->SqlQuery(buf, nullptr, nullptr);
        Bsnprintf(buf, sizeof(buf), "DELETE FROM Path WHERE PathId=%s", ed2);
        if (verbose > 2) { printf("%s\n", buf); }
        db->SqlQuery(buf, nullptr, nullptr);
      }
    }
    fflush(stdout);
  }
  FreeNameList(&name_list);
}

// repair functions

static void eliminate_orphaned_jobmedia_records()
{
  const char* query
      = "SELECT JobMedia.JobMediaId,Job.JobId FROM JobMedia "
        "LEFT OUTER JOIN Job USING(JobId) "
        "WHERE Job.JobId IS NULL LIMIT 300000";

  printf(T_("Checking for orphaned JobMedia entries.\n"));
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  // Loop doing 300000 at a time
  while (id_list.num_ids != 0) {
    printf(T_("Found %d orphaned JobMedia records.\n"), id_list.num_ids);
    if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
      for (int i = 0; i < id_list.num_ids; i++) {
        char ed1[50];
        Bsnprintf(
            buf, sizeof(buf),
            "SELECT JobMedia.JobMediaId,JobMedia.JobId,Media.VolumeName FROM "
            "JobMedia,Media "
            "WHERE JobMedia.JobMediaId=%s AND Media.MediaId=JobMedia.MediaId",
            edit_int64(id_list.Id[i], ed1));
        if (!db->SqlQuery(buf, PrintJobmediaHandler, nullptr)) {
          printf("%s\n", db->strerror());
        }
      }
    }
    if (quit) { return; }

    if (fix && id_list.num_ids > 0) {
      printf(T_("Deleting %d orphaned JobMedia records.\n"), id_list.num_ids);
      DeleteIdList("DELETE FROM JobMedia WHERE JobMediaId=%s", &id_list);
    } else {
      break; /* get out if not updating db */
    }
    if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  }
  fflush(stdout);
}

static void eliminate_orphaned_file_records()
{
  const char* query
      = "SELECT File.FileId,Job.JobId FROM File "
        "LEFT OUTER JOIN Job USING (JobId) "
        "WHERE Job.JobId IS NULL LIMIT 300000";

  printf(T_("Checking for orphaned File entries. This may take some time!\n"));
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  // Loop doing 300000 at a time
  while (id_list.num_ids != 0) {
    printf(T_("Found %d orphaned File records.\n"), id_list.num_ids);
    if (name_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
      for (int i = 0; i < id_list.num_ids; i++) {
        char ed1[50];
        Bsnprintf(buf, sizeof(buf),
                  "SELECT File.FileId,File.JobId,File.Name FROM File "
                  "WHERE File.FileId=%s",
                  edit_int64(id_list.Id[i], ed1));
        if (!db->SqlQuery(buf, PrintFileHandler, nullptr)) {
          printf("%s\n", db->strerror());
        }
      }
    }
    if (quit) { return; }
    if (fix && id_list.num_ids > 0) {
      printf(T_("Deleting %d orphaned File records.\n"), id_list.num_ids);
      DeleteIdList("DELETE FROM File WHERE FileId=%s", &id_list);
    } else {
      break; /* get out if not updating db */
    }
    if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  }
  fflush(stdout);
}

static void eliminate_orphaned_path_records()
{
  db_int64_ctx lctx;
  PoolMem query(PM_MESSAGE);

  lctx.count = 0;
  idx_tmp_name = nullptr;

  db->FillQuery(query, BareosDb::SQL_QUERY::get_orphaned_paths_0);

  printf(T_("Checking for orphaned Path entries. This may take some time!\n"));
  if (verbose > 1) { printf("%s\n", query.c_str()); }
  fflush(stdout);
  if (!MakeIdList(db, query.c_str(), &id_list)) { exit(BEXIT_FAILURE); }
  // Loop doing 300000 at a time
  while (id_list.num_ids != 0) {
    printf(T_("Found %d orphaned Path records.\n"), id_list.num_ids);
    fflush(stdout);
    if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
      for (int i = 0; i < id_list.num_ids; i++) {
        char ed1[50];
        Bsnprintf(buf, sizeof(buf), "SELECT Path FROM Path WHERE PathId=%s",
                  edit_int64(id_list.Id[i], ed1));
        db->SqlQuery(buf, PrintNameHandler, nullptr);
      }
      fflush(stdout);
    }
    if (quit) { return; }
    if (fix && id_list.num_ids > 0) {
      printf(T_("Deleting %d orphaned Path records.\n"), id_list.num_ids);
      fflush(stdout);
      DeleteIdList("DELETE FROM Path WHERE PathId=%s", &id_list);
    } else {
      break; /* get out if not updating db */
    }
    if (!MakeIdList(db, query.c_str(), &id_list)) { exit(BEXIT_FAILURE); }
  }
}

static void eliminate_orphaned_fileset_records()
{
  const char* query;

  printf(T_("Checking for orphaned FileSet entries. This takes some time!\n"));
  query
      = "SELECT FileSet.FileSetId,Job.FileSetId FROM FileSet "
        "LEFT OUTER JOIN Job USING(FileSetId) "
        "WHERE Job.FileSetId IS NULL";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d orphaned FileSet records.\n"), id_list.num_ids);
  fflush(stdout);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT FileSetId,FileSet,MD5 FROM FileSet "
                "WHERE FileSetId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintFilesetHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(T_("Deleting %d orphaned FileSet records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM FileSet WHERE FileSetId=%s", &id_list);
  }
}

static void eliminate_orphaned_client_records()
{
  const char* query;

  printf(T_("Checking for orphaned Client entries.\n"));
  /* In English:
   *   Wiffle through Client for every Client
   *   joining with the Job table including every Client even if
   *   there is not a match in Job (left outer join), then
   *   filter out only those where no Job points to a Client
   *   i.e. Job.Client is NULL */
  query
      = "SELECT Client.ClientId,Client.Name FROM Client "
        "LEFT OUTER JOIN Job USING(ClientId) "
        "WHERE Job.ClientId IS NULL";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d orphaned Client records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT ClientId,Name FROM Client "
                "WHERE ClientId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintClientHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(T_("Deleting %d orphaned Client records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Client WHERE ClientId=%s", &id_list);
  }
}

static void eliminate_orphaned_job_records()
{
  const char* query;

  printf(T_("Checking for orphaned Job entries.\n"));
  /* In English:
   *   Wiffle through Job for every Job
   *   joining with the Client table including every Job even if
   *   there is not a match in Client (left outer join), then
   *   filter out only those where no Client exists
   *   i.e. Client.Name is NULL */
  query
      = "SELECT Job.JobId,Job.Name FROM Job "
        "LEFT OUTER JOIN Client USING(ClientId) "
        "WHERE Client.Name IS NULL";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d orphaned Job records.\n"), id_list.num_ids);
  fflush(stdout);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT JobId,Name,StartTime FROM Job "
                "WHERE JobId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintJobHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(T_("Deleting %d orphaned Job records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Job WHERE JobId=%s", &id_list);
    printf(T_("Deleting JobMedia records of orphaned Job records.\n"));
    fflush(stdout);
    DeleteIdList("DELETE FROM JobMedia WHERE JobId=%s", &id_list);
    printf(T_("Deleting Log records of orphaned Job records.\n"));
    fflush(stdout);
    DeleteIdList("DELETE FROM Log WHERE JobId=%s", &id_list);
  }
}

static void eliminate_orphaned_storage_records()
{
  printf(T_("Checking for orphaned Storage entries.\n"));
  fflush(stdout);

  std::vector<std::string> orphaned_storage_names_list
      = get_orphaned_storages_names(db);

  printf(T_("Found %zu orphaned Storage records.\n"),
         orphaned_storage_names_list.size());

  std::vector<int> storages_to_be_deleted;

  if (orphaned_storage_names_list.size() > 0) {
    if (verbose && yes_no(T_("Print orhpaned storages? (yes/no): "))) {
      for (auto const& storage : orphaned_storage_names_list) {
        printf("'%s'\n", storage.c_str());
      }
      fflush(stdout);
    }

    storages_to_be_deleted
        = get_deletable_storageids(db, orphaned_storage_names_list);
  }
  if (quit) { return; }
  if (fix && storages_to_be_deleted.size() > 0) {
    printf(T_("Deleting %zu orphaned storage records.\n"),
           storages_to_be_deleted.size());
    fflush(stdout);
    delete_storages(db, storages_to_be_deleted);
  }
}

static void eliminate_admin_records()
{
  const char* query;

  printf(T_("Checking for Admin Job entries.\n"));
  query
      = "SELECT Job.JobId FROM Job "
        "WHERE Job.Type='D'";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d Admin Job records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT JobId,Name,StartTime FROM Job "
                "WHERE JobId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintJobHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(T_("Deleting %d Admin Job records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Job WHERE JobId=%s", &id_list);
  }
}

static void eliminate_restore_records()
{
  const char* query;

  printf(T_("Checking for Restore Job entries.\n"));
  query
      = "SELECT Job.JobId FROM Job "
        "WHERE Job.Type='R'";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d Restore Job records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT JobId,Name,StartTime FROM Job "
                "WHERE JobId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintJobHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(T_("Deleting %d Restore Job records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Job WHERE JobId=%s", &id_list);
  }
}

static void repair_bad_filenames()
{
  const char* query;
  int i;

  printf(T_("Checking for Filenames with a trailing slash\n"));
  query
      = "SELECT FileId,Name from File "
        "WHERE Name LIKE '%/'";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(db, query, &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d bad Filename records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Name FROM File WHERE FileId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintNameHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    POOLMEM* name = GetPoolMemory(PM_FNAME);
    char esc_name[5000];
    printf(T_("Reparing %d bad Filename records.\n"), id_list.num_ids);
    fflush(stdout);
    for (i = 0; i < id_list.num_ids; i++) {
      int len;
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Name FROM File WHERE FileId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, GetNameHandler, name)) {
        printf("%s\n", db->strerror());
      }
      // Strip trailing slash(es)
      for (len = strlen(name); len > 0 && IsPathSeparator(name[len - 1]);
           len--) {}
      if (len == 0) {
        len = 1;
        esc_name[0] = ' ';
        esc_name[1] = 0;
      } else {
        name[len - 1] = 0;
        db->EscapeString(nullptr, esc_name, name, len);
      }
      Bsnprintf(buf, sizeof(buf), "UPDATE File SET Name='%s' WHERE FileId=%s",
                esc_name, edit_int64(id_list.Id[i], ed1));
      if (verbose > 1) { printf("%s\n", buf); }
      db->SqlQuery(buf, nullptr, nullptr);
    }
    FreePoolMemory(name);
  }
  fflush(stdout);
}

static void repair_bad_paths()
{
  PoolMem query(PM_MESSAGE);
  int i;

  printf(T_("Checking for Paths without a trailing slash\n"));
  db->FillQuery(query, BareosDb::SQL_QUERY::get_bad_paths_0);
  if (verbose > 1) { printf("%s\n", query.c_str()); }
  fflush(stdout);
  if (!MakeIdList(db, query.c_str(), &id_list)) { exit(BEXIT_FAILURE); }
  printf(T_("Found %d bad Path records.\n"), id_list.num_ids);
  fflush(stdout);
  if (id_list.num_ids && verbose && yes_no(T_("Print them? (yes/no): "))) {
    for (i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Path FROM Path WHERE PathId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintNameHandler, nullptr)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    POOLMEM* name = GetPoolMemory(PM_FNAME);
    char esc_name[5000];
    printf(T_("Reparing %d bad Filename records.\n"), id_list.num_ids);
    fflush(stdout);
    for (i = 0; i < id_list.num_ids; i++) {
      int len;
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Path FROM Path WHERE PathId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, GetNameHandler, name)) {
        printf("%s\n", db->strerror());
      }
      // Strip trailing blanks
      for (len = strlen(name); len > 0 && name[len - 1] == ' '; len--) {
        name[len - 1] = 0;
      }
      // Add trailing slash
      len = PmStrcat(name, "/");
      db->EscapeString(nullptr, esc_name, name, len);
      Bsnprintf(buf, sizeof(buf), "UPDATE Path SET Path='%s' WHERE PathId=%s",
                esc_name, edit_int64(id_list.Id[i], ed1));
      if (verbose > 1) { printf("%s\n", buf); }
      db->SqlQuery(buf, nullptr, nullptr);
    }
    fflush(stdout);
    FreePoolMemory(name);
  }
}

static void run_all_commands()
{
  for (int i = 0; i < number_commands; i++) {
    if (commands[i].baserepaircmd) {
      printf("===========================================================\n");
      printf("=\n");
      printf("= %s, modify=%d\n", commands[i].description, fix);
      printf("=\n");

      /* execute the real function */
      (commands[i].func)();

      printf("=\n");
      printf("=\n");
      printf(
          "==========================================================="
          "\n\n\n\n");
    }
  }
}

static void print_commands()
{
  for (int i = 0; i < number_commands; i++) {
    printf("    %2d) %s\n", i, commands[i].description);
  }
}

static void do_interactive_mode()
{
  const char* cmd;

  printf(T_("Hello, this is the Bareos database check/correct program.\n"));

  while (!quit) {
    if (fix)
      printf(T_("Modify database is on."));
    else
      printf(T_("Modify database is off."));
    if (verbose)
      printf(T_(" Verbose is on.\n"));
    else
      printf(T_(" Verbose is off.\n"));

    printf(T_("Please select the function you want to perform.\n"));

    print_commands();
    cmd = GetCmd(T_("Select function number: "));
    if (cmd) {
      int item = atoi(cmd);
      if ((item >= 0) && (item < number_commands)) {
        /* run specified function */
        (commands[item].func)();
      }
    }
  }
}

// main
int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  MyNameIs(argc, argv, "dbcheck");
  InitMsg(nullptr, nullptr); /* setup message handler */

  OSDependentInit();

  memset(&id_list, 0, sizeof(id_list));
  memset(&name_list, 0, sizeof(name_list));

  CLI::App dbcheck_app;
  InitCLIApp(dbcheck_app, "The Bareos Database Checker.", 2002);

  std::string configfile;
  auto config_arg = dbcheck_app
                        .add_option("-c,--config", configfile,
                                    "Use <path> as Director configuration "
                                    "filename or configuration directory.")
                        ->check(CLI::ExistingPath)
                        ->type_name("<path>");

  bool print_catalog = false;
  dbcheck_app.add_flag("-B,--print-catalog", print_catalog,
                       "Print catalog configuration and exit.");

  dbcheck_app.add_flag("-b,--batch", batch, "Batch mode.");

  std::string catalogname;
  dbcheck_app
      .add_option("-C,--catalog", catalogname,
                  "Catalog name in the director configuration file.")
      ->type_name("<catalog>");

  std::string driver_name;
  dbcheck_app.add_option("-D,--driver", driver_name,
                         "Exists for backwards compatibility and is ignored.");

  AddDebugOptions(dbcheck_app);

  dbcheck_app.add_flag("-f,--fix", fix, "Fix inconsistencies.");

  AddVerboseOption(dbcheck_app);

  auto manual_args
      = dbcheck_app
            .add_option_group("Manual credentials",
                              "Setting database credentials manually. Can only "
                              "be used when no configuration is given.")
            ->excludes(config_arg);

  std::string workingdir;
  manual_args->add_option("working_directory", workingdir,
                          "Path to working directory.");

  std::string db_name = "bareos";
  manual_args->add_option("database_name", db_name, "Database name.");

  std::string user = db_name;
  manual_args->add_option("user", user, "Database user name.");

  std::string password = "";
  manual_args->add_option("password", password, "Database password.");

  std::string dbhost = "";
  manual_args->add_option("host", dbhost, "Database host.");

  int dbport = 0;
  manual_args->add_option("port", dbport, "Database port")
      ->check(CLI::PositiveNumber);

  ParseBareosApp(dbcheck_app, argc, argv);

  const char* db_driver = "postgresql";

  if (!configfile.empty() || manual_args->count_all() == 0) {
    CatalogResource* catalog = nullptr;
    int found = 0;

    my_config = InitDirConfig(configfile.c_str(), M_CONFIG_ERROR);
    my_config->ParseConfigOrExit();

    foreach_res (catalog, R_CATALOG) {
      if (!catalogname.empty()
          && bstrcmp(catalog->resource_name_, catalogname.c_str())) {
        ++found;
        break;
      } else if (catalogname
                     .empty()) {  // stop on first if no catalogname is given
        ++found;
        break;
      }
    }

    if (!found) {
      if (!catalogname.empty()) {
        Pmsg2(0,
              T_("Error can not find the Catalog name[%s] in the given config "
                 "file [%s]\n"),
              catalogname.c_str(), configfile.c_str());
      } else {
        Pmsg1(0,
              T_("Error there is no Catalog section in the given config file "
                 "[%s]\n"),
              configfile.c_str());
      }
      exit(BEXIT_FAILURE);
    } else {
      {
        ResLocker _{my_config};
        me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
        my_config->own_resource_ = me;
      }
      if (!me) {
        Pmsg0(0, T_("Error no Director resource defined.\n"));
        exit(BEXIT_FAILURE);
      }

      SetWorkingDirectory(me->working_directory);

      // Print catalog information and exit (-B)
      if (print_catalog) {
        PrintCatalogDetails(catalog);
        exit(BEXIT_SUCCESS);
      }

      db_name = catalog->db_name;
      user = catalog->db_user;
      password = catalog->db_password.value;
      if (catalog->db_address) { dbhost = catalog->db_address; }
      db_driver = catalog->db_driver;
      if (!dbhost.empty() && dbhost[0] == 0) { dbhost = ""; }
      dbport = catalog->db_port;
    }
  } else {
    working_directory = workingdir.c_str();
  }

  // Open database
  db = db_init_database(nullptr, db_driver, db_name.c_str(), user.c_str(),
                        password.c_str(), dbhost.c_str(), dbport, nullptr,
                        false, false, false, false);
  if (!db->OpenDatabase(nullptr)) {
    Emsg1(M_FATAL, 0, "%s", db->strerror());
    return 1;
  }

  // Drop temporary index idx_tmp_name if it already exists
  DropTmpIdx("idxPIchk", "File");

  if (batch) {
    run_all_commands();
  } else {
    do_interactive_mode();
  }

  // Drop temporary index idx_tmp_name
  DropTmpIdx("idxPIchk", "File");

  db->CloseDatabase(nullptr);
  CloseMsg(nullptr);
  TermMsg();

  return BEXIT_SUCCESS;
}
