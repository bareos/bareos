/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
 * Kern Sibbald, April MMII
 */
/**
 * @file
 * User Agent Commands
 *
 * These are "dot" commands, i.e. commands preceded
 * by a period. These commands are meant to be used
 * by a program, so there is no prompting, and the
 * returned results are (supposed to be) predictable.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/dird_globals.h"
#include "dird/sd_cmds.h"
#include "dird/fd_cmds.h"
#include "cats/bvfs.h"
#include "findlib/find.h"
#include "dird/ua_db.h"
#include "dird/ua_select.h"
#include "dird/storage.h"
#include "include/auth_protocol_types.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

namespace directordaemon {

/* Imported variables */
extern struct s_jl joblevels[];
extern struct s_jt jobtypes[];
extern struct s_kw ActionOnPurgeOptions[];
extern struct s_kw VolumeStatus[];

/* Imported functions */

#ifdef DEVELOPER
/* ua_cmds.c */
extern bool quit_cmd(UaContext* ua, const char* cmd);
#endif

/* ua_output.c */
extern void DoMessages(UaContext* ua, const char* cmd);

struct authorization_mapping {
  const char* type;
  int acl_type;
};

static authorization_mapping authorization_mappings[] = {
    {"job", Job_ACL},
    {"client", Client_ACL},
    {"storage", Storage_ACL},
    {"schedule", Schedule_ACL},
    {"pool", Pool_ACL},
    {"cmd", Command_ACL},
    {"fileset", FileSet_ACL},
    {"catalog", Catalog_ACL},
    {NULL, 0},
};

bool DotAuthorizedCmd(UaContext* ua, const char* cmd)
{
  bool retval = false;

  for (int i = 1; i < ua->argc; i++) {
    for (int j = 0; authorization_mappings[j].type; j++) {
      if (Bstrcasecmp(ua->argk[i], authorization_mappings[j].type)) {
        if (ua->argv[i] && ua->AclAccessOk(authorization_mappings[j].acl_type,
                                           ua->argv[i], false)) {
          retval = true;
        } else {
          retval = false;
        }
      }
    }
  }

  ua->send->ObjectStart(".authorized");
  if (retval) {
    ua->send->ObjectKeyValueBool("authorized", retval, "authorized\n");
  } else {
    ua->send->ObjectKeyValueBool("authorized", retval, "not authorized\n");
  }
  ua->send->ObjectEnd(".authorized");

  return retval;
}

bool DotBvfsUpdateCmd(UaContext* ua, const char* cmd)
{
  int pos;

  if (!OpenClientDb(ua, true)) { return 1; }
  pos = FindArgWithValue(ua, "jobid");
  if (pos != -1 && Is_a_number_list(ua->argv[pos])) {
    if (!ua->db->BvfsUpdatePathHierarchyCache(ua->jcr, ua->argv[pos])) {
      ua->ErrorMsg("ERROR: BVFS reported a problem for %s\n", ua->argv[pos]);
    }
  } else {
    /* update cache for all jobids */
    ua->db->BvfsUpdateCache(ua->jcr);
  }

  return true;
}

bool DotBvfsClearCacheCmd(UaContext* ua, const char* cmd)
{
  if (!OpenClientDb(ua, true)) { return 1; }

  int pos = FindArg(ua, "yes");
  if (pos != -1) {
    Bvfs fs(ua->jcr, ua->db);
    fs.clear_cache();
    ua->InfoMsg("OK\n");
  } else {
    ua->ErrorMsg("Can't find 'yes' argument\n");
  }

  return true;
}

static int BvfsStat(UaContext* ua, char* lstat, int32_t* LinkFI)
{
  struct stat statp;
  char en1[30], en2[30];

  memset(&statp, 0, sizeof(struct stat));
  DecodeStat(lstat, &statp, sizeof(statp), LinkFI);

  ua->send->ObjectStart("stat");
  ua->send->ObjectKeyValue("dev", statp.st_dev);
  ua->send->ObjectKeyValue("ino", statp.st_ino);
  ua->send->ObjectKeyValue("mode", statp.st_mode);
  ua->send->ObjectKeyValue("nlink", statp.st_nlink);
  ua->send->ObjectKeyValue("uid", statp.st_uid);
  ua->send->ObjectKeyValue("gid", statp.st_gid);
  ua->send->ObjectKeyValue(
      "user", ua->guid->uid_to_name(statp.st_uid, en1, sizeof(en1)));
  ua->send->ObjectKeyValue(
      "group", ua->guid->gid_to_name(statp.st_gid, en2, sizeof(en2)));
  ua->send->ObjectKeyValue("rdev", statp.st_rdev);
  ua->send->ObjectKeyValue("size", statp.st_size);
  ua->send->ObjectKeyValue("atime", statp.st_atime);
  ua->send->ObjectKeyValue("mtime", statp.st_mtime);
  ua->send->ObjectKeyValue("ctime", statp.st_ctime);
  ua->send->ObjectEnd("stat");

  return 0;
}

static int BvfsResultHandler(void* ctx, int fields, char** row)
{
  UaContext* ua = (UaContext*)ctx;
  char* fileid = row[BVFS_FileId];
  char* lstat = row[BVFS_LStat];
  char* jobid = row[BVFS_JobId];

  char empty[] = "A A A A A A A A A A A A A A";
  char zero[] = "0";
  int32_t LinkFI = 0;

  /*
   * We need to deal with non existant path
   */
  if (!fileid || !Is_a_number(fileid)) {
    lstat = empty;
    jobid = zero;
    fileid = zero;
  }

  Dmsg1(100, "type=%s\n", row[0]);
  if (BvfsIsDir(row)) {
    char* path = bvfs_basename_dir(row[BVFS_Name]);

    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("Type", row[BVFS_Type]);
    ua->send->ObjectKeyValue("PathId", str_to_uint64(row[BVFS_PathId]),
                             "%lld\t");
    ua->send->ObjectKeyValue("FileId", str_to_uint64(fileid), "%lld\t");
    ua->send->ObjectKeyValue("JobId", str_to_uint64(jobid), "%lld\t");
    ua->send->ObjectKeyValue("lstat", lstat, "%s\t");
    ua->send->ObjectKeyValue("Name", path, "%s\n");
    ua->send->ObjectKeyValue("Fullpath", row[BVFS_Name]);
    BvfsStat(ua, lstat, &LinkFI);
    ua->send->ObjectKeyValue("LinkFileIndex", LinkFI);
    ua->send->ObjectEnd();
  } else if (BvfsIsVersion(row)) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("Type", row[BVFS_Type]);
    ua->send->ObjectKeyValue("PathId", str_to_uint64(row[BVFS_PathId]),
                             "%lld\t");
    ua->send->ObjectKeyValue("FileId", str_to_uint64(fileid), "%lld\t");
    ua->send->ObjectKeyValue("JobId", str_to_uint64(jobid), "%lld\t");
    ua->send->ObjectKeyValue("lstat", lstat, "%s\t");
    ua->send->ObjectKeyValue("MD5", row[BVFS_Md5], "%s\t");
    ua->send->ObjectKeyValue("VolumeName", row[BVFS_VolName], "%s\t");
    ua->send->ObjectKeyValue("VolumeInChanger",
                             str_to_uint64(row[BVFS_VolInchanger]), "%lld\n");
    BvfsStat(ua, lstat, &LinkFI);
    ua->send->ObjectEnd();
  } else if (BvfsIsFile(row)) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("Type", row[BVFS_Type]);
    ua->send->ObjectKeyValue("PathId", str_to_uint64(row[BVFS_PathId]),
                             "%lld\t");
    ua->send->ObjectKeyValue("FileId", str_to_uint64(fileid), "%lld\t");
    ua->send->ObjectKeyValue("JobId", str_to_uint64(jobid), "%lld\t");
    ua->send->ObjectKeyValue("lstat", lstat, "%s\t");
    ua->send->ObjectKeyValue("Name", row[BVFS_Name], "%s\n");
    BvfsStat(ua, lstat, &LinkFI);
    ua->send->ObjectKeyValue("LinkFileIndex", LinkFI);
    ua->send->ObjectEnd();
  }

  return 0;
}

static inline bool BvfsParseArgVersion(UaContext* ua,
                                       char** client,
                                       char** fname,
                                       bool* versions,
                                       bool* copies)
{
  *fname = NULL;
  *client = NULL;
  *versions = false;
  *copies = false;

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("name")) ||
        Bstrcasecmp(ua->argk[i], NT_("fname")) ||
        Bstrcasecmp(ua->argk[i], NT_("filename"))) {
      *fname = ua->argv[i];
    }

    if (Bstrcasecmp(ua->argk[i], NT_("client"))) { *client = ua->argv[i]; }

    if (copies && Bstrcasecmp(ua->argk[i], NT_("copies"))) { *copies = true; }

    if (versions && Bstrcasecmp(ua->argk[i], NT_("versions"))) {
      *versions = true;
    }
  }

  return (*client && *fname);
}

static bool BvfsParseArg(UaContext* ua,
                         DBId_t* pathid,
                         char** path,
                         char** jobid,
                         int* limit,
                         int* offset)
{
  *pathid = 0;
  *limit = 2000;
  *offset = 0;
  *path = NULL;
  *jobid = NULL;

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("pathid"))) {
      if (ua->argv[i] && Is_a_number(ua->argv[i])) {
        *pathid = str_to_int64(ua->argv[i]);
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("path"))) { *path = ua->argv[i]; }

    if (Bstrcasecmp(ua->argk[i], NT_("jobid"))) {
      if (ua->argv[i] && Is_a_number_list(ua->argv[i])) {
        *jobid = ua->argv[i];
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("limit"))) {
      if (ua->argv[i] && Is_a_number(ua->argv[i])) {
        *limit = str_to_int64(ua->argv[i]);
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("offset"))) {
      if (ua->argv[i] && Is_a_number(ua->argv[i])) {
        *offset = str_to_int64(ua->argv[i]);
      }
    }
  }

  if (!((*pathid || *path) && *jobid)) { return false; }

  if (!OpenClientDb(ua, true)) { return false; }

  return true;
}

/**
 * This checks to see if the JobId given is allowed under the current
 * ACLs e.g. comparing the JobName against the Job_ACL and the client
 * against the Client_ACL.
 */
static inline bool BvfsValidateJobid(UaContext* ua,
                                     const char* jobid,
                                     bool audit_event)
{
  JobDbRecord jr;
  ClientDbRecord cr;
  bool retval = false;

  jr.JobId = str_to_int64(jobid);

  if (ua->db->GetJobRecord(ua->jcr, &jr)) {
    if (!ua->AclAccessOk(Job_ACL, jr.Name, audit_event)) { goto bail_out; }

    if (jr.ClientId) {
      cr.ClientId = jr.ClientId;
      if (ua->db->GetClientRecord(ua->jcr, &cr)) {
        if (!ua->AclAccessOk(Client_ACL, cr.Name, audit_event)) {
          goto bail_out;
        }
      }
    }

    retval = true;
  }

bail_out:
  return retval;
}

/**
 * This returns in filtered_jobids the list of allowed jobids in the
 * jobids variable under the current ACLs e.g. using BvfsValidateJobid().
 */
static bool BvfsValidateJobids(UaContext* ua,
                               const char* jobids,
                               PoolMem& filtered_jobids,
                               bool audit_event)
{
  int cnt = 0;
  char *cur_id, *bp;
  PoolMem temp(PM_FNAME);

  PmStrcpy(temp, jobids);
  PmStrcpy(filtered_jobids, "");

  cur_id = temp.c_str();
  while (cur_id && strlen(cur_id)) {
    bp = strchr(cur_id, ',');
    if (bp) { *bp++ = '\0'; }

    /*
     * See if this JobId is allowed under the current ACLs.
     */
    if (BvfsValidateJobid(ua, cur_id, audit_event)) {
      if (!cnt) {
        PmStrcpy(filtered_jobids, cur_id);
      } else {
        PmStrcat(filtered_jobids, ",");
        PmStrcat(filtered_jobids, cur_id);
      }
      cnt++;
    } else {
      Dmsg1(200, "Removing jobid from list, %s\n", cur_id);
    }

    cur_id = bp;
  }

  return (cnt > 0) ? true : false;
}

/**
 * .bvfs_cleanup path=b2XXXXX
 */
bool DotBvfsCleanupCmd(UaContext* ua, const char* cmd)
{
  int i;

  if ((i = FindArgWithValue(ua, "path")) < 0) {
    ua->ErrorMsg("Can't find path argument\n");
    return false; /* not enough param */
  }

  if (!OpenClientDb(ua, true)) { return false; }

  Bvfs fs(ua->jcr, ua->db);
  fs.DropRestoreList(ua->argv[i]);

  return true;
}

/**
 * .bvfs_restore path=b2XXXXX jobid=1,2 fileid=1,2 dirid=1,2 hardlink=1,2,3,4
 */
bool DotBvfsRestoreCmd(UaContext* ua, const char* cmd)
{
  DBId_t pathid = 0;
  char* empty = (char*)"";
  int limit = 2000, offset = 0;
  int i = 0;
  char *path = NULL, *jobid = NULL;
  char *fileid, *dirid, *hardlink;
  PoolMem filtered_jobids(PM_FNAME);

  fileid = dirid = hardlink = empty;
  if (!BvfsParseArg(ua, &pathid, &path, &jobid, &limit, &offset)) {
    ua->ErrorMsg("Can't find jobid, pathid or path argument\n");
    return false; /* not enough param */
  }

  if (!BvfsValidateJobids(ua, jobid, filtered_jobids, true)) {
    ua->ErrorMsg(_("Unauthorized command from this console.\n"));
    return false;
  }

  Bvfs fs(ua->jcr, ua->db);
  fs.SetJobids(filtered_jobids.c_str());

  if ((i = FindArgWithValue(ua, "fileid")) >= 0) { fileid = ua->argv[i]; }
  if ((i = FindArgWithValue(ua, "dirid")) >= 0) { dirid = ua->argv[i]; }
  if ((i = FindArgWithValue(ua, "hardlink")) >= 0) { hardlink = ua->argv[i]; }

  if (fs.compute_restore_list(fileid, dirid, hardlink, path)) {
    ua->SendMsg("OK\n");
  } else {
    ua->ErrorMsg("Can't create restore list\n");
  }

  return true;
}

/**
 * .bvfs_lsfiles jobid=1,2,3,4 path=/
 * .bvfs_lsfiles jobid=1,2,3,4 pathid=10
 */
bool DotBvfsLsfilesCmd(UaContext* ua, const char* cmd)
{
  int i;
  DBId_t pathid = 0;
  char* pattern = NULL;
  int limit = 2000, offset = 0;
  char *path = NULL, *jobid = NULL;
  PoolMem filtered_jobids(PM_FNAME);

  if (!BvfsParseArg(ua, &pathid, &path, &jobid, &limit, &offset)) {
    ua->ErrorMsg("Can't find jobid, pathid or path argument\n");
    return false; /* not enough param */
  }

  if (!BvfsValidateJobids(ua, jobid, filtered_jobids, true)) {
    ua->ErrorMsg(_("Unauthorized command from this console.\n"));
    return false;
  }

  if ((i = FindArgWithValue(ua, "pattern")) >= 0) { pattern = ua->argv[i]; }

  if (!ua->guid) { ua->guid = new_guid_list(); }

  Bvfs fs(ua->jcr, ua->db);
  fs.SetJobids(filtered_jobids.c_str());
  fs.SetHandler(BvfsResultHandler, ua);
  fs.SetLimit(limit);
  if (pattern) { fs.SetPattern(pattern); }
  if (pathid) {
    fs.ChDir(pathid);
  } else {
    fs.ChDir(path);
  }

  fs.SetOffset(offset);

  ua->send->ArrayStart("files");
  fs.ls_files();
  ua->send->ArrayEnd("files");

  return true;
}

/**
 * .bvfs_lsdirs jobid=1,2,3,4 path=
 * .bvfs_lsdirs jobid=1,2,3,4 path=/
 * .bvfs_lsdirs jobid=1,2,3,4 pathid=10
 */
bool DotBvfsLsdirsCmd(UaContext* ua, const char* cmd)
{
  DBId_t pathid = 0;
  int limit = 2000, offset = 0;
  char *path = NULL, *jobid = NULL;
  PoolMem filtered_jobids(PM_FNAME);

  if (!BvfsParseArg(ua, &pathid, &path, &jobid, &limit, &offset)) {
    ua->ErrorMsg("Can't find jobid, pathid or path argument\n");
    return true; /* not enough param */
  }

  if (!BvfsValidateJobids(ua, jobid, filtered_jobids, true)) {
    ua->ErrorMsg(_("Unauthorized command from this console.\n"));
    return false;
  }

  if (!ua->guid) { ua->guid = new_guid_list(); }

  Bvfs fs(ua->jcr, ua->db);
  fs.SetJobids(filtered_jobids.c_str());

  if (pathid) {
    fs.ChDir(pathid);
  } else {
    if (!fs.ChDir(path)) {
      /* path could not be found. Giving up. */
      return false;
    }
  }

  fs.SetHandler(BvfsResultHandler, ua);
  fs.SetOffset(offset);
  fs.SetLimit(limit);

  ua->send->ArrayStart("directories");
  fs.ls_dirs();
  ua->send->ArrayEnd("directories");

  return true;
}

/**
 * .bvfs_versions jobid=0 client=<client-name> filename=<file-name>
 * pathid=<number> [copies] [versions]
 *
 * jobid isn't used.
 * versions is set, but not used.
 */
bool DotBvfsVersionsCmd(UaContext* ua, const char* cmd)
{
  DBId_t pathid = 0;
  int limit = 2000, offset = 0;
  char *path = NULL, *jobid = NULL, *client = NULL, *fname = NULL;
  bool copies = false, versions = false;

  if (!BvfsParseArg(ua, &pathid, &path, &jobid, &limit, &offset)) {
    ua->ErrorMsg("Can't find jobid, pathid or path argument\n");
    return false; /* not enough param */
  }

  if (!BvfsParseArgVersion(ua, &client, &fname, &versions, &copies)) {
    ua->ErrorMsg("Can't find client or fname argument\n");
    return false; /* not enough param */
  }

  if (!ua->AclAccessOk(Client_ACL, client)) {
    ua->ErrorMsg(_("Unauthorized command from this console.\n"));
    return false;
  }

  if (!ua->guid) { ua->guid = new_guid_list(); }

  Bvfs fs(ua->jcr, ua->db);
  fs.SetSeeAllVersions(versions);
  fs.SetSeeCopies(copies);
  fs.SetHandler(BvfsResultHandler, ua);
  fs.SetLimit(limit);
  fs.SetOffset(offset);
  ua->send->ArrayStart("versions");
  if (pathid) {
    fs.GetAllFileVersions(pathid, fname, client);
  } else {
    fs.GetAllFileVersions(path, fname, client);
  }
  ua->send->ArrayEnd("versions");

  return true;
}

/**
 * .bvfs_get_jobids jobid=1
 *  -> returns needed jobids to restore
 * .bvfs_get_jobids jobid=1 all
 *  -> returns needed jobids to restore with all filesets a JobId=1 time
 * .bvfs_get_jobids ujobid=JobName
 *  -> returns needed jobids to restore
 */
bool DotBvfsGetJobidsCmd(UaContext* ua, const char* cmd)
{
  int pos;
  JobDbRecord jr;
  char ed1[50];
  dbid_list ids; /* Store all FileSetIds for this client */
  PoolMem query;
  db_list_ctx jobids, tempids;
  PoolMem filtered_jobids(PM_FNAME);

  if (!OpenClientDb(ua, true)) { return true; }

  if ((pos = FindArgWithValue(ua, "ujobid")) >= 0) {
    bstrncpy(jr.Job, ua->argv[pos], sizeof(jr.Job));
  } else if ((pos = FindArgWithValue(ua, "jobid")) >= 0) {
    jr.JobId = str_to_int64(ua->argv[pos]);
  } else {
    ua->ErrorMsg(_("Can't find ujobid or jobid argument\n"));
    return false;
  }

  if (!ua->db->GetJobRecord(ua->jcr, &jr)) {
    ua->ErrorMsg(_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                 ua->argv[pos], ua->db->strerror());
    return false;
  }

  /*
   * When in level base, we don't rely on any Full/Incr/Diff
   */
  if (jr.JobLevel == L_BASE) {
    jobids.add(edit_int64(jr.JobId, ed1));
  } else {
    /*
     * If we have the "all" option, we do a search on all defined fileset for
     * this client
     */
    if (FindArg(ua, "all") > 0) {
      ua->db->FillQuery(query, BareosDb::SQL_QUERY::uar_sel_filesetid,
                        edit_int64(jr.ClientId, ed1));
      ua->db->GetQueryDbids(ua->jcr, query, ids);
    } else {
      ids.num_ids = 1;
      ids.DBId[0] = jr.FileSetId;
    }

    jr.JobLevel = L_INCREMENTAL; /* Take Full+Diff+Incr */

    /*
     * Foreach different FileSet, we build a restore jobid list
     */
    for (int i = 0; i < ids.num_ids; i++) {
      FileSetDbRecord fs;

      /*
       * Lookup the FileSet.
       */
      fs.FileSetId = ids.DBId[i];
      if (!ua->db->GetFilesetRecord(ua->jcr, &fs)) { continue; }

      /*
       * Make sure the FileSet is allowed under the current ACLs.
       */
      if (!ua->AclAccessOk(FileSet_ACL, fs.FileSet, false)) { continue; }

      /*
       * Lookup the actual JobIds for the given fileset.
       */
      jr.FileSetId = fs.FileSetId;
      if (!ua->db->AccurateGetJobids(ua->jcr, &jr, &tempids)) { return true; }
      jobids.add(tempids);
    }
  }

  BvfsValidateJobids(ua, jobids.GetAsString().c_str(), filtered_jobids, false);
  switch (ua->api) {
    case API_MODE_JSON: {
      char *cur_id, *bp;

      ua->send->ArrayStart("jobids");
      cur_id = filtered_jobids.c_str();
      while (cur_id && strlen(cur_id)) {
        bp = strchr(cur_id, ',');
        if (bp) { *bp++ = '\0'; }

        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("id", cur_id, "%s\n");
        ua->send->ObjectEnd();

        cur_id = bp;
      }
      ua->send->ArrayEnd("jobids");
      break;
    }
    default:
      ua->SendMsg("%s\n", filtered_jobids.c_str());
      break;
  }

  return true;
}

bool DotGetmsgsCmd(UaContext* ua, const char* cmd)
{
  if (console_msg_pending) { DoMessages(ua, cmd); }
  return 1;
}

#ifdef DEVELOPER
static void DoStorageCmd(UaContext* ua, StorageResource* store, const char* cmd)
{
  BareosSocket* sd;
  JobControlRecord* jcr = ua->jcr;
  UnifiedStorageResource lstore;

  lstore.store = store;
  PmStrcpy(lstore.store_source, _("unknown source"));
  SetWstorage(jcr, &lstore);

  if (!(sd = open_sd_bsock(ua))) {
    ua->ErrorMsg(_("Could not open SD socket.\n"));
    return;
  }

  Dmsg0(120, _("Connected to storage daemon\n"));
  sd = jcr->store_bsock;
  sd->fsend("%s", cmd);
  if (sd->recv() >= 0) { ua->SendMsg("%s", sd->msg); }

  CloseSdBsock(ua);
  return;
}

static void DoClientCmd(UaContext* ua, ClientResource* client, const char* cmd)
{
  BareosSocket* fd;

  /* Connect to File daemon */

  ua->jcr->impl->res.client = client;
  /* Try to connect for 15 seconds */
  ua->SendMsg(_("Connecting to Client %s at %s:%d\n"), client->resource_name_,
              client->address, client->FDport);
  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false, ua)) {
    ua->ErrorMsg(_("Failed to connect to Client.\n"));
    return;
  }
  Dmsg0(120, "Connected to file daemon\n");
  fd = ua->jcr->file_bsock;
  fd->fsend("%s", cmd);
  if (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }
  fd->signal(BNET_TERMINATE);
  fd->close();
  ua->jcr->file_bsock = NULL;
  return;
}

/**
 * .die (seg fault)
 * .exit (no arg => .quit)
 */
bool DotAdminCmds(UaContext* ua, const char* cmd)
{
  int i, a;
  JobControlRecord* jcr = NULL;
  bool dir = false;
  bool result = true;
  const char* remote_cmd;
  StorageResource* store = NULL;
  ClientResource* client = NULL;
  bool do_deadlock = false;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  if (strncmp(ua->argk[0], ".die", 4) == 0) {
    if (FindArg(ua, "deadlock") > 0) {
      do_deadlock = true;
      remote_cmd = ".die deadlock";
    } else {
      remote_cmd = ".die";
    }
  } else if (strncmp(ua->argk[0], ".exit", 5) == 0) {
    remote_cmd = "exit";
  } else {
    ua->ErrorMsg(_("Unknown command: %s\n"), ua->argk[0]);
    return true;
  }

  /*
   * General debug?
   */
  for (i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], "dir") ||
        Bstrcasecmp(ua->argk[i], "director")) {
      dir = true;
    }
    if (Bstrcasecmp(ua->argk[i], "client") || Bstrcasecmp(ua->argk[i], "fd")) {
      client = NULL;
      if (ua->argv[i]) { client = ua->GetClientResWithName(ua->argv[i]); }
      if (!client) { client = select_client_resource(ua); }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("store")) ||
        Bstrcasecmp(ua->argk[i], NT_("storage")) ||
        Bstrcasecmp(ua->argk[i], NT_("sd"))) {
      store = NULL;
      if (ua->argv[i]) { store = ua->GetStoreResWithName(ua->argv[i]); }
      if (!store) { store = get_storage_resource(ua); }
    }
  }

  if (!dir && !store && !client) {
    /*
     * We didn't find an appropriate keyword above, so
     * prompt the user.
     */
    StartPrompt(ua, _("Available daemons are: \n"));
    AddPrompt(ua, _("Director"));
    AddPrompt(ua, _("Storage"));
    AddPrompt(ua, _("Client"));
    switch (DoPrompt(ua, "", _("Select daemon type to make die"), NULL, 0)) {
      case 0: /* Director */
        dir = true;
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
        ua->WarningMsg(_("Storage has non-native protocol.\n"));
        break;
      default:
        DoStorageCmd(ua, store, remote_cmd);
        break;
    }
  }

  if (client) { DoClientCmd(ua, client, remote_cmd); }

  if (dir) {
    if (strncmp(remote_cmd, ".die", 4) == 0) {
      if (do_deadlock) {
        ua->SendMsg(_("The Director will generate a deadlock.\n"));
        P(mutex);
        P(mutex);
      }
      ua->SendMsg(_("The Director will segment fault.\n"));
      a = jcr->JobId;    /* ref NULL pointer */
      jcr->JobId = 1000; /* another ref NULL pointer */
      jcr->JobId = a;

    } else if (strncmp(remote_cmd, ".exit", 5) == 0) {
      quit_cmd(ua, cmd);
    }
  }

  return result;
}
#else
/**
 * Dummy routine for non-development version
 */
bool DotAdminCmds(UaContext* ua, const char* cmd)
{
  ua->ErrorMsg(_("Unknown command: %s\n"), ua->argk[0]);
  return true;
}
#endif

bool DotJobdefsCmd(UaContext* ua, const char* cmd)
{
  JobResource* jobdefs;

  LockRes(my_config);
  ua->send->ArrayStart("jobdefs");
  foreach_res (jobdefs, R_JOBDEFS) {
    if (ua->AclAccessOk(Job_ACL, jobdefs->resource_name_)) {
      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("name", jobdefs->resource_name_, "%s\n");
      ua->send->ObjectEnd();
    }
  }
  ua->send->ArrayEnd("jobdefs");
  UnlockRes(my_config);

  return true;
}

/**
 * Can use an argument to filter on JobType
 * .jobs [type=B]
 */
bool DotJobsCmd(UaContext* ua, const char* cmd)
{
  int pos;
  JobResource* job;
  bool enabled;
  bool disabled;
  uint32_t type = 0;

  if ((pos = FindArgWithValue(ua, "type")) >= 0) { type = ua->argv[pos][0]; }

  enabled = FindArg(ua, NT_("enabled")) >= 0;
  disabled = FindArg(ua, NT_("disabled")) >= 0;

  LockRes(my_config);
  ua->send->ArrayStart("jobs");
  foreach_res (job, R_JOB) {
    if (!type || type == job->JobType) {
      if (ua->AclAccessOk(Job_ACL, job->resource_name_)) {
        if (enabled && !job->enabled) { continue; }
        if (disabled && job->enabled) { continue; }

        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("name", job->resource_name_, "%s\n");
        ua->send->ObjectKeyValueBool("enabled", job->enabled);
        ua->send->ObjectEnd();
      }
    }
  }
  ua->send->ArrayEnd("jobs");
  UnlockRes(my_config);

  return true;
}

bool DotJobstatusCmd(UaContext* ua, const char* cmd)
{
  bool retval = false;
  PoolMem select;
  PoolMem where;

  if (ua->argv[0]) {
    if (strlen(ua->argv[0]) == 1) {
      Mmsg(where, "WHERE JobStatus = '%c' ", ua->argv[0][0]);
    } else {
      ua->ErrorMsg(
          _("Unknown JobStatus '%s'. JobStatus must be a single character.\n"),
          ua->argv[0]);
      return false;
    }
  }

  ua->db->FillQuery(select, BareosDb::SQL_QUERY::get_jobstatus_details,
                    where.c_str());

  if (!OpenClientDb(ua)) { return false; }

  ua->send->ArrayStart("jobstatus");
  retval =
      ua->db->ListSqlQuery(ua->jcr, select.c_str(), ua->send, HORZ_LIST, false);
  ua->send->ArrayEnd("jobstatus");

  return retval;
}

bool DotFilesetsCmd(UaContext* ua, const char* cmd)
{
  FilesetResource* fs;

  LockRes(my_config);
  ua->send->ArrayStart("filesets");
  foreach_res (fs, R_FILESET) {
    if (ua->AclAccessOk(FileSet_ACL, fs->resource_name_)) {
      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("name", fs->resource_name_, "%s\n");
      ua->send->ObjectEnd();
    }
  }
  ua->send->ArrayEnd("filesets");
  UnlockRes(my_config);

  return true;
}

bool DotCatalogsCmd(UaContext* ua, const char* cmd)
{
  CatalogResource* cat;

  LockRes(my_config);
  ua->send->ArrayStart("catalogs");
  foreach_res (cat, R_CATALOG) {
    if (ua->AclAccessOk(Catalog_ACL, cat->resource_name_)) {
      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("name", cat->resource_name_, "%s\n");
      ua->send->ObjectEnd();
    }
  }
  ua->send->ArrayEnd("catalogs");
  UnlockRes(my_config);

  return true;
}

bool DotClientsCmd(UaContext* ua, const char* cmd)
{
  bool enabled;
  bool disabled;
  ClientResource* client;

  enabled = FindArg(ua, NT_("enabled")) >= 0;
  disabled = FindArg(ua, NT_("disabled")) >= 0;

  LockRes(my_config);
  ua->send->ArrayStart("clients");
  foreach_res (client, R_CLIENT) {
    if (ua->AclAccessOk(Client_ACL, client->resource_name_)) {
      if (enabled && !client->enabled) { continue; }
      if (disabled && client->enabled) { continue; }

      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("name", client->resource_name_, "%s\n");
      ua->send->ObjectKeyValueBool("enabled", client->enabled);
      ua->send->ObjectEnd();
    }
  }
  ua->send->ArrayEnd("clients");
  UnlockRes(my_config);

  return true;
}

bool DotConsolesCmd(UaContext* ua, const char* cmd)
{
  ConsoleResource* console;

  LockRes(my_config);
  ua->send->ArrayStart("consoles");
  foreach_res (console, R_CONSOLE) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", console->resource_name_, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("consoles");
  UnlockRes(my_config);

  return true;
}

bool DotUsersCmd(UaContext* ua, const char* cmd)
{
  UserResource* user;

  LockRes(my_config);
  ua->send->ArrayStart("users");
  foreach_res (user, R_USER) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", user->resource_name_, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("users");
  UnlockRes(my_config);

  return true;
}

bool DotMsgsCmd(UaContext* ua, const char* cmd)
{
  MessagesResource* msgs = NULL;

  LockRes(my_config);
  ua->send->ArrayStart("messages");
  foreach_res (msgs, R_MSGS) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("text", msgs->resource_name_, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("messages");
  UnlockRes(my_config);

  return true;
}

bool DotPoolsCmd(UaContext* ua, const char* cmd)
{
  int pos, length;
  PoolResource* pool;

  pos = FindArgWithValue(ua, "type");
  if (pos >= 0) {
    length = strlen(ua->argv[pos]);
  } else {
    length = 0;
  }

  LockRes(my_config);
  ua->send->ArrayStart("pools");
  foreach_res (pool, R_POOL) {
    if (ua->AclAccessOk(Pool_ACL, pool->resource_name_)) {
      if (pos == -1 || bstrncasecmp(pool->pool_type, ua->argv[pos], length)) {
        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("name", pool->resource_name_, "%s\n");
        ua->send->ObjectEnd();
      }
    }
  }
  ua->send->ArrayEnd("pools");
  UnlockRes(my_config);

  return true;
}

bool DotStorageCmd(UaContext* ua, const char* cmd)
{
  bool enabled;
  bool disabled;
  StorageResource* store;

  enabled = FindArg(ua, NT_("enabled")) >= 0;
  disabled = FindArg(ua, NT_("disabled")) >= 0;

  LockRes(my_config);
  ua->send->ArrayStart("storages");
  foreach_res (store, R_STORAGE) {
    if (ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
      if (enabled && !store->enabled) { continue; }
      if (disabled && store->enabled) { continue; }

      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("name", store->resource_name_, "%s\n");
      ua->send->ObjectKeyValueBool("enabled", store->enabled);
      ua->send->ObjectEnd();
    }
  }
  ua->send->ArrayEnd("storages");
  UnlockRes(my_config);

  return true;
}

bool DotProfilesCmd(UaContext* ua, const char* cmd)
{
  ProfileResource* profile;

  LockRes(my_config);
  ua->send->ArrayStart("profiles");
  foreach_res (profile, R_PROFILE) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", profile->resource_name_, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("profiles");
  UnlockRes(my_config);

  return true;
}

bool DotAopCmd(UaContext* ua, const char* cmd)
{
  ua->send->ArrayStart("actiononpurge");
  for (int i = 0; ActionOnPurgeOptions[i].name; i++) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", ActionOnPurgeOptions[i].name, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("actiononpurge");

  return true;
}

bool DotTypesCmd(UaContext* ua, const char* cmd)
{
  ua->send->ArrayStart("jobtypes");
  for (int i = 0; jobtypes[i].type_name; i++) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", jobtypes[i].type_name, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("jobtypes");

  return true;
}

/**
 * If this command is called, it tells the director that we
 * are a program that wants a sort of API, and hence,
 * we will probably suppress certain output, include more
 * error codes, and most of all send back a good number
 * of new signals that indicate whether or not the command
 * succeeded.
 */
bool DotApiCmd(UaContext* ua, const char* cmd)
{
  if (ua->argc == 1) {
    ua->api = 1;
  } else if ((ua->argc >= 2) && (ua->argc <= 3)) {
    if (Bstrcasecmp(ua->argk[1], "off") || Bstrcasecmp(ua->argk[1], "0")) {
      ua->api = API_MODE_OFF;
      ua->batch = false;
    } else if (Bstrcasecmp(ua->argk[1], "on") ||
               Bstrcasecmp(ua->argk[1], "1")) {
      ua->api = API_MODE_ON;
      ua->batch = false;
    } else if (Bstrcasecmp(ua->argk[1], "json") ||
               Bstrcasecmp(ua->argk[1], "2")) {
      ua->api = API_MODE_JSON;
      ua->batch = true;
      if ((ua->argc == 3) && (FindArgWithValue(ua, "compact") == 2)) {
        if (Bstrcasecmp(ua->argv[2], "yes")) {
          ua->send->SetCompact(true);
        } else {
          ua->send->SetCompact(false);
        }
      }
    } else {
      return false;
    }
  } else {
    return false;
  }

  ua->send->SetMode(ua->api);
  ua->send->ObjectKeyValue("api", "%s: ", ua->api, "%d\n");

  return true;
}

static int SqlHandler(void* ctx, int num_field, char** row)
{
  UaContext* ua = (UaContext*)ctx;
  PoolMem rows(PM_MESSAGE);

  /* Check for nonsense */
  if (num_field == 0 || row == NULL || row[0] == NULL) {
    return 0; /* nothing returned */
  }
  for (int i = 0; num_field--; i++) {
    if (i == 0) {
      PmStrcpy(rows, NPRT(row[0]));
    } else {
      PmStrcat(rows, NPRT(row[i]));
    }
    PmStrcat(rows, "\t");
  }
  if (!rows.c_str() || !*rows.c_str()) {
    ua->SendMsg("\t");
  } else {
    ua->SendMsg("%s", rows.c_str());
  }
  return 0;
}

bool DotSqlCmd(UaContext* ua, const char* cmd)
{
  int pos;
  bool retval = false;

  if (!OpenClientDb(ua, true)) { return false; }

  pos = FindArgWithValue(ua, "query");
  if (pos < 0) {
    ua->ErrorMsg(_("query keyword not found.\n"));
    return false;
  }

  switch (ua->api) {
    case API_MODE_ON:
      /*
       * BAT uses the ".sql" command and expects this format
       */
      retval = ua->db->SqlQuery(ua->argv[pos], SqlHandler, (void*)ua);
      break;
    default:
      /*
       * General format
       */
      retval = ua->db->ListSqlQuery(ua->jcr, ua->argv[pos], ua->send, HORZ_LIST,
                                    false);
      break;
  }

  if (!retval) {
    Dmsg1(100, "Query failed: ERR=%s", ua->db->strerror());
    ua->ErrorMsg(_("Query failed: %s. ERR=%s"), ua->cmd, ua->db->strerror());
  }

  return retval;
}

static int OneHandler(void* ctx, int num_field, char** row)
{
  UaContext* ua = (UaContext*)ctx;

  ua->send->ObjectStart();
  ua->send->ObjectKeyValue("name", row[0], "%s\n");
  ua->send->ObjectEnd();

  return 0;
}

bool DotMediatypesCmd(UaContext* ua, const char* cmd)
{
  if (!OpenClientDb(ua)) { return true; }

  ua->send->ArrayStart("mediatypes");
  if (!ua->db->SqlQuery(
          "SELECT DISTINCT MediaType FROM MediaType ORDER BY MediaType",
          OneHandler, (void*)ua)) {
    ua->ErrorMsg(_("List MediaType failed: ERR=%s\n"), ua->db->strerror());
  }
  ua->send->ArrayEnd("mediatypes");

  return true;
}

bool DotMediaCmd(UaContext* ua, const char* cmd)
{
  if (!OpenClientDb(ua)) { return true; }

  ua->send->ArrayStart("media");
  if (!ua->db->SqlQuery(
          "SELECT DISTINCT Media.VolumeName FROM Media ORDER BY VolumeName",
          OneHandler, (void*)ua)) {
    ua->ErrorMsg(_("List Media failed: ERR=%s\n"), ua->db->strerror());
  }
  ua->send->ArrayEnd("media");

  return true;
}

bool DotScheduleCmd(UaContext* ua, const char* cmd)
{
  bool enabled;
  bool disabled;
  ScheduleResource* sched;

  enabled = FindArg(ua, NT_("enabled")) >= 0;
  disabled = FindArg(ua, NT_("disabled")) >= 0;

  LockRes(my_config);
  ua->send->ArrayStart("schedules");
  foreach_res (sched, R_SCHEDULE) {
    if (ua->AclAccessOk(Schedule_ACL, sched->resource_name_)) {
      if (enabled && !sched->enabled) { continue; }
      if (disabled && sched->enabled) { continue; }

      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("name", sched->resource_name_, "%s\n");
      ua->send->ObjectKeyValueBool("enabled", sched->enabled);
      ua->send->ObjectEnd();
    }
  }
  ua->send->ArrayEnd("schedules");
  UnlockRes(my_config);

  return true;
}

bool DotLocationsCmd(UaContext* ua, const char* cmd)
{
  if (!OpenClientDb(ua)) { return true; }

  ua->send->ArrayStart("locations");
  if (!ua->db->SqlQuery(
          "SELECT DISTINCT Location FROM Location ORDER BY Location",
          OneHandler, (void*)ua)) {
    ua->ErrorMsg(_("List Location failed: ERR=%s\n"), ua->db->strerror());
  }
  ua->send->ArrayEnd("locations");

  return true;
}

bool DotLevelsCmd(UaContext* ua, const char* cmd)
{
  /*
   * Note some levels are blank, which means none is needed
   */
  ua->send->ArrayStart("levels");
  if (ua->argc == 1) {
    for (int i = 0; joblevels[i].level_name; i++) {
      if (joblevels[i].level_name[0] != ' ') {
        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("name", joblevels[i].level_name, "%s\n");
        ua->send->ObjectKeyValue("level", joblevels[i].level);
        ua->send->ObjectKeyValue("jobtype", joblevels[i].job_type);
        ua->send->ObjectEnd();
      }
    }
  } else if (ua->argc == 2) {
    int jobtype = 0;

    /*
     * Assume that first argument is the Job Type
     */
    for (int i = 0; jobtypes[i].type_name; i++) {
      if (Bstrcasecmp(ua->argk[1], jobtypes[i].type_name)) {
        jobtype = jobtypes[i].job_type;
        break;
      }
    }

    for (int i = 0; joblevels[i].level_name; i++) {
      if ((joblevels[i].job_type == jobtype) &&
          (joblevels[i].level_name[0] != ' ')) {
        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("name", joblevels[i].level_name, "%s\n");
        ua->send->ObjectKeyValue("level", joblevels[i].level);
        ua->send->ObjectKeyValue("jobtype", joblevels[i].job_type);
        ua->send->ObjectEnd();
      }
    }
  }
  ua->send->ArrayEnd("levels");

  return true;
}

bool DotVolstatusCmd(UaContext* ua, const char* cmd)
{
  ua->send->ArrayStart("volstatus");
  for (int i = 0; VolumeStatus[i].name; i++) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", VolumeStatus[i].name, "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("volstatus");

  return true;
}

/**
 * Return default values for a job
 */
bool DotDefaultsCmd(UaContext* ua, const char* cmd)
{
  char ed1[50];
  int pos = 0;

  ua->send->ObjectStart("defaults");
  if ((pos = FindArgWithValue(ua, "job")) >= 0) {
    JobResource* job;

    /*
     * Job defaults
     */
    job = ua->GetJobResWithName(ua->argv[pos]);
    if (job) {
      UnifiedStorageResource store;

      /*
       * BAT parses the result of this command message by message,
       * instead of looking for a separator.
       * Therefore the SendBuffer() function is called after each line.
       */
      ua->send->ObjectKeyValue("job", "%s=", job->resource_name_, "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue("pool", "%s=", job->pool->resource_name_,
                               "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue("messages", "%s=", job->messages->resource_name_,
                               "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue(
          "client",
          "%s=", ((job->client) ? job->client->resource_name_ : _("*None*")),
          "%s\n");
      ua->send->SendBuffer();
      GetJobStorage(&store, job, NULL);
      ua->send->ObjectKeyValue(
          "storage",
          "%s=", store.store ? store.store->resource_name_ : "*None*", "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue(
          "where", "%s=", (job->RestoreWhere ? job->RestoreWhere : ""), "%s\n");
      if (job->JobType == JT_RESTORE) {
        ua->send->SendBuffer();
        ua->send->ObjectKeyValue(
            "replace", "%s=", job_replace_to_str(job->replace), "%s\n");
      }
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue("level", "%s=", JobLevelToString(job->JobLevel),
                               "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue("type", "%s=", job_type_to_str(job->JobType),
                               "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue(
          "fileset",
          "%s=", ((job->fileset) ? job->fileset->resource_name_ : _("*None*")),
          "%s\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue("enabled", "%s=", job->enabled, "%d\n");
      ua->send->SendBuffer();
      ua->send->ObjectKeyValue(
          "catalog", "%s=",
          ((job->client) ? job->client->catalog->resource_name_ : _("*None*")),
          "%s\n");
      ua->send->SendBuffer();
    }
  } else if ((pos = FindArgWithValue(ua, "client")) >= 0) {
    ClientResource* client;

    /*
     * Client defaults
     */
    client = ua->GetClientResWithName(ua->argv[pos]);
    if (client) {
      ua->send->ObjectKeyValue("client", "%s=", client->resource_name_, "%s\n");
      ua->send->ObjectKeyValue("address", "%s=", client->address, "%s\n");
      ua->send->ObjectKeyValue("port", "%s=", client->FDport, "%d\n");
      ua->send->ObjectKeyValue("file_retention",
                               "%s=", edit_uint64(client->FileRetention, ed1),
                               "%s\n");
      ua->send->ObjectKeyValue("job_retention",
                               "%s=", edit_uint64(client->JobRetention, ed1),
                               "%s\n");
      ua->send->ObjectKeyValue("autoprune", "%s=", client->AutoPrune, "%d\n");
      ua->send->ObjectKeyValue("enabled", "%s=", client->enabled, "%d\n");
      ua->send->ObjectKeyValue("catalog",
                               "%s=", client->catalog->resource_name_, "%s\n");
    }
  } else if ((pos = FindArgWithValue(ua, "storage")) >= 0) {
    StorageResource* storage;

    /*
     * Storage defaults
     */
    storage = ua->GetStoreResWithName(ua->argv[pos]);
    if (storage) {
      DeviceResource* device_resource;
      PoolMem devices;

      ua->send->ObjectKeyValue("storage", "%s=", storage->resource_name_,
                               "%s\n");
      ua->send->ObjectKeyValue("address", "%s=", storage->address, "%s\n");
      ua->send->ObjectKeyValue("port", "%s=", storage->SDport, "%d\n");
      ua->send->ObjectKeyValue("enabled", "%s=", storage->enabled, "%d\n");
      ua->send->ObjectKeyValue("media_type", "%s=", storage->media_type,
                               "%s\n");

      device_resource = (DeviceResource*)storage->device->first();
      if (device_resource) {
        devices.strcpy(device_resource->resource_name_);
        if (storage->device->size() > 1) {
          while ((device_resource = (DeviceResource*)storage->device->next())) {
            devices.strcat(",");
            devices.strcat(device_resource->resource_name_);
          }
        }
        ua->send->ObjectKeyValue("device", "%s=", devices.c_str(), "%s\n");
      }
    }
  } else if ((pos = FindArgWithValue(ua, "pool")) >= 0) {
    PoolResource* pool;

    /*
     * Pool defaults
     */
    pool = ua->GetPoolResWithName(ua->argv[pos]);
    if (pool) {
      ua->send->ObjectKeyValue("pool", "%s=", pool->resource_name_, "%s\n");
      ua->send->ObjectKeyValue("pool_type", "%s=", pool->pool_type, "%s\n");
      ua->send->ObjectKeyValue(
          "label_format", "%s=", (pool->label_format ? pool->label_format : ""),
          "%s\n");
      ua->send->ObjectKeyValue("use_volume_once", "%s=", pool->use_volume_once,
                               "%d\n");
      ua->send->ObjectKeyValue(
          "purge_oldest_volume=", "%s=", pool->purge_oldest_volume, "%d\n");
      ua->send->ObjectKeyValue("recycle_oldest_volume",
                               "%s=", pool->recycle_oldest_volume, "%d\n");
      ua->send->ObjectKeyValue("max_volumes", "%s=", pool->max_volumes, "%d\n");
      ua->send->ObjectKeyValue(
          "vol_retention", "%s=", edit_uint64(pool->VolRetention, ed1), "%s\n");
      ua->send->ObjectKeyValue("vol_use_duration",
                               "%s=", edit_uint64(pool->VolUseDuration, ed1),
                               "%s\n");
      ua->send->ObjectKeyValue("max_vol_jobs", "%s=", pool->MaxVolJobs, "%d\n");
      ua->send->ObjectKeyValue("max_vol_files", "%s=", pool->MaxVolFiles,
                               "%d\n");
      ua->send->ObjectKeyValue(
          "max_vol_bytes", "%s=", edit_uint64(pool->MaxVolBytes, ed1), "%s\n");
      ua->send->ObjectKeyValue("auto_prune", "%s=", pool->AutoPrune, "%d\n");
      ua->send->ObjectKeyValue("recycle", "%s=", pool->Recycle, "%d\n");
      ua->send->ObjectKeyValue("file_retention",
                               "%s=", edit_uint64(pool->FileRetention, ed1),
                               "%s\n");
      ua->send->ObjectKeyValue(
          "job_retention", "%s=", edit_uint64(pool->JobRetention, ed1), "%s\n");
    }
  } else {
    ua->SendMsg(".defaults command requires a parameter.\n");
    return false;
  }
  ua->send->ObjectEnd("defaults");

  return true;
}
} /* namespace directordaemon */
