/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
 * BAREOS Director -- User Agent Commands
 *
 * These are "dot" commands, i.e. commands preceded
 * by a period. These commands are meant to be used
 * by a program, so there is no prompting, and the
 * returned results are (supposed to be) predictable.
 *
 * Kern Sibbald, April MMII
 */

#include "bareos.h"
#include "dird.h"
#include "cats/bvfs.h"
#include "findlib/find.h"

/* Imported variables */
extern struct s_jl joblevels[];
extern struct s_jt jobtypes[];
extern struct s_kw ActionOnPurgeOptions[];
extern struct s_kw VolumeStatus[];

/* Imported functions */
extern void do_messages(UAContext *ua, const char *cmd);
extern bool quit_cmd(UAContext *ua, const char *cmd);
extern bool dot_help_cmd(UAContext *ua, const char *cmd);
extern bool dot_status_cmd(UAContext *ua, const char *cmd);

/* Forward referenced functions */
static bool catalogscmd(UAContext *ua, const char *cmd);
static bool admin_cmds(UAContext *ua, const char *cmd);
static bool jobdefscmd(UAContext *ua, const char *cmd);
static bool jobscmd(UAContext *ua, const char *cmd);
static bool filesetscmd(UAContext *ua, const char *cmd);
static bool clientscmd(UAContext *ua, const char *cmd);
static bool msgscmd(UAContext *ua, const char *cmd);
static bool poolscmd(UAContext *ua, const char *cmd);
static bool schedulecmd(UAContext *ua, const char *cmd);
static bool storagecmd(UAContext *ua, const char *cmd);
static bool defaultscmd(UAContext *ua, const char *cmd);
static bool typescmd(UAContext *ua, const char *cmd);
static bool backupscmd(UAContext *ua, const char *cmd);
static bool levelscmd(UAContext *ua, const char *cmd);
static bool getmsgscmd(UAContext *ua, const char *cmd);
static bool volstatuscmd(UAContext *ua, const char *cmd);
static bool mediatypescmd(UAContext *ua, const char *cmd);
static bool locationscmd(UAContext *ua, const char *cmd);
static bool mediacmd(UAContext *ua, const char *cmd);
static bool profilescmd(UAContext *ua, const char *cmd);
static bool aopcmd(UAContext *ua, const char *cmd);

static bool dot_bvfs_lsdirs(UAContext *ua, const char *cmd);
static bool dot_bvfs_lsfiles(UAContext *ua, const char *cmd);
static bool dot_bvfs_update(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_jobids(UAContext *ua, const char *cmd);
static bool dot_bvfs_versions(UAContext *ua, const char *cmd);
static bool dot_bvfs_restore(UAContext *ua, const char *cmd);
static bool dot_bvfs_cleanup(UAContext *ua, const char *cmd);
static bool dot_bvfs_clear_cache(UAContext *ua, const char *cmd);

static bool api_cmd(UAContext *ua, const char *cmd);
static bool sql_cmd(UAContext *ua, const char *cmd);
static bool dot_quit_cmd(UAContext *ua, const char *cmd);
static int one_handler(void *ctx, int num_field, char **row);

struct cmdstruct {
   const char *key;
   bool (*func)(UAContext *ua, const char *cmd);
   const char *help;       /* Help */
   const char *usage;      /* All arguments to build usage */
   const bool use_in_rs;   /* Can be used in runscript */
   const bool audit_event; /* Log an audit event when this Command is executed */
};
static struct cmdstruct commands[] = {
   { NT_(".api"), api_cmd, _("Switch between different api modes"),
     NT_("0 | 1 | 2 | off | json"), false, false },
   { NT_(".backups"), backupscmd, NULL, NULL, false, false },
   { NT_(".clients"), clientscmd, NULL, NULL, true, false },
   { NT_(".catalogs"), catalogscmd, NULL, NULL, false, false },
   { NT_(".defaults"), defaultscmd, NULL, NULL, false, false },
   { NT_(".die"), admin_cmds, NULL, NULL, false, true },
   { NT_(".dump"), admin_cmds, NULL, NULL, false, true },
   { NT_(".exit"), admin_cmds, NULL, NULL, false, false },
   { NT_(".filesets"), filesetscmd, NULL, NULL, false, false },
   { NT_(".help"), dot_help_cmd, _("Print parsable information about a command"),
     NT_("[ all | item=cmd ]"), false, false },
   { NT_(".jobdefs"), jobdefscmd, NULL, NULL, true, false },
   { NT_(".jobs"), jobscmd, NULL,
     NT_("type=<jobtype>"), true, false },
   { NT_(".levels"), levelscmd, NULL, NULL, false, false },
   { NT_(".messages"), getmsgscmd, NULL, NULL, false, false },
   { NT_(".msgs"), msgscmd, NULL, NULL, false, false },
   { NT_(".pools"), poolscmd, NULL, NULL, true, false },
   { NT_(".quit"), dot_quit_cmd, NULL, NULL, false, false },
   { NT_(".sql"), sql_cmd, NULL, NULL, false, true },
   { NT_(".schedule"), schedulecmd, NULL, NULL, false, false },
   { NT_(".status"), dot_status_cmd, NULL, NULL, false, true },
   { NT_(".storage"), storagecmd, NULL, NULL, true, false },
   { NT_(".volstatus"), volstatuscmd, NULL, NULL, true, false },
   { NT_(".media"), mediacmd, NULL, NULL, true, false },
   { NT_(".mediatypes"), mediatypescmd, NULL, NULL, true, false },
   { NT_(".locations"), locationscmd, NULL, NULL, true, false },
   { NT_(".profiles"), profilescmd, NULL, NULL, true, false },
   { NT_(".actiononpurge"), aopcmd, NULL, NULL, true, false },
   { NT_(".bvfs_lsdirs"), dot_bvfs_lsdirs, NULL, NULL, true, true },
   { NT_(".bvfs_lsfiles"),dot_bvfs_lsfiles, NULL, NULL, true, true },
   { NT_(".bvfs_update"), dot_bvfs_update, NULL, NULL, true, true },
   { NT_(".bvfs_get_jobids"), dot_bvfs_get_jobids, NULL, NULL, true, true },
   { NT_(".bvfs_versions"), dot_bvfs_versions, NULL, NULL, true, true },
   { NT_(".bvfs_restore"), dot_bvfs_restore, NULL, NULL, true, true },
   { NT_(".bvfs_cleanup"), dot_bvfs_cleanup, NULL, NULL, true, true },
   { NT_(".bvfs_clear_cache"), dot_bvfs_clear_cache, NULL, NULL, false, true },
   { NT_(".types"), typescmd, NULL, NULL, false, false }
};
#define comsize ((int)(sizeof(commands)/sizeof(struct cmdstruct)))

/*
 * Execute a command from the UA
 */
bool do_a_dot_command(UAContext *ua)
{
   int i;
   int len;
   bool ok = false;
   bool found = false;
   BSOCK *user = ua->UA_sock;

   Dmsg1(1400, "Dot command: %s\n", user->msg);
   if (ua->argc == 0) {
      return false;
   }

   len = strlen(ua->argk[0]);
   if (len == 1) {
      if (ua->api) {
         user->signal(BNET_CMD_BEGIN);
      }

      if (ua->api) {
         user->signal(BNET_CMD_OK);
      }

      return true;                    /* no op */
   }

   for (i = 0; i < comsize; i++) { /* search for command */
      if (bstrncasecmp(ua->argk[0],  _(commands[i].key), len)) {
         bool gui = ua->gui;

         /*
          * Check if this command is authorized in RunScript
          */
         if (ua->runscript && !commands[i].use_in_rs) {
            ua->error_msg(_("Can't use %s command in a runscript"), ua->argk[0]);
            break;
         }

         /*
          * If we need to audit this event do it now.
          */
         if (audit_event_wanted(ua, commands[i].audit_event)) {
            log_audit_event_cmdline(ua);
         }

         /*
          * Check if command permitted, but "quit" is always OK
          */
         if (!bstrcmp(ua->argk[0], NT_(".quit")) &&
             !acl_access_ok(ua, Command_ACL, ua->argk[0], len, true)) {
            break;
         }
         Dmsg1(100, "Cmd: %s\n", ua->cmd);

         ua->gui = true;
         if (ua->api) {
            user->signal(BNET_CMD_BEGIN);
         }
         ua->send->set_mode(ua->api);
         ok = (*commands[i].func)(ua, ua->cmd);   /* go execute command */
         ua->send->finalize_result(ok);
         if (ua->api) {
            user->signal(ok ? BNET_CMD_OK : BNET_CMD_FAILED);
         }
         ua->gui = gui;
         found = true;
         break;
      }
   }

   if (!found) {
      ua->error_msg("%s%s", ua->argk[0], _(": is an invalid command.\n"));
      ok = false;
   }

   return ok;
}

static bool dot_bvfs_update(UAContext *ua, const char *cmd)
{
   int pos;

   if (!open_client_db(ua, true)) {
      return 1;
   }

   pos = find_arg_with_value(ua, "jobid");
   if (pos != -1 && is_a_number_list(ua->argv[pos])) {
      if (!bvfs_update_path_hierarchy_cache(ua->jcr, ua->db, ua->argv[pos])) {
         ua->error_msg("ERROR: BVFS reported a problem for %s\n", ua->argv[pos]);
      }
   } else {
      /* update cache for all jobids */
      bvfs_update_cache(ua->jcr, ua->db);
   }

   return true;
}

static bool dot_bvfs_clear_cache(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua, true)) {
      return 1;
   }

   int pos = find_arg(ua, "yes");
   if (pos != -1) {
      Bvfs fs(ua->jcr, ua->db);
      fs.clear_cache();
      ua->info_msg("OK\n");
   } else {
      ua->error_msg("Can't find 'yes' argument\n");
   }

   return true;
}

static int bvfs_stat(UAContext *ua, char *lstat)
{
   struct stat statp;
   int32_t LinkFI;

   memset(&statp, 0, sizeof(struct stat));
   decode_stat(lstat, &statp, sizeof(statp), &LinkFI);

   ua->send->object_start("stat");
   ua->send->object_key_value("dev", statp.st_dev);
   ua->send->object_key_value("ino", statp.st_ino);
   ua->send->object_key_value("mode", statp.st_mode);
   ua->send->object_key_value("nlink", statp.st_nlink);
   ua->send->object_key_value("uid", statp.st_uid);
   ua->send->object_key_value("gid", statp.st_gid);
   ua->send->object_key_value("rdev", statp.st_rdev);
   ua->send->object_key_value("size", statp.st_size);
   ua->send->object_key_value("atime", statp.st_atime);
   ua->send->object_key_value("mtime", statp.st_mtime);
   ua->send->object_key_value("ctime", statp.st_ctime);
   ua->send->object_end("stat");

   return 0;
}

static int bvfs_result_handler(void *ctx, int fields, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   char *fileid=row[BVFS_FileId];
   char *lstat=row[BVFS_LStat];
   char *jobid=row[BVFS_JobId];

   char empty[] = "A A A A A A A A A A A A A A";
   char zero[] = "0";

   /* We need to deal with non existant path */
   if (!fileid || !is_a_number(fileid)) {
      lstat = empty;
      jobid = zero;
      fileid = zero;
   }

   Dmsg1(100, "type=%s\n", row[0]);
   if (bvfs_is_dir(row)) {
      char *path = bvfs_basename_dir(row[BVFS_Name]);
      ua->send->object_start(row[BVFS_Name]);
      ua->send->object_key_value("Type", row[BVFS_Type]);
      ua->send->object_key_value("PathId", str_to_uint64(row[BVFS_PathId]), "%lld\t");
      ua->send->object_key_value("FilenameId", FileId_t(0), "%lld\t");
      ua->send->object_key_value("FileId", str_to_uint64(fileid), "%lld\t");
      ua->send->object_key_value("JobId", str_to_uint64(jobid), "%lld\t");
      ua->send->object_key_value("lstat", lstat, "%s\t");
      ua->send->object_key_value("Name", path, "%s\n");
      ua->send->object_key_value("Fullpath", row[BVFS_Name]);
      bvfs_stat(ua, lstat);
      ua->send->object_end(row[BVFS_Name]);
  } else if (bvfs_is_version(row)) {
      ua->send->object_start(row[BVFS_Name]);
      ua->send->object_key_value("Type", row[BVFS_Type]);
      ua->send->object_key_value("PathId", str_to_uint64(row[BVFS_PathId]), "%lld\t");
      ua->send->object_key_value("FilenameId", str_to_uint64(row[BVFS_FilenameId]), "%lld\t");
      ua->send->object_key_value("FileId", str_to_uint64(fileid), "%lld\t");
      ua->send->object_key_value("JobId", str_to_uint64(jobid), "%lld\t");
      ua->send->object_key_value("lstat", lstat, "%s\t");
      ua->send->object_key_value("MD5", row[BVFS_Md5], "%s\t");
      ua->send->object_key_value("VolumeName", row[BVFS_VolName], "%s\t");
      ua->send->object_key_value("VolumeInChanger", str_to_uint64(row[BVFS_VolInchanger]), "%lld\n");
      ua->send->object_end(row[BVFS_Name]);
   } else if (bvfs_is_file(row)) {
      ua->send->object_start(row[BVFS_Name]);
      ua->send->object_key_value("Type", row[BVFS_Type]);
      ua->send->object_key_value("PathId", str_to_uint64(row[BVFS_PathId]), "%lld\t");
      ua->send->object_key_value("FilenameId", str_to_uint64(row[BVFS_FilenameId]), "%lld\t");
      ua->send->object_key_value("FileId", str_to_uint64(fileid), "%lld\t");
      ua->send->object_key_value("JobId", str_to_uint64(jobid), "%lld\t");
      ua->send->object_key_value("lstat", lstat, "%s\t");
      ua->send->object_key_value("Name", row[BVFS_Name], "%s\n");
      bvfs_stat(ua, lstat);
      ua->send->object_end(row[BVFS_Name]);
   }

   return 0;
}

static bool bvfs_parse_arg_version(UAContext *ua,
                                   char **client,
                                   DBId_t *fnid,
                                   bool *versions,
                                   bool *copies)
{
   *fnid = 0;
   *client = NULL;
   *versions = false;
   *copies = false;

   for (int i = 1; i < ua->argc; i++) {
      if (fnid && bstrcasecmp(ua->argk[i], NT_("fnid"))) {
         if (is_a_number(ua->argv[i])) {
            *fnid = str_to_int64(ua->argv[i]);
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("client"))) {
         *client = ua->argv[i];
      }

      if (copies && bstrcasecmp(ua->argk[i], NT_("copies"))) {
         *copies = true;
      }

      if (versions && bstrcasecmp(ua->argk[i], NT_("versions"))) {
         *versions = true;
      }
   }

   return (*client && *fnid > 0);
}

static bool bvfs_parse_arg(UAContext *ua,
                           DBId_t *pathid,
                           char **path,
                           char **jobid,
                           char **username,
                           int *limit,
                           int *offset)
{
   *pathid = 0;
   *limit = 2000;
   *offset = 0;
   *path = NULL;
   *jobid = NULL;
   *username = NULL;

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("pathid"))) {
         if (is_a_number(ua->argv[i])) {
            *pathid = str_to_int64(ua->argv[i]);
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("path"))) {
         *path = ua->argv[i];
      }

      if (bstrcasecmp(ua->argk[i], NT_("username"))) {
         *username = ua->argv[i];
      }

      if (bstrcasecmp(ua->argk[i], NT_("jobid"))) {
         if (is_a_number_list(ua->argv[i])) {
            *jobid = ua->argv[i];
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("limit"))) {
         if (is_a_number(ua->argv[i])) {
            *limit = str_to_int64(ua->argv[i]);
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("offset"))) {
         if (is_a_number(ua->argv[i])) {
            *offset = str_to_int64(ua->argv[i]);
         }
      }
   }

   if (!((*pathid || *path) && *jobid)) {
      return false;
   }

   if (!open_client_db(ua, true)) {
      return false;
   }

   return true;
}

/*
 * .bvfs_cleanup path=b2XXXXX
 */
static bool dot_bvfs_cleanup(UAContext *ua, const char *cmd)
{
   int i;

   if ((i = find_arg_with_value(ua, "path")) >= 0) {
      if (!open_client_db(ua, true)) {
         return false;
      }
      Bvfs fs(ua->jcr, ua->db);
      fs.drop_restore_list(ua->argv[i]);
   }
   return true;
}

/*
 * .bvfs_restore path=b2XXXXX jobid=1,2 fileid=1,2 dirid=1,2 hardlink=1,2,3,4
 */
static bool dot_bvfs_restore(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int limit=2000, offset=0, i;
   char *path=NULL, *jobid=NULL, *username=NULL;
   char *empty = (char *)"";
   char *fileid, *dirid, *hardlink;
   fileid = dirid = hardlink = empty;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return false;             /* not enough param */
   }

   Bvfs fs(ua->jcr, ua->db);
   fs.set_username(username);
   fs.set_jobids(jobid);

   if ((i = find_arg_with_value(ua, "fileid")) >= 0) {
      fileid = ua->argv[i];
   }
   if ((i = find_arg_with_value(ua, "dirid")) >= 0) {
      dirid = ua->argv[i];
   }
   if ((i = find_arg_with_value(ua, "hardlink")) >= 0) {
      hardlink = ua->argv[i];
   }

   if (fs.compute_restore_list(fileid, dirid, hardlink, path)) {
      ua->send_msg("OK\n");
   } else {
      ua->error_msg("Can't create restore list\n");
   }
   return true;
}

/*
 * .bvfs_lsfiles jobid=1,2,3,4 pathid=10
 * .bvfs_lsfiles jobid=1,2,3,4 path=/
 */
static bool dot_bvfs_lsfiles(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int limit=2000, offset=0;
   char *path=NULL, *jobid=NULL, *username=NULL;
   char *pattern=NULL;
   int i;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return false;             /* not enough param */
   }

   if ((i = find_arg_with_value(ua, "pattern")) >= 0) {
      pattern = ua->argv[i];
   }

   Bvfs fs(ua->jcr, ua->db);
   fs.set_username(username);
   fs.set_jobids(jobid);
   fs.set_handler(bvfs_result_handler, ua);
   fs.set_limit(limit);
   if (pattern) {
      fs.set_pattern(pattern);
   }
   if (pathid) {
      fs.ch_dir(pathid);
   } else {
      fs.ch_dir(path);
   }

   fs.set_offset(offset);

   fs.ls_files();

   return true;
}

/*
 * .bvfs_lsdirs jobid=1,2,3,4 pathid=10
 * .bvfs_lsdirs jobid=1,2,3,4 path=/
 * .bvfs_lsdirs jobid=1,2,3,4 path=
 */
static bool dot_bvfs_lsdirs(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int limit=2000, offset=0;
   char *path=NULL, *jobid=NULL, *username=NULL;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
   }

   Bvfs fs(ua->jcr, ua->db);
   fs.set_username(username);
   fs.set_jobids(jobid);
   fs.set_limit(limit);
   fs.set_handler(bvfs_result_handler, ua);

   if (pathid) {
      fs.ch_dir(pathid);
   } else {
      fs.ch_dir(path);
   }

   fs.set_offset(offset);

   fs.ls_special_dirs();
   fs.ls_dirs();

   return true;
}

/*
 * .bvfs_versions jobid=x fnid=10 pathid=10 copies versions (jobid isn't used)
 */
static bool dot_bvfs_versions(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0, fnid=0;
   int limit=2000, offset=0;
   char *path=NULL, *jobid=NULL, *client=NULL, *username=NULL;
   bool copies=false, versions=false;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return false;             /* not enough param */
   }

   if (!bvfs_parse_arg_version(ua, &client, &fnid, &versions, &copies)) {
      ua->error_msg("Can't find client or fnid argument\n");
      return true;              /* not enough param */
   }

   Bvfs fs(ua->jcr, ua->db);
   fs.set_limit(limit);
   fs.set_see_all_versions(versions);
   fs.set_see_copies(copies);
   fs.set_handler(bvfs_result_handler, ua);
   fs.set_offset(offset);
   fs.get_all_file_versions(pathid, fnid, client);

   return true;
}

/*
 * .bvfs_get_jobids jobid=1
 *  -> returns needed jobids to restore
 * .bvfs_get_jobids jobid=1 all
 *  -> returns needed jobids to restore with all filesets a JobId=1 time
 * .bvfs_get_jobids ujobid=JobName
 *  -> returns needed jobids to restore
 */
static bool dot_bvfs_get_jobids(UAContext *ua, const char *cmd)
{
   JOB_DBR jr;
   db_list_ctx jobids, tempids;
   int pos;
   char ed1[50];
   POOL_MEM query;
   dbid_list ids;               /* Store all FileSetIds for this client */

   if (!open_client_db(ua, true)) {
      return true;
   }

   memset(&jr, 0, sizeof(jr));

   if ((pos = find_arg_with_value(ua, "ujobid")) >= 0) {
      bstrncpy(jr.Job, ua->argv[pos], sizeof(jr.Job));
   }

   if ((pos = find_arg_with_value(ua, "jobid")) >= 0) {
      jr.JobId = str_to_int64(ua->argv[pos]);
   }

   if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
      ua->error_msg(_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                    ua->cmd, db_strerror(ua->db));
      return true;
   }

   /*
    * When in level base, we don't rely on any Full/Incr/Diff
    */
   if (jr.JobLevel == L_BASE) {
      jobids.add(edit_int64(jr.JobId, ed1));
   } else {
      /*
       * If we have the "all" option, we do a search on all defined fileset for this client
       */
      if (find_arg(ua, "all") > 0) {
         edit_int64(jr.ClientId, ed1);
         Mmsg(query, uar_sel_filesetid, ed1);
         db_get_query_dbids(ua->jcr, ua->db, query, ids);
      } else {
         ids.num_ids = 1;
         ids.DBId[0] = jr.FileSetId;
      }

      jr.JobLevel = L_INCREMENTAL; /* Take Full+Diff+Incr */

      /*
       * Foreach different FileSet, we build a restore jobid list
       */
      for (int i = 0; i < ids.num_ids; i++) {
         jr.FileSetId = ids.DBId[i];
         if (!db_accurate_get_jobids(ua->jcr, ua->db, &jr, &tempids)) {
            return true;
         }
         jobids.add(tempids);
      }
   }

   switch (ua->api) {
   case API_MODE_JSON: {
      char *cur_id, *bp;

      ua->send->object_start();
      cur_id = jobids.list;
      while (cur_id && strlen(cur_id)) {
         bp = strchr(cur_id, ',');
         if (bp) {
            *bp++ = '\0';
         }

         ua->send->object_start("jobids");
         ua->send->object_key_value("id", cur_id, "%s\n");
         ua->send->object_end("jobids");

         cur_id = bp;
      }
      ua->send->object_end();
      break;
   }
   default:
      ua->send_msg("%s\n", jobids.list);
      break;
   }

   return true;
}

static bool dot_quit_cmd(UAContext *ua, const char *cmd)
{
   return quit_cmd(ua, cmd);
}

static bool getmsgscmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending) {
      do_messages(ua, cmd);
   }
   return 1;
}

#ifdef DEVELOPER
static void do_storage_cmd(UAContext *ua, STORERES *store, const char *cmd)
{
   BSOCK *sd;
   JCR *jcr = ua->jcr;
   USTORERES lstore;

   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(jcr, &lstore);

   if (!(sd = open_sd_bsock(ua))) {
      ua->error_msg(_("Could not open SD socket.\n"));
      return;
   }

   Dmsg0(120, _("Connected to storage daemon\n"));
   sd = jcr->store_bsock;
   sd->fsend("%s", cmd);
   if (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }

   close_sd_bsock(ua);
   return;
}

static void do_client_cmd(UAContext *ua, CLIENTRES *client, const char *cmd)
{
   BSOCK *fd;

   /* Connect to File daemon */

   ua->jcr->res.client = client;
   /* Try to connect for 15 seconds */
   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
      client->name(), client->address, client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, false, false)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return;
   }
   Dmsg0(120, "Connected to file daemon\n");
   fd = ua->jcr->file_bsock;
   fd->fsend("%s", cmd);
   if (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);
   fd->close();
   ua->jcr->file_bsock = NULL;
   return;
}

/*
 *   .die (seg fault)
 *   .dump (sm_dump)
 *   .exit (no arg => .quit)
 */
static bool admin_cmds(UAContext *ua, const char *cmd)
{
   pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   STORERES *store = NULL;
   CLIENTRES *client = NULL;
   bool dir=false;
   bool do_deadlock=false;
   const char *remote_cmd;
   int i;
   JCR *jcr = NULL;
   int a;
   if (strncmp(ua->argk[0], ".die", 4) == 0) {
      if (find_arg(ua, "deadlock") > 0) {
         do_deadlock = true;
         remote_cmd = ".die deadlock";
      } else {
         remote_cmd = ".die";
      }
   } else if (strncmp(ua->argk[0], ".dump", 5) == 0) {
      remote_cmd = "sm_dump";
   } else if (strncmp(ua->argk[0], ".exit", 5) == 0) {
      remote_cmd = "exit";
   } else {
      ua->error_msg(_("Unknown command: %s\n"), ua->argk[0]);
      return true;
   }
   /* General debug? */
   for (i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], "dir") ||
          bstrcasecmp(ua->argk[i], "director")) {
         dir = true;
      }
      if (bstrcasecmp(ua->argk[i], "client") ||
          bstrcasecmp(ua->argk[i], "fd")) {
         client = NULL;
         if (ua->argv[i]) {
            client = (CLIENTRES *)GetResWithName(R_CLIENT, ua->argv[i]);
         }
         if (!client) {
            client = select_client_resource(ua);
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("store")) ||
          bstrcasecmp(ua->argk[i], NT_("storage")) ||
          bstrcasecmp(ua->argk[i], NT_("sd"))) {
         store = NULL;
         if (ua->argv[i]) {
            store = (STORERES *)GetResWithName(R_STORAGE, ua->argv[i]);
         }
         if (!store) {
            store = get_storage_resource(ua);
         }
      }
   }

   if (!dir && !store && !client) {
      /*
       * We didn't find an appropriate keyword above, so
       * prompt the user.
       */
      start_prompt(ua, _("Available daemons are: \n"));
      add_prompt(ua, _("Director"));
      add_prompt(ua, _("Storage"));
      add_prompt(ua, _("Client"));
      switch(do_prompt(ua, "", _("Select daemon type to make die"), NULL, 0)) {
      case 0:                         /* Director */
         dir=true;
         break;
      case 1:
         store = get_storage_resource(ua);
         break;
      case 2:
         client = select_client_resource(ua);
         break;
      default:
         break;
      }
   }

   if (store) {
      switch (store->Protocol) {
      case APT_NDMPV2:
      case APT_NDMPV3:
      case APT_NDMPV4:
         ua->warning_msg(_("Storage has non-native protocol.\n"));
         break;
      default:
         do_storage_cmd(ua, store, remote_cmd);
         break;
      }
   }

   if (client) {
      do_client_cmd(ua, client, remote_cmd);
   }

   if (dir) {
      if (strncmp(remote_cmd, ".die", 4) == 0) {
         if (do_deadlock) {
            ua->send_msg(_("The Director will generate a deadlock.\n"));
            P(mutex);
            P(mutex);
         }
         ua->send_msg(_("The Director will segment fault.\n"));
         a = jcr->JobId; /* ref NULL pointer */
         jcr->JobId = 1000; /* another ref NULL pointer */
         jcr->JobId = a;

      } else if (strncmp(remote_cmd, ".dump", 5) == 0) {
         sm_dump(false, true);
      } else if (strncmp(remote_cmd, ".exit", 5) == 0) {
         dot_quit_cmd(ua, cmd);
      }
   }

   return true;
}
#else
/*
 * Dummy routine for non-development version
 */
static bool admin_cmds(UAContext *ua, const char *cmd)
{
   ua->error_msg(_("Unknown command: %s\n"), ua->argk[0]);
   return true;
}
#endif

static bool jobdefscmd(UAContext *ua, const char *cmd)
{
   JOBRES *jobdefs;

   LockRes();
   ua->send->object_start();
   foreach_res(jobdefs, R_JOBDEFS) {
      if (acl_access_ok(ua, Job_ACL, jobdefs->name())) {
         ua->send->object_start("jobdefs");
         ua->send->object_key_value("name", jobdefs->name(), "%s\n");
         ua->send->object_end("jobdefs");
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

/*
 * Can use an argument to filter on JobType
 * .jobs [type=B]
 */
static bool jobscmd(UAContext *ua, const char *cmd)
{
   JOBRES *job;

   uint32_t type = 0;
   int pos;
   if ((pos = find_arg_with_value(ua, "type")) >= 0) {
      type = ua->argv[pos][0];
   }
   LockRes();
   ua->send->object_start();
   foreach_res(job, R_JOB) {
      if (!type || type == job->JobType) {
         if (acl_access_ok(ua, Job_ACL, job->name())) {
            ua->send->object_start("jobs");
            ua->send->object_key_value("name", job->name(), "%s\n");
            ua->send->object_end("jobs");
         }
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool filesetscmd(UAContext *ua, const char *cmd)
{
   FILESETRES *fs;

   LockRes();
   ua->send->object_start();
   foreach_res(fs, R_FILESET) {
      if (acl_access_ok(ua, FileSet_ACL, fs->name())) {
         ua->send->object_start("filesets");
         ua->send->object_key_value("name", fs->name(), "%s\n");
         ua->send->object_end("filesets");
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool catalogscmd(UAContext *ua, const char *cmd)
{
   CATRES *cat;

   LockRes();
   ua->send->object_start();
   foreach_res(cat, R_CATALOG) {
      if (acl_access_ok(ua, Catalog_ACL, cat->name())) {
         ua->send->object_start("catalogs");
         ua->send->object_key_value("name", cat->name(), "%s\n");
         ua->send->object_end("catalogs");
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool clientscmd(UAContext *ua, const char *cmd)
{
   CLIENTRES *client;

   LockRes();
   ua->send->object_start();
   foreach_res(client, R_CLIENT) {
      if (acl_access_ok(ua, Client_ACL, client->name())) {
         ua->send->object_start("clients");
         ua->send->object_key_value("name", client->name(), "%s\n");
         ua->send->object_end("clients");
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool msgscmd(UAContext *ua, const char *cmd)
{
   MSGSRES *msgs = NULL;

   LockRes();
   ua->send->object_start("messages");
   foreach_res(msgs, R_MSGS) {
      ua->send->object_start();
      ua->send->object_key_value("text", msgs->name(), "%s\n");
      ua->send->object_end();
   }
   ua->send->object_end("messages");
   UnlockRes();

   return true;
}

static bool poolscmd(UAContext *ua, const char *cmd)
{
   POOLRES *pool;

   LockRes();
   ua->send->object_start();
   foreach_res(pool, R_POOL) {
      if (acl_access_ok(ua, Pool_ACL, pool->name())) {
         ua->send->object_start("pools");
         ua->send->object_key_value("name", pool->name(), "%s\n");
         ua->send->object_end("pools");
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool storagecmd(UAContext *ua, const char *cmd)
{
   STORERES *store;

   LockRes();
   ua->send->object_start();
   foreach_res(store, R_STORAGE) {
      if (acl_access_ok(ua, Storage_ACL, store->name())) {
         ua->send->object_start("storages");
         ua->send->object_key_value("name", store->name(), "%s\n");
         ua->send->object_end("storages");
      }
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool profilescmd(UAContext *ua, const char *cmd)
{
   PROFILERES *profile;

   LockRes();
   ua->send->object_start();
   foreach_res(profile, R_PROFILE) {
      ua->send->object_start("profiles");
      ua->send->object_key_value("name", profile->name(), "%s\n");
      ua->send->object_end("profiles");
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool aopcmd(UAContext *ua, const char *cmd)
{
   ua->send->object_start();
   for (int i = 0; ActionOnPurgeOptions[i].name; i++) {
      ua->send->object_start("actiononpurge");
      ua->send->object_key_value("name", ActionOnPurgeOptions[i].name, "%s\n");
      ua->send->object_end("actionsonpurge");
   }
   ua->send->object_end();

   return true;
}

static bool typescmd(UAContext *ua, const char *cmd)
{
   ua->send->object_start();
   for (int i = 0; jobtypes[i].type_name; i++) {
      ua->send->object_start("jobtypes");
      ua->send->object_key_value("name", jobtypes[i].type_name, "%s\n");
      ua->send->object_end("jobtypes");
   }
   ua->send->object_end();

   return true;
}

/*
 * If this command is called, it tells the director that we
 *  are a program that wants a sort of API, and hence,
 *  we will probably suppress certain output, include more
 *  error codes, and most of all send back a good number
 *  of new signals that indicate whether or not the command
 *  succeeded.
 */
static bool api_cmd(UAContext *ua, const char *cmd)
{
   int value = 0;

   if (ua->argc == 2) {
      if (bstrcasecmp(ua->argk[1], "off")) {
         ua->api = API_MODE_OFF;
      } else if (bstrcasecmp(ua->argk[1], "on")) {
         ua->api = API_MODE_ON;
      } else if (bstrcasecmp(ua->argk[1], "json")) {
         ua->api = API_MODE_JSON;
      } else {
         value = atoi(ua->argk[1]);
         if ((value < API_MODE_OFF) || (value > API_MODE_JSON)) {
            return false;
         }
         ua->api = value;
      }
   } else {
      ua->api = 1;
   }

   ua->send->set_mode(ua->api);
   ua->send->object_start();
   ua->send->object_key_value("api", "%s: ", ua->api, "%d\n");
   ua->send->object_end();

   return true;
}

static int client_backups_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   ua->send_msg("| %s | %s | %s | %s | %s | %s | %s | %s |\n",
      row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
   return 0;
}

/*
 * Return the backups for this client
 *
 * .backups client=xxx fileset=yyy
 */
static bool backupscmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (ua->argc != 3 ||
       !bstrcmp(ua->argk[1], "client") ||
       !bstrcmp(ua->argk[2], "fileset")) {
      return true;
   }

   if (!acl_access_ok(ua, Client_ACL, ua->argv[1], true) ||
       !acl_access_ok(ua, FileSet_ACL, ua->argv[2], true)) {
      ua->error_msg(_("Access to specified Client or FileSet not allowed.\n"));
      return true;
   }

   Mmsg(ua->cmd, client_backups, ua->argv[1], ua->argv[2]);
   if (!db_sql_query(ua->db, ua->cmd, client_backups_handler, (void *)ua)) {
      ua->error_msg(_("Query failed: %s. ERR=%s\n"), ua->cmd, db_strerror(ua->db));
      return true;
   }

   return true;
}

static int sql_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   POOL_MEM rows(PM_MESSAGE);

   /* Check for nonsense */
   if (num_field == 0 || row == NULL || row[0] == NULL) {
      return 0;                       /* nothing returned */
   }
   for (int i = 0; num_field--; i++) {
      if (i == 0) {
         pm_strcpy(rows, NPRT(row[0]));
      } else {
         pm_strcat(rows, NPRT(row[i]));
      }
      pm_strcat(rows, "\t");
   }
   if (!rows.c_str() || !*rows.c_str()) {
      ua->send_msg("\t");
   } else {
      ua->send_msg("%s", rows.c_str());
   }
   return 0;
}

static bool sql_cmd(UAContext *ua, const char *cmd)
{
   int index;

   if (!open_client_db(ua, true)) {
      return true;
   }

   index = find_arg_with_value(ua, "query");
   if (index < 0) {
      ua->error_msg(_("query keyword not found.\n"));
      return true;
   }

   if (!db_sql_query(ua->db, ua->argv[index], sql_handler, (void *)ua)) {
      Dmsg1(100, "Query failed: ERR=%s\n", db_strerror(ua->db));
      ua->error_msg(_("Query failed: %s. ERR=%s\n"), ua->cmd, db_strerror(ua->db));
      return true;
   }

   return true;
}

static int one_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;

   ua->send->object_start();
   ua->send->object_key_value("name", row[0], "%s\n");
   ua->send->object_end();

   return 0;
}

static bool mediatypescmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }

   ua->send->object_start("mediatypes");
   if (!db_sql_query(ua->db,
                     "SELECT DISTINCT MediaType FROM MediaType ORDER BY MediaType",
                     one_handler, (void *)ua)) {
      ua->error_msg(_("List MediaType failed: ERR=%s\n"), db_strerror(ua->db));
   }
   ua->send->object_end("mediatypes");

   return true;
}

static bool mediacmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }

   ua->send->object_start("media");
   if (!db_sql_query(ua->db,
                     "SELECT DISTINCT Media.VolumeName FROM Media ORDER BY VolumeName",
                     one_handler, (void *)ua)) {
      ua->error_msg(_("List Media failed: ERR=%s\n"), db_strerror(ua->db));
   }
   ua->send->object_end("media");

   return true;
}

static bool schedulecmd(UAContext *ua, const char *cmd)
{
   SCHEDRES *sched;

   LockRes();
   ua->send->object_start();
   foreach_res(sched, R_SCHEDULE) {
      ua->send->object_start("schedules");
      ua->send->object_key_value("name", sched->hdr.name, "%s\n");
      ua->send->object_end("schedules");
   }
   ua->send->object_end();
   UnlockRes();

   return true;
}

static bool locationscmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }

   ua->send->object_start("locations");
   if (!db_sql_query(ua->db,
                     "SELECT DISTINCT Location FROM Location ORDER BY Location",
                     one_handler, (void *)ua)) {
      ua->error_msg(_("List Location failed: ERR=%s\n"), db_strerror(ua->db));
   }
   ua->send->object_end("locations");

   return true;
}

static bool levelscmd(UAContext *ua, const char *cmd)
{
   /*
    * Note some levels are blank, which means none is needed
    */
   ua->send->object_start();
   if (ua->argc == 1) {
      for (int i = 0; joblevels[i].level_name; i++) {
         if (joblevels[i].level_name[0] != ' ') {
            ua->send->object_start("levels");
            ua->send->object_key_value("name", joblevels[i].level_name, "%s\n");
            ua->send->object_key_value("level", joblevels[i].level);
            ua->send->object_key_value("jobtype", joblevels[i].job_type);
            ua->send->object_end("levels");
         }
      }
   } else if (ua->argc == 2) {
      int jobtype = 0;

      /*
       * Assume that first argument is the Job Type
       */
      for (int i = 0; jobtypes[i].type_name; i++) {
         if (bstrcasecmp(ua->argk[1], jobtypes[i].type_name)) {
            jobtype = jobtypes[i].job_type;
            break;
         }
      }

      for (int i = 0; joblevels[i].level_name; i++) {
         if ((joblevels[i].job_type == jobtype) && (joblevels[i].level_name[0] != ' ')) {
            ua->send->object_start("levels");
            ua->send->object_key_value("name", joblevels[i].level_name, "%s\n");
            ua->send->object_key_value("level", joblevels[i].level);
            ua->send->object_key_value("jobtype", joblevels[i].job_type);
            ua->send->object_end("levels");
         }
      }
   }
   ua->send->object_end();

   return true;
}

static bool volstatuscmd(UAContext *ua, const char *cmd)
{
   ua->send->object_start();
   for (int i = 0; VolumeStatus[i].name; i++) {
      ua->send->object_start("volstatus");
      ua->send->object_key_value("name", VolumeStatus[i].name, "%s\n");
      ua->send->object_end("volstatus");
   }
   ua->send->object_end();

   return true;
}

/*
 * Return default values for a job
 */
static bool defaultscmd(UAContext *ua, const char *cmd)
{
   char ed1[50];

   if (ua->argc != 2 || !ua->argv[1]) {
      return true;
   }

   if (bstrcmp(ua->argk[1], "job")) {
      JOBRES *job;

      /*
       * Job defaults
       */
      if (!acl_access_ok(ua, Job_ACL, ua->argv[1], true)) {
         return true;
      }

      job = (JOBRES *)GetResWithName(R_JOB, ua->argv[1]);
      if (job) {
         USTORERES store;

         ua->send_msg("job=%s", job->name());
         ua->send_msg("pool=%s", job->pool->name());
         ua->send_msg("messages=%s", job->messages->name());
         ua->send_msg("client=%s", (job->client) ? job->client->name() : _("*None*"));
         get_job_storage(&store, job, NULL);
         ua->send_msg("storage=%s", store.store->name());
         ua->send_msg("where=%s", job->RestoreWhere ? job->RestoreWhere : "");
         ua->send_msg("level=%s", level_to_str(job->JobLevel));
         ua->send_msg("type=%s", job_type_to_str(job->JobType));
         ua->send_msg("fileset=%s", (job->fileset) ? job->fileset->name() : _("*None*"));
         ua->send_msg("enabled=%d", job->enabled);
         ua->send_msg("catalog=%s", (job->client) ? job->client->catalog->name() : _("*None*"));
      }
   } else if (bstrcmp(ua->argk[1], "client")) {
      CLIENTRES *client;

      /*
       * Client defaults
       */
      if (!acl_access_ok(ua, Client_ACL, ua->argv[1], true)) {
         return true;
      }

      client = (CLIENTRES *)GetResWithName(R_CLIENT, ua->argv[1]);
      if (client) {
         ua->send_msg("client=%s", client->name());
         ua->send_msg("address=%s", client->address);
         ua->send_msg("fdport=%d", client->FDport);
         ua->send_msg("file_retention=%s", edit_uint64(client->FileRetention, ed1));
         ua->send_msg("job_retention=%s", edit_uint64(client->JobRetention, ed1));
         ua->send_msg("autoprune=%d", client->AutoPrune);
         ua->send_msg("enabled=%d", client->enabled);
         ua->send_msg("catalog=%s", client->catalog->name());
      }
   } else if (bstrcmp(ua->argk[1], "storage")) {
      STORERES *storage;
      DEVICERES *device;

      /*
       * Storage defaults
       */
      if (!acl_access_ok(ua, Storage_ACL, ua->argv[1], true)) {
         return true;
      }

      storage = (STORERES *)GetResWithName(R_STORAGE, ua->argv[1]);
      if (storage) {
         ua->send_msg("storage=%s", storage->name());
         ua->send_msg("address=%s", storage->address);
         ua->send_msg("enabled=%d", storage->enabled);
         ua->send_msg("media_type=%s", storage->media_type);
         ua->send_msg("sdport=%d", storage->SDport);
         device = (DEVICERES *)storage->device->first();
         ua->send_msg("device=%s", device->name());
         if (storage->device->size() > 1) {
            while ((device = (DEVICERES *)storage->device->next())) {
               ua->send_msg(",%s", device->name());
            }
         }
      }
   } else if (bstrcmp(ua->argk[1], "pool")) {
      POOLRES *pool;

      /*
       * Pool defaults
       */
      if (!acl_access_ok(ua, Pool_ACL, ua->argv[1], true)) {
         return true;
      }

      pool = (POOLRES *)GetResWithName(R_POOL, ua->argv[1]);
      if (pool) {
         ua->send_msg("pool=%s", pool->name());
         ua->send_msg("pool_type=%s", pool->pool_type);
         ua->send_msg("label_format=%s", pool->label_format?pool->label_format:"");
         ua->send_msg("use_volume_once=%d", pool->use_volume_once);
         ua->send_msg("purge_oldest_volume=%d", pool->purge_oldest_volume);
         ua->send_msg("recycle_oldest_volume=%d", pool->recycle_oldest_volume);
         ua->send_msg("recycle_current_volume=%d", pool->recycle_current_volume);
         ua->send_msg("max_volumes=%d", pool->max_volumes);
         ua->send_msg("vol_retention=%s", edit_uint64(pool->VolRetention, ed1));
         ua->send_msg("vol_use_duration=%s", edit_uint64(pool->VolUseDuration, ed1));
         ua->send_msg("max_vol_jobs=%d", pool->MaxVolJobs);
         ua->send_msg("max_vol_files=%d", pool->MaxVolFiles);
         ua->send_msg("max_vol_bytes=%s", edit_uint64(pool->MaxVolBytes, ed1));
         ua->send_msg("auto_prune=%d", pool->AutoPrune);
         ua->send_msg("recycle=%d", pool->Recycle);
         ua->send_msg("file_retention=%s", edit_uint64(pool->FileRetention, ed1));
         ua->send_msg("job_retention=%s", edit_uint64(pool->JobRetention, ed1));
      }
   }

   return true;
}
