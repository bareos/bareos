/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- User Agent Commands
 *     These are "dot" commands, i.e. commands preceded
 *        by a period. These commands are meant to be used
 *        by a program, so there is no prompting, and the
 *        returned results are (supposed to be) predictable.
 *
 *     Kern Sibbald, April MMII
 *
 */

#include "bacula.h"
#include "dird.h"
#include "cats/bvfs.h"
#include "findlib/find.h"

/* Imported variables */
extern struct s_jl joblevels[];
extern struct s_jt jobtypes[];

/* Imported functions */
extern void do_messages(UAContext *ua, const char *cmd);
extern int quit_cmd(UAContext *ua, const char *cmd);
extern int qhelp_cmd(UAContext *ua, const char *cmd);
extern bool dot_status_cmd(UAContext *ua, const char *cmd);


/* Forward referenced functions */
static bool admin_cmds(UAContext *ua, const char *cmd);
static bool jobscmd(UAContext *ua, const char *cmd);
static bool filesetscmd(UAContext *ua, const char *cmd);
static bool clientscmd(UAContext *ua, const char *cmd);
static bool msgscmd(UAContext *ua, const char *cmd);
static bool poolscmd(UAContext *ua, const char *cmd);
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
static bool aopcmd(UAContext *ua, const char *cmd);
static bool catalogscmd(UAContext *ua, const char *cmd);

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
static bool dot_help_cmd(UAContext *ua, const char *cmd);
static int one_handler(void *ctx, int num_field, char **row);

struct cmdstruct { const char *key; bool (*func)(UAContext *ua, const char *cmd); const char *help;const bool use_in_rs;};
static struct cmdstruct commands[] = { /* help */  /* can be used in runscript */
 { NT_(".api"),        api_cmd,                  NULL,       false},
 { NT_(".backups"),    backupscmd,               NULL,       false},
 { NT_(".clients"),    clientscmd,               NULL,       true},
 { NT_(".catalogs"),   catalogscmd,              NULL,       false},
 { NT_(".defaults"),   defaultscmd,              NULL,       false},
 { NT_(".die"),        admin_cmds,               NULL,       false},
 { NT_(".dump"),       admin_cmds,               NULL,       false},
 { NT_(".exit"),       admin_cmds,               NULL,       false},
 { NT_(".filesets"),   filesetscmd,              NULL,       false},
 { NT_(".help"),       dot_help_cmd,             NULL,       false},
 { NT_(".jobs"),       jobscmd,                  NULL,       true},
 { NT_(".levels"),     levelscmd,                NULL,       false},
 { NT_(".messages"),   getmsgscmd,               NULL,       false},
 { NT_(".msgs"),       msgscmd,                  NULL,       false},
 { NT_(".pools"),      poolscmd,                 NULL,       true},
 { NT_(".quit"),       dot_quit_cmd,             NULL,       false},
 { NT_(".sql"),        sql_cmd,                  NULL,       false},
 { NT_(".status"),     dot_status_cmd,           NULL,       false},
 { NT_(".storage"),    storagecmd,               NULL,       true},
 { NT_(".volstatus"),  volstatuscmd,             NULL,       true},
 { NT_(".media"),      mediacmd,                 NULL,       true},
 { NT_(".mediatypes"), mediatypescmd,            NULL,       true},
 { NT_(".locations"),  locationscmd,             NULL,       true},
 { NT_(".actiononpurge"),aopcmd,                 NULL,       true},
 { NT_(".bvfs_lsdirs"), dot_bvfs_lsdirs,         NULL,       true},
 { NT_(".bvfs_lsfiles"),dot_bvfs_lsfiles,        NULL,       true},
 { NT_(".bvfs_update"), dot_bvfs_update,         NULL,       true},
 { NT_(".bvfs_get_jobids"), dot_bvfs_get_jobids, NULL,       true},
 { NT_(".bvfs_versions"), dot_bvfs_versions,     NULL,       true},
 { NT_(".bvfs_restore"), dot_bvfs_restore,       NULL,       true},
 { NT_(".bvfs_cleanup"), dot_bvfs_cleanup,       NULL,       true},
 { NT_(".bvfs_clear_cache"),dot_bvfs_clear_cache,NULL,       false},
 { NT_(".types"),      typescmd,                 NULL,       false}
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
      if (ua->api) user->signal(BNET_CMD_BEGIN);
      if (ua->api) user->signal(BNET_CMD_OK);
      return true;                    /* no op */
   }
   for (i=0; i<comsize; i++) {     /* search for command */
      if (strncasecmp(ua->argk[0],  _(commands[i].key), len) == 0) {
         /* Check if this command is authorized in RunScript */
         if (ua->runscript && !commands[i].use_in_rs) {
            ua->error_msg(_("Can't use %s command in a runscript"), ua->argk[0]);
            break;
         }
         bool gui = ua->gui;
         /* Check if command permitted, but "quit" is always OK */
         if (strcmp(ua->argk[0], NT_(".quit")) != 0 &&
             !acl_access_ok(ua, Command_ACL, ua->argk[0], len)) {
            break;
         }
         Dmsg1(100, "Cmd: %s\n", ua->cmd);
         ua->gui = true;
         if (ua->api) user->signal(BNET_CMD_BEGIN);
         ok = (*commands[i].func)(ua, ua->cmd);   /* go execute command */
         if (ua->api) user->signal(ok?BNET_CMD_OK:BNET_CMD_FAILED);
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
   if (!open_new_client_db(ua)) {
      return 1;
   }

   int pos = find_arg_with_value(ua, "jobid");
   if (pos != -1 && is_a_number_list(ua->argv[pos])) {
      if (!bvfs_update_path_hierarchy_cache(ua->jcr, ua->db, ua->argv[pos])) {
         ua->error_msg("ERROR: BVFS reported a problem for %s\n", 
                       ua->argv[pos]);
      }
   } else {
      /* update cache for all jobids */
      bvfs_update_cache(ua->jcr, ua->db);
   }
   
   close_db(ua);
   return true;
}

static bool dot_bvfs_clear_cache(UAContext *ua, const char *cmd)
{
   if (!open_new_client_db(ua)) {
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

   close_db(ua);
   return true;
}

static int bvfs_result_handler(void *ctx, int fields, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   struct stat statp;
   int32_t LinkFI;
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

   memset(&statp, 0, sizeof(struct stat));
   decode_stat(lstat, &statp, sizeof(statp), &LinkFI);

   Dmsg1(100, "type=%s\n", row[0]);
   if (bvfs_is_dir(row)) {
      char *path = bvfs_basename_dir(row[BVFS_Name]);
      ua->send_msg("%s\t0\t%s\t%s\t%s\t%s\n", row[BVFS_PathId], fileid,
                   jobid, lstat, path);

   } else if (bvfs_is_version(row)) {
      ua->send_msg("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", row[BVFS_PathId],
                   row[BVFS_FilenameId], fileid, jobid,
                   lstat, row[BVFS_Md5], row[BVFS_VolName], 
                   row[BVFS_VolInchanger]);

   } else if (bvfs_is_file(row)) {
      ua->send_msg("%s\t%s\t%s\t%s\t%s\t%s\n", row[BVFS_PathId],
                   row[BVFS_FilenameId], fileid, jobid,
                   lstat, row[BVFS_Name]);
   }

   return 0;
}

static bool bvfs_parse_arg_version(UAContext *ua,
                                   char **client,
                                   DBId_t *fnid, 
                                   bool *versions, 
                                   bool *copies)
{
   *fnid=0;
   *client=NULL;
   *versions=false;
   *copies=false;

   for (int i=1; i<ua->argc; i++) {
      if (fnid && strcasecmp(ua->argk[i], NT_("fnid")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *fnid = str_to_int64(ua->argv[i]);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("client")) == 0) {
         *client = ua->argv[i];
      }

      if (copies && strcasecmp(ua->argk[i], NT_("copies")) == 0) {
         *copies = true;
      }

      if (versions && strcasecmp(ua->argk[i], NT_("versions")) == 0) {
         *versions = true;
      }
   }
   return (*client && *fnid > 0);
}

static bool bvfs_parse_arg(UAContext *ua, 
                           DBId_t *pathid, char **path, char **jobid,
                           char **username,
                           int *limit, int *offset)
{
   *pathid=0;
   *limit=2000;
   *offset=0;
   *path=NULL;
   *jobid=NULL;
   *username=NULL;

   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("pathid")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *pathid = str_to_int64(ua->argv[i]);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("path")) == 0) {
         *path = ua->argv[i];
      }

      if (strcasecmp(ua->argk[i], NT_("username")) == 0) {
         *username = ua->argv[i];
      }
      
      if (strcasecmp(ua->argk[i], NT_("jobid")) == 0) {
         if (is_a_number_list(ua->argv[i])) {
            *jobid = ua->argv[i];
         }
      }

      if (strcasecmp(ua->argk[i], NT_("limit")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *limit = str_to_int64(ua->argv[i]);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("offset")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *offset = str_to_int64(ua->argv[i]);
         }
      }
   }

   if (!((*pathid || *path) && *jobid)) {
      return false;
   }

   if (!open_client_db(ua)) {
      return false;
   }

   return true;
}

/* .bvfs_cleanup path=b2XXXXX
 */
static bool dot_bvfs_cleanup(UAContext *ua, const char *cmd)
{
   int i;
   if ((i = find_arg_with_value(ua, "path")) >= 0) {
      open_client_db(ua);
      Bvfs fs(ua->jcr, ua->db);
      fs.drop_restore_list(ua->argv[i]);
   }
   return true;
}

/* .bvfs_restore path=b2XXXXX jobid=1,2 fileid=1,2 dirid=1,2 hardlink=1,2,3,4
 */
static bool dot_bvfs_restore(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int limit=2000, offset=0, i;
   char *path=NULL, *jobid=NULL, *username=NULL;
   char *empty = (char *)"";
   char *fileid, *dirid, *hardlink;
   fileid = dirid = hardlink = empty;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
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

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
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

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset))
   {
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
 * .bvfs_versions jobid=x fnid=10 pathid=10 copies versions
 * (jobid isn't used)
 */
static bool dot_bvfs_versions(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0, fnid=0;
   int limit=2000, offset=0;
   char *path=NULL, *jobid=NULL, *client=NULL, *username=NULL;
   bool copies=false, versions=false;
   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
   }

   if (!bvfs_parse_arg_version(ua, &client, &fnid, &versions, &copies))
   {
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

/* .bvfs_get_jobids jobid=1
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

   if (!open_client_db(ua)) {
      return true;
   }

   memset(&jr, 0, sizeof(JOB_DBR));

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

   /* When in level base, we don't rely on any Full/Incr/Diff */
   if (jr.JobLevel == L_BASE) {
      ua->send_msg("%s\n", edit_int64(jr.JobId, ed1));
      return true;
   }

   /* If we have the "all" option, we do a search on all defined fileset
    * for this client
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

   /* Foreach different FileSet, we build a restore jobid list */
   for (int i=0; i < ids.num_ids; i++) {
      jr.FileSetId = ids.DBId[i];
      if (!db_accurate_get_jobids(ua->jcr, ua->db, &jr, &tempids)) {
         return true;
      }
      jobids.add(tempids);
   }

   ua->send_msg("%s\n", jobids.list);
   return true;
}

static bool dot_quit_cmd(UAContext *ua, const char *cmd)
{
   quit_cmd(ua, cmd);
   return true;
}

static bool dot_help_cmd(UAContext *ua, const char *cmd)
{
   qhelp_cmd(ua, cmd);
   return true;
}

static bool getmsgscmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending) {
      do_messages(ua, cmd);
   }
   return 1;
}

#ifdef DEVELOPER
static void do_storage_cmd(UAContext *ua, STORE *store, const char *cmd)
{
   BSOCK *sd;
   JCR *jcr = ua->jcr;
   USTORE lstore;
   
   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(jcr, &lstore);
   /* Try connecting for up to 15 seconds */
   ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
      store->name(), store->address, store->SDport);
   if (!connect_to_storage_daemon(jcr, 1, 15, 0)) {
      ua->error_msg(_("Failed to connect to Storage daemon.\n"));
      return;
   }
   Dmsg0(120, _("Connected to storage daemon\n"));
   sd = jcr->store_bsock;
   sd->fsend("%s", cmd);
   if (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal(BNET_TERMINATE);
   sd->close();
   jcr->store_bsock = NULL;
   return;
}

static void do_client_cmd(UAContext *ua, CLIENT *client, const char *cmd)
{
   BSOCK *fd;

   /* Connect to File daemon */

   ua->jcr->client = client;
   /* Try to connect for 15 seconds */
   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
      client->name(), client->address, client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
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
   STORE *store=NULL;
   CLIENT *client=NULL;
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
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], "dir") == 0 ||
          strcasecmp(ua->argk[i], "director") == 0) {
         dir = true;
      }
      if (strcasecmp(ua->argk[i], "client") == 0 ||
          strcasecmp(ua->argk[i], "fd") == 0) {
         client = NULL;
         if (ua->argv[i]) {
            client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[i]);
         }
         if (!client) {
            client = select_client_resource(ua);
         }
      }
   
      if (strcasecmp(ua->argk[i], NT_("store")) == 0 ||
          strcasecmp(ua->argk[i], NT_("storage")) == 0 ||
          strcasecmp(ua->argk[i], NT_("sd")) == 0) {
         store = NULL;
         if (ua->argv[i]) {
            store = (STORE *)GetResWithName(R_STORAGE, ua->argv[i]);
         }
         if (!store) {
            store = get_storage_resource(ua, false/*no default*/);
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
         store = get_storage_resource(ua, false/*no default*/);
         break;
      case 2:
         client = select_client_resource(ua);
         break;
      default:
         break;
      }
   }

   if (store) {
      do_storage_cmd(ua, store, remote_cmd);
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

/* 
 * Can use an argument to filter on JobType
 * .jobs [type=B]
 */
static bool jobscmd(UAContext *ua, const char *cmd)
{
   JOB *job;
   uint32_t type = 0;
   int pos;
   if ((pos = find_arg_with_value(ua, "type")) >= 0) {
      type = ua->argv[pos][0];
   }
   LockRes();
   foreach_res(job, R_JOB) {
      if (!type || type == job->JobType) {
         if (acl_access_ok(ua, Job_ACL, job->name())) {
            ua->send_msg("%s\n", job->name());
         }
      }
   }
   UnlockRes();
   return true;
}

static bool filesetscmd(UAContext *ua, const char *cmd)
{
   FILESET *fs;
   LockRes();
   foreach_res(fs, R_FILESET) {
      if (acl_access_ok(ua, FileSet_ACL, fs->name())) {
         ua->send_msg("%s\n", fs->name());
      }
   }
   UnlockRes();
   return true;
}

static bool catalogscmd(UAContext *ua, const char *cmd)
{
   CAT *cat;
   LockRes();
   foreach_res(cat, R_CATALOG) {
      if (acl_access_ok(ua, Catalog_ACL, cat->name())) {
         ua->send_msg("%s\n", cat->name());
      }
   }
   UnlockRes();
   return true;
}

static bool clientscmd(UAContext *ua, const char *cmd)
{
   CLIENT *client;       
   LockRes();
   foreach_res(client, R_CLIENT) {
      if (acl_access_ok(ua, Client_ACL, client->name())) {
         ua->send_msg("%s\n", client->name());
      }
   }
   UnlockRes();
   return true;
}

static bool msgscmd(UAContext *ua, const char *cmd)
{
   MSGS *msgs = NULL;
   LockRes();
   foreach_res(msgs, R_MSGS) {
      ua->send_msg("%s\n", msgs->name());
   }
   UnlockRes();
   return true;
}

static bool poolscmd(UAContext *ua, const char *cmd)
{
   POOL *pool;       
   LockRes();
   foreach_res(pool, R_POOL) {
      if (acl_access_ok(ua, Pool_ACL, pool->name())) {
         ua->send_msg("%s\n", pool->name());
      }
   }
   UnlockRes();
   return true;
}

static bool storagecmd(UAContext *ua, const char *cmd)
{
   STORE *store;
   LockRes();
   foreach_res(store, R_STORAGE) {
      if (acl_access_ok(ua, Storage_ACL, store->name())) {
         ua->send_msg("%s\n", store->name());
      }
   }
   UnlockRes();
   return true;
}

static bool aopcmd(UAContext *ua, const char *cmd)
{
   ua->send_msg("None\n");
   ua->send_msg("Truncate\n");
   return true;
}

static bool typescmd(UAContext *ua, const char *cmd)
{
   ua->send_msg("Backup\n");
   ua->send_msg("Restore\n");
   ua->send_msg("Admin\n");
   ua->send_msg("Verify\n");
   ua->send_msg("Migrate\n");
   ua->send_msg("Copy\n");
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
   if (ua->argc == 2) {
      ua->api = atoi(ua->argk[1]);
   } else {
      ua->api = 1;
   }
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
 *  .backups client=xxx fileset=yyy
 *
 */
static bool backupscmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (ua->argc != 3 || strcmp(ua->argk[1], "client") != 0 || 
       strcmp(ua->argk[2], "fileset") != 0) {
      return true;
   }
   if (!acl_access_ok(ua, Client_ACL, ua->argv[1]) ||
       !acl_access_ok(ua, FileSet_ACL, ua->argv[2])) {
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
   for (int i=0; num_field--; i++) {
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
   if (!open_client_db(ua)) {
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
   ua->send_msg("%s\n", row[0]);
   return 0;
}

static bool mediatypescmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (!db_sql_query(ua->db, 
                  "SELECT DISTINCT MediaType FROM MediaType ORDER BY MediaType",
                  one_handler, (void *)ua)) 
   {
      ua->error_msg(_("List MediaType failed: ERR=%s\n"), db_strerror(ua->db));
   }
   return true;
}

static bool mediacmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (!db_sql_query(ua->db, 
                  "SELECT DISTINCT Media.VolumeName FROM Media ORDER BY VolumeName",
                  one_handler, (void *)ua)) 
   {
      ua->error_msg(_("List Media failed: ERR=%s\n"), db_strerror(ua->db));
   }
   return true;
}

static bool locationscmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (!db_sql_query(ua->db, 
                  "SELECT DISTINCT Location FROM Location ORDER BY Location",
                  one_handler, (void *)ua)) 
   {
      ua->error_msg(_("List Location failed: ERR=%s\n"), db_strerror(ua->db));
   }
   return true;
}

static bool levelscmd(UAContext *ua, const char *cmd)
{
   int i;
   /* Note some levels are blank, which means none is needed */
   if (ua->argc == 1) {
      for (i=0; joblevels[i].level_name; i++) {
         if (joblevels[i].level_name[0] != ' ') {
            ua->send_msg("%s\n", joblevels[i].level_name);
         }
      }
   } else if (ua->argc == 2) {
      int jobtype = 0;
      /* Assume that first argument is the Job Type */
      for (i=0; jobtypes[i].type_name; i++) {
         if (strcasecmp(ua->argk[1], jobtypes[i].type_name) == 0) {
            jobtype = jobtypes[i].job_type;
            break;
         }
      }
      for (i=0; joblevels[i].level_name; i++) {
         if ((joblevels[i].job_type == jobtype) && (joblevels[i].level_name[0] != ' ')) {
            ua->send_msg("%s\n", joblevels[i].level_name);
         }
      }
   }

   return true;
}

static bool volstatuscmd(UAContext *ua, const char *cmd)
{
   ua->send_msg("Append\n");
   ua->send_msg("Full\n");
   ua->send_msg("Used\n");
   ua->send_msg("Recycle\n");
   ua->send_msg("Purged\n");
   ua->send_msg("Cleaning\n");
   ua->send_msg("Error\n");
   return true;
}

/*
 * Return default values for a job
 */
static bool defaultscmd(UAContext *ua, const char *cmd)
{
   JOB *job;
   CLIENT *client;
   STORE *storage;
   POOL *pool;
   char ed1[50];

   if (ua->argc != 2 || !ua->argv[1]) {
      return true;
   }

   /* Job defaults */   
   if (strcmp(ua->argk[1], "job") == 0) {
      if (!acl_access_ok(ua, Job_ACL, ua->argv[1])) {
         return true;
      }
      job = (JOB *)GetResWithName(R_JOB, ua->argv[1]);
      if (job) {
         USTORE store;
         ua->send_msg("job=%s", job->name());
         ua->send_msg("pool=%s", job->pool->name());
         ua->send_msg("messages=%s", job->messages->name());
         ua->send_msg("client=%s", job->client->name());
         get_job_storage(&store, job, NULL);
         ua->send_msg("storage=%s", store.store->name());
         ua->send_msg("where=%s", job->RestoreWhere?job->RestoreWhere:"");
         ua->send_msg("level=%s", level_to_str(job->JobLevel));
         ua->send_msg("type=%s", job_type_to_str(job->JobType));
         ua->send_msg("fileset=%s", job->fileset->name());
         ua->send_msg("enabled=%d", job->enabled);
         ua->send_msg("catalog=%s", job->client->catalog->name());
      }
   } 
   /* Client defaults */
   else if (strcmp(ua->argk[1], "client") == 0) {
      if (!acl_access_ok(ua, Client_ACL, ua->argv[1])) {
         return true;   
      }
      client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[1]);
      if (client) {
         ua->send_msg("client=%s", client->name());
         ua->send_msg("address=%s", client->address);
         ua->send_msg("fdport=%d", client->FDport);
         ua->send_msg("file_retention=%s", edit_uint64(client->FileRetention, ed1));
         ua->send_msg("job_retention=%s", edit_uint64(client->JobRetention, ed1));
         ua->send_msg("autoprune=%d", client->AutoPrune);
         ua->send_msg("catalog=%s", client->catalog->name());
      }
   }
   /* Storage defaults */
   else if (strcmp(ua->argk[1], "storage") == 0) {
      if (!acl_access_ok(ua, Storage_ACL, ua->argv[1])) {
         return true;
      }
      storage = (STORE *)GetResWithName(R_STORAGE, ua->argv[1]);
      DEVICE *device;
      if (storage) {
         ua->send_msg("storage=%s", storage->name());
         ua->send_msg("address=%s", storage->address);
         ua->send_msg("enabled=%d", storage->enabled);
         ua->send_msg("media_type=%s", storage->media_type);
         ua->send_msg("sdport=%d", storage->SDport);
         device = (DEVICE *)storage->device->first();
         ua->send_msg("device=%s", device->name());
         if (storage->device->size() > 1) {
            while ((device = (DEVICE *)storage->device->next())) {
               ua->send_msg(",%s", device->name());
            }
         }
      }
   }
   /* Pool defaults */
   else if (strcmp(ua->argk[1], "pool") == 0) {
      if (!acl_access_ok(ua, Pool_ACL, ua->argv[1])) {
         return true;
      }
      pool = (POOL *)GetResWithName(R_POOL, ua->argv[1]);
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
