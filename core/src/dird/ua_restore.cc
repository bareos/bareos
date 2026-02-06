/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
 * Kern Sibbald, July MMII
 * Tree handling routines split into ua_tree.c July MMIII.
 * BootStrapRecord (bootstrap record) handling routines split into
 * bsr.c July MMIII
 */
/**
 * @file
 * User Agent Database restore Command
 *                    Creates a bootstrap file for restoring files and
 *                    starts the restore job.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_select.h"
#include "dird/ua_tree.h"
#include "dird/ua_run.h"
#include "dird/ua_restore.h"
#include "dird/bsr.h"
#include "lib/breg.h"
#include "lib/edit.h"
#include "lib/berrno.h"
#include "lib/parse_conf.h"
#include "lib/tree.h"
#include "include/protocol_types.h"

#include <vector>

namespace directordaemon {

/* Imported functions */
extern void PrintBsr(UaContext* ua, RestoreBootstrapRecord* bsr);


/* Forward referenced functions */
static int LastFullHandler(void* ctx, int num_fields, char** row);
static int JobidHandler(void* ctx, int num_fields, char** row);
static int UserSelectJobidsOrFiles(UaContext* ua, RestoreContext* rx);
static int FilesetHandler(void* ctx, int num_fields, char** row);
static bool SelectBackupsBeforeDate(UaContext* ua,
                                    RestoreContext* rx,
                                    char* date);
static bool BuildDirectoryTree(UaContext* ua, RestoreContext* rx);
static void free_rx(RestoreContext* rx);
static void SplitPathAndFilename(UaContext* ua,
                                 RestoreContext* rx,
                                 char* fname);
static int JobidFileindexHandler(void* ctx, int num_fields, char** row);
static bool InsertFileIntoFindexList(UaContext* ua,
                                     RestoreContext* rx,
                                     char* file,
                                     char* date);
static bool InsertDirIntoFindexList(UaContext* ua,
                                    RestoreContext* rx,
                                    char* dir,
                                    char* date);
static void InsertOneFileOrDir(UaContext* ua,
                               RestoreContext* rx,
                               char* p,
                               char* date,
                               bool dir);
static bool GetClientName(UaContext* ua, RestoreContext* rx);
static bool GetRestoreClientName(UaContext* ua, RestoreContext& rx);
static bool get_date(UaContext* ua, char* date, int date_len);
static int RestoreCountHandler(void* ctx, int num_fields, char** row);
static bool InsertTableIntoFindexList(UaContext* ua,
                                      RestoreContext* rx,
                                      char* table);
static bool CheckAndSetFileregex(UaContext* ua,
                                 RestoreContext* rx,
                                 const char* regex);
static bool AddAllFindex(RestoreContext* rx);

// Restore files
bool RestoreCmd(UaContext* ua, const char*)
{
  RestoreContext rx; /* restore context */
  PoolMem buf;
  JobResource* job;
  int i;
  JobControlRecord* jcr = ua->jcr;
  char* escaped_bsr_name = NULL;
  char* escaped_where_name = NULL;
  char *strip_prefix, *add_prefix, *add_suffix, *regexp;
  strip_prefix = add_prefix = add_suffix = regexp = NULL;

  rx.path = GetPoolMemory(PM_FNAME);
  rx.fname = GetPoolMemory(PM_FNAME);
  rx.JobIds = GetPoolMemory(PM_FNAME);
  rx.JobIds[0] = 0;
  rx.query = GetPoolMemory(PM_FNAME);
  rx.bsr = std::make_unique<RestoreBootstrapRecord>();

  i = FindArgWithValue(ua, "comment");
  if (i >= 0) {
    rx.comment = ua->argv[i];
    if (!IsCommentLegal(ua, rx.comment)) { goto bail_out; }
  }

  i = FindArgWithValue(ua, "backupformat");
  if (i >= 0) { rx.backup_format = ua->argv[i]; }

  i = FindArgWithValue(ua, "where");
  if (i >= 0) { rx.where = ua->argv[i]; }

  i = FindArgWithValue(ua, "replace");
  if (i >= 0) { rx.replace = ua->argv[i]; }

  i = FindArgWithValue(ua, "pluginoptions");
  if (i >= 0) { rx.plugin_options = ua->argv[i]; }

  i = FindArgWithValue(ua, "strip_prefix");
  if (i >= 0) { strip_prefix = ua->argv[i]; }

  i = FindArgWithValue(ua, "add_prefix");
  if (i >= 0) { add_prefix = ua->argv[i]; }

  i = FindArgWithValue(ua, "add_suffix");
  if (i >= 0) { add_suffix = ua->argv[i]; }

  i = FindArgWithValue(ua, "regexwhere");
  if (i >= 0) { rx.RegexWhere = ua->argv[i]; }

  i = FindArgWithValue(ua, "fileregex");
  if (i >= 0) {
    if (!CheckAndSetFileregex(ua, &rx, ua->argv[i])) {
      ua->ErrorMsg(T_("Invalid \"FileRegex\" value.\n"));
      goto bail_out;
    }
  }

  i = FindArg(ua, "archive");
  if (i >= 0) {
    rx.job_filter = RestoreContext::JobTypeFilter::Archive;
  } else {
    rx.job_filter = RestoreContext::JobTypeFilter::Backup;
  }

  if (strip_prefix || add_suffix || add_prefix) {
    int len = BregexpGetBuildWhereSize(strip_prefix, add_prefix, add_suffix);
    regexp = (char*)malloc(len * sizeof(char));

    bregexp_build_where(regexp, len, strip_prefix, add_prefix, add_suffix);
    rx.RegexWhere = regexp;
  }

  /* TODO: add acl for regexwhere ? */

  if (rx.RegexWhere) {
    if (!ua->AclAccessOk(Where_ACL, rx.RegexWhere, true)) {
      ua->ErrorMsg(T_("\"RegexWhere\" specification not authorized.\n"));
      goto bail_out;
    }
  }

  if (rx.where) {
    if (!ua->AclAccessOk(Where_ACL, rx.where, true)) {
      ua->ErrorMsg(T_("\"where\" specification not authorized.\n"));
      goto bail_out;
    }
  }

  if (!OpenClientDb(ua, true)) { goto bail_out; }

  /* Ensure there is at least one Restore Job */
  {
    foreach_res (job, R_JOB) {
      if (job->JobType == JT_RESTORE) {
        if (!rx.restore_job) { rx.restore_job = job; }
        rx.restore_jobs++;
      }
    }
  }
  if (!rx.restore_jobs) {
    ua->ErrorMsg(
        T_("No Restore Job Resource found in %s.\n"
           "You must create at least one before running this command.\n"),
        my_config->get_base_config_path().c_str());
    goto bail_out;
  }

  /* Request user to select JobIds or files by various different methods
   *  last 20 jobs, where File saved, most recent backup, ...
   *  In the end, a list of files are pumped into
   *  AddFindex() */
  switch (UserSelectJobidsOrFiles(ua, &rx)) {
    case 0: /* error */
      goto bail_out;
    case 1: /* selected by jobid */
      if (!BuildDirectoryTree(ua, &rx)) {
        ua->SendMsg(T_("Restore not done.\n"));
        goto bail_out;
      }
      break;
    case 2: /* selected by filename, no tree needed */
      break;
    case 3: /* selected by fileregex only, add all findexes */
      if (!AddAllFindex(&rx)) {
        ua->ErrorMsg(T_("No JobId specified cannot continue.\n"));
        ua->SendMsg(T_("Restore not done.\n"));
        goto bail_out;
      }
      break;
  }

  if (rx.restore_jobs == 1) {
    job = rx.restore_job;
  } else {
    job = get_restore_job(ua);
  }
  if (!job) { goto bail_out; }

  /* When doing NDMP_NATIVE restores, we don't create any bootstrap file
   * as we only send a namelist for restore. The storage handling is
   * done by the NDMP state machine via robot and tape interface. */
  if (job->Protocol == PT_NDMP_NATIVE) {
    ua->InfoMsg(
        T_("Skipping BootStrapRecord creation as we are doing NDMP_NATIVE "
           "restore.\n"));

  } else {
    if (rx.bsr->JobId) {
      char ed1[50];
      if (!AddVolumeInformationToBsr(ua, rx.bsr.get())) {
        ua->ErrorMsg(T_(
            "Unable to construct a valid BootStrapRecord. Cannot continue.\n"));
        goto bail_out;
      }
      if (!(rx.selected_files = WriteBsrFile(ua, rx))) {
        ua->WarningMsg(T_("No files selected to be restored.\n"));
        goto bail_out;
      }
      DisplayBsrInfo(ua, rx); /* display vols needed, etc */

      if (rx.selected_files == 1) {
        ua->InfoMsg(T_("\n1 file selected to be restored.\n\n"));
      } else {
        ua->InfoMsg(T_("\n%s files selected to be restored.\n\n"),
                    edit_uint64_with_commas(rx.selected_files, ed1));
      }
    } else {
      ua->WarningMsg(T_("No files selected to be restored.\n"));
      goto bail_out;
    }
  }

  if (!GetClientName(ua, &rx)) { goto bail_out; }
  if (!rx.ClientName) {
    ua->ErrorMsg(T_("No Client resource found!\n"));
    goto bail_out;
  }
  if (!GetRestoreClientName(ua, rx)) { goto bail_out; }

  escaped_bsr_name = escape_filename(jcr->RestoreBootstrap);

  Mmsg(ua->cmd,
       "run job=\"%s\" client=\"%s\" restoreclient=\"%s\" storage=\"%s\""
       " bootstrap=\"%s\" files=%u catalog=\"%s\"",
       job->resource_name_, rx.ClientName, rx.RestoreClientName,
       rx.store ? rx.store->resource_name_ : "",
       escaped_bsr_name ? escaped_bsr_name : jcr->RestoreBootstrap,
       rx.selected_files, ua->catalog->resource_name_);

  // Build run command
  if (rx.backup_format) {
    Mmsg(buf, " backupformat=%s", rx.backup_format);
    PmStrcat(ua->cmd, buf);
  }

  PmStrcpy(buf, "");
  if (rx.RegexWhere) {
    escaped_where_name = escape_filename(rx.RegexWhere);
    Mmsg(buf, " regexwhere=\"%s\"",
         escaped_where_name ? escaped_where_name : rx.RegexWhere);

  } else if (rx.where) {
    escaped_where_name = escape_filename(rx.where);
    Mmsg(buf, " where=\"%s\"",
         escaped_where_name ? escaped_where_name : rx.where);
  }
  PmStrcat(ua->cmd, buf);

  if (rx.replace) {
    Mmsg(buf, " replace=%s", rx.replace);
    PmStrcat(ua->cmd, buf);
  }

  if (rx.plugin_options) {
    Mmsg(buf, " pluginoptions=%s", rx.plugin_options);
    PmStrcat(ua->cmd, buf);
  }

  if (rx.comment) {
    Mmsg(buf, " comment=\"%s\"", rx.comment);
    PmStrcat(ua->cmd, buf);
  }

  if (escaped_bsr_name != NULL) { free(escaped_bsr_name); }

  if (escaped_where_name != NULL) { free(escaped_where_name); }

  if (regexp) { free(regexp); }

  if (FindArg(ua, NT_("yes")) > 0) {
    PmStrcat(ua->cmd, " yes"); /* pass it on to the run command */
  }

  Dmsg1(200, "Submitting: %s\n", ua->cmd);

  // Transfer jobids to jcr to for picking up restore objects
  jcr->JobIds = rx.JobIds;
  rx.JobIds = NULL;

  ParseUaArgs(ua);
  RunCmd(ua, ua->cmd);
  free_rx(&rx);
  return true;

bail_out:
  if (escaped_bsr_name != NULL) { free(escaped_bsr_name); }

  if (escaped_where_name != NULL) { free(escaped_where_name); }

  if (regexp) { free(regexp); }

  /* restore_tree_root only gets freed if either the backup starts
   * or the ua session ends.  Since the first definitely does not happen
   * after this point, and we cannot control the second one, we need
   * to ensure that we free the tree here before returning false; otherwise
   * this memory will leak, if another restore is attempted. */
  if (jcr->dir_impl->restore_tree_root) {
    FreeTree(jcr->dir_impl->restore_tree_root);
    jcr->dir_impl->restore_tree_root = nullptr;
  }

  free_rx(&rx);
  return false;
}

static void free_rx(RestoreContext* rx)
{
  rx->bsr.reset(nullptr);

  if (rx->ClientName) {
    free(rx->ClientName);
    rx->ClientName = NULL;
  }

  if (rx->RestoreClientName) {
    free(rx->RestoreClientName);
    rx->RestoreClientName = NULL;
  }

  FreeAndNullPoolMemory(rx->JobIds);
  FreeAndNullPoolMemory(rx->fname);
  FreeAndNullPoolMemory(rx->path);
  FreeAndNullPoolMemory(rx->query);
}

static bool HasValue(UaContext* ua, int i)
{
  if (!ua->argv[i]) {
    ua->ErrorMsg(T_("Missing value for keyword: %s\n"), ua->argk[i]);
    return false;
  }
  return true;
}

// This gets the client name from which the backup was made
static bool GetClientName(UaContext* ua, RestoreContext* rx)
{
  int i;
  ClientDbRecord cr;

  // If no client name specified yet, get it now
  if (!rx->ClientName) {
    // Try command line argument
    i = FindArgWithValue(ua, NT_("client"));
    if (i < 0) { i = FindArgWithValue(ua, NT_("backupclient")); }
    if (i >= 0) {
      if (!IsNameValid(ua->argv[i], ua->errmsg)) {
        ua->ErrorMsg("%s argument: %s", ua->argk[i], ua->errmsg.c_str());
        return false;
      }
      bstrncpy(cr.Name, ua->argv[i], sizeof(cr.Name));
      if (!ua->db->GetClientRecord(ua->jcr, &cr)) {
        ua->ErrorMsg("invalid %s argument: %s\n", ua->argk[i], ua->argv[i]);
        return false;
      }
      rx->ClientName = strdup(ua->argv[i]);
      return true;
    }
    if (!GetClientDbr(ua, &cr)) { return false; }
    rx->ClientName = strdup(cr.Name);
  }

  return true;
}

// This is where we pick up a client name to restore to.
static bool GetRestoreClientName(UaContext* ua, RestoreContext& rx)
{
  int i;

  // Try command line argument
  i = FindArgWithValue(ua, NT_("restoreclient"));
  if (i >= 0) {
    if (!IsNameValid(ua->argv[i], ua->errmsg)) {
      ua->ErrorMsg("%s argument: %s", ua->argk[i], ua->errmsg.c_str());
      return false;
    }
    if (!ua->GetClientResWithName(ua->argv[i])) {
      ua->ErrorMsg("invalid %s argument: %s\n", ua->argk[i], ua->argv[i]);
      return false;
    }
    rx.RestoreClientName = strdup(ua->argv[i]);
    return true;
  }

  rx.RestoreClientName = strdup(rx.ClientName);
  return true;
}

/**
 * The first step in the restore process is for the user to
 *  select a list of JobIds from which he will subsequently
 *  select which files are to be restored.
 *
 *  Returns:  3  if only fileregex specified
 *            2  if filename list made
 *            1  if jobid list made
 *            0  on error
 */
static int UserSelectJobidsOrFiles(UaContext* ua, RestoreContext* rx)
{
  const char* p;
  char date[MAX_TIME_LENGTH];
  bool have_date = false;
  /* Include current second if using current time */
  utime_t now = time(NULL) + 1;
  JobId_t JobId;
  bool done = false;
  int i, j;
  const char* list[] = {
      T_("List last 20 Jobs run"),
      T_("List Jobs where a given File is saved"),
      T_("Enter list of comma separated JobIds to select"),
      T_("Enter SQL list command"),
      T_("Select the most recent backup for a client"),
      T_("Select backup for a client before a specified time"),
      T_("Enter a list of files to restore"),
      T_("Enter a list of files to restore before a specified time"),
      T_("Find the JobIds of the most recent backup for a client"),
      T_("Find the JobIds for a backup for a client before a specified time"),
      T_("Enter a list of directories to restore for found JobIds"),
      T_("Select full restore to a specified Job date"),
      T_("Cancel"),
      NULL};

  const char* kw[] = {             // These keywords are handled in a for loop
                      "jobid",     /* 0 */
                      "current",   /* 1 */
                      "before",    /* 2 */
                      "file",      /* 3 */
                      "directory", /* 4 */
                      "select",    /* 5 */
                      "pool",      /* 6 */
                      "all",       /* 7 */
                      "fileregex", /* 8 */

                      // The keyword below are handled by individual arg lookups
                      "client",        /* 9 */
                      "storage",       /* 10 */
                      "fileset",       /* 11 */
                      "where",         /* 12 */
                      "yes",           /* 13 */
                      "bootstrap",     /* 14 */
                      "done",          /* 15 */
                      "strip_prefix",  /* 16 */
                      "add_prefix",    /* 17 */
                      "add_suffix",    /* 18 */
                      "regexwhere",    /* 19 */
                      "restoreclient", /* 20 */
                      "copies",        /* 21 */
                      "comment",       /* 22 */
                      "restorejob",    /* 23 */
                      "replace",       /* 24 */
                      "pluginoptions", /* 25 */
                      "archive",       /* 26 */
                      NULL};

  rx->JobIds[0] = 0;

  std::vector<char*> files;
  std::vector<char*> dirs;
  bool use_select = false;
  bool use_fileregex = false;

  for (i = 1; i < ua->argc; i++) { /* loop through arguments */
    bool found_kw = false;
    for (j = 0; kw[j]; j++) { /* loop through keywords */
      if (Bstrcasecmp(kw[j], ua->argk[i])) {
        found_kw = true;
        break;
      }
    }
    if (!found_kw) {
      ua->ErrorMsg(T_("Unknown keyword: %s\n"), ua->argk[i]);
      return 0;
    }
    /* Found keyword in kw[] list, process it */
    switch (j) {
      case 0: /* jobid */
        if (!HasValue(ua, i)) { return 0; }
        if (*rx->JobIds != 0) { PmStrcat(rx->JobIds, ","); }
        PmStrcat(rx->JobIds, ua->argv[i]);
        bstrncpy(rx->last_jobid, ua->argv[i], sizeof(rx->last_jobid));
        done = true;
        break;
      case 1: /* current */
        /* Note, we add one second here just to include any job
         *  that may have finished within the current second,
         *  which happens a lot in scripting small jobs. */
        bstrutime(date, sizeof(date), now);
        have_date = true;
        break;
      case 2: /* before */
      {
        if (have_date || !HasValue(ua, i)) { return 0; }

        std::string cpdate = CompensateShortDate(ua->argv[i]);

        if (StrToUtime(cpdate.c_str()) == 0) {
          ua->ErrorMsg(T_("Improper date format: %s\n"), cpdate.c_str());
          return 0;
        }
        bstrncpy(date, cpdate.c_str(), sizeof(date));
        have_date = true;
        break;
      }
      case 3: /* file */
        if (!HasValue(ua, i)) { return 0; }
        files.push_back(ua->argv[i]);
        break;
      case 4: /* dir */
        if (!HasValue(ua, i)) { return 0; }
        dirs.push_back(ua->argv[i]);
        break;
      case 5: /* select */
        use_select = true;
        break;
      case 6: /* pool specified */
        if (!HasValue(ua, i)) { return 0; }
        rx->pool = ua->GetPoolResWithName(ua->argv[i]);
        if (!rx->pool) {
          ua->ErrorMsg(T_("Error: Pool resource \"%s\" does not exist.\n"),
                       ua->argv[i]);
          return 0;
        }
        break;
      case 7: /* all specified */
        rx->all = true;
        break;
      case 8: /* fileregex */
        use_fileregex = true;
        break;
      default:
        // All keywords 7 or greater are ignored or handled by a select prompt
        break;
    }
  }

  if (files.size() + dirs.size() > 0 || use_fileregex) {
    if (!have_date) { bstrutime(date, sizeof(date), now); }
    if (!GetClientName(ua, rx)) { return 0; }

    for (auto& file : files) { InsertOneFileOrDir(ua, rx, file, date, false); }

    for (auto& dir : dirs) { InsertOneFileOrDir(ua, rx, dir, date, true); }

    if (files.size() + dirs.size() == 0) {
      /* If only fileregex but no specific files or dirs were specified
       * then restore all files and filter by fileregex. Before that we
       * need to select the jobids if none were specified. This makes
       * fileregex behave similarly to the file parameter. */
      if (*rx->JobIds == 0 && !SelectBackupsBeforeDate(ua, rx, date)) {
        return 0;
      }
      return 3;
    }

    return 2;
  }

  if (use_select) {
    if (!have_date) { bstrutime(date, sizeof(date), now); }
    if (!SelectBackupsBeforeDate(ua, rx, date)) { return 0; }
    done = true;
  }

  if (!done) {
    ua->SendMsg(
        T_("\nFirst you select one or more JobIds that contain files\n"
           "to be restored. You will be presented several methods\n"
           "of specifying the JobIds. Then you will be allowed to\n"
           "select which files from those JobIds are to be restored.\n\n"));
  }

  char filter_name = RestoreContext::FilterIdentifier(rx->job_filter);

  /* If choice not already made above, prompt */
  for (; !done;) {
    char* fname;
    int len;
    bool gui_save;
    db_list_ctx jobids;

    StartPrompt(ua,
                T_("To select the JobIds, you have the following choices:\n"));
    for (int k = 0; list[k]; k++) { AddPrompt(ua, list[k]); }
    done = true;
    switch (DoPrompt(ua, "", T_("Select item: "), NULL, 0)) {
      case -1: /* error or cancel */
        return 0;
      case 0: /* list last 20 Jobs run */
      {
        PoolMem query;
        ua->db->FillQuery(query, BareosDb::SQL_QUERY::uar_list_jobs,
                          filter_name);
        if (!ua->AclAccessOk(Command_ACL, NT_("sqlquery"), true)) {
          ua->ErrorMsg(T_("SQL query not authorized.\n"));
          return 0;
        }
        gui_save = ua->jcr->gui;
        ua->jcr->gui = true;
        ua->db->ListSqlQuery(ua->jcr, query.c_str(), ua->send, HORZ_LIST, true);
        ua->jcr->gui = gui_save;
        done = false;
      } break;
      case 1: /* list where a file is saved */
        if (!GetClientName(ua, rx)) { return 0; }
        if (!GetCmd(ua, T_("Enter Filename (no path):"))) { return 0; }
        len = strlen(ua->cmd);
        fname = (char*)malloc(len * 2 + 1);
        ua->db->EscapeString(ua->jcr, fname, ua->cmd, len);
        ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_file,
                          rx->ClientName, fname);
        free(fname);
        gui_save = ua->jcr->gui;
        ua->jcr->gui = true;
        ua->db->ListSqlQuery(ua->jcr, rx->query, ua->send, HORZ_LIST, true);
        ua->jcr->gui = gui_save;
        done = false;
        break;
      case 2: /* enter a list of JobIds */
        if (!GetCmd(ua, T_("Enter JobId(s), comma separated, to restore: "))) {
          return 0;
        }
        PmStrcpy(rx->JobIds, ua->cmd);
        break;
      case 3: /* Enter an SQL list command */
        if (!ua->AclAccessOk(Command_ACL, NT_("sqlquery"), true)) {
          ua->ErrorMsg(T_("SQL query not authorized.\n"));
          return 0;
        }
        if (!GetCmd(ua, T_("Enter SQL list command: "))) { return 0; }
        gui_save = ua->jcr->gui;
        ua->jcr->gui = true;
        ua->db->ListSqlQuery(ua->jcr, ua->cmd, ua->send, HORZ_LIST, true);
        ua->jcr->gui = gui_save;
        done = false;
        break;
      case 4: /* Select the most recent backups */
        if (!have_date) { bstrutime(date, sizeof(date), now); }
        if (!SelectBackupsBeforeDate(ua, rx, date)) { return 0; }
        break;
      case 5: /* select backup at specified time */
        if (!have_date) {
          if (!get_date(ua, date, sizeof(date))) { return 0; }
        }
        if (!SelectBackupsBeforeDate(ua, rx, date)) { return 0; }
        break;
      case 6: /* Enter files */
        if (!have_date) { bstrutime(date, sizeof(date), now); }
        if (!GetClientName(ua, rx)) { return 0; }
        ua->SendMsg(
            T_("Enter file names with paths, or < to enter a filename\n"
               "containing a list of file names with paths, and Terminate\n"
               "them with a blank line.\n"));
        for (;;) {
          if (!GetCmd(ua, T_("Enter full filename: "))) { return 0; }
          len = strlen(ua->cmd);
          if (len == 0) { break; }
          InsertOneFileOrDir(ua, rx, ua->cmd, date, false);
        }
        return 2;
      case 7: /* enter files backed up before specified time */
        if (!have_date) {
          if (!get_date(ua, date, sizeof(date))) { return 0; }
        }
        if (!GetClientName(ua, rx)) { return 0; }
        ua->SendMsg(
            T_("Enter file names with paths, or < to enter a filename\n"
               "containing a list of file names with paths, and Terminate\n"
               "them with a blank line.\n"));
        for (;;) {
          if (!GetCmd(ua, T_("Enter full filename: "))) { return 0; }
          len = strlen(ua->cmd);
          if (len == 0) { break; }
          InsertOneFileOrDir(ua, rx, ua->cmd, date, false);
        }
        return 2;

      case 8: /* Find JobIds for current backup */
        if (!have_date) { bstrutime(date, sizeof(date), now); }
        if (!SelectBackupsBeforeDate(ua, rx, date)) { return 0; }
        done = false;
        break;

      case 9: /* Find JobIds for give date */
        if (!have_date) {
          if (!get_date(ua, date, sizeof(date))) { return 0; }
        }
        if (!SelectBackupsBeforeDate(ua, rx, date)) { return 0; }
        done = false;
        break;

      case 10: /* Enter directories */
        if (*rx->JobIds != 0) {
          ua->SendMsg(
              T_("You have already selected the following JobIds: %s\n"),
              rx->JobIds);
        } else if (GetCmd(
                       ua,
                       T_("Enter JobId(s), comma separated, to restore: "))) {
          if (*rx->JobIds != 0 && *ua->cmd) { PmStrcat(rx->JobIds, ","); }
          bstrncpy(rx->last_jobid, ua->cmd, sizeof(rx->last_jobid));
          PmStrcat(rx->JobIds, ua->cmd);
        }
        if (*rx->JobIds == 0 || *rx->JobIds == '.') {
          *rx->JobIds = 0;
          return 0; /* nothing entered, return */
        }
        if (!have_date) { bstrutime(date, sizeof(date), now); }
        if (!GetClientName(ua, rx)) { return 0; }
        ua->SendMsg(
            T_("Enter full directory names or start the name\n"
               "with a < to indicate it is a filename containing a list\n"
               "of directories and Terminate them with a blank line.\n"));
        for (;;) {
          if (!GetCmd(ua, T_("Enter directory name: "))) { return 0; }
          len = strlen(ua->cmd);
          if (len == 0) { break; }
          /* Add trailing slash to end of directory names */
          if (ua->cmd[0] != '<' && !IsPathSeparator(ua->cmd[len - 1])) {
            strcat(ua->cmd, "/");
          }
          InsertOneFileOrDir(ua, rx, ua->cmd, date, true);
        }
        return 2;

      case 11: /* Choose a jobid and select jobs */
        if (!GetCmd(ua, T_("Enter JobId to get the state to restore: "))
            || !IsAnInteger(ua->cmd)) {
          return 0;
        }
        {
          JobDbRecord jr;
          jr.JobId = str_to_int64(ua->cmd);
          if (!ua->db->GetJobRecord(ua->jcr, &jr)) {
            ua->ErrorMsg(T_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                         ua->cmd, ua->db->strerror());
            return 0;
          }
          ua->SendMsg(T_("Selecting jobs to build the Full state at %s\n"),
                      jr.cStartTime);
          jr.JobLevel = L_INCREMENTAL; /* Take Full+Diff+Incr */
          if (!ua->db->AccurateGetJobids(ua->jcr, &jr, &jobids)) { return 0; }
        }
        PmStrcpy(rx->JobIds, jobids.GetAsString().c_str());
        Dmsg1(30, "Item 12: jobids = %s\n", rx->JobIds);
        break;
      case 12: /* Cancel or quit */
        return 0;
    }
  }

  POOLMEM* JobIds = GetPoolMemory(PM_FNAME);
  *JobIds = 0;
  rx->TotalFiles = 0;
  /* Find total number of files to be restored, and filter the JobId
   *  list to contain only ones permitted by the ACL conditions. */
  JobDbRecord jr;
  for (p = rx->JobIds;;) {
    char ed1[50];
    int status = GetNextJobidFromList(&p, &JobId);
    if (status < 0) {
      ua->ErrorMsg(T_("Invalid JobId in list.\n"));
      FreePoolMemory(JobIds);
      return 0;
    }
    if (status == 0) { break; }
    if (jr.JobId == JobId) { continue; /* duplicate of last JobId */ }
    jr = JobDbRecord{};
    jr.JobId = JobId;
    if (!ua->db->GetJobRecord(ua->jcr, &jr)) {
      ua->ErrorMsg(T_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                   edit_int64(JobId, ed1), ua->db->strerror());
      FreePoolMemory(JobIds);
      return 0;
    }
    if (!ua->AclAccessOk(Job_ACL, jr.Name, true)) {
      ua->ErrorMsg(
          T_("Access to JobId=%s (Job \"%s\") not authorized. Not selected.\n"),
          edit_int64(JobId, ed1), jr.Name);
      continue;
    }
    if (*JobIds != 0) { PmStrcat(JobIds, ","); }
    PmStrcat(JobIds, edit_int64(JobId, ed1));
    rx->TotalFiles += jr.JobFiles;
  }
  FreePoolMemory(rx->JobIds);
  rx->JobIds = JobIds; /* Set ACL filtered list */
  if (*rx->JobIds == 0) {
    ua->WarningMsg(T_("No Jobs selected.\n"));
    return 0;
  }

  if (strchr(rx->JobIds, ',')) {
    ua->InfoMsg(T_("You have selected the following JobIds: %s\n"), rx->JobIds);
  } else {
    ua->InfoMsg(T_("You have selected the following JobId: %s\n"), rx->JobIds);
  }
  return 1;
}

// Get date from user
static bool get_date(UaContext* ua, char* date, int date_len)
{
  ua->SendMsg(
      T_("The restored files will the most current backup\n"
         "BEFORE the date you specify below.\n\n"));
  std::string cmpdate;
  for (;;) {
    if (!GetCmd(ua, T_("Enter date as YYYY-MM-DD HH:MM:SS :"))) {
      return false;
    }
    cmpdate = CompensateShortDate(ua->cmd);

    if (StrToUtime(cmpdate.c_str()) != 0) { break; }
    ua->ErrorMsg(T_("Improper date format.\n"));
  }
  bstrncpy(date, cmpdate.c_str(), date_len);
  return true;
}

/**
 * Compensate missing date-time elements in shorter formats
 * Returns a full compensated date when argument has a correct short format
 * Returns the argument unchanged, if there is a format problem
 */
std::string CompensateShortDate(const char* cmd)
{
  tm datetime{};
  char trailinggarbage[16]{""};

  if ((sscanf(cmd, "%u-%u-%u %u:%u:%u%15s", &datetime.tm_year, &datetime.tm_mon,
              &datetime.tm_mday, &datetime.tm_hour, &datetime.tm_min,
              &datetime.tm_sec, trailinggarbage)
           == 7
       || sscanf(cmd, "%u-%u-%u %u:%u%15s", &datetime.tm_year, &datetime.tm_mon,
                 &datetime.tm_mday, &datetime.tm_hour, &datetime.tm_min,
                 trailinggarbage)
              == 6
       || sscanf(cmd, "%u-%u-%u %u%15s", &datetime.tm_year, &datetime.tm_mon,
                 &datetime.tm_mday, &datetime.tm_hour, trailinggarbage)
              == 5
       || sscanf(cmd, "%u-%u-%u%15s", &datetime.tm_year, &datetime.tm_mon,
                 &datetime.tm_mday, trailinggarbage)
              == 4
       || sscanf(cmd, "%u-%u%15s", &datetime.tm_year, &datetime.tm_mon,
                 trailinggarbage)
              == 3
       || sscanf(cmd, "%u%s", &datetime.tm_year, trailinggarbage) == 2)
      && (trailinggarbage[0] == '\0')) {
    if (datetime.tm_mon == 0) { datetime.tm_mon = 1; }
    if (datetime.tm_mday == 0) { datetime.tm_mday = 1; }

    if (datetime.tm_year >= 1900) {
      datetime.tm_year -= 1900;
    } else {
      return cmd;
    }

    if (datetime.tm_mon > 0) { datetime.tm_mon--; }

    char buffer[MAX_TIME_LENGTH];
    std::strftime(buffer, MAX_TIME_LENGTH, "%Y-%m-%d %H:%M:%S", &datetime);
    return buffer;
  }
  return cmd;
}

// Insert a single file, or read a list of files from a file
static void InsertOneFileOrDir(UaContext* ua,
                               RestoreContext* rx,
                               char* p,
                               char* date,
                               bool dir)
{
  FILE* ffd;
  char file[5000];
  int line = 0;

  switch (*p) {
    case '<':
      p++;
      if ((ffd = fopen(p, "rb")) == NULL) {
        BErrNo be;
        ua->ErrorMsg(T_("Cannot open file %s: ERR=%s\n"), p, be.bstrerror());
        break;
      }
      while (fgets(file, sizeof(file), ffd)) {
        line++;
        if (dir) {
          if (!InsertDirIntoFindexList(ua, rx, file, date)) {
            ua->ErrorMsg(T_("Error occurred on line %d of file \"%s\"\n"), line,
                         p);
          }
        } else {
          if (!InsertFileIntoFindexList(ua, rx, file, date)) {
            ua->ErrorMsg(T_("Error occurred on line %d of file \"%s\"\n"), line,
                         p);
          }
        }
      }
      fclose(ffd);
      break;
    case '?':
      p++;
      InsertTableIntoFindexList(ua, rx, p);
      break;
    default:
      if (dir) {
        InsertDirIntoFindexList(ua, rx, p, date);
      } else {
        InsertFileIntoFindexList(ua, rx, p, date);
      }
      break;
  }
}

/**
 * For a given file (path+filename), split into path and file, then
 * lookup the most recent backup in the catalog to get the JobId
 * and FileIndex, then insert them into the findex list.
 */
static bool InsertFileIntoFindexList(UaContext* ua,
                                     RestoreContext* rx,
                                     char* file,
                                     char* date)
{
  StripTrailingNewline(file);
  SplitPathAndFilename(ua, rx, file);

  char filter_name = RestoreContext::FilterIdentifier(rx->job_filter);
  if (*rx->JobIds == 0) {
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_jobid_fileindex, date,
                      rx->path, rx->fname, rx->ClientName, filter_name);
  } else {
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_jobids_fileindex,
                      rx->JobIds, date, rx->path, rx->fname, rx->ClientName,
                      filter_name);
  }

  // Find and insert jobid and File Index
  rx->found = false;
  if (!ua->db->SqlQuery(rx->query, JobidFileindexHandler, (void*)rx)) {
    ua->ErrorMsg(T_("Query failed: %s. ERR=%s\n"), rx->query,
                 ua->db->strerror());
  }
  if (!rx->found) {
    ua->ErrorMsg(T_("No database record found for: %s\n"), file);
    return true;
  }
  return true;
}

/**
 * For a given path lookup the most recent backup in the catalog
 * to get the JobId and FileIndexes of all files in that directory.
 */
static bool InsertDirIntoFindexList(UaContext* ua,
                                    RestoreContext* rx,
                                    char* dir,
                                    char*)
{
  StripTrailingJunk(dir);

  if (*rx->JobIds == 0) {
    ua->ErrorMsg(T_("No JobId specified cannot continue.\n"));
    return false;
  } else {
    ua->db->FillQuery(rx->query,
                      BareosDb::SQL_QUERY::uar_jobid_fileindex_from_dir,
                      rx->JobIds, dir, rx->ClientName);
  }

  // Find and insert jobid and File Index
  rx->found = false;
  if (!ua->db->SqlQuery(rx->query, JobidFileindexHandler, (void*)rx)) {
    ua->ErrorMsg(T_("Query failed: %s. ERR=%s\n"), rx->query,
                 ua->db->strerror());
  }
  if (!rx->found) {
    ua->ErrorMsg(T_("No database record found for: %s\n"), dir);
    return true;
  }
  return true;
}

// Get the JobId and FileIndexes of all files in the specified table
static bool InsertTableIntoFindexList(UaContext* ua,
                                      RestoreContext* rx,
                                      char* table)
{
  StripTrailingJunk(table);

  ua->db->FillQuery(rx->query,
                    BareosDb::SQL_QUERY::uar_jobid_fileindex_from_table, table);

  // Find and insert jobid and File Index
  rx->found = false;
  if (!ua->db->SqlQuery(rx->query, JobidFileindexHandler, (void*)rx)) {
    ua->ErrorMsg(T_("Query failed: %s. ERR=%s\n"), rx->query,
                 ua->db->strerror());
  }
  if (!rx->found) {
    ua->ErrorMsg(T_("No table found: %s\n"), table);
    return true;
  }
  return true;
}

static void SplitPathAndFilename(UaContext* ua, RestoreContext* rx, char* name)
{
  char *p, *f;

  /* Find path without the filename.
   * I.e. everything after the last / is a "filename".
   * OK, maybe it is a directory name, but we treat it like
   * a filename. If we don't find a / then the whole name
   * must be a path name (e.g. c:).
   */
  for (p = f = name; *p; p++) {
    if (IsPathSeparator(*p)) { f = p; /* set pos of last slash */ }
  }
  if (IsPathSeparator(*f)) { /* did we find a slash? */
    f++;                     /* yes, point to filename */
  } else {                   /* no, whole thing must be path name */
    f = p;
  }

  /* If filename doesn't exist (i.e. root directory), we
   * simply create a blank name consisting of a single
   * space. This makes handling zero length filenames
   * easier.
   */
  rx->fnl = p - f;
  if (rx->fnl > 0) {
    rx->fname = CheckPoolMemorySize(rx->fname, 2 * (rx->fnl) + 1);
    ua->db->EscapeString(ua->jcr, rx->fname, f, rx->fnl);
  } else {
    rx->fname[0] = 0;
    rx->fnl = 0;
  }

  rx->pnl = f - name;
  if (rx->pnl > 0) {
    rx->path = CheckPoolMemorySize(rx->path, 2 * (rx->pnl) + 1);
    ua->db->EscapeString(ua->jcr, rx->path, name, rx->pnl);
  } else {
    rx->path[0] = 0;
    rx->pnl = 0;
  }

  Dmsg2(100, "split path=%s file=%s\n", rx->path, rx->fname);
}

static bool CheckAndSetFileregex(UaContext* ua,
                                 RestoreContext* rx,
                                 const char* regex)
{
  regex_t fileregex_re{};

  int rc = regcomp(&fileregex_re, regex, REG_EXTENDED | REG_NOSUB);
  if (rc != 0) {
    char errmsg[500];
    regerror(rc, &fileregex_re, errmsg, sizeof(errmsg));
    ua->SendMsg(T_("Regex compile error: %s\n"), errmsg);
    return false;
  }
  regfree(&fileregex_re);

  if (rx->bsr->fileregex) free(rx->bsr->fileregex);
  rx->bsr->fileregex = strdup(regex);
  return true;
}

static bool AskForFileregex(UaContext* ua, RestoreContext* rx)
{
  /* if user enters all on command line select everything */
  if (FindArg(ua, NT_("all")) >= 0
      || FindArgWithValue(ua, NT_("fileregex")) >= 0) {
    return true;
  }
  ua->SendMsg(
      T_("\n\nFor one or more of the JobIds selected, no files were found,\n"
         "so file selection is not possible.\n"
         "Most likely your retention policy pruned the files.\n"));
  if (GetYesno(ua, T_("\nDo you want to restore all the files? (yes|no): "))) {
    if (ua->pint32_val) { return true; }

    while (GetCmd(
        ua, T_("\nRegexp matching files to restore? (empty to abort): "))) {
      if (ua->cmd[0] == '\0') {
        break;
      } else if (CheckAndSetFileregex(ua, rx, ua->cmd)) {
        return true;
      }
    }
  }

  return false;
}

/* Walk on the delta_list of a TREE_NODE item and insert all parts
 * TODO: Optimize for bootstrap creation, remove recursion
 * 6 -> 5 -> 4 -> 3 -> 2 -> 1 -> 0
 * should insert as
 * 0, 1, 2, 3, 4, 5, 6
 */
static void AddDeltaListFindex(RestoreContext* rx, struct delta_list* lst)
{
  if (lst == NULL) { return; }
  if (lst->next) { AddDeltaListFindex(rx, lst->next); }
  AddFindex(rx->bsr.get(), lst->JobId, lst->FileIndex);
}

static bool AddAllFindex(RestoreContext* rx)
{
  bool has_jobid = false;
  JobId_t JobId, last_JobId = 0;
  for (const char* p = rx->JobIds; GetNextJobidFromList(&p, &JobId) > 0;) {
    if (JobId == last_JobId) { continue; /* eliminate duplicate JobIds */ }
    AddFindexAll(rx->bsr.get(), JobId);
    has_jobid = true;
  }
  return has_jobid;
}

static bool BuildDirectoryTree(UaContext* ua, RestoreContext* rx)
{
  TreeContext tree;
  JobId_t JobId;
  const char* p;
  bool OK = true;
  char ed1[50];

  // Build the directory tree containing JobIds user selected
  tree.root = new_tree(rx->TotalFiles);
  tree.ua = ua;
  tree.all = rx->all;

  /* For display purposes, the same JobId, with different volumes may
   * appear more than once, however, we only insert it once. */
  p = rx->JobIds;
  tree.FileEstimate = 0;

  if (GetNextJobidFromList(&p, &JobId) > 0) {
    // Use first JobId as estimate of the number of files to restore
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_count_files,
                      edit_int64(JobId, ed1));
    rx->found = false;
    if (!ua->db->SqlQuery(rx->query, RestoreCountHandler, (void*)rx)) {
      ua->ErrorMsg("%s\n", ua->db->strerror());
    }
    if (rx->found) {
      // Add about 25% more than this job for over estimate
      tree.FileEstimate = rx->JobId + (rx->JobId >> 2);
      tree.DeltaCount = rx->JobId / 50; /* print 50 ticks */
    }
  }

  ua->InfoMsg(T_("\nBuilding directory tree for JobId(s) %s ...  "),
              rx->JobIds);

  ua->LogAuditEventInfoMsg(T_("Building directory tree for JobId(s) %s"),
                           rx->JobIds);

  if (!ua->db->GetFileList(ua->jcr, rx->JobIds, false /* do not use md5 */,
                           true /* get delta */, InsertTreeHandler,
                           (void*)&tree)) {
    ua->ErrorMsg("%s", ua->db->strerror());
  }


  /* Look at the first JobId on the list (presumably the oldest) and
   *  if it is marked purged, don't do the manual selection because
   *  the Job was pruned, so the tree is incomplete. */
  if (tree.FileCount != 0) {
    // Find out if any Job is purged
    Mmsg(rx->query, "SELECT SUM(PurgedFiles) FROM Job WHERE JobId IN (%s)",
         rx->JobIds);
    rx->found = false;
    if (!ua->db->SqlQuery(rx->query, RestoreCountHandler, (void*)rx)) {
      ua->ErrorMsg("%s\n", ua->db->strerror());
    }
    // rx->JobId is the PurgedFiles flag
    if (rx->found && rx->JobId > 0) {
      tree.FileCount = 0; /* set count to zero, no tree selection */
    }
  }

  if (tree.FileCount == 0) {
    OK = AskForFileregex(ua, rx);
    if (OK) { AddAllFindex(rx); }
  } else {
    char ec1[50];
    if (tree.all) {
      ua->InfoMsg(
          T_("\n%s files inserted into the tree and marked for extraction.\n"),
          edit_uint64_with_commas(tree.FileCount, ec1));
    } else {
      ua->InfoMsg(T_("\n%s files inserted into the tree.\n"),
                  edit_uint64_with_commas(tree.cnt, ec1));
    }

    if (FindArg(ua, NT_("done")) < 0) {
      // Let the user interact in selecting which files to restore
      OK = UserSelectFilesFromTree(&tree);
    }

    /* Walk down through the tree finding all files marked to be
     *  extracted making a bootstrap file. */
    if (OK) {
      for (tree_node* node = FirstTreeNode(tree.root); node;
           node = NextTreeNode(node)) {
        Dmsg2(400, "FI=%" PRIu32 " node=%p\n", node->FileIndex, node);
        if (node->extract) {
          Dmsg3(400, "JobId=%" PRIu32 " type=%d FI=%" PRId32 "\n", node->JobId,
                to_underlying(node->type), node->FileIndex);
          /* TODO: optimize bsr insertion when jobid are non sorted */
          AddDeltaListFindex(rx, node->delta_list);
          AddFindex(rx->bsr.get(), node->JobId, node->FileIndex);
          if (node->extract && node->type != tree_node_type::NewDir) {
            rx->selected_files++; /* count only saved files */
          }
        }
      }
    }
  }

  /* We keep the tree with selected restore files.
   * For NDMP restores its used in the DMA to know what to restore.
   * The tree is freed by the DMA when its done. */
  ua->jcr->dir_impl->restore_tree_root = tree.root;

  return OK;
}

/**
 * This routine is used to insert the current full backup into the temporary
 * table temp using another temporary table temp1.
 * Returns whether the operations succeeded without errors regardless of
 * wether a row was inserted or not!
 */
static bool InsertLastFullBackupOfType(UaContext* ua,
                                       RestoreContext* rx,
                                       RestoreContext::JobTypeFilter filter,
                                       char* client_id,
                                       char* date,
                                       char* file_set,
                                       char* pool_select)
{
  char filter_name = RestoreContext::FilterIdentifier(filter);
  // Find JobId of last Full backup for this client, fileset
  if (pool_select && pool_select[0]) {
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_last_full, client_id,
                      date, filter_name, file_set, pool_select);

    if (!ua->db->SqlQuery(rx->query)) {
      ua->ErrorMsg("%s\n", ua->db->strerror());
      return false;
    }
  } else {
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_last_full_no_pool,
                      client_id, date, filter_name, file_set);
    if (!ua->db->SqlQuery(rx->query)) {
      ua->ErrorMsg("%s\n", ua->db->strerror());
      return false;
    }
  }

  // Find all Volumes used by that JobId
  ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_full, filter_name);
  if (!ua->db->SqlQuery(rx->query)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }

  return true;
}

/**
 * This routine is used to get the current backup or a backup before the
 * specified date.
 */
static bool SelectBackupsBeforeDate(UaContext* ua,
                                    RestoreContext* rx,
                                    char* date)
{
  int i;
  ClientDbRecord cr;
  FileSetDbRecord fsr;
  bool ok = false;
  char ed1[50], ed2[50];
  char pool_select[MAX_NAME_LENGTH];
  char fileset_name[MAX_NAME_LENGTH];
  char filter_name = RestoreContext::FilterIdentifier(rx->job_filter);

  // Create temp tables
  ua->db->SqlQuery(BareosDb::SQL_QUERY::uar_del_temp);
  ua->db->SqlQuery(BareosDb::SQL_QUERY::uar_del_temp1);

  if (!ua->db->SqlQuery(BareosDb::SQL_QUERY::uar_create_temp)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
  }
  if (!ua->db->SqlQuery(BareosDb::SQL_QUERY::uar_create_temp1)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
  }
  // Select Client from the Catalog
  if (!GetClientDbr(ua, &cr)) { goto bail_out; }
  if (rx->ClientName) { free(rx->ClientName); }
  rx->ClientName = strdup(cr.Name);

  // Get FileSet
  i = FindArgWithValue(ua, "FileSet");

  if (i >= 0 && IsNameValid(ua->argv[i], ua->errmsg)) {
    bstrncpy(fsr.FileSet, ua->argv[i], sizeof(fsr.FileSet));
    if (!ua->db->GetFilesetRecord(ua->jcr, &fsr)) {
      ua->ErrorMsg(T_("Error getting FileSet \"%s\": ERR=%s\n"), fsr.FileSet,
                   ua->db->strerror());
      i = -1;
    }
  } else if (i >= 0) { /* name is invalid */
    ua->ErrorMsg(T_("FileSet argument: %s\n"), ua->errmsg.c_str());
  }

  if (i < 0) { /* fileset not found */
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_sel_fileset,
                      edit_int64(cr.ClientId, ed1), ed1);

    StartPrompt(ua, T_("The defined FileSet resources are:\n"));
    if (!ua->db->SqlQuery(rx->query, FilesetHandler, (void*)ua)) {
      ua->ErrorMsg("%s\n", ua->db->strerror());
    }
    if (DoPrompt(ua, T_("FileSet"), T_("Select FileSet resource"), fileset_name,
                 sizeof(fileset_name))
        < 0) {
      ua->ErrorMsg(T_("No FileSet found for client \"%s\".\n"), cr.Name);
      goto bail_out;
    }

    bstrncpy(fsr.FileSet, fileset_name, sizeof(fsr.FileSet));
    if (!ua->db->GetFilesetRecord(ua->jcr, &fsr)) {
      ua->WarningMsg(T_("Error getting FileSet record: %s\n"),
                     ua->db->strerror());
      ua->SendMsg(
          T_("This probably means you modified the FileSet.\n"
             "Continuing anyway.\n"));
    }
  }

  // If Pool specified, add PoolId specification
  pool_select[0] = 0;
  if (rx->pool) {
    PoolDbRecord pr;
    bstrncpy(pr.Name, rx->pool->resource_name_, sizeof(pr.Name));
    if (ua->db->GetPoolRecord(ua->jcr, &pr)) {
      Bsnprintf(pool_select, sizeof(pool_select), "AND Media.PoolId=%s ",
                edit_int64(pr.PoolId, ed1));
    } else {
      ua->WarningMsg(T_("Pool \"%s\" not found, using any pool.\n"), pr.Name);
    }
  }


  if (!InsertLastFullBackupOfType(ua, rx, rx->job_filter,
                                  edit_int64(cr.ClientId, ed1), date,
                                  fsr.FileSet, pool_select)) {
    goto bail_out;
  }

  /* Note, this is needed because I don't seem to get the callback from the call
   * just above. */
  rx->JobTDate = 0;
  ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_sel_all_temp1);
  if (!ua->db->SqlQuery(rx->query, LastFullHandler, (void*)rx)) {
    ua->WarningMsg("%s\n", ua->db->strerror());
  }
  if (rx->JobTDate == 0) {
    ua->ErrorMsg(T_("No Full backup%s before %s found.\n"),
                 (rx->job_filter == RestoreContext::JobTypeFilter::Backup)
                     ? ""
                     : " archive",
                 date);

    // if no full backups were found while searching for archives/backups
    // try to see if there are any valid full backups using the opposite filter.
    // if there are send a message to the user that he can try restoring those.
    RestoreContext::JobTypeFilter opposite
        = RestoreContext::JobTypeFilter::Backup;
    switch (rx->job_filter) {
      case RestoreContext::JobTypeFilter::Archive: {
        opposite = RestoreContext::JobTypeFilter::Backup;
      } break;
      case RestoreContext::JobTypeFilter::Backup: {
        opposite = RestoreContext::JobTypeFilter::Archive;
      } break;
    }
    if (InsertLastFullBackupOfType(ua, rx, opposite,
                                   edit_int64(cr.ClientId, ed1), date,
                                   fsr.FileSet, pool_select)) {
      ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_sel_all_temp1);
      if (!ua->db->SqlQuery(rx->query, LastFullHandler, (void*)rx)) {
        // ignore warnings here, since they would not make any sense
        // to the end user
        goto bail_out;
      }
      if (rx->JobTDate != 0) {
        const char* filter_addition
            = (opposite == RestoreContext::JobTypeFilter::Backup) ? ""
                                                                  : " archive";
        const char* alternative_command
            = (opposite == RestoreContext::JobTypeFilter::Backup)
                  ? "normal restore"
                  : "restore archive";
        ua->SendMsg(
            "A suitable full backup%s was found. Try %s <...> instead.\n",
            filter_addition, alternative_command);
      }
    }

    goto bail_out;
  }

  // Now find most recent Differential Job after Full save, if any
  ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_dif,
                    edit_uint64(rx->JobTDate, ed1), date,
                    edit_int64(cr.ClientId, ed2), filter_name, fsr.FileSet,
                    pool_select);
  if (!ua->db->SqlQuery(rx->query)) {
    ua->WarningMsg("%s\n", ua->db->strerror());
  }

  // Now update JobTDate to look into Differential, if any
  ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_sel_all_temp);
  if (!ua->db->SqlQuery(rx->query, LastFullHandler, (void*)rx)) {
    ua->WarningMsg("%s\n", ua->db->strerror());
  }

  // Now find all Incremental Jobs after Full/dif save
  ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_inc,
                    edit_uint64(rx->JobTDate, ed1), date,
                    edit_int64(cr.ClientId, ed2), filter_name, fsr.FileSet,
                    pool_select);
  if (!ua->db->SqlQuery(rx->query)) {
    ua->WarningMsg("%s\n", ua->db->strerror());
  }

  // Get the JobIds from that list
  rx->last_jobid[0] = rx->JobIds[0] = 0;

  ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_sel_jobid_temp);
  if (!ua->db->SqlQuery(rx->query, JobidHandler, (void*)rx)) {
    ua->WarningMsg("%s\n", ua->db->strerror());
  }

  if (rx->JobIds[0] != 0) {
    if (FindArg(ua, NT_("copies")) > 0) {
      // Display a list of all copies
      ua->db->ListCopiesRecords(ua->jcr, "", rx->JobIds, ua->send, HORZ_LIST);

      if (FindArg(ua, NT_("yes")) > 0) {
        ua->pint32_val = 1;
      } else {
        GetYesno(ua,
                 T_("\nDo you want to restore from these copies? (yes|no): "));
      }

      if (ua->pint32_val) {
        PoolMem JobIds(PM_FNAME);

        /* Change the list of jobs needed to do the restore to the copies of the
         * Job. */
        PmStrcpy(JobIds, rx->JobIds);
        rx->last_jobid[0] = rx->JobIds[0] = 0;
        ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_sel_jobid_copies,
                          JobIds.c_str());
        if (!ua->db->SqlQuery(rx->query, JobidHandler, (void*)rx)) {
          ua->WarningMsg("%s\n", ua->db->strerror());
        }
      }
    }

    // Display a list of Jobs selected for this restore
    ua->db->FillQuery(rx->query, BareosDb::SQL_QUERY::uar_list_jobs_by_idlist,
                      rx->JobIds);
    ua->db->ListSqlQuery(ua->jcr, rx->query, ua->send, HORZ_LIST, true);

    ok = true;
  } else {
    ua->WarningMsg(T_("No jobs found.\n"));
  }

bail_out:
  ua->db->SqlQuery(BareosDb::SQL_QUERY::drop_deltabs);
  ua->db->SqlQuery(BareosDb::SQL_QUERY::uar_del_temp1);

  return ok;
}

static int RestoreCountHandler(void* ctx, int, char** row)
{
  RestoreContext* rx = (RestoreContext*)ctx;
  rx->JobId = str_to_int64(row[0]);
  rx->found = true;
  return 0;
}

/**
 * Callback handler to get JobId and FileIndex for files
 *   can insert more than one depending on the caller.
 */
static int JobidFileindexHandler(void* ctx, int num_fields, char** row)
{
  RestoreContext* rx = (RestoreContext*)ctx;

  Dmsg2(200, "JobId=%s FileIndex=%s\n", row[0], row[1]);
  rx->JobId = str_to_int64(row[0]);
  AddFindex(rx->bsr.get(), rx->JobId, str_to_int64(row[1]));
  rx->found = true;
  rx->selected_files++;

  JobidHandler(ctx, num_fields, row);

  return 0;
}

// Callback handler make list of JobIds
static int JobidHandler(void* ctx, int, char** row)
{
  RestoreContext* rx = (RestoreContext*)ctx;

  if (bstrcmp(rx->last_jobid, row[0])) { return 0; /* duplicate id */ }
  bstrncpy(rx->last_jobid, row[0], sizeof(rx->last_jobid));
  if (rx->JobIds[0] != 0) { PmStrcat(rx->JobIds, ","); }
  PmStrcat(rx->JobIds, row[0]);
  return 0;
}

// Callback handler to pickup last Full backup JobTDate
static int LastFullHandler(void* ctx, int, char** row)
{
  RestoreContext* rx = (RestoreContext*)ctx;

  rx->JobTDate = str_to_int64(row[1]);
  return 0;
}

// Callback handler build FileSet name prompt list
static int FilesetHandler(void* ctx, int, char** row)
{
  /* row[0] = FileSet (name) */
  if (row[0]) { AddPrompt((UaContext*)ctx, row[0]); }
  return 0;
}

void FindStorageResource(UaContext* ua,
                         RestoreContext& rx,
                         char* Storage,
                         char* MediaType)
{
  StorageResource* store;

  if (rx.store) {
    Dmsg1(200, "Already have store=%s\n", rx.store->resource_name_);
    return;
  }
  // Try looking up Storage by name

  foreach_res (store, R_STORAGE) {
    if (bstrcmp(Storage, store->resource_name_)) {
      if (ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
        rx.store = store;
      }
      break;
    }
  }


  if (rx.store) {
    int i;

    // Check if an explicit storage resource is given
    store = NULL;
    i = FindArgWithValue(ua, "storage");
    if (i > 0) { store = ua->GetStoreResWithName(ua->argv[i]); }
    if (store && (store != rx.store)) {
      ua->InfoMsg(
          T_("Warning default storage overridden by \"%s\" on command line.\n"),
          store->resource_name_);
      rx.store = store;
      Dmsg1(200, "Set store=%s\n", rx.store->resource_name_);
    }
    return;
  }

  // If no storage resource, try to find one from MediaType
  if (!rx.store) {
    foreach_res (store, R_STORAGE) {
      if (bstrcmp(MediaType, store->media_type)) {
        if (ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
          rx.store = store;
          Dmsg1(200, "Set store=%s\n", rx.store->resource_name_);
          if (Storage == NULL) {
            ua->WarningMsg(T_("Using Storage \"%s\" from MediaType \"%s\".\n"),
                           store->resource_name_, MediaType);
          } else {
            ua->WarningMsg(T_("Storage \"%s\" not found, using Storage \"%s\" "
                              "from MediaType \"%s\".\n"),
                           Storage, store->resource_name_, MediaType);
          }
        }
        return;
      }
    }
    ua->WarningMsg(T_("\nUnable to find Storage resource for\n"
                      "MediaType \"%s\", needed by the Jobs you selected.\n"),
                   MediaType);
  }

  // Take command line arg, or ask user if none
  rx.store = get_storage_resource(ua);
  if (rx.store) { Dmsg1(200, "Set store=%s\n", rx.store->resource_name_); }
}
} /* namespace directordaemon */
