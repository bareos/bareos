/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- User Agent Prompt and Selection code
 *
 *     Kern Sibbald, October MMI
 *
 */

#include "bacula.h"
#include "dird.h"

/* Imported variables */
extern struct s_jl joblevels[];


/*
 * Confirm a retention period
 */
int confirm_retention(UAContext *ua, utime_t *ret, const char *msg)
{
   char ed1[100];
   int val;

   int yes_in_arg = find_arg(ua, NT_("yes"));

   for ( ;; ) {
       ua->info_msg(_("The current %s retention period is: %s\n"),
          msg, edit_utime(*ret, ed1, sizeof(ed1)));
       if (yes_in_arg != -1) {
          return 1;
       }
       if (!get_cmd(ua, _("Continue? (yes/mod/no): "))) {
          return 0;
       }
       if (strcasecmp(ua->cmd, _("mod")) == 0) {
          if (!get_cmd(ua, _("Enter new retention period: "))) {
             return 0;
          }
          if (!duration_to_utime(ua->cmd, ret)) {
             ua->error_msg(_("Invalid period.\n"));
             continue;
          }
          continue;
       }
       if (is_yesno(ua->cmd, &val)) {
          return val;           /* is 1 for yes, 0 for no */
       }
    }
    return 1;
}

/*
 * Given a list of keywords, find the first one
 *  that is in the argument list.
 * Returns: -1 if not found
 *          index into list (base 0) on success
 */
int find_arg_keyword(UAContext *ua, const char **list)
{
   for (int i=1; i<ua->argc; i++) {
      for(int j=0; list[j]; j++) {
         if (strcasecmp(list[j], ua->argk[i]) == 0) {
            return j;
         }
      }
   }
   return -1;
}

/*
 * Given one keyword, find the first one that
 *   is in the argument list.
 * Returns: argk index (always gt 0)
 *          -1 if not found
 */
int find_arg(UAContext *ua, const char *keyword)
{
   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(keyword, ua->argk[i]) == 0) {
         return i;
      }
   }
   return -1;
}

/*
 * Given a single keyword, find it in the argument list, but
 *   it must have a value
 * Returns: -1 if not found or no value
 *           list index (base 0) on success
 */
int find_arg_with_value(UAContext *ua, const char *keyword)
{
   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(keyword, ua->argk[i]) == 0) {
         if (ua->argv[i]) {
            return i;
         } else {
            return -1;
         }
      }
   }
   return -1;
}

/*
 * Given a list of keywords, prompt the user
 * to choose one.
 *
 * Returns: -1 on failure
 *          index into list (base 0) on success
 */
int do_keyword_prompt(UAContext *ua, const char *msg, const char **list)
{
   int i;
   start_prompt(ua, _("You have the following choices:\n"));
   for (i=0; list[i]; i++) {
      add_prompt(ua, list[i]);
   }
   return do_prompt(ua, "", msg, NULL, 0);
}


/*
 * Select a Storage resource from prompt list
 */
STORE *select_storage_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   STORE *store;

   start_prompt(ua, _("The defined Storage resources are:\n"));
   LockRes();
   foreach_res(store, R_STORAGE) {
      if (acl_access_ok(ua, Storage_ACL, store->name())) {
         add_prompt(ua, store->name());
      }
   }
   UnlockRes();
   if (do_prompt(ua, _("Storage"),  _("Select Storage resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   store = (STORE *)GetResWithName(R_STORAGE, name);
   return store;
}

/*
 * Select a FileSet resource from prompt list
 */
FILESET *select_fileset_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   FILESET *fs;

   start_prompt(ua, _("The defined FileSet resources are:\n"));
   LockRes();
   foreach_res(fs, R_FILESET) {
      if (acl_access_ok(ua, FileSet_ACL, fs->name())) {
         add_prompt(ua, fs->name());
      }
   }
   UnlockRes();
   if (do_prompt(ua, _("FileSet"), _("Select FileSet resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   fs = (FILESET *)GetResWithName(R_FILESET, name);
   return fs;
}


/*
 * Get a catalog resource from prompt list
 */
CAT *get_catalog_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   CAT *catalog = NULL;
   int i;

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("catalog")) == 0 && ua->argv[i]) {
         if (acl_access_ok(ua, Catalog_ACL, ua->argv[i])) {
            catalog = (CAT *)GetResWithName(R_CATALOG, ua->argv[i]);
            break;
         }
      }
   }
   if (ua->gui && !catalog) {
      LockRes();
      catalog = (CAT *)GetNextRes(R_CATALOG, NULL);
      UnlockRes();
      if (!catalog) {
         ua->error_msg(_("Could not find a Catalog resource\n"));
         return NULL;
      } else if (!acl_access_ok(ua, Catalog_ACL, catalog->name())) {
         ua->error_msg(_("You must specify a \"use <catalog-name>\" command before continuing.\n"));
         return NULL;
      }
      return catalog;
   }
   if (!catalog) {
      start_prompt(ua, _("The defined Catalog resources are:\n"));
      LockRes();
      foreach_res(catalog, R_CATALOG) {
         if (acl_access_ok(ua, Catalog_ACL, catalog->name())) {
            add_prompt(ua, catalog->name());
         }
      }
      UnlockRes();
      if (do_prompt(ua, _("Catalog"),  _("Select Catalog resource"), name, sizeof(name)) < 0) {
         return NULL;
      }
      catalog = (CAT *)GetResWithName(R_CATALOG, name);
   }
   return catalog;
}


/*
 * Select a job to enable or disable   
 */
JOB *select_enable_disable_job_resource(UAContext *ua, bool enable)
{
   char name[MAX_NAME_LENGTH];
   JOB *job;

   LockRes();
   start_prompt(ua, _("The defined Job resources are:\n"));
   foreach_res(job, R_JOB) {
      if (!acl_access_ok(ua, Job_ACL, job->name())) {
         continue;
      }
      if (job->enabled == enable) {   /* Already enabled/disabled? */
         continue;                    /* yes, skip */
      }
      add_prompt(ua, job->name());
   }
   UnlockRes();
   if (do_prompt(ua, _("Job"), _("Select Job resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   job = (JOB *)GetResWithName(R_JOB, name);
   return job;
}

/*
 * Select a Job resource from prompt list
 */
JOB *select_job_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   JOB *job;

   start_prompt(ua, _("The defined Job resources are:\n"));
   LockRes();
   foreach_res(job, R_JOB) {
      if (acl_access_ok(ua, Job_ACL, job->name())) {
         add_prompt(ua, job->name());
      }
   }
   UnlockRes();
   if (do_prompt(ua, _("Job"), _("Select Job resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   job = (JOB *)GetResWithName(R_JOB, name);
   return job;
}

/* 
 * Select a Restore Job resource from argument or prompt
 */
JOB *get_restore_job(UAContext *ua)
{
   JOB *job;
   int i = find_arg_with_value(ua, "restorejob");
   if (i >= 0 && acl_access_ok(ua, Job_ACL, ua->argv[i])) {
      job = (JOB *)GetResWithName(R_JOB, ua->argv[i]);
      if (job && job->JobType == JT_RESTORE) {
         return job;
      }
      ua->error_msg(_("Error: Restore Job resource \"%s\" does not exist.\n"),
                    ua->argv[i]);
   }
   return select_restore_job_resource(ua);
}

/*
 * Select a Restore Job resource from prompt list
 */
JOB *select_restore_job_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   JOB *job;

   start_prompt(ua, _("The defined Restore Job resources are:\n"));
   LockRes();
   foreach_res(job, R_JOB) {
      if (job->JobType == JT_RESTORE && acl_access_ok(ua, Job_ACL, job->name())) {
         add_prompt(ua, job->name());
      }
   }
   UnlockRes();
   if (do_prompt(ua, _("Job"), _("Select Restore Job"), name, sizeof(name)) < 0) {
      return NULL;
   }
   job = (JOB *)GetResWithName(R_JOB, name);
   return job;
}



/*
 * Select a client resource from prompt list
 */
CLIENT *select_client_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   CLIENT *client;

   start_prompt(ua, _("The defined Client resources are:\n"));
   LockRes();
   foreach_res(client, R_CLIENT) {
      if (acl_access_ok(ua, Client_ACL, client->name())) {
         add_prompt(ua, client->name());
      }
   }
   UnlockRes();
   if (do_prompt(ua, _("Client"),  _("Select Client (File daemon) resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   client = (CLIENT *)GetResWithName(R_CLIENT, name);
   return client;
}

/*
 *  Get client resource, start by looking for
 *   client=<client-name>
 *  if we don't find the keyword, we prompt the user.
 */
CLIENT *get_client_resource(UAContext *ua)
{
   CLIENT *client = NULL;
   int i;

   for (i=1; i<ua->argc; i++) {
      if ((strcasecmp(ua->argk[i], NT_("client")) == 0 ||
           strcasecmp(ua->argk[i], NT_("fd")) == 0) && ua->argv[i]) {
         if (!acl_access_ok(ua, Client_ACL, ua->argv[i])) {
            break;
         }
         client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[i]);
         if (client) {
            return client;
         }
         ua->error_msg(_("Error: Client resource %s does not exist.\n"), ua->argv[i]);
         break;
      }
   }
   return select_client_resource(ua);
}

/* Scan what the user has entered looking for:
 *
 *  client=<client-name>
 *
 *  if error or not found, put up a list of client DBRs
 *  to choose from.
 *
 *   returns: 0 on error
 *            1 on success and fills in CLIENT_DBR
 */
bool get_client_dbr(UAContext *ua, CLIENT_DBR *cr)
{
   int i;

   if (cr->Name[0]) {                 /* If name already supplied */
      if (db_get_client_record(ua->jcr, ua->db, cr)) {
         return 1;
      }
      ua->error_msg(_("Could not find Client %s: ERR=%s"), cr->Name, db_strerror(ua->db));
   }
   for (i=1; i<ua->argc; i++) {
      if ((strcasecmp(ua->argk[i], NT_("client")) == 0 ||
           strcasecmp(ua->argk[i], NT_("fd")) == 0) && ua->argv[i]) {
         if (!acl_access_ok(ua, Client_ACL, ua->argv[i])) {
            break;
         }
         bstrncpy(cr->Name, ua->argv[i], sizeof(cr->Name));
         if (!db_get_client_record(ua->jcr, ua->db, cr)) {
            ua->error_msg(_("Could not find Client \"%s\": ERR=%s"), ua->argv[i],
                     db_strerror(ua->db));
            cr->ClientId = 0;
            break;
         }
         return 1;
      }
   }
   if (!select_client_dbr(ua, cr)) {  /* try once more by proposing a list */
      return 0;
   }
   return 1;
}

/*
 * Select a Client record from the catalog
 *  Returns 1 on success
 *          0 on failure
 */
bool select_client_dbr(UAContext *ua, CLIENT_DBR *cr)
{
   CLIENT_DBR ocr;
   char name[MAX_NAME_LENGTH];
   int num_clients, i;
   uint32_t *ids;


   cr->ClientId = 0;
   if (!db_get_client_ids(ua->jcr, ua->db, &num_clients, &ids)) {
      ua->error_msg(_("Error obtaining client ids. ERR=%s\n"), db_strerror(ua->db));
      return 0;
   }
   if (num_clients <= 0) {
      ua->error_msg(_("No clients defined. You must run a job before using this command.\n"));
      return 0;
   }

   start_prompt(ua, _("Defined Clients:\n"));
   for (i=0; i < num_clients; i++) {
      ocr.ClientId = ids[i];
      if (!db_get_client_record(ua->jcr, ua->db, &ocr) ||
          !acl_access_ok(ua, Client_ACL, ocr.Name)) {
         continue;
      }
      add_prompt(ua, ocr.Name);
   }
   free(ids);
   if (do_prompt(ua, _("Client"),  _("Select the Client"), name, sizeof(name)) < 0) {
      return 0;
   }
   memset(&ocr, 0, sizeof(ocr));
   bstrncpy(ocr.Name, name, sizeof(ocr.Name));

   if (!db_get_client_record(ua->jcr, ua->db, &ocr)) {
      ua->error_msg(_("Could not find Client \"%s\": ERR=%s"), name, db_strerror(ua->db));
      return 0;
   }
   memcpy(cr, &ocr, sizeof(ocr));
   return 1;
}

/* Scan what the user has entered looking for:
 *
 *  argk=<pool-name>
 *
 *  where argk can be : pool, recyclepool, scratchpool, nextpool etc..
 *
 *  if error or not found, put up a list of pool DBRs
 *  to choose from.
 *
 *   returns: false on error
 *            true  on success and fills in POOL_DBR
 */
bool get_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk)
{
   if (pr->Name[0]) {                 /* If name already supplied */
      if (db_get_pool_record(ua->jcr, ua->db, pr) &&
          acl_access_ok(ua, Pool_ACL, pr->Name)) {
         return true;
      }
      ua->error_msg(_("Could not find Pool \"%s\": ERR=%s"), pr->Name, db_strerror(ua->db));
   }
   if (!select_pool_dbr(ua, pr, argk)) {  /* try once more */
      return false;
   }
   return true;
}

/*
 * Select a Pool record from catalog
 * argk can be pool, recyclepool, scratchpool etc..
 */
bool select_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk)
{
   POOL_DBR opr;
   char name[MAX_NAME_LENGTH];
   int num_pools, i;
   uint32_t *ids;

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], argk) == 0 && ua->argv[i] &&
          acl_access_ok(ua, Pool_ACL, ua->argv[i])) {
         bstrncpy(pr->Name, ua->argv[i], sizeof(pr->Name));
         if (!db_get_pool_record(ua->jcr, ua->db, pr)) {
            ua->error_msg(_("Could not find Pool \"%s\": ERR=%s"), ua->argv[i],
                     db_strerror(ua->db));
            pr->PoolId = 0;
            break;
         }
         return true;
      }
   }

   pr->PoolId = 0;
   if (!db_get_pool_ids(ua->jcr, ua->db, &num_pools, &ids)) {
      ua->error_msg(_("Error obtaining pool ids. ERR=%s\n"), db_strerror(ua->db));
      return 0;
   }
   if (num_pools <= 0) {
      ua->error_msg(_("No pools defined. Use the \"create\" command to create one.\n"));
      return false;
   }

   start_prompt(ua, _("Defined Pools:\n"));
   if (bstrcmp(argk, NT_("recyclepool"))) {
      add_prompt(ua, _("*None*"));
   }
   for (i=0; i < num_pools; i++) {
      opr.PoolId = ids[i];
      if (!db_get_pool_record(ua->jcr, ua->db, &opr) ||
          !acl_access_ok(ua, Pool_ACL, opr.Name)) {
         continue;
      }
      add_prompt(ua, opr.Name);
   }
   free(ids);
   if (do_prompt(ua, _("Pool"),  _("Select the Pool"), name, sizeof(name)) < 0) {
      return false;
   }

   memset(&opr, 0, sizeof(opr));
   /* *None* is only returned when selecting a recyclepool, and in that case
    * the calling code is only interested in opr.Name, so then we can leave
    * pr as all zero.
    */
   if (!bstrcmp(name, _("*None*"))) {
     bstrncpy(opr.Name, name, sizeof(opr.Name));

     if (!db_get_pool_record(ua->jcr, ua->db, &opr)) {
        ua->error_msg(_("Could not find Pool \"%s\": ERR=%s"), name, db_strerror(ua->db));
        return false;
     }
   }

   memcpy(pr, &opr, sizeof(opr));
   return true;
}

/*
 * Select a Pool and a Media (Volume) record from the database
 */
int select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr)
{

   if (!select_media_dbr(ua, mr)) {
      return 0;
   }
   memset(pr, 0, sizeof(POOL_DBR));
   pr->PoolId = mr->PoolId;
   if (!db_get_pool_record(ua->jcr, ua->db, pr)) {
      ua->error_msg("%s", db_strerror(ua->db));
      return 0;
   }
   if (!acl_access_ok(ua, Pool_ACL, pr->Name)) {
      ua->error_msg(_("No access to Pool \"%s\"\n"), pr->Name);
      return 0;
   }
   return 1;
}

/* Select a Media (Volume) record from the database */
int select_media_dbr(UAContext *ua, MEDIA_DBR *mr)
{
   int i;
   int ret = 0;
   POOLMEM *err = get_pool_memory(PM_FNAME);
   *err=0;

   mr->clear();
   i = find_arg_with_value(ua, "volume");
   if (i >= 0) {
      if (is_name_valid(ua->argv[i], &err)) {
         bstrncpy(mr->VolumeName, ua->argv[i], sizeof(mr->VolumeName));
      } else {
         goto bail_out;
      }
   }
   if (mr->VolumeName[0] == 0) {
      POOL_DBR pr;
      memset(&pr, 0, sizeof(pr));
      /* Get the pool from pool=<pool-name> */
      if (!get_pool_dbr(ua, &pr)) {
         goto bail_out;
      }
      mr->PoolId = pr.PoolId;
      db_list_media_records(ua->jcr, ua->db, mr, prtit, ua, HORZ_LIST);
      if (!get_cmd(ua, _("Enter *MediaId or Volume name: "))) {
         goto bail_out;
      }
      if (ua->cmd[0] == '*' && is_a_number(ua->cmd+1)) {
         mr->MediaId = str_to_int64(ua->cmd+1);
      } else if (is_name_valid(ua->cmd, &err)) {
         bstrncpy(mr->VolumeName, ua->cmd, sizeof(mr->VolumeName));
      } else {
         goto bail_out;
      }
   }

   if (!db_get_media_record(ua->jcr, ua->db, mr)) {
      pm_strcpy(err, db_strerror(ua->db));
      goto bail_out;
   }
   ret = 1;

bail_out:
   if (!ret && *err) {
      ua->error_msg("%s", err);
   }
   free_pool_memory(err);
   return ret;
}


/*
 * Select a pool resource from prompt list
 */
POOL *select_pool_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];
   POOL *pool;

   start_prompt(ua, _("The defined Pool resources are:\n"));
   LockRes();
   foreach_res(pool, R_POOL) {
      if (acl_access_ok(ua, Pool_ACL, pool->name())) {
         add_prompt(ua, pool->name());
      }
   }
   UnlockRes();
   if (do_prompt(ua, _("Pool"), _("Select Pool resource"), name, sizeof(name)) < 0) {
      return NULL;
   }
   pool = (POOL *)GetResWithName(R_POOL, name);
   return pool;
}


/*
 *  If you are thinking about using it, you
 *  probably want to use select_pool_dbr()
 *  or get_pool_dbr() above.
 */
POOL *get_pool_resource(UAContext *ua)
{
   POOL *pool = NULL;
   int i;

   i = find_arg_with_value(ua, "pool");
   if (i >= 0 && acl_access_ok(ua, Pool_ACL, ua->argv[i])) {
      pool = (POOL *)GetResWithName(R_POOL, ua->argv[i]);
      if (pool) {
         return pool;
      }
      ua->error_msg(_("Error: Pool resource \"%s\" does not exist.\n"), ua->argv[i]);
   }
   return select_pool_resource(ua);
}

/*
 * List all jobs and ask user to select one
 */
int select_job_dbr(UAContext *ua, JOB_DBR *jr)
{
   db_list_job_records(ua->jcr, ua->db, jr, prtit, ua, HORZ_LIST);
   if (!get_pint(ua, _("Enter the JobId to select: "))) {
      return 0;
   }
   jr->JobId = ua->int64_val;
   if (!db_get_job_record(ua->jcr, ua->db, jr)) {
      ua->error_msg("%s", db_strerror(ua->db));
      return 0;
   }
   return jr->JobId;

}


/* Scan what the user has entered looking for:
 *
 *  jobid=nn
 *
 *  if error or not found, put up a list of Jobs
 *  to choose from.
 *
 *   returns: 0 on error
 *            JobId on success and fills in JOB_DBR
 */
int get_job_dbr(UAContext *ua, JOB_DBR *jr)
{
   int i;

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("ujobid")) == 0 && ua->argv[i]) {
         jr->JobId = 0;
         bstrncpy(jr->Job, ua->argv[i], sizeof(jr->Job));
      } else if (strcasecmp(ua->argk[i], NT_("jobid")) == 0 && ua->argv[i]) {
         jr->JobId = str_to_int64(ua->argv[i]);
         jr->Job[0] = 0;
      } else {
         continue;
      }
      if (!db_get_job_record(ua->jcr, ua->db, jr)) {
         ua->error_msg(_("Could not find Job \"%s\": ERR=%s"), ua->argv[i],
                  db_strerror(ua->db));
         jr->JobId = 0;
         break;
      }
      return jr->JobId;
   }

   jr->JobId = 0;
   jr->Job[0] = 0;

   for (i=1; i<ua->argc; i++) {
      if ((strcasecmp(ua->argk[i], NT_("jobname")) == 0 ||
           strcasecmp(ua->argk[i], NT_("job")) == 0) && ua->argv[i]) {
         jr->JobId = 0;
         bstrncpy(jr->Name, ua->argv[i], sizeof(jr->Name));
         break;
      }
   }
   if (!select_job_dbr(ua, jr)) {  /* try once more */
      return 0;
   }
   return jr->JobId;
}

/*
 * Implement unique set of prompts
 */
void start_prompt(UAContext *ua, const char *msg)
{
  if (ua->max_prompts == 0) {
     ua->max_prompts = 10;
     ua->prompt = (char **)bmalloc(sizeof(char *) * ua->max_prompts);
  }
  ua->num_prompts = 1;
  ua->prompt[0] = bstrdup(msg);
}

/*
 * Add to prompts -- keeping them unique
 */
void add_prompt(UAContext *ua, const char *prompt)
{
   int i;
   if (ua->num_prompts == ua->max_prompts) {
      ua->max_prompts *= 2;
      ua->prompt = (char **)brealloc(ua->prompt, sizeof(char *) *
         ua->max_prompts);
    }
    for (i=1; i < ua->num_prompts; i++) {
       if (strcmp(ua->prompt[i], prompt) == 0) {
          return;
       }
    }
    ua->prompt[ua->num_prompts++] = bstrdup(prompt);
}

/*
 * Display prompts and get user's choice
 *
 *  Returns: -1 on error
 *            index base 0 on success, and choice
 *               is copied to prompt if not NULL
 *             prompt is set to the chosen prompt item string
 */
int do_prompt(UAContext *ua, const char *automsg, const char *msg, 
              char *prompt, int max_prompt)
{
   int i, item;
   char pmsg[MAXSTRING];
   BSOCK *user = ua->UA_sock;

   if (prompt) {
      *prompt = 0;
   }
   if (ua->num_prompts == 2) {
      item = 1;
      if (prompt) {
         bstrncpy(prompt, ua->prompt[1], max_prompt);
      }
      ua->send_msg(_("Automatically selected %s: %s\n"), automsg, ua->prompt[1]);
      goto done;
   }
   /* If running non-interactive, bail out */
   if (ua->batch) {
      /* First print the choices he wanted to make */
      ua->send_msg(ua->prompt[0]);
      for (i=1; i < ua->num_prompts; i++) {
         ua->send_msg("%6d: %s\n", i, ua->prompt[i]);
      }
      /* Now print error message */
      ua->send_msg(_("Your request has multiple choices for \"%s\". Selection is not possible in batch mode.\n"), automsg);
      item = -1;
      goto done;
   }
   if (ua->api) user->signal(BNET_START_SELECT);
   ua->send_msg(ua->prompt[0]);
   for (i=1; i < ua->num_prompts; i++) {
      if (ua->api) {
         ua->send_msg("%s", ua->prompt[i]);
      } else {
         ua->send_msg("%6d: %s\n", i, ua->prompt[i]);
      }
   }
   if (ua->api) user->signal(BNET_END_SELECT);

   for ( ;; ) {
      /* First item is the prompt string, not the items */
      if (ua->num_prompts == 1) {
         ua->error_msg(_("Selection list for \"%s\" is empty!\n"), automsg);
         item = -1;                    /* list is empty ! */
         break;
      }
      if (ua->num_prompts == 2) {
         item = 1;
         ua->send_msg(_("Automatically selected: %s\n"), ua->prompt[1]);
         if (prompt) {
            bstrncpy(prompt, ua->prompt[1], max_prompt);
         }
         break;
      } else {
         sprintf(pmsg, "%s (1-%d): ", msg, ua->num_prompts-1);
      }
      /* Either a . or an @ will get you out of the loop */
      if (ua->api) user->signal(BNET_SELECT_INPUT);
      if (!get_pint(ua, pmsg)) {
         item = -1;                   /* error */
         ua->info_msg(_("Selection aborted, nothing done.\n"));
         break;
      }
      item = ua->pint32_val;
      if (item < 1 || item >= ua->num_prompts) {
         ua->warning_msg(_("Please enter a number between 1 and %d\n"), ua->num_prompts-1);
         continue;
      }
      if (prompt) {
         bstrncpy(prompt, ua->prompt[item], max_prompt);
      }
      break;
   }

done:
   for (i=0; i < ua->num_prompts; i++) {
      free(ua->prompt[i]);
   }
   ua->num_prompts = 0;
   return item>0 ? item-1 : item;
}


/*
 * We scan what the user has entered looking for
 *    storage=<storage-resource>
 *    job=<job_name>
 *    jobid=<jobid>
 *    ?              (prompt him with storage list)
 *    <some-error>   (prompt him with storage list)
 *
 * If use_default is set, we assume that any keyword without a value
 *   is the name of the Storage resource wanted.
 */
STORE *get_storage_resource(UAContext *ua, bool use_default)
{
   char *store_name = NULL;
   STORE *store = NULL;
   int jobid;
   JCR *jcr;
   int i;
   char ed1[50];

   for (i=1; i<ua->argc; i++) {
      if (use_default && !ua->argv[i]) {
         /* Ignore slots, scan and barcode(s) keywords */
         if (strcasecmp("scan", ua->argk[i]) == 0 ||
             strcasecmp("barcode", ua->argk[i]) == 0 ||
             strcasecmp("barcodes", ua->argk[i]) == 0 ||
             strcasecmp("slots", ua->argk[i]) == 0) {
            continue;
         }
         /* Default argument is storage */
         if (store_name) {
            ua->error_msg(_("Storage name given twice.\n"));
            return NULL;
         }
         store_name = ua->argk[i];
         if (*store_name == '?') {
            *store_name = 0;
            break;
         }
      } else {
         if (strcasecmp(ua->argk[i], NT_("storage")) == 0 ||
             strcasecmp(ua->argk[i], NT_("sd")) == 0) {
            store_name = ua->argv[i];
            break;

         } else if (strcasecmp(ua->argk[i], NT_("jobid")) == 0) {
            jobid = str_to_int64(ua->argv[i]);
            if (jobid <= 0) {
               ua->error_msg(_("Expecting jobid=nn command, got: %s\n"), ua->argk[i]);
               return NULL;
            }
            if (!(jcr=get_jcr_by_id(jobid))) {
               ua->error_msg(_("JobId %s is not running.\n"), edit_int64(jobid, ed1));
               return NULL;
            }
            store = jcr->wstore;
            free_jcr(jcr);
            break;

         } else if (strcasecmp(ua->argk[i], NT_("job")) == 0 ||
                    strcasecmp(ua->argk[i], NT_("jobname")) == 0) {
            if (!ua->argv[i]) {
               ua->error_msg(_("Expecting job=xxx, got: %s.\n"), ua->argk[i]);
               return NULL;
            }
            if (!(jcr=get_jcr_by_partial_name(ua->argv[i]))) {
               ua->error_msg(_("Job \"%s\" is not running.\n"), ua->argv[i]);
               return NULL;
            }
            store = jcr->wstore;
            free_jcr(jcr);
            break;
         } else if (strcasecmp(ua->argk[i], NT_("ujobid")) == 0) {
            if (!ua->argv[i]) {
               ua->error_msg(_("Expecting ujobid=xxx, got: %s.\n"), ua->argk[i]);
               return NULL;
            }
            if (!(jcr=get_jcr_by_full_name(ua->argv[i]))) {
               ua->error_msg(_("Job \"%s\" is not running.\n"), ua->argv[i]);
               return NULL;
            }
            store = jcr->wstore;
            free_jcr(jcr);
            break;
        }
      }
   }
   if (store && !acl_access_ok(ua, Storage_ACL, store->name())) {
      store = NULL;
   }

   if (!store && store_name && store_name[0] != 0) {
      store = (STORE *)GetResWithName(R_STORAGE, store_name);
      if (!store) {
         ua->error_msg(_("Storage resource \"%s\": not found\n"), store_name);
      }
   }
   if (store && !acl_access_ok(ua, Storage_ACL, store->name())) {
      store = NULL;
   }
   /* No keywords found, so present a selection list */
   if (!store) {
      store = select_storage_resource(ua);
   }
   return store;
}

/* Get drive that we are working with for this storage */
int get_storage_drive(UAContext *ua, STORE *store)
{
   int i, drive = -1;
   /* Get drive for autochanger if possible */
   i = find_arg_with_value(ua, "drive");
   if (i >=0) {
      drive = atoi(ua->argv[i]);
   } else if (store && store->autochanger) {
      /* If our structure is not set ask SD for # drives */
      if (store->drives == 0) {
         store->drives = get_num_drives_from_SD(ua);
      }
      /* If only one drive, default = 0 */
      if (store->drives == 1) {
         drive = 0;
      } else {
         /* Ask user to enter drive number */
         ua->cmd[0] = 0;
         if (!get_cmd(ua, _("Enter autochanger drive[0]: "))) {
            drive = -1;  /* None */
         } else {
            drive = atoi(ua->cmd);
         }
     }
   }
   return drive;
}

/* Get slot that we are working with for this storage */
int get_storage_slot(UAContext *ua, STORE *store)
{
   int i, slot = -1;
   /* Get slot for autochanger if possible */
   i = find_arg_with_value(ua, "slot");
   if (i >=0) {
      slot = atoi(ua->argv[i]);
   } else if (store && store->autochanger) {
      /* Ask user to enter slot number */
      ua->cmd[0] = 0;
      if (!get_cmd(ua, _("Enter autochanger slot: "))) {
         slot = -1;  /* None */
      } else {
         slot = atoi(ua->cmd);
      }
   }
   return slot;
}



/*
 * Scan looking for mediatype=
 *
 *  if not found or error, put up selection list
 *
 *  Returns: 0 on error
 *           1 on success, MediaType is set
 */
int get_media_type(UAContext *ua, char *MediaType, int max_media)
{
   STORE *store;
   int i;

   i = find_arg_with_value(ua, "mediatype");
   if (i >= 0) {
      bstrncpy(MediaType, ua->argv[i], max_media);
      return 1;
   }

   start_prompt(ua, _("Media Types defined in conf file:\n"));
   LockRes();
   foreach_res(store, R_STORAGE) {
      add_prompt(ua, store->media_type);
   }
   UnlockRes();
   return (do_prompt(ua, _("Media Type"), _("Select the Media Type"), MediaType, max_media) < 0) ? 0 : 1;
}

bool get_level_from_name(JCR *jcr, const char *level_name)
{
   /* Look up level name and pull code */
   bool found = false;
   for (int i=0; joblevels[i].level_name; i++) {
      if (strcasecmp(level_name, joblevels[i].level_name) == 0) {
         jcr->setJobLevel(joblevels[i].level);
         found = true;
         break;
      }
   }
   return found;
}

/* Get a running job
 * "reason" is used in user messages
 * can be: cancel, limit, ...
 *  Returns: NULL on error
 *           JCR on success (should be free_jcr() after)
 */
JCR *select_running_job(UAContext *ua, const char *reason)
{
   int i;
   int njobs = 0;
   JCR *jcr = NULL;
   char JobName[MAX_NAME_LENGTH];
   char temp[256];

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("jobid")) == 0) {
         uint32_t JobId;
         JobId = str_to_int64(ua->argv[i]);
         if (!JobId) {
            break;
         }
         if (!(jcr=get_jcr_by_id(JobId))) {
            ua->error_msg(_("JobId %s is not running. Use Job name to %s inactive jobs.\n"),  ua->argv[i], _(reason));
            return NULL;
         }
         break;
      } else if (strcasecmp(ua->argk[i], NT_("job")) == 0) {
         if (!ua->argv[i]) {
            break;
         }
         if (!(jcr=get_jcr_by_partial_name(ua->argv[i]))) {
            ua->warning_msg(_("Warning Job %s is not running. Continuing anyway ...\n"), ua->argv[i]);
            jcr = new_jcr(sizeof(JCR), dird_free_jcr);
            bstrncpy(jcr->Job, ua->argv[i], sizeof(jcr->Job));
         }
         break;
      } else if (strcasecmp(ua->argk[i], NT_("ujobid")) == 0) {
         if (!ua->argv[i]) {
            break;
         }
         if (!(jcr=get_jcr_by_full_name(ua->argv[i]))) {
            ua->warning_msg(_("Warning Job %s is not running. Continuing anyway ...\n"), ua->argv[i]);
            jcr = new_jcr(sizeof(JCR), dird_free_jcr);
            bstrncpy(jcr->Job, ua->argv[i], sizeof(jcr->Job));
         }
         break;
      }

   }
   if (jcr) {
      if (jcr->job && !acl_access_ok(ua, Job_ACL, jcr->job->name())) {
         ua->error_msg(_("Unauthorized command from this console.\n"));
         return NULL;
      }
   } else {
     /*
      * If we still do not have a jcr,
      *   throw up a list and ask the user to select one.
      */
      char buf[1000];
      int tjobs = 0;                  /* total # number jobs */
      /* Count Jobs running */
      foreach_jcr(jcr) {
         if (jcr->JobId == 0) {      /* this is us */
            continue;
         }
         tjobs++;                    /* count of all jobs */
         if (!acl_access_ok(ua, Job_ACL, jcr->job->name())) {
            continue;               /* skip not authorized */
         }
         njobs++;                   /* count of authorized jobs */
      }
      endeach_jcr(jcr);

      if (njobs == 0) {            /* no authorized */
         if (tjobs == 0) {
            ua->send_msg(_("No Jobs running.\n"));
         } else {
            ua->send_msg(_("None of your jobs are running.\n"));
         }
         return NULL;
      }

      start_prompt(ua, _("Select Job:\n"));
      foreach_jcr(jcr) {
         char ed1[50];
         if (jcr->JobId == 0) {      /* this is us */
            continue;
         }
         if (!acl_access_ok(ua, Job_ACL, jcr->job->name())) {
            continue;               /* skip not authorized */
         }
         bsnprintf(buf, sizeof(buf), _("JobId=%s Job=%s"), edit_int64(jcr->JobId, ed1), jcr->Job);
         add_prompt(ua, buf);
      }
      endeach_jcr(jcr);
      bsnprintf(temp, sizeof(temp), _("Choose Job to %s"), _(reason));
      if (do_prompt(ua, _("Job"),  temp, buf, sizeof(buf)) < 0) {
         return NULL;
      }
      if (!strcmp(reason, "cancel")) {
         if (ua->api && njobs == 1) {
            char nbuf[1000];
            bsnprintf(nbuf, sizeof(nbuf), _("Cancel: %s\n\n%s"), buf,  
                      _("Confirm cancel?"));
            if (!get_yesno(ua, nbuf) || ua->pint32_val == 0) {
               return NULL;
            }
         } else {
            if (njobs == 1) {
               if (!get_yesno(ua, _("Confirm cancel (yes/no): ")) || ua->pint32_val == 0) {
                  return NULL;
               }
            }
         }
      }
      sscanf(buf, "JobId=%d Job=%127s", &njobs, JobName);
      jcr = get_jcr_by_full_name(JobName);
      if (!jcr) {
         ua->warning_msg(_("Job \"%s\" not found.\n"), JobName);
         return NULL;
      }
   }
   return jcr;
}
