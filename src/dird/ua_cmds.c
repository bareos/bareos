/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 *
 *     Kern Sibbald, September MM
 *
 */
 
#include "bacula.h"
#include "dird.h"

#ifdef HAVE_PYTHON

#undef _POSIX_C_SOURCE
#include <Python.h>

#include "lib/pythonlib.h"

/* Imported Functions */
extern PyObject *job_getattr(PyObject *self, char *attrname);
extern int job_setattr(PyObject *self, char *attrname, PyObject *value);

#endif /* HAVE_PYTHON */

/* Imported subroutines */

/* Imported variables */
extern jobq_t job_queue;              /* job queue */


/* Imported functions */
extern int autodisplay_cmd(UAContext *ua, const char *cmd);
extern int gui_cmd(UAContext *ua, const char *cmd);
extern int label_cmd(UAContext *ua, const char *cmd);
extern int list_cmd(UAContext *ua, const char *cmd);
extern int llist_cmd(UAContext *ua, const char *cmd);
extern int messagescmd(UAContext *ua, const char *cmd);
extern int prunecmd(UAContext *ua, const char *cmd);
extern int purgecmd(UAContext *ua, const char *cmd);
extern int querycmd(UAContext *ua, const char *cmd);
extern int relabel_cmd(UAContext *ua, const char *cmd);
extern int restore_cmd(UAContext *ua, const char *cmd);
extern int retentioncmd(UAContext *ua, const char *cmd);
extern int show_cmd(UAContext *ua, const char *cmd);
extern int sqlquerycmd(UAContext *ua, const char *cmd);
extern int status_cmd(UAContext *ua, const char *cmd);
extern int update_cmd(UAContext *ua, const char *cmd);

/* Forward referenced functions */
static int add_cmd(UAContext *ua, const char *cmd);
static int automount_cmd(UAContext *ua, const char *cmd);
static int cancel_cmd(UAContext *ua, const char *cmd);
static int create_cmd(UAContext *ua, const char *cmd);
static int delete_cmd(UAContext *ua, const char *cmd);
static int disable_cmd(UAContext *ua, const char *cmd);
static int enable_cmd(UAContext *ua, const char *cmd);
static int estimate_cmd(UAContext *ua, const char *cmd);
static int help_cmd(UAContext *ua, const char *cmd);
static int memory_cmd(UAContext *ua, const char *cmd);
static int mount_cmd(UAContext *ua, const char *cmd);
static int python_cmd(UAContext *ua, const char *cmd);
static int release_cmd(UAContext *ua, const char *cmd);
static int reload_cmd(UAContext *ua, const char *cmd);
static int setdebug_cmd(UAContext *ua, const char *cmd);
static int setip_cmd(UAContext *ua, const char *cmd);
static int time_cmd(UAContext *ua, const char *cmd);
static int trace_cmd(UAContext *ua, const char *cmd);
static int unmount_cmd(UAContext *ua, const char *cmd);
static int use_cmd(UAContext *ua, const char *cmd);
static int var_cmd(UAContext *ua, const char *cmd);
static int version_cmd(UAContext *ua, const char *cmd);
static int wait_cmd(UAContext *ua, const char *cmd);

static void do_job_delete(UAContext *ua, JobId_t JobId);
static bool delete_job_id_range(UAContext *ua, char *tok);
static int delete_volume(UAContext *ua);
static int delete_pool(UAContext *ua);
static void delete_job(UAContext *ua);

int qhelp_cmd(UAContext *ua, const char *cmd);
int quit_cmd(UAContext *ua, const char *cmd);

/* not all in alphabetical order.  New commands are added after existing commands with similar letters
   to prevent breakage of existing user scripts.  */
struct cmdstruct { 
   const char *key;                             /* command */
   int (*func)(UAContext *ua, const char *cmd); /* handler */
   const char *help;            /* main purpose */
   const char *usage;           /* all arguments to build usage */
   const bool use_in_rs;        /* Can use it in Console RunScript */
};
static struct cmdstruct commands[] = {                                      /* Can use it in Console RunScript*/
 { NT_("add"),        add_cmd,     _("Add media to a pool"),   NT_("pool=<pool-name> storage=<storage> jobid=<JobId>"),  false},
 { NT_("autodisplay"), autodisplay_cmd,_("Autodisplay console messages"), NT_("on | off"),    false},
 { NT_("automount"),   automount_cmd,  _("Automount after label"),        NT_("on | off"),    false},
 { NT_("cancel"),     cancel_cmd,    _("Cancel a job"), NT_("jobid=<number> job=<job-name> ujobid=<unique-jobid>"), false},
 { NT_("create"),     create_cmd,    _("Create DB Pool from resource"), NT_("pool=<pool-name>"),                    false},
 { NT_("delete"),     delete_cmd,    _("Delete volume, pool or job"), NT_("volume=<vol-name> pool=<pool-name> jobid=<id>"), true},
 { NT_("disable"),    disable_cmd,   _("Disable a job"), NT_("job=<name>"),        true},
 { NT_("enable"),     enable_cmd,    _("Enable a job"), NT_("job=<name>"),          true},
 { NT_("estimate"),   estimate_cmd,  _("Performs FileSet estimate, listing gives full listing"), 
   NT_("fileset=<fs> client=<cli> level=<level> accurate=<yes/no> job=<job> listing"), true},

 { NT_("exit"),       quit_cmd,      _("Terminate Bconsole session"), NT_(""),         false},
 { NT_("gui"),        gui_cmd,       _("Non-interactive gui mode"),   NT_("on | off"), false},
 { NT_("help"),       help_cmd,      _("Print help on specific command"),  
   NT_("add autodisplay automount cancel create delete disable\n\tenable estimate exit gui label list llist"
       "\n\tmessages memory mount prune purge python quit query\n\trestore relabel release reload run status"
       "\n\tsetdebug setip show sqlquery time trace unmount\n\tumount update use var version wait"),         false},

 { NT_("label"),      label_cmd,     _("Label a tape"), NT_("storage=<storage> volume=<vol> pool=<pool> slot=<slot> barcodes"), false},
 { NT_("list"),       list_cmd,      _("List objects from catalog"), 
   NT_("pools | jobs | jobtotals | volume | media <pool=pool-name> | files jobid=<nn> | copies jobid=<nn>"), true},

 { NT_("llist"),      llist_cmd,     _("Full or long list like list command"),
   NT_("pools | jobs | jobtotals | media <pool=pool-name> | files jobid=<nn> | copies jobid=<nn>"), true},

 { NT_("messages"),   messagescmd,   _("Display pending messages"),   NT_(""),    false},
 { NT_("memory"),     memory_cmd,    _("Print current memory usage"), NT_(""),    true},
 { NT_("mount"),      mount_cmd,     _("Mount storage"), 
   NT_("storage=<storage-name> slot=<num> drive=<num> [ jobid=<id> | job=<job-name> ]"), false},

 { NT_("prune"),      prunecmd,      _("Prune expired records from catalog"), 
   NT_("files | jobs | pool=<pool> | client=<client-name> | volume=<volume-name> "), true},

 { NT_("purge"),      purgecmd,      _("Purge records from catalog"), NT_("files jobs volume=<vol> [action=<action> devicetype=<type> pool=<pool> allpools storage=<st> drive=<num>]"),  true},
 { NT_("python"),     python_cmd,    _("Python control commands"),    NT_(""),              false},
 { NT_("quit"),       quit_cmd,      _("Terminate Bconsole session"), NT_(""),              false},
 { NT_("query"),      querycmd,      _("Query catalog"),              NT_(""),              false},
 { NT_("restore"),    restore_cmd,   _("Restore files"), 
   NT_("where=</path> client=<client> storage=<storage> bootstrap=<file> "
       "restorejob=<job>"
       "\n\tcomment=<text> jobid=<jobid> done select all"), false},

 { NT_("relabel"),    relabel_cmd,   _("Relabel a tape"), 
   NT_("storage=<storage-name> oldvolume=<old-volume-name>\n\tvolume=<newvolume-name> pool=<pool>"), false},

 { NT_("release"),    release_cmd,   _("Release storage"),  NT_("storage=<storage-name>"),      false},
 { NT_("reload"),     reload_cmd,    _("Reload conf file"), NT_(""),                  true},
 { NT_("run"),        run_cmd,       _("Run a job"), 
   NT_("job=<job-name> client=<client-name>\n\tfileset=<FileSet-name> level=<level-keyword>\n\tstorage=<storage-name>"
       "where=<directory-prefix>\n\twhen=<universal-time-specification>\n\tcomment=<text> yes"), false}, 

 { NT_("status"),     status_cmd,    _("Report status"), 
   NT_("all | dir=<dir-name> | director | client=<client-name> | storage=<storage-name> slots | days=nnn"), true},

 { NT_("setdebug"),   setdebug_cmd,  _("Sets debug level"), 
   NT_("level=<nn> trace=0/1 client=<client-name> | dir | storage=<storage-name> | all"), true},

 { NT_("setip"),      setip_cmd,     _("Sets new client address -- if authorized"), NT_(""),   false},
 { NT_("show"),       show_cmd,      _("Show resource records"), 
   NT_("job=<xxx> |  pool=<yyy> | fileset=<aaa> schedule=<sss> | client=<zzz> | disabled | all"), true},

 { NT_("sqlquery"),   sqlquerycmd,   _("Use SQL to query catalog"), NT_(""),          false},
 { NT_("time"),       time_cmd,      _("Print current time"),       NT_(""),          true},
 { NT_("trace"),      trace_cmd,     _("Turn on/off trace to file"), NT_("on | off"), true},
 { NT_("unmount"),    unmount_cmd,   _("Unmount storage"), 
   NT_("storage=<storage-name> [ drive=<num> ] | jobid=<id> | job=<job-name>"), false},

 { NT_("umount"),     unmount_cmd,   _("Umount - for old-time Unix guys, see unmount"), 
   NT_("storage=<storage-name> [ drive=<num> ] | jobid=<id> | job=<job-name>"), false},

 { NT_("update"),     update_cmd,    _("Update volume, pool or stats"), 
   NT_("stats\n\tpool=<poolname>\n\tslots storage=<storage> scan"
       "\n\tvolume=<volname> volstatus=<status> volretention=<time-def>"
       "\n\t pool=<pool> recycle=<yes/no> slot=<number>\n\t inchanger=<yes/no>"
       "\n\t maxvolbytes=<size> maxvolfiles=<nb> maxvoljobs=<nb>"
       "\n\t enabled=<yes/no> recyclepool=<pool> actiononpurge=<action>"),true},
 { NT_("use"),        use_cmd,       _("Use catalog xxx"), NT_("catalog=<catalog>"),     false},
 { NT_("var"),        var_cmd,       _("Does variable expansion"), NT_(""),  false},
 { NT_("version"),    version_cmd,   _("Print Director version"),  NT_(""),  true},
 { NT_("wait"),       wait_cmd,      _("Wait until no jobs are running"), 
   NT_("jobname=<name> | jobid=<nnn> | ujobid=<complete_name>"), false}
};

#define comsize ((int)(sizeof(commands)/sizeof(struct cmdstruct)))

/*
 * Execute a command from the UA
 */
bool do_a_command(UAContext *ua)
{
   int i;
   int len;
   bool ok = false;
   bool found = false;
   BSOCK *user = ua->UA_sock;


   Dmsg1(900, "Command: %s\n", ua->argk[0]);
   if (ua->argc == 0) {
      return false;
   }

   while (ua->jcr->wstorage->size()) {
      ua->jcr->wstorage->remove(0);
   }

   len = strlen(ua->argk[0]);
   for (i=0; i<comsize; i++) {     /* search for command */
      if (strncasecmp(ua->argk[0],  commands[i].key, len) == 0) {
         /* Check if command permitted, but "quit" is always OK */
         if (strcmp(ua->argk[0], NT_("quit")) != 0 &&
             !acl_access_ok(ua, Command_ACL, ua->argk[0], len)) {
            break;
         }
         /* Check if this command is authorized in RunScript */
         if (ua->runscript && !commands[i].use_in_rs) {
            ua->error_msg(_("Can't use %s command in a runscript"), ua->argk[0]);
            break;
         }
         if (ua->api) user->signal(BNET_CMD_BEGIN);
         ok = (*commands[i].func)(ua, ua->cmd);   /* go execute command */
         if (ua->api) user->signal(ok?BNET_CMD_OK:BNET_CMD_FAILED);
         found = true;
         break;
      }
   }
   if (!found) {
      ua->error_msg(_("%s: is an invalid command.\n"), ua->argk[0]);
      ok = false;
   }
   return ok;
}

/*
 * This is a common routine used to stuff the Pool DB record defaults
 *   into the Media DB record just before creating a media (Volume)
 *   record.
 */
void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr)
{
   mr->PoolId = pr->PoolId;
   bstrncpy(mr->VolStatus, NT_("Append"), sizeof(mr->VolStatus));
   mr->Recycle = pr->Recycle;
   mr->VolRetention = pr->VolRetention;
   mr->VolUseDuration = pr->VolUseDuration;
   mr->ActionOnPurge = pr->ActionOnPurge;
   mr->RecyclePoolId = pr->RecyclePoolId;
   mr->MaxVolJobs = pr->MaxVolJobs;
   mr->MaxVolFiles = pr->MaxVolFiles;
   mr->MaxVolBytes = pr->MaxVolBytes;
   mr->LabelType = pr->LabelType;
   mr->Enabled = 1;
}


/*
 *  Add Volumes to an existing Pool
 */
static int add_cmd(UAContext *ua, const char *cmd)
{
   POOL_DBR pr;
   MEDIA_DBR mr;
   int num, i, max, startnum;
   char name[MAX_NAME_LENGTH];
   STORE *store;
   int Slot = 0, InChanger = 0;

   ua->send_msg(_(
"You probably don't want to be using this command since it\n"
"creates database records without labeling the Volumes.\n"
"You probably want to use the \"label\" command.\n\n"));

   if (!open_client_db(ua)) {
      return 1;
   }

   memset(&pr, 0, sizeof(pr));

   if (!get_pool_dbr(ua, &pr)) {
      return 1;
   }

   Dmsg4(120, "id=%d Num=%d Max=%d type=%s\n", pr.PoolId, pr.NumVols,
      pr.MaxVols, pr.PoolType);

   while (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
      ua->warning_msg(_("Pool already has maximum volumes=%d\n"), pr.MaxVols);
      if (!get_pint(ua, _("Enter new maximum (zero for unlimited): "))) {
         return 1;
      }
      pr.MaxVols = ua->pint32_val;
   }

   /* Get media type */
   if ((store = get_storage_resource(ua, false/*no default*/)) != NULL) {
      bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));
   } else if (!get_media_type(ua, mr.MediaType, sizeof(mr.MediaType))) {
      return 1;
   }

   if (pr.MaxVols == 0) {
      max = 1000;
   } else {
      max = pr.MaxVols - pr.NumVols;
   }
   for (;;) {
      char buf[100];
      bsnprintf(buf, sizeof(buf), _("Enter number of Volumes to create. 0=>fixed name. Max=%d: "), max);
      if (!get_pint(ua, buf)) {
         return 1;
      }
      num = ua->pint32_val;
      if (num < 0 || num > max) {
         ua->warning_msg(_("The number must be between 0 and %d\n"), max);
         continue;
      }
      break;
   }

   for (;;) {
      if (num == 0) {
         if (!get_cmd(ua, _("Enter Volume name: "))) {
            return 1;
         }
      } else {
         if (!get_cmd(ua, _("Enter base volume name: "))) {
            return 1;
         }
      }
      /* Don't allow | in Volume name because it is the volume separator character */
      if (!is_volume_name_legal(ua, ua->cmd)) {
         continue;
      }
      if (strlen(ua->cmd) >= MAX_NAME_LENGTH-10) {
         ua->warning_msg(_("Volume name too long.\n"));
         continue;
      }
      if (strlen(ua->cmd) == 0) {
         ua->warning_msg(_("Volume name must be at least one character long.\n"));
         continue;
      }
      break;
   }

   bstrncpy(name, ua->cmd, sizeof(name));
   if (num > 0) {
      bstrncat(name, "%04d", sizeof(name));

      for (;;) {
         if (!get_pint(ua, _("Enter the starting number: "))) {
            return 1;
         }
         startnum = ua->pint32_val;
         if (startnum < 1) {
            ua->warning_msg(_("Start number must be greater than zero.\n"));
            continue;
         }
         break;
      }
   } else {
      startnum = 1;
      num = 1;
   }

   if (store && store->autochanger) {
      if (!get_pint(ua, _("Enter slot (0 for none): "))) {
         return 1;
      }
      Slot = ua->pint32_val;
      if (!get_yesno(ua, _("InChanger? yes/no: "))) {
         return 1;
      }
      InChanger = ua->pint32_val;
   }

   set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
   for (i=startnum; i < num+startnum; i++) {
      bsnprintf(mr.VolumeName, sizeof(mr.VolumeName), name, i);
      mr.Slot = Slot++;
      mr.InChanger = InChanger;
      mr.Enabled = 1;
      set_storageid_in_mr(store, &mr);
      Dmsg1(200, "Create Volume %s\n", mr.VolumeName);
      if (!db_create_media_record(ua->jcr, ua->db, &mr)) {
         ua->error_msg("%s", db_strerror(ua->db));
         return 1;
      }
   }
   pr.NumVols += num;
   Dmsg0(200, "Update pool record.\n");
   if (db_update_pool_record(ua->jcr, ua->db, &pr) != 1) {
      ua->warning_msg("%s", db_strerror(ua->db));
      return 1;
   }
   ua->send_msg(_("%d Volumes created in pool %s\n"), num, pr.Name);

   return 1;
}

/*
 * Turn auto mount on/off
 *
 *  automount on
 *  automount off
 */
int automount_cmd(UAContext *ua, const char *cmd)
{
   char *onoff;

   if (ua->argc != 2) {
      if (!get_cmd(ua, _("Turn on or off? "))) {
            return 1;
      }
      onoff = ua->cmd;
   } else {
      onoff = ua->argk[1];
   }

   ua->automount = (strcasecmp(onoff, NT_("off")) == 0) ? 0 : 1;
   return 1;
}

/*
 * Cancel a job
 */
static int cancel_cmd(UAContext *ua, const char *cmd)
{
   JCR *jcr = select_running_job(ua, "cancel");
   if (!jcr) {
      return 1;
   }
   int ret = cancel_job(ua, jcr);
   free_jcr(jcr);
   return ret;
}

/*
 * This is a common routine to create or update a
 *   Pool DB base record from a Pool Resource. We handle
 *   the setting of MaxVols and NumVols slightly differently
 *   depending on if we are creating the Pool or we are
 *   simply bringing it into agreement with the resource (updage).
 *
 * Caution : RecyclePoolId isn't setup in this function.
 *           You can use set_pooldbr_recyclepoolid();
 *
 */
void set_pooldbr_from_poolres(POOL_DBR *pr, POOL *pool, e_pool_op op)
{
   bstrncpy(pr->PoolType, pool->pool_type, sizeof(pr->PoolType));
   if (op == POOL_OP_CREATE) {
      pr->MaxVols = pool->max_volumes;
      pr->NumVols = 0;
   } else {          /* update pool */
      if (pr->MaxVols != pool->max_volumes) {
         pr->MaxVols = pool->max_volumes;
      }
      if (pr->MaxVols != 0 && pr->MaxVols < pr->NumVols) {
         pr->MaxVols = pr->NumVols;
      }
   }
   pr->LabelType = pool->LabelType;
   pr->UseOnce = pool->use_volume_once;
   pr->UseCatalog = pool->use_catalog;
   pr->Recycle = pool->Recycle;
   pr->VolRetention = pool->VolRetention;
   pr->VolUseDuration = pool->VolUseDuration;
   pr->MaxVolJobs = pool->MaxVolJobs;
   pr->MaxVolFiles = pool->MaxVolFiles;
   pr->MaxVolBytes = pool->MaxVolBytes;
   pr->AutoPrune = pool->AutoPrune;
   pr->ActionOnPurge = pool->action_on_purge;
   pr->Recycle = pool->Recycle;
   if (pool->label_format) {
      bstrncpy(pr->LabelFormat, pool->label_format, sizeof(pr->LabelFormat));
   } else {
      bstrncpy(pr->LabelFormat, "*", sizeof(pr->LabelFormat));    /* none */
   }
}

/* set/update Pool.RecyclePoolId and Pool.ScratchPoolId in Catalog */
int update_pool_references(JCR *jcr, B_DB *db, POOL *pool)
{
   POOL_DBR  pr;

   if (!pool->RecyclePool && !pool->ScratchPool) {
      return 1;
   }

   memset(&pr, 0, sizeof(POOL_DBR));
   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));

   if (!db_get_pool_record(jcr, db, &pr)) {
      return -1;                       /* not exists in database */
   }

   set_pooldbr_from_poolres(&pr, pool, POOL_OP_UPDATE);

   if (!set_pooldbr_references(jcr, db, &pr, pool)) {
      return -1;                      /* error */
   }

   if (!db_update_pool_record(jcr, db, &pr)) {
      return -1;                      /* error */
   }
   return 1;
}

/* set POOL_DBR.RecyclePoolId and POOL_DBR.ScratchPoolId from Pool resource 
 * works with set_pooldbr_from_poolres
 */
bool set_pooldbr_references(JCR *jcr, B_DB *db, POOL_DBR *pr, POOL *pool)
{
   POOL_DBR rpool;
   bool ret = true;

   if (pool->RecyclePool) {
      memset(&rpool, 0, sizeof(POOL_DBR));

      bstrncpy(rpool.Name, pool->RecyclePool->name(), sizeof(rpool.Name));
      if (db_get_pool_record(jcr, db, &rpool)) {
        pr->RecyclePoolId = rpool.PoolId;
      } else {
        Jmsg(jcr, M_WARNING, 0,
        _("Can't set %s RecyclePool to %s, %s is not in database.\n" \
          "Try to update it with 'update pool=%s'\n"),
        pool->name(), rpool.Name, rpool.Name,pool->name());

        ret = false;
      }
   } else {                    /* no RecyclePool used, set it to 0 */
      pr->RecyclePoolId = 0;
   }

   if (pool->ScratchPool) {
      memset(&rpool, 0, sizeof(POOL_DBR));

      bstrncpy(rpool.Name, pool->ScratchPool->name(), sizeof(rpool.Name));
      if (db_get_pool_record(jcr, db, &rpool)) {
        pr->ScratchPoolId = rpool.PoolId;
      } else {
        Jmsg(jcr, M_WARNING, 0,
        _("Can't set %s ScratchPool to %s, %s is not in database.\n" \
          "Try to update it with 'update pool=%s'\n"),
        pool->name(), rpool.Name, rpool.Name,pool->name());
        ret = false;
      }
   } else {                    /* no ScratchPool used, set it to 0 */
      pr->ScratchPoolId = 0;
   }
 
   return ret;
}


/*
 * Create a pool record from a given Pool resource
 *   Also called from backup.c
 * Returns: -1  on error
 *           0  record already exists
 *           1  record created
 */

int create_pool(JCR *jcr, B_DB *db, POOL *pool, e_pool_op op)
{
   POOL_DBR  pr;

   memset(&pr, 0, sizeof(POOL_DBR));

   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));

   if (db_get_pool_record(jcr, db, &pr)) {
      /* Pool Exists */
      if (op == POOL_OP_UPDATE) {  /* update request */
         set_pooldbr_from_poolres(&pr, pool, op);
         set_pooldbr_references(jcr, db, &pr, pool);
         db_update_pool_record(jcr, db, &pr);
      }
      return 0;                       /* exists */
   }

   set_pooldbr_from_poolres(&pr, pool, op);
   set_pooldbr_references(jcr, db, &pr, pool);

   if (!db_create_pool_record(jcr, db, &pr)) {
      return -1;                      /* error */
   }
   return 1;
}



/*
 * Create a Pool Record in the database.
 *  It is always created from the Resource record.
 */
static int create_cmd(UAContext *ua, const char *cmd)
{
   POOL *pool;

   if (!open_client_db(ua)) {
      return 1;
   }

   pool = get_pool_resource(ua);
   if (!pool) {
      return 1;
   }

   switch (create_pool(ua->jcr, ua->db, pool, POOL_OP_CREATE)) {
   case 0:
      ua->error_msg(_("Error: Pool %s already exists.\n"
               "Use update to change it.\n"), pool->name());
      break;

   case -1:
      ua->error_msg("%s", db_strerror(ua->db));
      break;

   default:
     break;
   }
   ua->send_msg(_("Pool %s created.\n"), pool->name());
   return 1;
}


extern DIRRES *director;
extern char *configfile;

/*
 * Python control command
 *  python restart (restarts interpreter)
 */
static int python_cmd(UAContext *ua, const char *cmd)
{
#ifdef HAVE_PYTHON
   init_python_interpreter_args python_args;

   if (ua->argc >= 2 && strcasecmp(ua->argk[1], NT_("restart")) == 0) {
      term_python_interpreter();

      python_args.progname = director->name();
      python_args.scriptdir = director->scripts_directory;
      python_args.modulename = "DirStartUp";
      python_args.configfile = configfile;
      python_args.workingdir = director->working_directory;
      python_args.job_getattr = job_getattr;
      python_args.job_setattr = job_setattr;

      init_python_interpreter(&python_args);

      ua->send_msg(_("Python interpreter restarted.\n"));
   } else {
#endif /* HAVE_PYTHON */
      ua->warning_msg(_("Nothing done.\n"));
#ifdef HAVE_PYTHON
   }
#endif /* HAVE_PYTHON */
   return 1;
}

/*
 * Set a new address in a Client resource. We do this only
 *  if the Console name is the same as the Client name
 *  and the Console can access the client.
 */
static int setip_cmd(UAContext *ua, const char *cmd)
{
   CLIENT *client;
   char buf[1024];
   if (!ua->cons || !acl_access_ok(ua, Client_ACL, ua->cons->name())) {
      ua->error_msg(_("Unauthorized command from this console.\n"));
      return 1;
   }
   LockRes();
   client = GetClientResWithName(ua->cons->name());

   if (!client) {
      ua->error_msg(_("Client \"%s\" not found.\n"), ua->cons->name());
      goto get_out;
   }
   if (client->address) {
      free(client->address);
   }
   /* MA Bug 6 remove ifdef */
   sockaddr_to_ascii(&(ua->UA_sock->client_addr), buf, sizeof(buf));
   client->address = bstrdup(buf);
   ua->send_msg(_("Client \"%s\" address set to %s\n"),
            client->name(), client->address);
get_out:
   UnlockRes();
   return 1;
}


static void do_en_disable_cmd(UAContext *ua, bool setting)
{
   JOB *job;
   int i;

   i = find_arg_with_value(ua, NT_("job")); 
   if (i < 0) { 
      job = select_enable_disable_job_resource(ua, setting);
      if (!job) {
         return;
      }
   } else {
      LockRes();
      job = GetJobResWithName(ua->argv[i]);
      UnlockRes();
   } 
   if (!job) {
      ua->error_msg(_("Job \"%s\" not found.\n"), ua->argv[i]);
      return;
   }

   if (!acl_access_ok(ua, Job_ACL, job->name())) {
      ua->error_msg(_("Unauthorized command from this console.\n"));
      return;
   }
   job->enabled = setting;
   ua->send_msg(_("Job \"%s\" %sabled\n"), job->name(), setting?"en":"dis");
   return;
}

static int enable_cmd(UAContext *ua, const char *cmd)
{
   do_en_disable_cmd(ua, true);
   return 1;
}

static int disable_cmd(UAContext *ua, const char *cmd)
{
   do_en_disable_cmd(ua, false);
   return 1;
}

static void do_storage_setdebug(UAContext *ua, STORE *store, int level, int trace_flag)
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
   sd->fsend("setdebug=%d trace=%d\n", level, trace_flag);
   if (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal(BNET_TERMINATE);
   sd->close();
   jcr->store_bsock = NULL;
   return;
}

/*
 * For the client, we have the following values that can be set
 *  level = debug level
 *  trace = send debug output to a file
 *  hangup = how many records to send to SD before hanging up
 *    obviously this is most useful for testing restarting
 *    failed jobs.
 */
static void do_client_setdebug(UAContext *ua, CLIENT *client, 
              int level, int trace, int hangup)
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
   fd->fsend("setdebug=%d trace=%d hangup=%d\n", level, trace, hangup);
   if (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);
   fd->close();
   ua->jcr->file_bsock = NULL;
   return;
}


static void do_all_setdebug(UAContext *ua, int level, int trace_flag, int hangup)
{
   STORE *store, **unique_store;
   CLIENT *client, **unique_client;
   int i, j, found;

   /* Director */
   debug_level = level;

   /* Count Storage items */
   LockRes();
   store = NULL;
   i = 0;
   foreach_res(store, R_STORAGE) {
      i++;
   }
   unique_store = (STORE **) malloc(i * sizeof(STORE));
   /* Find Unique Storage address/port */
   store = (STORE *)GetNextRes(R_STORAGE, NULL);
   i = 0;
   unique_store[i++] = store;
   while ((store = (STORE *)GetNextRes(R_STORAGE, (RES *)store))) {
      found = 0;
      for (j=0; j<i; j++) {
         if (strcmp(unique_store[j]->address, store->address) == 0 &&
             unique_store[j]->SDport == store->SDport) {
            found = 1;
            break;
         }
      }
      if (!found) {
         unique_store[i++] = store;
         Dmsg2(140, "Stuffing: %s:%d\n", store->address, store->SDport);
      }
   }
   UnlockRes();

   /* Call each unique Storage daemon */
   for (j=0; j<i; j++) {
      do_storage_setdebug(ua, unique_store[j], level, trace_flag);
   }
   free(unique_store);

   /* Count Client items */
   LockRes();
   client = NULL;
   i = 0;
   foreach_res(client, R_CLIENT) {
      i++;
   }
   unique_client = (CLIENT **) malloc(i * sizeof(CLIENT));
   /* Find Unique Client address/port */
   client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   i = 0;
   unique_client[i++] = client;
   while ((client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client))) {
      found = 0;
      for (j=0; j<i; j++) {
         if (strcmp(unique_client[j]->address, client->address) == 0 &&
             unique_client[j]->FDport == client->FDport) {
            found = 1;
            break;
         }
      }
      if (!found) {
         unique_client[i++] = client;
         Dmsg2(140, "Stuffing: %s:%d\n", client->address, client->FDport);
      }
   }
   UnlockRes();

   /* Call each unique File daemon */
   for (j=0; j<i; j++) {
      do_client_setdebug(ua, unique_client[j], level, trace_flag, hangup);
   }
   free(unique_client);
}

/*
 * setdebug level=nn all trace=1/0
 */
static int setdebug_cmd(UAContext *ua, const char *cmd)
{
   STORE *store;
   CLIENT *client;
   int level;
   int trace_flag = -1;
   int hangup = -1;
   int i;

   Dmsg1(120, "setdebug:%s:\n", cmd);

   level = -1;
   i = find_arg_with_value(ua, "level");
   if (i >= 0) {
      level = atoi(ua->argv[i]);
   }
   if (level < 0) {
      if (!get_pint(ua, _("Enter new debug level: "))) {
         return 1;
      }
      level = ua->pint32_val;
   }

   /* Look for trace flag. -1 => not change */
   i = find_arg_with_value(ua, "trace");
   if (i >= 0) {
      trace_flag = atoi(ua->argv[i]);
      if (trace_flag > 0) {
         trace_flag = 1;
      }
   }

   /* Look for hangup (debug only)flag. -1 => not change */
   i = find_arg_with_value(ua, "hangup");
   if (i >= 0) {
      hangup = atoi(ua->argv[i]);
   }


   /* General debug? */
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], "all") == 0) {
         do_all_setdebug(ua, level, trace_flag, hangup);
         return 1;
      }
      if (strcasecmp(ua->argk[i], "dir") == 0 ||
          strcasecmp(ua->argk[i], "director") == 0) {
         debug_level = level;
         set_trace(trace_flag);
         return 1;
      }
      if (strcasecmp(ua->argk[i], "client") == 0 ||
          strcasecmp(ua->argk[i], "fd") == 0) {
         client = NULL;
         if (ua->argv[i]) {
            client = GetClientResWithName(ua->argv[i]);
            if (client) {
               do_client_setdebug(ua, client, level, trace_flag, hangup);
               return 1;
            }
         }
         client = select_client_resource(ua);
         if (client) {
            do_client_setdebug(ua, client, level, trace_flag, hangup);
            return 1;
         }
      }

      if (strcasecmp(ua->argk[i], NT_("store")) == 0 ||
          strcasecmp(ua->argk[i], NT_("storage")) == 0 ||
          strcasecmp(ua->argk[i], NT_("sd")) == 0) {
         store = NULL;
         if (ua->argv[i]) {
            store = GetStoreResWithName(ua->argv[i]);
            if (store) {
               do_storage_setdebug(ua, store, level, trace_flag);
               return 1;
            }
         }
         store = get_storage_resource(ua, false/*no default*/);
         if (store) {
            do_storage_setdebug(ua, store, level, trace_flag);
            return 1;
         }
      }
   }
   /*
    * We didn't find an appropriate keyword above, so
    * prompt the user.
    */
   start_prompt(ua, _("Available daemons are: \n"));
   add_prompt(ua, _("Director"));
   add_prompt(ua, _("Storage"));
   add_prompt(ua, _("Client"));
   add_prompt(ua, _("All"));
   switch(do_prompt(ua, "", _("Select daemon type to set debug level"), NULL, 0)) {
   case 0:                         /* Director */
      debug_level = level;
      set_trace(trace_flag);
      break;
   case 1:
      store = get_storage_resource(ua, false/*no default*/);
      if (store) {
         do_storage_setdebug(ua, store, level, trace_flag);
      }
      break;
   case 2:
      client = select_client_resource(ua);
      if (client) {
         do_client_setdebug(ua, client, level, trace_flag, hangup);
      }
      break;
   case 3:
      do_all_setdebug(ua, level, trace_flag, hangup);
      break;
   default:
      break;
   }
   return 1;
}

/*
 * Turn debug tracing to file on/off
 */
static int trace_cmd(UAContext *ua, const char *cmd)
{
   char *onoff;

   if (ua->argc != 2) {
      if (!get_cmd(ua, _("Turn on or off? "))) {
            return 1;
      }
      onoff = ua->cmd;
   } else {
      onoff = ua->argk[1];
   }

   set_trace((strcasecmp(onoff, NT_("off")) == 0) ? false : true);
   return 1;

}

static int var_cmd(UAContext *ua, const char *cmd)
{
   POOLMEM *val = get_pool_memory(PM_FNAME);
   char *var;

   if (!open_client_db(ua)) {
      return 1;
   }
   for (var=ua->cmd; *var != ' '; ) {    /* skip command */
      var++;
   }
   while (*var == ' ') {                 /* skip spaces */
      var++;
   }
   Dmsg1(100, "Var=%s:\n", var);
   variable_expansion(ua->jcr, var, &val);
   ua->send_msg("%s\n", val);
   free_pool_memory(val);
   return 1;
}

static int estimate_cmd(UAContext *ua, const char *cmd)
{
   JOB *job = NULL;
   CLIENT *client = NULL;
   FILESET *fileset = NULL;
   int listing = 0;
   char since[MAXSTRING];
   JCR *jcr = ua->jcr;
   int accurate=-1;

   jcr->setJobLevel(L_FULL);
   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("client")) == 0 ||
          strcasecmp(ua->argk[i], NT_("fd")) == 0) {
         if (ua->argv[i]) {
            client = GetClientResWithName(ua->argv[i]);
            if (!client) {
               ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }
            if (!acl_access_ok(ua, Client_ACL, client->name())) {
               ua->error_msg(_("No authorization for Client \"%s\"\n"), client->name());
               return 1;
            }
            continue;
         } else {
            ua->error_msg(_("Client name missing.\n"));
            return 1;
         }
      }
      if (strcasecmp(ua->argk[i], NT_("job")) == 0) {
         if (ua->argv[i]) {
            job = GetJobResWithName(ua->argv[i]);
            if (!job) {
               ua->error_msg(_("Job \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }
            if (!acl_access_ok(ua, Job_ACL, job->name())) {
               ua->error_msg(_("No authorization for Job \"%s\"\n"), job->name());
               return 1;
            }
            continue;
         } else {
            ua->error_msg(_("Job name missing.\n"));
            return 1;
         }

      }
      if (strcasecmp(ua->argk[i], NT_("fileset")) == 0) {
         if (ua->argv[i]) {
            fileset = GetFileSetResWithName(ua->argv[i]);
            if (!fileset) {
               ua->error_msg(_("Fileset \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }
            if (!acl_access_ok(ua, FileSet_ACL, fileset->name())) {
               ua->error_msg(_("No authorization for FileSet \"%s\"\n"), fileset->name());
               return 1;
            }
            continue;
         } else {
            ua->error_msg(_("Fileset name missing.\n"));
            return 1;
         }
      }
      if (strcasecmp(ua->argk[i], NT_("listing")) == 0) {
         listing = 1;
         continue;
      }
      if (strcasecmp(ua->argk[i], NT_("level")) == 0) {
         if (ua->argv[i]) {
            if (!get_level_from_name(ua->jcr, ua->argv[i])) {
               ua->error_msg(_("Level \"%s\" not valid.\n"), ua->argv[i]);
            }
            continue;
         } else {
            ua->error_msg(_("Level value missing.\n"));
            return 1;
         }
      }
      if (strcasecmp(ua->argk[i], NT_("accurate")) == 0) {
         if (ua->argv[i]) {
            if (!is_yesno(ua->argv[i], &accurate)) {
               ua->error_msg(_("Invalid value for accurate. " 
                               "It must be yes or no.\n"));
            }
            continue;
         } else {
            ua->error_msg(_("Accurate value missing.\n"));
            return 1;
         }
      }
   }
   if (!job && !(client && fileset)) {
      if (!(job = select_job_resource(ua))) {
         return 1;
      }
   }
   if (!job) {
      job = GetJobResWithName(ua->argk[1]);
      if (!job) {
         ua->error_msg(_("No job specified.\n"));
         return 1;
      }
      if (!acl_access_ok(ua, Job_ACL, job->name())) {
         ua->error_msg(_("No authorization for Job \"%s\"\n"), job->name());
         return 1;
      }
   }
   if (!client) {
      client = job->client;
   }
   if (!fileset) {
      fileset = job->fileset;
   }
   jcr->client = client;
   jcr->fileset = fileset;
   close_db(ua);
   if (job->pool->catalog) {
      ua->catalog = job->pool->catalog;
   } else {
      ua->catalog = client->catalog;
   }

   if (!open_db(ua)) {
      return 1;
   }

   jcr->job = job;
   jcr->setJobType(JT_BACKUP);
   jcr->start_time = time(NULL);
   init_jcr_job_record(jcr);

   if (!get_or_create_client_record(jcr)) {
      return 1;
   }
   if (!get_or_create_fileset_record(jcr)) {
      return 1;
   }

   get_level_since_time(ua->jcr, since, sizeof(since));

   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
      jcr->client->name(), jcr->client->address, jcr->client->FDport);
   if (!connect_to_file_daemon(jcr, 1, 15, 0)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return 1;
   }

   /* The level string change if accurate mode is enabled */
   if (accurate >= 0) {
      jcr->accurate = accurate;
   } else {
      jcr->accurate = job->accurate;
   }

   if (!send_level_command(jcr)) {
      goto bail_out;
   }

   if (!send_include_list(jcr)) {
      ua->error_msg(_("Error sending include list.\n"));
      goto bail_out;
   }

   if (!send_exclude_list(jcr)) {
      ua->error_msg(_("Error sending exclude list.\n"));
      goto bail_out;
   }

   /*
    * If the job is in accurate mode, we send the list of
    * all files to FD.
    */
   Dmsg1(40, "estimate accurate=%d\n", jcr->accurate);
   if (!send_accurate_current_files(jcr)) {
      goto bail_out;
   }

   jcr->file_bsock->fsend("estimate listing=%d\n", listing);
   while (jcr->file_bsock->recv() >= 0) {
      ua->send_msg("%s", jcr->file_bsock->msg);
   }

bail_out:
   if (jcr->file_bsock) {
      jcr->file_bsock->signal(BNET_TERMINATE);
      jcr->file_bsock->close();
      jcr->file_bsock = NULL;
   }
   return 1;
}


/*
 * print time
 */
static int time_cmd(UAContext *ua, const char *cmd)
{
   char sdt[50];
   time_t ttime = time(NULL);
   struct tm tm;
   (void)localtime_r(&ttime, &tm);
   strftime(sdt, sizeof(sdt), "%d-%b-%Y %H:%M:%S", &tm);
   ua->send_msg("%s\n", sdt);
   return 1;
}

/*
 * reload the conf file
 */
extern "C" void reload_config(int sig);

static int reload_cmd(UAContext *ua, const char *cmd)
{
   reload_config(1);
   return 1;
}

/*
 * Delete Pool records (should purge Media with it).
 *
 *  delete pool=<pool-name>
 *  delete volume pool=<pool-name> volume=<name>
 *  delete jobid=xxx
 */
static int delete_cmd(UAContext *ua, const char *cmd)
{
   static const char *keywords[] = {
      NT_("volume"),
      NT_("pool"),
      NT_("jobid"),
      NULL};

   if (!open_client_db(ua)) {
      return 1;
   }

   switch (find_arg_keyword(ua, keywords)) {
   case 0:
      delete_volume(ua);
      return 1;
   case 1:
      delete_pool(ua);
      return 1;
   case 2:
      int i;
      while ((i = find_arg(ua, "jobid")) > 0) {
         delete_job(ua);
         *ua->argk[i] = 0;         /* zap keyword already visited */
      }
      return 1;
   default:
      break;
   }

   ua->warning_msg(_(
"In general it is not a good idea to delete either a\n"
"Pool or a Volume since they may contain data.\n\n"));

   switch (do_keyword_prompt(ua, _("Choose catalog item to delete"), keywords)) {
   case 0:
      delete_volume(ua);
      break;
   case 1:
      delete_pool(ua);
      break;
   case 2:
      delete_job(ua);
      return 1;
   default:
      ua->warning_msg(_("Nothing done.\n"));
      break;
   }
   return 1;
}

/*
 * delete_job has been modified to parse JobID lists like the following:
 * delete JobID=3,4,6,7-11,14
 *
 * Thanks to Phil Stracchino for the above addition.
 */
static void delete_job(UAContext *ua)
{
   int i;
   JobId_t JobId;
   char *s, *sep, *tok;

   i = find_arg_with_value(ua, NT_("jobid"));
   if (i >= 0) {
      if (strchr(ua->argv[i], ',') || strchr(ua->argv[i], '-')) {
         s = bstrdup(ua->argv[i]);
         tok = s;

         /*
          * We could use strtok() here.  But we're not going to, because:
          * (a) strtok() is deprecated, having been replaced by strsep();
          * (b) strtok() is broken in significant ways.
          * we could use strsep() instead, but it's not universally available.
          * so we grow our own using strchr().
          */
         sep = strchr(tok, ',');
         while (sep != NULL) {
            *sep = '\0';
            if (!delete_job_id_range(ua, tok)) {
               if (is_a_number(tok)) {
                  JobId = (JobId_t)str_to_uint64(tok);
                  do_job_delete(ua, JobId);
               } else {
                  ua->warning_msg(_("Illegal JobId %s ignored\n"), tok);
               }
            }
            tok = ++sep;
            sep = strchr(tok, ',');
         }

         /*
          * Pick up the last token
          */
         if (!delete_job_id_range(ua, tok)) {
            if (is_a_number(tok)) {
               JobId = (JobId_t)str_to_uint64(tok);
               do_job_delete(ua, JobId);
            } else {
               ua->warning_msg(_("Illegal JobId %s ignored\n"), tok);
            }
         }

         free(s);
      } else {
         if (is_a_number(ua->argv[i])) {
            JobId = (JobId_t)str_to_uint64(ua->argv[i]);
            do_job_delete(ua, JobId);
         } else {
            ua->warning_msg(_("Illegal JobId %s ignored\n"), ua->argv[i]);
         }
      }
   } else if (!get_pint(ua, _("Enter JobId to delete: "))) {
      return;
   } else {
      JobId = ua->int64_val;
      do_job_delete(ua, JobId);
   }
}

/*
 * We call delete_job_id_range to parse range tokens and iterate over ranges
 */
static bool delete_job_id_range(UAContext *ua, char *tok)
{
   char buf[64];
   char *tok2;
   JobId_t j, j1, j2;

   tok2 = strchr(tok, '-');
   if (!tok2) {
      return false;
   }

   *tok2 = '\0';
   tok2++;

   if (is_a_number(tok) && is_a_number(tok2)) {
      j1 = (JobId_t)str_to_uint64(tok);
      j2 = (JobId_t)str_to_uint64(tok2);

      if (j1 > j2) {
         /*
          * See if the range is big if more then 25 Jobs are deleted
          * ask the user for confirmation.
          */
         if ((j2 - j1) > 25) {
            bsnprintf(buf, sizeof(buf),
                      _("Are you sure you want to delete %d JobIds ? (yes/no): "),
                      j2 - j1);
            if (!get_yesno(ua, buf)) {
               return true;
            }
         }
         for (j = j1; j <= j2; j++) {
            do_job_delete(ua, j);
         }
      } else {
         ua->warning_msg(_("Illegal JobId range %s - %s should define increasing JobIds, ignored\n"),
                         tok, tok2);
      }
   } else {
      ua->warning_msg(_("Illegal JobId range %s - %s, ignored\n"), tok, tok2);
   }

   return true;
}

/*
 * do_job_delete now performs the actual delete operation atomically
 */
static void do_job_delete(UAContext *ua, JobId_t JobId)
{
   char ed1[50];

   edit_int64(JobId, ed1);
   purge_jobs_from_catalog(ua, ed1);
   ua->send_msg(_("Jobid %s and associated records deleted from the catalog.\n"), ed1);
}

/*
 * Delete media records from database -- dangerous
 */
static int delete_volume(UAContext *ua)
{
   MEDIA_DBR mr;
   char buf[1000];
   db_list_ctx lst;

   if (!select_media_dbr(ua, &mr)) {
      return 1;
   }
   ua->warning_msg(_("\nThis command will delete volume %s\n"
      "and all Jobs saved on that volume from the Catalog\n"),
      mr.VolumeName);

   if (find_arg(ua, "yes") >= 0) {
      ua->pint32_val = 1; /* Have "yes" on command line already" */
   } else {
      bsnprintf(buf, sizeof(buf), _("Are you sure you want to delete Volume \"%s\"? (yes/no): "),
         mr.VolumeName);
      if (!get_yesno(ua, buf)) {
         return 1;
      }
   }
   if (!ua->pint32_val) {
      return 1;
   }

   /* If not purged, do it */
   if (strcmp(mr.VolStatus, "Purged") != 0) {
      if (!db_get_volume_jobids(ua->jcr, ua->db, &mr, &lst)) {
         ua->error_msg(_("Can't list jobs on this volume\n"));
         return 1;
      }
      if (lst.count) {
         purge_jobs_from_catalog(ua, lst.list);
      }
   }

   db_delete_media_record(ua->jcr, ua->db, &mr);
   return 1;
}

/*
 * Delete a pool record from the database -- dangerous
 */
static int delete_pool(UAContext *ua)
{
   POOL_DBR  pr;
   char buf[200];

   memset(&pr, 0, sizeof(pr));

   if (!get_pool_dbr(ua, &pr)) {
      return 1;
   }
   bsnprintf(buf, sizeof(buf), _("Are you sure you want to delete Pool \"%s\"? (yes/no): "),
      pr.Name);
   if (!get_yesno(ua, buf)) {
      return 1;
   }
   if (ua->pint32_val) {
      db_delete_pool_record(ua->jcr, ua->db, &pr);
   }
   return 1;
}

int memory_cmd(UAContext *ua, const char *cmd)
{
   garbage_collect_memory();
   list_dir_status_header(ua);
   sm_dump(false, true);
   return 1;
}

static void do_mount_cmd(UAContext *ua, const char *command)
{
   USTORE store;
   BSOCK *sd;
   JCR *jcr = ua->jcr;
   char dev_name[MAX_NAME_LENGTH];
   int drive;
   int slot = -1;

   if (!open_client_db(ua)) {
      return;
   }
   Dmsg2(120, "%s: %s\n", command, ua->UA_sock->msg);

   store.store = get_storage_resource(ua, true/*arg is storage*/);
   if (!store.store) {
      return;
   }
   pm_strcpy(store.store_source, _("unknown source"));
   set_wstorage(jcr, &store);
   drive = get_storage_drive(ua, store.store);
   if (strcmp(command, "mount") == 0) {
      slot = get_storage_slot(ua, store.store);
   }

   Dmsg3(120, "Found storage, MediaType=%s DevName=%s drive=%d\n",
      store.store->media_type, store.store->dev_name(), drive);

   if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
      ua->error_msg(_("Failed to connect to Storage daemon.\n"));
      return;
   }
   sd = jcr->store_bsock;
   bstrncpy(dev_name, store.store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   if (slot > 0) {
      sd->fsend("%s %s drive=%d slot=%d", command, dev_name, drive, slot);
   } else {
      sd->fsend("%s %s drive=%d", command, dev_name, drive);
   }
   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal(BNET_TERMINATE);
   sd->close();
   jcr->store_bsock = NULL;
}

/*
 * mount [storage=<name>] [drive=nn] [slot=mm]
 */
static int mount_cmd(UAContext *ua, const char *cmd)
{
   do_mount_cmd(ua, "mount");          /* mount */
   return 1;
}


/*
 * unmount [storage=<name>] [drive=nn]
 */
static int unmount_cmd(UAContext *ua, const char *cmd)
{
   do_mount_cmd(ua, "unmount");          /* unmount */
   return 1;
}


/*
 * release [storage=<name>] [drive=nn]
 */
static int release_cmd(UAContext *ua, const char *cmd)
{
   do_mount_cmd(ua, "release");          /* release */
   return 1;
}


/*
 * Switch databases
 *   use catalog=<name>
 */
static int use_cmd(UAContext *ua, const char *cmd)
{
   CAT *oldcatalog, *catalog;


   close_db(ua);                      /* close any previously open db */
   oldcatalog = ua->catalog;

   if (!(catalog = get_catalog_resource(ua))) {
      ua->catalog = oldcatalog;
   } else {
      ua->catalog = catalog;
   }
   if (open_db(ua)) {
      ua->send_msg(_("Using Catalog name=%s DB=%s\n"),
         ua->catalog->name(), ua->catalog->db_name);
   }
   return 1;
}

int quit_cmd(UAContext *ua, const char *cmd)
{
   ua->quit = true;
   return 1;
}

/* Handler to get job status */
static int status_handler(void *ctx, int num_fields, char **row)
{
   char *val = (char *)ctx;

   if (row[0]) {
      *val = row[0][0];
   } else {
      *val = '?';               /* Unknown by default */
   }

   return 0;
}

/*
 * Wait until no job is running
 */
int wait_cmd(UAContext *ua, const char *cmd)
{
   JCR *jcr;
   int i;
   time_t stop_time = 0;

   /*
    * no args
    * Wait until no job is running
    */
   if (ua->argc == 1) {
      bmicrosleep(0, 200000);            /* let job actually start */
      for (bool running=true; running; ) {
         running = false;
         foreach_jcr(jcr) {
            if (jcr->JobId != 0) {
               running = true;
               break;
            }
         }
         endeach_jcr(jcr);

         if (running) {
            bmicrosleep(1, 0);
         }
      }
      return 1;
   }

   i = find_arg_with_value(ua, NT_("timeout")); 
   if (i > 0 && ua->argv[i]) {
      stop_time = time(NULL) + str_to_int64(ua->argv[i]);
   }

   /* we have jobid, jobname or ujobid argument */

   uint32_t jobid = 0 ;

   if (!open_client_db(ua)) {
      ua->error_msg(_("ERR: Can't open db\n")) ;
      return 1;
   }

   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], "jobid") == 0) {
         if (!ua->argv[i]) {
            break;
         }
         jobid = str_to_int64(ua->argv[i]);
         break;
      } else if (strcasecmp(ua->argk[i], "jobname") == 0 ||
                 strcasecmp(ua->argk[i], "job") == 0) {
         if (!ua->argv[i]) {
            break;
         }
         jcr=get_jcr_by_partial_name(ua->argv[i]) ;
         if (jcr) {
            jobid = jcr->JobId ;
            free_jcr(jcr);
         }
         break;
      } else if (strcasecmp(ua->argk[i], "ujobid") == 0) {
         if (!ua->argv[i]) {
            break;
         }
         jcr=get_jcr_by_full_name(ua->argv[i]) ;
         if (jcr) {
            jobid = jcr->JobId ;
            free_jcr(jcr);
         }
         break;
      /* Wait for a mount request */
      } else if (strcasecmp(ua->argk[i], "mount") == 0) {
         for (bool waiting=false; !waiting; ) {
            foreach_jcr(jcr) {
               if (jcr->JobId != 0 && 
                   (jcr->JobStatus == JS_WaitMedia || jcr->JobStatus == JS_WaitMount)) {
                  waiting = true;
                  break;
               }
            }
            endeach_jcr(jcr);
            if (waiting) {
               break;
            }
            if (stop_time && (time(NULL) >= stop_time)) {
               ua->warning_msg(_("Wait on mount timed out\n"));
               return 1;
            }
            bmicrosleep(1, 0);
         }
         return 1;
      }
   }

   if (jobid == 0) {
      ua->error_msg(_("ERR: Job was not found\n"));
      return 1 ;
   }

   /*
    * We wait the end of a specific job
    */

   bmicrosleep(0, 200000);            /* let job actually start */
   for (bool running=true; running; ) {
      running = false;

      jcr=get_jcr_by_id(jobid) ;

      if (jcr) {
         running = true ;
         free_jcr(jcr);
      }

      if (running) {
         bmicrosleep(1, 0);
      }
   }

   /*
    * We have to get JobStatus
    */

   int status ;
   char jobstatus = '?';        /* Unknown by default */
   char buf[256] ;

   bsnprintf(buf, sizeof(buf),
             "SELECT JobStatus FROM Job WHERE JobId='%i'", jobid);


   db_sql_query(ua->db, buf,
                status_handler, (void *)&jobstatus);

   switch (jobstatus) {
   case JS_Error:
      status = 1 ;         /* Warning */
      break;

   case JS_FatalError:
   case JS_ErrorTerminated:
   case JS_Canceled:
      status = 2 ;         /* Critical */
      break;

   case JS_Warnings:
   case JS_Terminated:
      status = 0 ;         /* Ok */
      break;

   default:
      status = 3 ;         /* Unknown */
      break;
   }

   ua->send_msg("JobId=%i\n", jobid) ;
   ua->send_msg("JobStatus=%s (%c)\n", 
            job_status_to_str(jobstatus), 
            jobstatus) ;

   if (ua->gui || ua->api) {
      ua->send_msg("ExitStatus=%i\n", status) ;
   }

   return 1;
}


static int help_cmd(UAContext *ua, const char *cmd)
{
   int i;
   ua->send_msg(_("  Command       Description\n  =======       ===========\n"));
   for (i=0; i<comsize; i++) {
      if (ua->argc == 2) {
         if (!strcasecmp(ua->argk[1], commands[i].key)) {
            ua->send_msg(_("  %-13s %s\n\nArguments:\n\t%s\n"), commands[i].key, 
                         commands[i].help, commands[i].usage);
            break;
         }
      } else {
         ua->send_msg(_("  %-13s %s\n"), commands[i].key, commands[i].help);
      }
   }
   if (i == comsize && ua->argc == 2) {
      ua->send_msg(_("\nCan't find %s command.\n\n"), ua->argk[1]);
   }
   ua->send_msg(_("\nWhen at a prompt, entering a period cancels the command.\n\n"));
   return 1;
}

int qhelp_cmd(UAContext *ua, const char *cmd)
{
   int i,j;
   /* Want to display only commands */
   j = find_arg(ua, NT_("all"));
   if (j >= 0) {
      for (i=0; i<comsize; i++) {
         ua->send_msg("%s\n", commands[i].key);
      }
      return 1;
   }
   /* Want to display a specific help section */
   j = find_arg_with_value(ua, NT_("item"));
   if (j >= 0 && ua->argk[j]) {
      for (i=0; i<comsize; i++) {
         if (bstrcmp(commands[i].key, ua->argv[j])) {
            ua->send_msg("%s\n", commands[i].usage);
            break;
         }
      }
      return 1;
   }
   /* Want to display everything */
   for (i=0; i<comsize; i++) {
      ua->send_msg("%s %s -- %s\n", commands[i].key, commands[i].help, commands[i].usage);
   }
   return 1;
}

#if 1 
static int version_cmd(UAContext *ua, const char *cmd)
{
   ua->send_msg(_("%s Version: %s (%s) %s %s %s %s\n"), my_name, VERSION, BDATE,
                HOST_OS, DISTNAME, DISTVER, NPRTB(director->verid));
   return 1;
}
#else
/*
 *  Test code -- turned on only for debug testing 
 */
static int version_cmd(UAContext *ua, const char *cmd)
{
   dbid_list ids;
   POOL_MEM query(PM_MESSAGE);
   open_db(ua);
   Mmsg(query, "select MediaId from Media,Pool where Pool.PoolId=Media.PoolId and Pool.Name='Full'");
   db_get_query_dbids(ua->jcr, ua->db, query, ids);
   ua->send_msg("num_ids=%d max_ids=%d tot_ids=%d\n", ids.num_ids, ids.max_ids, ids.tot_ids);
   for (int i=0; i < ids.num_ids; i++) {
      ua->send_msg("id=%d\n", ids.DBId[i]);
   }
   close_db(ua);
   return 1;
}
#endif

/* 
 * This call uses open_client_db() and force a
 * new dedicated connection to the catalog
 */
bool open_new_client_db(UAContext *ua)
{   
   bool ret;

   /* Force a new dedicated connection */
   close_db(ua);
   ua->force_mult_db_connections = true;
   ret = open_client_db(ua);
   ua->force_mult_db_connections = false;
   return ret;
}

/* 
 * This call explicitly checks for a catalog=xxx and
 *  if given, opens that catalog.  It also checks for
 *  client=xxx and if found, opens the catalog 
 *  corresponding to that client. If we still don't 
 *  have a catalog, look for a Job keyword and get the
 *  catalog from its client record.
 */
bool open_client_db(UAContext *ua)
{
   int i;
   CAT *catalog;
   CLIENT *client;
   JOB *job;

   /* Try for catalog keyword */
   i = find_arg_with_value(ua, NT_("catalog"));
   if (i >= 0) {
      if (!acl_access_ok(ua, Catalog_ACL, ua->argv[i])) {
         ua->error_msg(_("No authorization for Catalog \"%s\"\n"), ua->argv[i]);
         return false;
      }
      catalog = GetCatalogResWithName(ua->argv[i]);
      if (catalog) {
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         ua->catalog = catalog;
         return open_db(ua);
      }
   }

   /* Try for client keyword */
   i = find_arg_with_value(ua, NT_("client"));
   if (i >= 0) {
      if (!acl_access_ok(ua, Client_ACL, ua->argv[i])) {
         ua->error_msg(_("No authorization for Client \"%s\"\n"), ua->argv[i]);
         return false;
      }
      client = GetClientResWithName(ua->argv[i]);
      if (client) {
         catalog = client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         if (!acl_access_ok(ua, Catalog_ACL, catalog->name())) {
            ua->error_msg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return open_db(ua);
      }
   }

   /* Try for Job keyword */
   i = find_arg_with_value(ua, NT_("job"));
   if (i >= 0) {
      if (!acl_access_ok(ua, Job_ACL, ua->argv[i])) {
         ua->error_msg(_("No authorization for Job \"%s\"\n"), ua->argv[i]);
         return false;
      }
      job = GetJobResWithName(ua->argv[i]);
      if (job) {
         catalog = job->client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         if (!acl_access_ok(ua, Catalog_ACL, catalog->name())) {
            ua->error_msg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return open_db(ua);
      }
   }

   return open_db(ua);
}


/*
 * Open the catalog database.
 */
bool open_db(UAContext *ua)
{
   bool mult_db_conn;

   if (ua->db) {
      return true;
   }
   if (!ua->catalog) {
      ua->catalog = get_catalog_resource(ua);
      if (!ua->catalog) {
         ua->error_msg( _("Could not find a Catalog resource\n"));
         return false;
      }
   }

   /* Some modules like bvfs need their own catalog connection */
   mult_db_conn = ua->catalog->mult_db_connections;
   if (ua->force_mult_db_connections) {
      mult_db_conn = true;
   }

   ua->jcr->catalog = ua->catalog;

   Dmsg0(100, "UA Open database\n");
   ua->db = db_init_database(ua->jcr, ua->catalog->db_driver, ua->catalog->db_name, 
                             ua->catalog->db_user,
                             ua->catalog->db_password, ua->catalog->db_address,
                             ua->catalog->db_port, ua->catalog->db_socket,
                             mult_db_conn, ua->catalog->disable_batch_insert);
   if (!ua->db || !db_open_database(ua->jcr, ua->db)) {
      ua->error_msg(_("Could not open catalog database \"%s\".\n"),
                 ua->catalog->db_name);
      if (ua->db) {
         ua->error_msg("%s", db_strerror(ua->db));
      }
      close_db(ua);
      return false;
   }
   ua->jcr->db = ua->db;
   if (!ua->api) {
      ua->send_msg(_("Using Catalog \"%s\"\n"), ua->catalog->name()); 
   }
   Dmsg1(150, "DB %s opened\n", ua->catalog->db_name);
   return true;
}

void close_db(UAContext *ua)
{
   if (ua->db) {
      db_close_database(ua->jcr, ua->db);
      ua->db = NULL;
      if (ua->jcr) {
         ua->jcr->db = NULL;
      }
   }
}
