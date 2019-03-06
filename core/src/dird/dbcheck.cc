/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/*
 * Kern E. Sibbald, August 2002
 */
/**
 * Program to check a BAREOS database for consistency and to make repairs
 */

#include "include/bareos.h"
#include "cats/cats.h"
#include "cats/cats_backends.h"
#include "lib/runscript.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

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

/*
 * Global variables
 */
static bool fix = false;
static bool batch = false;
static BareosDb* db;
static ID_LIST id_list;
static NameList name_list;
static char buf[20000];
static bool quit = false;
static const char* idx_tmp_name;
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
static const char* backend_directory = _PATH_BAREOS_BACKENDDIR;
#endif

#define MAX_ID_LIST_LEN 10000000

/*
 * Forward referenced functions
 */
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
    {eliminate_admin_records, "Check for all Admin records", true},
    {eliminate_restore_records, "Check for all Restore records", true},
    {run_all_commands, "Run ALL checks", false},
};

const int number_commands =
    (sizeof(commands) / sizeof(struct dbcheck_cmdstruct));


static void usage()
{
  fprintf(stderr,
          "Usage: bareos-dbcheck [ options ] <working-directory> "
          "<bareos-database> <user> <password> [<dbhost>] [<dbport>]\n"
          "       -b                batch mode\n"
          "       -B                print catalog configuration and exit\n"
          "       -c  <config>      Director configuration filename or "
          "configuration directory (e.g. /etc/bareos)\n"
          "       -C  <catalog>     catalog name in the director configuration "
          "file\n"
          "       -d  <nnn>         set debug level to <nnn>\n"
          "       -dt               print a timestamp in debug output\n"
          "       -D  <driver name> specify the database driver name (default "
          "NULL) <postgresql|mysql|sqlite3>\n"
          "       -f                fix inconsistencies\n"
          "       -v                verbose\n"
          "       -?                print this message\n\n");
  exit(1);
}

/**
 * helper functions
 */

/**
 * Gen next input command from the terminal
 */
static char* GetCmd(const char* prompt)
{
  static char cmd[1000];

  printf("%s", prompt);
  fflush(stdout);
  if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
    printf("\n");
    quit = true;
    return NULL;
  }
  StripTrailingJunk(cmd);
  return cmd;
}

static bool yes_no(const char* prompt, bool batchvalue = true)
{
  char* cmd;
  /*
   * return the batchvalue if batch operation is set
   */
  if (batch) { return batchvalue; }
  cmd = GetCmd(prompt);
  if (!cmd) {
    quit = true;
    return false;
  }
  return (Bstrcasecmp(cmd, "yes")) || (Bstrcasecmp(cmd, _("yes")));
}

static void set_quit() { quit = true; }

static void toggle_modify()
{
  fix = !fix;
  if (fix)
    printf(_("Database will be modified.\n"));
  else
    printf(_("Database will NOT be modified.\n"));
}

static void toggle_verbose()
{
  verbose = verbose ? 0 : 1;
  if (verbose)
    printf(_(" Verbose is on.\n"));
  else
    printf(_(" Verbose is off.\n"));
}


static void PrintCatalogDetails(CatalogResource* catalog,
                                const char* working_dir)
{
  POOLMEM* catalog_details = GetPoolMemory(PM_MESSAGE);

  /*
   * Instantiate a BareosDb class and see what db_type gets assigned to it.
   */
  db = db_init_database(NULL, catalog->db_driver, catalog->db_name,
                        catalog->db_user, catalog->db_password.value,
                        catalog->db_address, catalog->db_port,
                        catalog->db_socket, catalog->mult_db_connections,
                        catalog->disable_batch_insert, catalog->try_reconnect,
                        catalog->exit_on_fatal);
  if (db) {
    printf("%sdb_type=%s\nworking_dir=%s\n", catalog->display(catalog_details),
           db->GetType(), working_directory);
    db->CloseDatabase(NULL);
  }
  FreePoolMemory(catalog_details);
}

static int PrintNameHandler(void* ctx, int num_fields, char** row)
{
  if (row[0]) { printf("%s\n", row[0]); }
  return 0;
}

static int GetNameHandler(void* ctx, int num_fields, char** row)
{
  POOLMEM* name = (POOLMEM*)ctx;

  if (row[0]) { PmStrcpy(name, row[0]); }
  return 0;
}

static int PrintJobHandler(void* ctx, int num_fields, char** row)
{
  printf(_("JobId=%s Name=\"%s\" StartTime=%s\n"), NPRT(row[0]), NPRT(row[1]),
         NPRT(row[2]));
  return 0;
}

static int PrintJobmediaHandler(void* ctx, int num_fields, char** row)
{
  printf(_("Orphaned JobMediaId=%s JobId=%s Volume=\"%s\"\n"), NPRT(row[0]),
         NPRT(row[1]), NPRT(row[2]));
  return 0;
}

static int PrintFileHandler(void* ctx, int num_fields, char** row)
{
  printf(_("Orphaned FileId=%s JobId=%s Volume=\"%s\"\n"), NPRT(row[0]),
         NPRT(row[1]), NPRT(row[2]));
  return 0;
}

static int PrintFilesetHandler(void* ctx, int num_fields, char** row)
{
  printf(_("Orphaned FileSetId=%s FileSet=\"%s\" MD5=%s\n"), NPRT(row[0]),
         NPRT(row[1]), NPRT(row[2]));
  return 0;
}

static int PrintClientHandler(void* ctx, int num_fields, char** row)
{
  printf(_("Orphaned ClientId=%s Name=\"%s\"\n"), NPRT(row[0]), NPRT(row[1]));
  return 0;
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

/**
 * Drop temporary index
 */
static bool DropTmpIdx(const char* idx_name, const char* table_name)
{
  if (idx_tmp_name != NULL) {
    printf(_("Drop temporary index.\n"));
    fflush(stdout);
    Bsnprintf(buf, sizeof(buf), "DROP INDEX %s ON %s", idx_name, table_name);
    if (verbose) { printf("%s\n", buf); }
    if (!db->SqlQuery(buf, NULL, NULL)) {
      printf("%s\n", db->strerror());
      return false;
    } else {
      if (verbose) { printf(_("Temporary index %s deleted.\n"), idx_tmp_name); }
    }
    fflush(stdout);
  }
  idx_tmp_name = NULL;
  return true;
}


/*
 * Called here with each id to be added to the list
 */
static int IdListHandler(void* ctx, int num_fields, char** row)
{
  ID_LIST* lst = (ID_LIST*)ctx;

  if (lst->num_ids == MAX_ID_LIST_LEN) { return 1; }
  if (lst->num_ids == lst->max_ids) {
    if (lst->max_ids == 0) {
      lst->max_ids = 10000;
      lst->Id = (int64_t*)bmalloc(sizeof(int64_t) * lst->max_ids);
    } else {
      lst->max_ids = (lst->max_ids * 3) / 2;
      lst->Id = (int64_t*)brealloc(lst->Id, sizeof(int64_t) * lst->max_ids);
    }
  }
  lst->Id[lst->num_ids++] = str_to_int64(row[0]);
  return 0;
}

/*
 * Construct record id list
 */
static int MakeIdList(const char* query, ID_LIST* id_list)
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

/*
 * Delete all entries in the list
 */
static int DeleteIdList(const char* query, ID_LIST* id_list)
{
  char ed1[50];

  for (int i = 0; i < id_list->num_ids; i++) {
    Bsnprintf(buf, sizeof(buf), query, edit_int64(id_list->Id[i], ed1));
    if (verbose) { printf(_("Deleting: %s\n"), buf); }
    db->SqlQuery(buf, NULL, NULL);
  }
  return 1;
}

/*
 * Called here with each name to be added to the list
 */
static int NameListHandler(void* ctx, int num_fields, char** row)
{
  NameList* name = (NameList*)ctx;

  if (name->num_ids == MAX_ID_LIST_LEN) { return 1; }
  if (name->num_ids == name->max_ids) {
    if (name->max_ids == 0) {
      name->max_ids = 10000;
      name->name = (char**)bmalloc(sizeof(char*) * name->max_ids);
    } else {
      name->max_ids = (name->max_ids * 3) / 2;
      name->name = (char**)brealloc(name->name, sizeof(char*) * name->max_ids);
    }
  }
  name->name[name->num_ids++] = bstrdup(row[0]);
  return 0;
}

/*
 * Construct name list
 */
static int MakeNameList(const char* query, NameList* name_list)
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

/*
 * Print names in the list
 */
static void PrintNameList(NameList* name_list)
{
  for (int i = 0; i < name_list->num_ids; i++) {
    printf("%s\n", name_list->name[i]);
  }
}

/*
 * Free names in the list
 */
static void FreeNameList(NameList* name_list)
{
  for (int i = 0; i < name_list->num_ids; i++) { free(name_list->name[i]); }
  name_list->num_ids = 0;
}

static void eliminate_duplicate_paths()
{
  const char* query;
  char esc_name[5000];

  printf(_("Checking for duplicate Path entries.\n"));
  fflush(stdout);

  /*
   * Make list of duplicated names
   */
  query =
      "SELECT Path, count(Path) as Count FROM Path "
      "GROUP BY Path HAVING count(Path) > 1";

  if (!MakeNameList(query, &name_list)) { exit(1); }
  printf(_("Found %d duplicate Path records.\n"), name_list.num_ids);
  fflush(stdout);
  if (name_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    PrintNameList(&name_list);
  }
  if (quit) { return; }
  if (fix) {
    /*
     * Loop through list of duplicate names
     */
    for (int i = 0; i < name_list.num_ids; i++) {
      /*
       * Get all the Ids of each name
       */
      db->EscapeString(NULL, esc_name, name_list.name[i],
                       strlen(name_list.name[i]));
      Bsnprintf(buf, sizeof(buf), "SELECT PathId FROM Path WHERE Path='%s'",
                esc_name);
      if (verbose > 1) { printf("%s\n", buf); }
      if (!MakeIdList(buf, &id_list)) { exit(1); }
      if (verbose) {
        printf(_("Found %d for: %s\n"), id_list.num_ids, name_list.name[i]);
      }
      /*
       * Force all records to use the first id then delete the other ids
       */
      for (int j = 1; j < id_list.num_ids; j++) {
        char ed1[50], ed2[50];
        Bsnprintf(buf, sizeof(buf), "UPDATE File SET PathId=%s WHERE PathId=%s",
                  edit_int64(id_list.Id[0], ed1),
                  edit_int64(id_list.Id[j], ed2));
        if (verbose > 1) { printf("%s\n", buf); }
        db->SqlQuery(buf, NULL, NULL);
        Bsnprintf(buf, sizeof(buf), "DELETE FROM Path WHERE PathId=%s", ed2);
        if (verbose > 2) { printf("%s\n", buf); }
        db->SqlQuery(buf, NULL, NULL);
      }
    }
    fflush(stdout);
  }
  FreeNameList(&name_list);
}

/*
 * repair functions
 */

static void eliminate_orphaned_jobmedia_records()
{
  const char* query =
      "SELECT JobMedia.JobMediaId,Job.JobId FROM JobMedia "
      "LEFT OUTER JOIN Job USING(JobId) "
      "WHERE Job.JobId IS NULL LIMIT 300000";

  printf(_("Checking for orphaned JobMedia entries.\n"));
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  /*
   * Loop doing 300000 at a time
   */
  while (id_list.num_ids != 0) {
    printf(_("Found %d orphaned JobMedia records.\n"), id_list.num_ids);
    if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
      for (int i = 0; i < id_list.num_ids; i++) {
        char ed1[50];
        Bsnprintf(
            buf, sizeof(buf),
            "SELECT JobMedia.JobMediaId,JobMedia.JobId,Media.VolumeName FROM "
            "JobMedia,Media "
            "WHERE JobMedia.JobMediaId=%s AND Media.MediaId=JobMedia.MediaId",
            edit_int64(id_list.Id[i], ed1));
        if (!db->SqlQuery(buf, PrintJobmediaHandler, NULL)) {
          printf("%s\n", db->strerror());
        }
      }
    }
    if (quit) { return; }

    if (fix && id_list.num_ids > 0) {
      printf(_("Deleting %d orphaned JobMedia records.\n"), id_list.num_ids);
      DeleteIdList("DELETE FROM JobMedia WHERE JobMediaId=%s", &id_list);
    } else {
      break; /* get out if not updating db */
    }
    if (!MakeIdList(query, &id_list)) { exit(1); }
  }
  fflush(stdout);
}

static void eliminate_orphaned_file_records()
{
  const char* query =
      "SELECT File.FileId,Job.JobId FROM File "
      "LEFT OUTER JOIN Job USING (JobId) "
      "WHERE Job.JobId IS NULL LIMIT 300000";

  printf(_("Checking for orphaned File entries. This may take some time!\n"));
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  /*
   * Loop doing 300000 at a time
   */
  while (id_list.num_ids != 0) {
    printf(_("Found %d orphaned File records.\n"), id_list.num_ids);
    if (name_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
      for (int i = 0; i < id_list.num_ids; i++) {
        char ed1[50];
        Bsnprintf(buf, sizeof(buf),
                  "SELECT File.FileId,File.JobId,File.Name FROM File "
                  "WHERE File.FileId=%s",
                  edit_int64(id_list.Id[i], ed1));
        if (!db->SqlQuery(buf, PrintFileHandler, NULL)) {
          printf("%s\n", db->strerror());
        }
      }
    }
    if (quit) { return; }
    if (fix && id_list.num_ids > 0) {
      printf(_("Deleting %d orphaned File records.\n"), id_list.num_ids);
      DeleteIdList("DELETE FROM File WHERE FileId=%s", &id_list);
    } else {
      break; /* get out if not updating db */
    }
    if (!MakeIdList(query, &id_list)) { exit(1); }
  }
  fflush(stdout);
}

static void eliminate_orphaned_path_records()
{
  db_int64_ctx lctx;
  PoolMem query(PM_MESSAGE);

  lctx.count = 0;
  idx_tmp_name = NULL;

  db->FillQuery(query, BareosDb::SQL_QUERY_get_orphaned_paths_0);

  printf(_("Checking for orphaned Path entries. This may take some time!\n"));
  if (verbose > 1) { printf("%s\n", query.c_str()); }
  fflush(stdout);
  if (!MakeIdList(query.c_str(), &id_list)) { exit(1); }
  /*
   * Loop doing 300000 at a time
   */
  while (id_list.num_ids != 0) {
    printf(_("Found %d orphaned Path records.\n"), id_list.num_ids);
    fflush(stdout);
    if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
      for (int i = 0; i < id_list.num_ids; i++) {
        char ed1[50];
        Bsnprintf(buf, sizeof(buf), "SELECT Path FROM Path WHERE PathId=%s",
                  edit_int64(id_list.Id[i], ed1));
        db->SqlQuery(buf, PrintNameHandler, NULL);
      }
      fflush(stdout);
    }
    if (quit) { return; }
    if (fix && id_list.num_ids > 0) {
      printf(_("Deleting %d orphaned Path records.\n"), id_list.num_ids);
      fflush(stdout);
      DeleteIdList("DELETE FROM Path WHERE PathId=%s", &id_list);
    } else {
      break; /* get out if not updating db */
    }
    if (!MakeIdList(query.c_str(), &id_list)) { exit(1); }
  }
}

static void eliminate_orphaned_fileset_records()
{
  const char* query;

  printf(_("Checking for orphaned FileSet entries. This takes some time!\n"));
  query =
      "SELECT FileSet.FileSetId,Job.FileSetId FROM FileSet "
      "LEFT OUTER JOIN Job USING(FileSetId) "
      "WHERE Job.FileSetId IS NULL";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  printf(_("Found %d orphaned FileSet records.\n"), id_list.num_ids);
  fflush(stdout);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT FileSetId,FileSet,MD5 FROM FileSet "
                "WHERE FileSetId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintFilesetHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(_("Deleting %d orphaned FileSet records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM FileSet WHERE FileSetId=%s", &id_list);
  }
}

static void eliminate_orphaned_client_records()
{
  const char* query;

  printf(_("Checking for orphaned Client entries.\n"));
  /*
   * In English:
   *   Wiffle through Client for every Client
   *   joining with the Job table including every Client even if
   *   there is not a match in Job (left outer join), then
   *   filter out only those where no Job points to a Client
   *   i.e. Job.Client is NULL
   */
  query =
      "SELECT Client.ClientId,Client.Name FROM Client "
      "LEFT OUTER JOIN Job USING(ClientId) "
      "WHERE Job.ClientId IS NULL";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  printf(_("Found %d orphaned Client records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT ClientId,Name FROM Client "
                "WHERE ClientId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintClientHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(_("Deleting %d orphaned Client records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Client WHERE ClientId=%s", &id_list);
  }
}

static void eliminate_orphaned_job_records()
{
  const char* query;

  printf(_("Checking for orphaned Job entries.\n"));
  /*
   * In English:
   *   Wiffle through Job for every Job
   *   joining with the Client table including every Job even if
   *   there is not a match in Client (left outer join), then
   *   filter out only those where no Client exists
   *   i.e. Client.Name is NULL
   */
  query =
      "SELECT Job.JobId,Job.Name FROM Job "
      "LEFT OUTER JOIN Client USING(ClientId) "
      "WHERE Client.Name IS NULL";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  printf(_("Found %d orphaned Job records.\n"), id_list.num_ids);
  fflush(stdout);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT JobId,Name,StartTime FROM Job "
                "WHERE JobId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintJobHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(_("Deleting %d orphaned Job records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Job WHERE JobId=%s", &id_list);
    printf(_("Deleting JobMedia records of orphaned Job records.\n"));
    fflush(stdout);
    DeleteIdList("DELETE FROM JobMedia WHERE JobId=%s", &id_list);
    printf(_("Deleting Log records of orphaned Job records.\n"));
    fflush(stdout);
    DeleteIdList("DELETE FROM Log WHERE JobId=%s", &id_list);
  }
}

static void eliminate_admin_records()
{
  const char* query;

  printf(_("Checking for Admin Job entries.\n"));
  query =
      "SELECT Job.JobId FROM Job "
      "WHERE Job.Type='D'";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  printf(_("Found %d Admin Job records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT JobId,Name,StartTime FROM Job "
                "WHERE JobId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintJobHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(_("Deleting %d Admin Job records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Job WHERE JobId=%s", &id_list);
  }
}

static void eliminate_restore_records()
{
  const char* query;

  printf(_("Checking for Restore Job entries.\n"));
  query =
      "SELECT Job.JobId FROM Job "
      "WHERE Job.Type='R'";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  printf(_("Found %d Restore Job records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (int i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf),
                "SELECT JobId,Name,StartTime FROM Job "
                "WHERE JobId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintJobHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    printf(_("Deleting %d Restore Job records.\n"), id_list.num_ids);
    fflush(stdout);
    DeleteIdList("DELETE FROM Job WHERE JobId=%s", &id_list);
  }
}

static void repair_bad_filenames()
{
  const char* query;
  int i;

  printf(_("Checking for Filenames with a trailing slash\n"));
  query =
      "SELECT FileId,Name from File "
      "WHERE Name LIKE '%/'";
  if (verbose > 1) { printf("%s\n", query); }
  fflush(stdout);
  if (!MakeIdList(query, &id_list)) { exit(1); }
  printf(_("Found %d bad Filename records.\n"), id_list.num_ids);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Name FROM File WHERE FileId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintNameHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    POOLMEM* name = GetPoolMemory(PM_FNAME);
    char esc_name[5000];
    printf(_("Reparing %d bad Filename records.\n"), id_list.num_ids);
    fflush(stdout);
    for (i = 0; i < id_list.num_ids; i++) {
      int len;
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Name FROM File WHERE FileId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, GetNameHandler, name)) {
        printf("%s\n", db->strerror());
      }
      /*
       * Strip trailing slash(es)
       */
      for (len = strlen(name); len > 0 && IsPathSeparator(name[len - 1]);
           len--) {}
      if (len == 0) {
        len = 1;
        esc_name[0] = ' ';
        esc_name[1] = 0;
      } else {
        name[len - 1] = 0;
        db->EscapeString(NULL, esc_name, name, len);
      }
      Bsnprintf(buf, sizeof(buf), "UPDATE File SET Name='%s' WHERE FileId=%s",
                esc_name, edit_int64(id_list.Id[i], ed1));
      if (verbose > 1) { printf("%s\n", buf); }
      db->SqlQuery(buf, NULL, NULL);
    }
    FreePoolMemory(name);
  }
  fflush(stdout);
}

static void repair_bad_paths()
{
  PoolMem query(PM_MESSAGE);
  int i;

  printf(_("Checking for Paths without a trailing slash\n"));
  db->FillQuery(query, BareosDb::SQL_QUERY_get_bad_paths_0);
  if (verbose > 1) { printf("%s\n", query.c_str()); }
  fflush(stdout);
  if (!MakeIdList(query.c_str(), &id_list)) { exit(1); }
  printf(_("Found %d bad Path records.\n"), id_list.num_ids);
  fflush(stdout);
  if (id_list.num_ids && verbose && yes_no(_("Print them? (yes/no): "))) {
    for (i = 0; i < id_list.num_ids; i++) {
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Path FROM Path WHERE PathId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, PrintNameHandler, NULL)) {
        printf("%s\n", db->strerror());
      }
    }
    fflush(stdout);
  }
  if (quit) { return; }
  if (fix && id_list.num_ids > 0) {
    POOLMEM* name = GetPoolMemory(PM_FNAME);
    char esc_name[5000];
    printf(_("Reparing %d bad Filename records.\n"), id_list.num_ids);
    fflush(stdout);
    for (i = 0; i < id_list.num_ids; i++) {
      int len;
      char ed1[50];
      Bsnprintf(buf, sizeof(buf), "SELECT Path FROM Path WHERE PathId=%s",
                edit_int64(id_list.Id[i], ed1));
      if (!db->SqlQuery(buf, GetNameHandler, name)) {
        printf("%s\n", db->strerror());
      }
      /*
       * Strip trailing blanks
       */
      for (len = strlen(name); len > 0 && name[len - 1] == ' '; len--) {
        name[len - 1] = 0;
      }
      /*
       * Add trailing slash
       */
      len = PmStrcat(name, "/");
      db->EscapeString(NULL, esc_name, name, len);
      Bsnprintf(buf, sizeof(buf), "UPDATE Path SET Path='%s' WHERE PathId=%s",
                esc_name, edit_int64(id_list.Id[i], ed1));
      if (verbose > 1) { printf("%s\n", buf); }
      db->SqlQuery(buf, NULL, NULL);
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

  printf(_("Hello, this is the Bareos database check/correct program.\n"));

  while (!quit) {
    if (fix)
      printf(_("Modify database is on."));
    else
      printf(_("Modify database is off."));
    if (verbose)
      printf(_(" Verbose is on.\n"));
    else
      printf(_(" Verbose is off.\n"));

    printf(_("Please select the function you want to perform.\n"));

    print_commands();
    cmd = GetCmd(_("Select function number: "));
    if (cmd) {
      int item = atoi(cmd);
      if ((item >= 0) && (item < number_commands)) {
        /* run specified function */
        (commands[item].func)();
      }
    }
  }
}

/**
 * main
 */
int main(int argc, char* argv[])
{
  int ch;
  const char* db_driver = NULL;
  const char *user, *password, *db_name, *dbhost;
  int dbport = 0;
  bool print_catalog = false;
  char* configfile = NULL;
  char* catalogname = NULL;
  char* endptr;
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  alist* backend_directories = NULL;
#endif

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  MyNameIs(argc, argv, "dbcheck");
  InitMsg(NULL, NULL); /* setup message handler */

  memset(&id_list, 0, sizeof(id_list));
  memset(&name_list, 0, sizeof(name_list));

  while ((ch = getopt(argc, argv, "bc:C:D:d:fvBt?")) != -1) {
    switch (ch) {
      case 'B':
        print_catalog = true; /* get catalog information from config */
        break;
      case 'b': /* batch */
        batch = true;
        break;
      case 'C': /* CatalogName */
        catalogname = optarg;
        break;
      case 'c': /* configfile */
        configfile = optarg;
        break;

      case 'D': /* db_driver */
        db_driver = optarg;
        break;
      case 'd': /* debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;
      case 'f': /* fix inconsistencies */
        fix = true;
        break;
      case 'v':
        verbose++;
        break;
      case '?':
      default:
        usage();
    }
  }
  argc -= optind;
  argv += optind;

  OSDependentInit();

  if (configfile || (argc == 0)) {
    CatalogResource* catalog = NULL;
    int found = 0;
    if (argc > 0) {
      Pmsg0(0, _("Warning skipping the additional parameters for working "
                 "directory/dbname/user/password/host.\n"));
    }
    my_config = InitDirConfig(configfile, M_ERROR_TERM);
    my_config->ParseConfig();
    LockRes(my_config);
    foreach_res (catalog, R_CATALOG) {
      if (catalogname && bstrcmp(catalog->hdr.name, catalogname)) {
        ++found;
        break;
      } else if (!catalogname) {  // stop on first if no catalogname is given
        ++found;
        break;
      }
    }
    UnlockRes(my_config);
    if (!found) {
      if (catalogname) {
        Pmsg2(0,
              _("Error can not find the Catalog name[%s] in the given config "
                "file [%s]\n"),
              catalogname, configfile);
      } else {
        Pmsg1(0,
              _("Error there is no Catalog section in the given config file "
                "[%s]\n"),
              configfile);
      }
      exit(1);
    } else {
      LockRes(my_config);
      me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, NULL);
      UnlockRes(my_config);
      if (!me) {
        Pmsg0(0, _("Error no Director resource defined.\n"));
        exit(1);
      }

      SetWorkingDirectory(me->working_directory);
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
      DbSetBackendDirs(me->backend_directories);
#endif

      /*
       * Print catalog information and exit (-B)
       */
      if (print_catalog) {
        PrintCatalogDetails(catalog, me->working_directory);
        exit(0);
      }

      db_name = catalog->db_name;
      user = catalog->db_user;
      password = catalog->db_password.value;
      dbhost = catalog->db_address;
      db_driver = catalog->db_driver;
      if (dbhost && dbhost[0] == 0) { dbhost = NULL; }
      dbport = catalog->db_port;
    }
  } else {
    if (argc > 6) {
      Pmsg0(0, _("Wrong number of arguments.\n"));
      usage();
    }

    /*
     * This is needed by SQLite to find the db
     */
    working_directory = argv[0];
    db_name = "bareos";
    user = db_name;
    password = "";
    dbhost = NULL;

    if (argc == 2) {
      db_name = argv[1];
      user = db_name;
    } else if (argc == 3) {
      db_name = argv[1];
      user = argv[2];
    } else if (argc == 4) {
      db_name = argv[1];
      user = argv[2];
      password = argv[3];
    } else if (argc == 5) {
      db_name = argv[1];
      user = argv[2];
      password = argv[3];
      dbhost = argv[4];
    } else if (argc == 6) {
      db_name = argv[1];
      user = argv[2];
      password = argv[3];
      dbhost = argv[4];
      errno = 0;
      dbport = strtol(argv[5], &endptr, 10);
      if (*endptr != '\0') {
        Pmsg0(0, _("Database port must be a numeric value.\n"));
        exit(1);
      } else if (errno == ERANGE) {
        Pmsg0(0, _("Database port must be a int value.\n"));
        exit(1);
      }
    }

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
    backend_directories = New(alist(10, owned_by_alist));
    backend_directories->append((char*)backend_directory);

    DbSetBackendDirs(backend_directories);
#endif
  }

  /*
   * Open database
   */
  db = db_init_database(NULL, db_driver, db_name, user, password, dbhost,
                        dbport, NULL, false, false, false, false);
  if (!db->OpenDatabase(NULL)) {
    Emsg1(M_FATAL, 0, "%s", db->strerror());
    return 1;
  }

  /*
   * Drop temporary index idx_tmp_name if it already exists
   */
  DropTmpIdx("idxPIchk", "File");

  if (batch) {
    run_all_commands();
  } else {
    do_interactive_mode();
  }

  /*
   * Drop temporary index idx_tmp_name
   */
  DropTmpIdx("idxPIchk", "File");

  db->CloseDatabase(NULL);
  DbFlushBackends();
  CloseMsg(NULL);
  TermMsg();

  return 0;
}
