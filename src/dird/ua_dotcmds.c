/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#ifdef DEVELOPER
/* ua_cmds.c */
extern bool quit_cmd(UAContext *ua, const char *cmd);
#endif

/* ua_output.c */
extern void do_messages(UAContext *ua, const char *cmd);

struct authorization_mapping {
   const char *type;
   int acl_type;
};

static authorization_mapping authorization_mappings[] = {
   { "job", Job_ACL },
   { "client", Client_ACL },
   { "storage", Storage_ACL },
   { "schedule", Schedule_ACL },
   { "pool", Pool_ACL },
   { "cmd", Command_ACL },
   { "fileset", FileSet_ACL },
   { "catalog", Catalog_ACL },
   { NULL, 0 },
};

bool dot_authorized_cmd(UAContext *ua, const char *cmd)
{
   bool retval = false;

   for (int i = 1; i < ua->argc; i++) {
      for (int j = 0; authorization_mappings[j].type; j++) {
         if (bstrcasecmp(ua->argk[i], authorization_mappings[j].type)) {
            if (acl_access_ok(ua, authorization_mappings[j].acl_type, ua->argv[i], false)) {
               retval = true;
            } else {
               retval = false;
            }
         }
      }
   }

   ua->send->object_start(".authorized");
   if (retval) {
      ua->send->object_key_value_bool("authorized", retval, "authorized\n");
   } else {
      ua->send->object_key_value_bool("authorized", retval, "not authorized\n");
   }
   ua->send->object_end(".authorized");

   return retval;
}

bool dot_bvfs_update_cmd(UAContext *ua, const char *cmd)
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

bool dot_bvfs_clear_cache_cmd(UAContext *ua, const char *cmd)
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

static int bvfs_stat(UAContext *ua, char *lstat, int32_t *LinkFI)
{
   struct stat statp;
   char en1[30], en2[30];

   memset(&statp, 0, sizeof(struct stat));
   decode_stat(lstat, &statp, sizeof(statp), LinkFI);

   ua->send->object_start("stat");
   ua->send->object_key_value("dev", statp.st_dev);
   ua->send->object_key_value("ino", statp.st_ino);
   ua->send->object_key_value("mode", statp.st_mode);
   ua->send->object_key_value("nlink", statp.st_nlink);
   ua->send->object_key_value("user", ua->guid->uid_to_name(statp.st_uid, en1, sizeof(en1)));
   ua->send->object_key_value("group", ua->guid->gid_to_name(statp.st_gid, en2, sizeof(en2)));
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
   int32_t LinkFI = 0;

   /*
    * We need to deal with non existant path
    */
   if (!fileid || !is_a_number(fileid)) {
      lstat = empty;
      jobid = zero;
      fileid = zero;
   }

   Dmsg1(100, "type=%s\n", row[0]);
   if (bvfs_is_dir(row)) {
      char *path = bvfs_basename_dir(row[BVFS_Name]);

      ua->send->object_start();
      ua->send->object_key_value("Type", row[BVFS_Type]);
      ua->send->object_key_value("PathId", str_to_uint64(row[BVFS_PathId]), "%lld\t");
      ua->send->object_key_value("FilenameId", (uint64_t)0, "%lld\t");
      ua->send->object_key_value("FileId", str_to_uint64(fileid), "%lld\t");
      ua->send->object_key_value("JobId", str_to_uint64(jobid), "%lld\t");
      ua->send->object_key_value("lstat", lstat, "%s\t");
      ua->send->object_key_value("Name", path, "%s\n");
      ua->send->object_key_value("Fullpath", row[BVFS_Name]);
      bvfs_stat(ua, lstat, &LinkFI);
      ua->send->object_key_value("LinkFileIndex", LinkFI);
      ua->send->object_end();
  } else if (bvfs_is_version(row)) {
      ua->send->object_start();
      ua->send->object_key_value("Type", row[BVFS_Type]);
      ua->send->object_key_value("PathId", str_to_uint64(row[BVFS_PathId]), "%lld\t");
      ua->send->object_key_value("FilenameId", str_to_uint64(row[BVFS_FilenameId]), "%lld\t");
      ua->send->object_key_value("FileId", str_to_uint64(fileid), "%lld\t");
      ua->send->object_key_value("JobId", str_to_uint64(jobid), "%lld\t");
      ua->send->object_key_value("lstat", lstat, "%s\t");
      ua->send->object_key_value("MD5", row[BVFS_Md5], "%s\t");
      ua->send->object_key_value("VolumeName", row[BVFS_VolName], "%s\t");
      ua->send->object_key_value("VolumeInChanger", str_to_uint64(row[BVFS_VolInchanger]), "%lld\n");
      ua->send->object_end();
   } else if (bvfs_is_file(row)) {
      ua->send->object_start();
      ua->send->object_key_value("Type", row[BVFS_Type]);
      ua->send->object_key_value("PathId", str_to_uint64(row[BVFS_PathId]), "%lld\t");
      ua->send->object_key_value("FilenameId", str_to_uint64(row[BVFS_FilenameId]), "%lld\t");
      ua->send->object_key_value("FileId", str_to_uint64(fileid), "%lld\t");
      ua->send->object_key_value("JobId", str_to_uint64(jobid), "%lld\t");
      ua->send->object_key_value("lstat", lstat, "%s\t");
      ua->send->object_key_value("Name", row[BVFS_Name], "%s\n");
      bvfs_stat(ua, lstat, &LinkFI);
      ua->send->object_key_value("LinkFileIndex", LinkFI);
      ua->send->object_end();
   }

   return 0;
}

static bool bvfs_parse_arg_version(UAContext *ua, char **client, DBId_t *fnid, bool *versions, bool *copies)
{
   *fnid = 0;
   *client = NULL;
   *versions = false;
   *copies = false;

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("fnid")) ||
          bstrcasecmp(ua->argk[i], NT_("filenameid"))) {
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

static bool bvfs_parse_arg(UAContext *ua, DBId_t *pathid, char **path, char **jobid, int *limit, int *offset)
{
   *pathid = 0;
   *limit = 2000;
   *offset = 0;
   *path = NULL;
   *jobid = NULL;

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("pathid"))) {
         if (is_a_number(ua->argv[i])) {
            *pathid = str_to_int64(ua->argv[i]);
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("path"))) {
         *path = ua->argv[i];
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
bool dot_bvfs_cleanup_cmd(UAContext *ua, const char *cmd)
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
bool dot_bvfs_restore_cmd(UAContext *ua, const char *cmd)
{
   DBId_t pathid = 0;
   int limit = 2000, offset = 0, i;
   char *path = NULL, *jobid = NULL;
   char *empty = (char *)"";
   char *fileid, *dirid, *hardlink;
   fileid = dirid = hardlink = empty;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return false;             /* not enough param */
   }

   Bvfs fs(ua->jcr, ua->db);
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
bool dot_bvfs_lsfiles_cmd(UAContext *ua, const char *cmd)
{
   DBId_t pathid = 0;
   int limit = 2000, offset = 0;
   char *path = NULL, *jobid = NULL;
   char *pattern = NULL;
   int i;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return false;             /* not enough param */
   }

   if ((i = find_arg_with_value(ua, "pattern")) >= 0) {
      pattern = ua->argv[i];
   }

   if (!ua->guid) {
      ua->guid = new_guid_list();
   }

   Bvfs fs(ua->jcr, ua->db);
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

   ua->send->array_start("files");
   fs.ls_files();
   ua->send->array_end("files");

   return true;
}

/*
 * .bvfs_lsdirs jobid=1,2,3,4 pathid=10
 * .bvfs_lsdirs jobid=1,2,3,4 path=/
 * .bvfs_lsdirs jobid=1,2,3,4 path=
 */
bool dot_bvfs_lsdirs_cmd(UAContext *ua, const char *cmd)
{
   DBId_t pathid = 0;
   int limit = 2000, offset = 0;
   char *path = NULL, *jobid = NULL;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
   }

   if (!ua->guid) {
      ua->guid = new_guid_list();
   }

   Bvfs fs(ua->jcr, ua->db);
   fs.set_jobids(jobid);
   fs.set_limit(limit);
   fs.set_handler(bvfs_result_handler, ua);

   if (pathid) {
      fs.ch_dir(pathid);
   } else {
      fs.ch_dir(path);
   }

   fs.set_offset(offset);

   ua->send->array_start("directories");
   fs.ls_special_dirs();
   fs.ls_dirs();
   ua->send->array_end("directories");

   return true;
}

/*
 * .bvfs_versions jobid=0 client=<client-name> fnid=10 pathid=10 copies versions (jobid isn't used)
 */
bool dot_bvfs_versions_cmd(UAContext *ua, const char *cmd)
{
   DBId_t pathid = 0, fnid = 0;
   int limit = 2000, offset = 0;
   char *path = NULL, *jobid = NULL, *client = NULL;
   bool copies = false, versions = false;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &limit, &offset)) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return false;             /* not enough param */
   }

   if (!bvfs_parse_arg_version(ua, &client, &fnid, &versions, &copies)) {
      ua->error_msg("Can't find client or fnid argument\n");
      return false;              /* not enough param */
   }

   if (!ua->guid) {
      ua->guid = new_guid_list();
   }

   Bvfs fs(ua->jcr, ua->db);
   fs.set_limit(limit);
   fs.set_see_all_versions(versions);
   fs.set_see_copies(copies);
   fs.set_handler(bvfs_result_handler, ua);
   fs.set_offset(offset);
   ua->send->array_start("versions");
   fs.get_all_file_versions(pathid, fnid, client);
   ua->send->array_end("versions");

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
bool dot_bvfs_get_jobids_cmd(UAContext *ua, const char *cmd)
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
   } else if ((pos = find_arg_with_value(ua, "jobid")) >= 0) {
      jr.JobId = str_to_int64(ua->argv[pos]);
   } else {
      ua->error_msg(_("Can't find ujobid or jobid argument\n"));
      return false;
   }

   if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
      ua->error_msg(_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                    ua->argv[pos], db_strerror(ua->db));
      return false;
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

      ua->send->array_start("jobids");
      cur_id = jobids.list;
      while (cur_id && strlen(cur_id)) {
         bp = strchr(cur_id, ',');
         if (bp) {
            *bp++ = '\0';
         }

         ua->send->object_start();
         ua->send->object_key_value("id", cur_id, "%s\n");
         ua->send->object_end();

         cur_id = bp;
      }
      ua->send->array_end("jobids");
      break;
   }
   default:
      ua->send_msg("%s\n", jobids.list);
      break;
   }

   return true;
}

bool dot_getmsgs_cmd(UAContext *ua, const char *cmd)
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
   if (!connect_to_file_daemon(ua->jcr, 1, 15, false)) {
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
 * .die (seg fault)
 * .dump (sm_dump)
 * .exit (no arg => .quit)
 */
bool dot_admin_cmds(UAContext *ua, const char *cmd)
{
   pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   bool result = true;
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
   } else if (strncmp(ua->argk[0], ".memcheck", 8) == 0) {
      remote_cmd = "sm_check";
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

      } else if (strncmp(remote_cmd, "sm_dump", 7) == 0) {
         sm_dump(false, true);
         Dsm_check(100);
      } else if (strncmp(remote_cmd, "sm_check", 8) == 0) {
         result = sm_check_rtn(__FILE__, __LINE__, true);
         if (result) {
            ua->send_msg("memory: OK\n");
         } else {
            ua->error_msg("Memory managing problems detected.\n");
         }
      } else if (strncmp(remote_cmd, ".exit", 5) == 0) {
         quit_cmd(ua, cmd);
      }
   }

   return result;
}
#else
/*
 * Dummy routine for non-development version
 */
bool dot_admin_cmds(UAContext *ua, const char *cmd)
{
   ua->error_msg(_("Unknown command: %s\n"), ua->argk[0]);
   return true;
}
#endif

bool dot_jobdefs_cmd(UAContext *ua, const char *cmd)
{
   JOBRES *jobdefs;

   LockRes();
   ua->send->array_start("jobdefs");
   foreach_res(jobdefs, R_JOBDEFS) {
      if (acl_access_ok(ua, Job_ACL, jobdefs->name())) {
         ua->send->object_start();
         ua->send->object_key_value("name", jobdefs->name(), "%s\n");
         ua->send->object_end();
      }
   }
   ua->send->array_end("jobdefs");
   UnlockRes();

   return true;
}

/*
 * Can use an argument to filter on JobType
 * .jobs [type=B]
 */
bool dot_jobs_cmd(UAContext *ua, const char *cmd)
{
   int pos;
   JOBRES *job;
   uint32_t type = 0;

   if ((pos = find_arg_with_value(ua, "type")) >= 0) {
      type = ua->argv[pos][0];
   }

   LockRes();
   ua->send->array_start("jobs");
   foreach_res(job, R_JOB) {
      if (!type || type == job->JobType) {
         if (acl_access_ok(ua, Job_ACL, job->name())) {
            ua->send->object_start();
            ua->send->object_key_value("name", job->name(), "%s\n");
            ua->send->object_end();
         }
      }
   }
   ua->send->array_end("jobs");
   UnlockRes();

   return true;
}

bool dot_jobstatus_cmd(UAContext *ua, const char *cmd)
{
   bool retval = false;
   POOL_MEM select;
   POOL_MEM where;

   if (ua->argv[0]) {
      if (strlen(ua->argv[0]) == 1) {
         Mmsg(where, "WHERE JobStatus = '%c' ", ua->argv[0][0]);
      } else {
         ua->error_msg(_("Unknown JobStatus '%s'. JobStatus must be a single character.\n"), ua->argv[0]);
         return false;
      }
   }
   Mmsg(select, get_jobstatus_details, where.c_str());

   if (!open_client_db(ua)) {
      return false;
   }

   ua->send->array_start("jobstatus");
   retval = db_list_sql_query(ua->jcr, ua->db, select.c_str(), ua->send, HORZ_LIST, false);
   ua->send->array_end("jobstatus");

   return retval;
}

bool dot_filesets_cmd(UAContext *ua, const char *cmd)
{
   FILESETRES *fs;

   LockRes();
   ua->send->array_start("filesets");
   foreach_res(fs, R_FILESET) {
      if (acl_access_ok(ua, FileSet_ACL, fs->name())) {
         ua->send->object_start();
         ua->send->object_key_value("name", fs->name(), "%s\n");
         ua->send->object_end();
      }
   }
   ua->send->array_end("filesets");
   UnlockRes();

   return true;
}

bool dot_catalogs_cmd(UAContext *ua, const char *cmd)
{
   CATRES *cat;

   LockRes();
   ua->send->array_start("catalogs");
   foreach_res(cat, R_CATALOG) {
      if (acl_access_ok(ua, Catalog_ACL, cat->name())) {
         ua->send->object_start();
         ua->send->object_key_value("name", cat->name(), "%s\n");
         ua->send->object_end();
      }
   }
   ua->send->array_end("catalogs");
   UnlockRes();

   return true;
}

bool dot_clients_cmd(UAContext *ua, const char *cmd)
{
   CLIENTRES *client;

   LockRes();
   ua->send->array_start("clients");
   foreach_res(client, R_CLIENT) {
      if (acl_access_ok(ua, Client_ACL, client->name())) {
         ua->send->object_start();
         ua->send->object_key_value("name", client->name(), "%s\n");
         ua->send->object_end();
      }
   }
   ua->send->array_end("clients");
   UnlockRes();

   return true;
}

bool dot_msgs_cmd(UAContext *ua, const char *cmd)
{
   MSGSRES *msgs = NULL;

   LockRes();
   ua->send->array_start("messages");
   foreach_res(msgs, R_MSGS) {
      ua->send->object_start();
      ua->send->object_key_value("text", msgs->name(), "%s\n");
      ua->send->object_end();
   }
   ua->send->array_end("messages");
   UnlockRes();

   return true;
}

bool dot_pools_cmd(UAContext *ua, const char *cmd)
{
   int pos, length;
   POOLRES *pool;

   pos = find_arg_with_value(ua, "type");
   if (pos >= 0) {
      length = strlen(ua->argv[pos]);
   } else {
      length = 0;
   }

   LockRes();
   ua->send->array_start("pools");
   foreach_res(pool, R_POOL) {
      if (acl_access_ok(ua, Pool_ACL, pool->name())) {
         if (pos == -1 || bstrncasecmp(pool->pool_type, ua->argv[pos], length)) {
            ua->send->object_start();
            ua->send->object_key_value("name", pool->name(), "%s\n");
            ua->send->object_end();
         }
      }
   }
   ua->send->array_end("pools");
   UnlockRes();

   return true;
}

bool dot_storage_cmd(UAContext *ua, const char *cmd)
{
   STORERES *store;

   LockRes();
   ua->send->array_start("storages");
   foreach_res(store, R_STORAGE) {
      if (acl_access_ok(ua, Storage_ACL, store->name())) {
         ua->send->object_start();
         ua->send->object_key_value("name", store->name(), "%s\n");
         ua->send->object_end();
      }
   }
   ua->send->array_end("storages");
   UnlockRes();

   return true;
}

bool dot_profiles_cmd(UAContext *ua, const char *cmd)
{
   PROFILERES *profile;

   LockRes();
   ua->send->array_start("profiles");
   foreach_res(profile, R_PROFILE) {
      ua->send->object_start();
      ua->send->object_key_value("name", profile->name(), "%s\n");
      ua->send->object_end();
   }
   ua->send->array_end("profiles");
   UnlockRes();

   return true;
}

bool dot_aop_cmd(UAContext *ua, const char *cmd)
{
   ua->send->array_start("actiononpurge");
   for (int i = 0; ActionOnPurgeOptions[i].name; i++) {
      ua->send->object_start();
      ua->send->object_key_value("name", ActionOnPurgeOptions[i].name, "%s\n");
      ua->send->object_end();
   }
   ua->send->array_end("actiononpurge");

   return true;
}

bool dot_types_cmd(UAContext *ua, const char *cmd)
{
   ua->send->array_start("jobtypes");
   for (int i = 0; jobtypes[i].type_name; i++) {
      ua->send->object_start();
      ua->send->object_key_value("name", jobtypes[i].type_name, "%s\n");
      ua->send->object_end();
   }
   ua->send->array_end("jobtypes");

   return true;
}

/*
 * If this command is called, it tells the director that we
 * are a program that wants a sort of API, and hence,
 * we will probably suppress certain output, include more
 * error codes, and most of all send back a good number
 * of new signals that indicate whether or not the command
 * succeeded.
 */
bool dot_api_cmd(UAContext *ua, const char *cmd)
{
   if (ua->argc == 1) {
      ua->api = 1;
   } else if ((ua->argc >= 2) && (ua->argc <= 3)) {
      if (bstrcasecmp(ua->argk[1], "off") || bstrcasecmp(ua->argk[1], "0")) {
         ua->api = API_MODE_OFF;
         ua->batch = false;
      } else if (bstrcasecmp(ua->argk[1], "on") || bstrcasecmp(ua->argk[1], "1")) {
         ua->api = API_MODE_ON;
         ua->batch = false;
      } else if (bstrcasecmp(ua->argk[1], "json") || bstrcasecmp(ua->argk[1], "2")) {
         ua->api = API_MODE_JSON;
         ua->batch = true;
         if ((ua->argc == 3) && (find_arg_with_value(ua, "compact") == 2)) {
            if (bstrcasecmp(ua->argv[2], "yes")) {
               ua->send->set_compact(true);
            } else {
               ua->send->set_compact(false);
            }
         }
      } else {
         return false;
      }
   } else {
      return false;
   }

   ua->send->set_mode(ua->api);
   ua->send->object_key_value("api", "%s: ", ua->api, "%d\n");

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

bool dot_sql_cmd(UAContext *ua, const char *cmd)
{
   int pos;
   bool retval = false;

   if (!open_client_db(ua, true)) {
      return false;
   }

   pos = find_arg_with_value(ua, "query");
   if (pos < 0) {
      ua->error_msg(_("query keyword not found.\n"));
      return false;
   }

   switch (ua->api) {
   case API_MODE_ON:
      /*
       * BAT uses the ".sql" command and expects this format
       */
      retval = db_sql_query(ua->db, ua->argv[pos], sql_handler, (void *)ua);
      break;
   default:
      /*
       * General format
       */
      retval = db_list_sql_query(ua->jcr, ua->db, ua->argv[pos], ua->send, HORZ_LIST, false);
      break;
   }

   if (!retval) {
      Dmsg1(100, "Query failed: ERR=%s", db_strerror(ua->db));
      ua->error_msg(_("Query failed: %s. ERR=%s"), ua->cmd, db_strerror(ua->db));
   }

   return retval;
}

static int one_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;

   ua->send->object_start();
   ua->send->object_key_value("name", row[0], "%s\n");
   ua->send->object_end();

   return 0;
}

bool dot_mediatypes_cmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }

   ua->send->array_start("mediatypes");
   if (!db_sql_query(ua->db,
                     "SELECT DISTINCT MediaType FROM MediaType ORDER BY MediaType",
                     one_handler, (void *)ua)) {
      ua->error_msg(_("List MediaType failed: ERR=%s\n"), db_strerror(ua->db));
   }
   ua->send->array_end("mediatypes");

   return true;
}

bool dot_media_cmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }

   ua->send->array_start("media");
   if (!db_sql_query(ua->db,
                     "SELECT DISTINCT Media.VolumeName FROM Media ORDER BY VolumeName",
                     one_handler, (void *)ua)) {
      ua->error_msg(_("List Media failed: ERR=%s\n"), db_strerror(ua->db));
   }
   ua->send->array_end("media");

   return true;
}

bool dot_schedule_cmd(UAContext *ua, const char *cmd)
{
   SCHEDRES *sched;

   LockRes();
   ua->send->array_start("schedules");
   foreach_res(sched, R_SCHEDULE) {
      ua->send->object_start();
      ua->send->object_key_value("name", sched->hdr.name, "%s\n");
      ua->send->object_end();
   }
   ua->send->array_end("schedules");
   UnlockRes();

   return true;
}

bool dot_locations_cmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }

   ua->send->array_start("locations");
   if (!db_sql_query(ua->db,
                     "SELECT DISTINCT Location FROM Location ORDER BY Location",
                     one_handler, (void *)ua)) {
      ua->error_msg(_("List Location failed: ERR=%s\n"), db_strerror(ua->db));
   }
   ua->send->array_end("locations");

   return true;
}

bool dot_levels_cmd(UAContext *ua, const char *cmd)
{
   /*
    * Note some levels are blank, which means none is needed
    */
   ua->send->array_start("levels");
   if (ua->argc == 1) {
      for (int i = 0; joblevels[i].level_name; i++) {
         if (joblevels[i].level_name[0] != ' ') {
            ua->send->object_start();
            ua->send->object_key_value("name", joblevels[i].level_name, "%s\n");
            ua->send->object_key_value("level", joblevels[i].level);
            ua->send->object_key_value("jobtype", joblevels[i].job_type);
            ua->send->object_end();
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
            ua->send->object_start();
            ua->send->object_key_value("name", joblevels[i].level_name, "%s\n");
            ua->send->object_key_value("level", joblevels[i].level);
            ua->send->object_key_value("jobtype", joblevels[i].job_type);
            ua->send->object_end();
         }
      }
   }
   ua->send->array_end("levels");

   return true;
}

bool dot_volstatus_cmd(UAContext *ua, const char *cmd)
{
   ua->send->array_start("volstatus");
   for (int i = 0; VolumeStatus[i].name; i++) {
      ua->send->object_start();
      ua->send->object_key_value("name", VolumeStatus[i].name, "%s\n");
      ua->send->object_end();
   }
   ua->send->array_end("volstatus");

   return true;
}

/*
 * Return default values for a job
 */
bool dot_defaults_cmd(UAContext *ua, const char *cmd)
{
   char ed1[50];
   int pos = 0;

   ua->send->object_start("defaults");
   if ((pos = find_arg_with_value(ua, "job")) >= 0) {
      JOBRES *job;

      /*
       * Job defaults
       */
      if (!acl_access_ok(ua, Job_ACL, ua->argv[pos], true)) {
         return true;
      }

      job = (JOBRES *)GetResWithName(R_JOB, ua->argv[pos]);
      if (job) {
         USTORERES store;

         /*
          * BAT parses the result of this command message by message,
          * instead of looking for a seperator.
          * Therefore the send_buffer() function is called after each line.
          */
         ua->send->object_key_value("job", "%s=", job->name(), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("pool", "%s=", job->pool->name(), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("messages", "%s=", job->messages->name(), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("client", "%s=", ((job->client) ? job->client->name() : _("*None*")), "%s\n");
         ua->send->send_buffer();
         get_job_storage(&store, job, NULL);
         ua->send->object_key_value("storage", "%s=", store.store->name(), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("where", "%s=", (job->RestoreWhere ? job->RestoreWhere : ""), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("level", "%s=", level_to_str(job->JobLevel), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("type", "%s=", job_type_to_str(job->JobType), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("fileset", "%s=", ((job->fileset) ? job->fileset->name() : _("*None*")), "%s\n");
         ua->send->send_buffer();
         ua->send->object_key_value("enabled", "%s=", job->enabled, "%d\n");
         ua->send->send_buffer();
         ua->send->object_key_value("catalog", "%s=", ((job->client) ? job->client->catalog->name() : _("*None*")), "%s\n");
         ua->send->send_buffer();
      }
   } else if ((pos = find_arg_with_value(ua, "client")) >= 0) {
      CLIENTRES *client;

      /*
       * Client defaults
       */
      if (!acl_access_ok(ua, Client_ACL, ua->argv[pos], true)) {
         return true;
      }

      client = (CLIENTRES *)GetResWithName(R_CLIENT, ua->argv[pos]);
      if (client) {
         ua->send->object_key_value("client", "%s=", client->name(), "%s\n");
         ua->send->object_key_value("address", "%s=", client->address, "%s\n");
         ua->send->object_key_value("port", "%s=", client->FDport, "%d\n");
         ua->send->object_key_value("file_retention", "%s=", edit_uint64(client->FileRetention, ed1), "%s\n");
         ua->send->object_key_value("job_retention", "%s=", edit_uint64(client->JobRetention, ed1), "%s\n");
         ua->send->object_key_value("autoprune", "%s=", client->AutoPrune, "%d\n");
         ua->send->object_key_value("enabled", "%s=", client->enabled, "%d\n");
         ua->send->object_key_value("catalog", "%s=", client->catalog->name(), "%s\n");
      }
   } else if ((pos = find_arg_with_value(ua, "storage")) >= 0) {
      STORERES *storage;

      /*
       * Storage defaults
       */
      if (!acl_access_ok(ua, Storage_ACL, ua->argv[pos], true)) {
         return true;
      }

      storage = (STORERES *)GetResWithName(R_STORAGE, ua->argv[pos]);
      if (storage) {
         DEVICERES *device;
         POOL_MEM devices;

         ua->send->object_key_value("storage", "%s=", storage->name(), "%s\n");
         ua->send->object_key_value("address", "%s=", storage->address, "%s\n");
         ua->send->object_key_value("port", "%s=", storage->SDport, "%d\n");
         ua->send->object_key_value("enabled", "%s=", storage->enabled, "%d\n");
         ua->send->object_key_value("media_type", "%s=", storage->media_type, "%s\n");

         device = (DEVICERES *)storage->device->first();
         if (device) {
            devices.strcpy(device->name());
            if (storage->device->size() > 1) {
               while ((device = (DEVICERES *)storage->device->next())) {
                  devices.strcat(",");
                  devices.strcat(device->name());
               }
            }
            ua->send->object_key_value("device", "%s=", devices.c_str(), "%s\n");
         }
      }
   } else if ((pos = find_arg_with_value(ua, "pool")) >= 0) {
      POOLRES *pool;

      /*
       * Pool defaults
       */
      if (!acl_access_ok(ua, Pool_ACL, ua->argv[pos], true)) {
         return true;
      }

      pool = (POOLRES *)GetResWithName(R_POOL, ua->argv[pos]);
      if (pool) {
         ua->send->object_key_value("pool", "%s=", pool->name(), "%s\n");
         ua->send->object_key_value("pool_type", "%s=", pool->pool_type, "%s\n");
         ua->send->object_key_value("label_format", "%s=", (pool->label_format?pool->label_format:""), "%s\n");
         ua->send->object_key_value("use_volume_once", "%s=", pool->use_volume_once, "%d\n");
         ua->send->object_key_value("purge_oldest_volume=", "%s=", pool->purge_oldest_volume, "%d\n");
         ua->send->object_key_value("recycle_oldest_volume", "%s=", pool->recycle_oldest_volume, "%d\n");
         ua->send->object_key_value("max_volumes", "%s=", pool->max_volumes, "%d\n");
         ua->send->object_key_value("vol_retention", "%s=", edit_uint64(pool->VolRetention, ed1), "%s\n");
         ua->send->object_key_value("vol_use_duration", "%s=", edit_uint64(pool->VolUseDuration, ed1), "%s\n");
         ua->send->object_key_value("max_vol_jobs", "%s=", pool->MaxVolJobs, "%d\n");
         ua->send->object_key_value("max_vol_files", "%s=", pool->MaxVolFiles, "%d\n");
         ua->send->object_key_value("max_vol_bytes", "%s=", edit_uint64(pool->MaxVolBytes, ed1), "%s\n");
         ua->send->object_key_value("auto_prune", "%s=", pool->AutoPrune, "%d\n");
         ua->send->object_key_value("recycle", "%s=", pool->Recycle, "%d\n");
         ua->send->object_key_value("file_retention", "%s=", edit_uint64(pool->FileRetention, ed1), "%s\n");
         ua->send->object_key_value("job_retention", "%s=", edit_uint64(pool->JobRetention, ed1), "%s\n");
      }
   } else {
      ua->send_msg(".defaults command requires a parameter.\n");
      return false;
   }
   ua->send->object_end("defaults");

   return true;
}
