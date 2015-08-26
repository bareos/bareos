/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, September MM
 */

#include "bareos.h"
#include "dird.h"

/* Imported subroutines */

/* Imported variables */
extern jobq_t job_queue;              /* job queue */

/* Imported functions */
extern int autodisplay_cmd(UAContext *ua, const char *cmd);
extern int configure_cmd(UAContext *ua, const char *cmd);
extern int gui_cmd(UAContext *ua, const char *cmd);
extern int label_cmd(UAContext *ua, const char *cmd);
extern int list_cmd(UAContext *ua, const char *cmd);
extern int llist_cmd(UAContext *ua, const char *cmd);
extern int messages_cmd(UAContext *ua, const char *cmd);
extern int prune_cmd(UAContext *ua, const char *cmd);
extern int purge_cmd(UAContext *ua, const char *cmd);
extern int query_cmd(UAContext *ua, const char *cmd);
extern int relabel_cmd(UAContext *ua, const char *cmd);
extern int restore_cmd(UAContext *ua, const char *cmd);
extern int show_cmd(UAContext *ua, const char *cmd);
extern int sqlquery_cmd(UAContext *ua, const char *cmd);
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
static int release_cmd(UAContext *ua, const char *cmd);
static int reload_cmd(UAContext *ua, const char *cmd);
static int setdebug_cmd(UAContext *ua, const char *cmd);
static int setbwlimit_cmd(UAContext *ua, const char *cmd);
static int setip_cmd(UAContext *ua, const char *cmd);
static int time_cmd(UAContext *ua, const char *cmd);
static int resolve_cmd(UAContext *ua, const char *cmd);
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

/*
 * Not all in alphabetical order.
 * New commands are added after existing commands with similar letters
 * to prevent breakage of existing user scripts.
 */
struct cmdstruct {
   const char *key; /* Command */
   int (*func)(UAContext *ua, const char *cmd); /* Handler */
   const char *help; /* Main purpose */
   const char *usage; /* All arguments to build usage */
   const bool use_in_rs; /* Can use it in Console RunScript */
   const bool audit_event; /* Log an audit event when this Command is executed */
};
static struct cmdstruct commands[] = {
   { NT_("add"), add_cmd, _("Add media to a pool"),
     NT_("pool=<pool-name> storage=<storage-name> jobid=<jobid>"), false, true },
   { NT_("autodisplay"), autodisplay_cmd,_("Autodisplay console messages"),
     NT_("on | off"), false, false },
   { NT_("automount"), automount_cmd, _("Automount after label"),
     NT_("on | off"), false, true },
   { NT_("cancel"), cancel_cmd, _("Cancel a job"),
     NT_("storage=<storage-name> | jobid=<jobid> | job=<job-name> | ujobid=<unique-jobid> | state=<job_state> | all yes"), false, true },
   { NT_("configure"), configure_cmd, _("Configure director"),
     NT_("terminal"), false, true },
   { NT_("create"), create_cmd, _("Create DB Pool from resource"),
     NT_("pool=<pool-name>"), false, true },
   { NT_("delete"), delete_cmd, _("Delete volume, pool or job"),
     NT_("volume=<vol-name> pool=<pool-name> jobid=<jobid>"), true, true },
   { NT_("disable"), disable_cmd, _("Disable a job/client/schedule"),
     NT_("job=<job-name> client=<client-name> schedule=<schedule-name>"), true, true },
   { NT_("enable"), enable_cmd, _("Enable a job/client/schedule"),
     NT_("job=<job-name> client=<client-name> schedule=<schedule-name>"), true, true },
   { NT_("estimate"), estimate_cmd, _("Performs FileSet estimate, listing gives full listing"),
     NT_("fileset=<fileset-name> client=<client-name> level=<level> accurate=<yes/no> job=<job-name> listing"), true, true },
   { NT_("exit"), quit_cmd, _("Terminate Bconsole session"),
     NT_(""), false, false },
   { NT_("export"), export_cmd,_("Export volumes from normal slots to import/export slots"),
     NT_("storage=<storage-name> srcslots=<slot-selection> [ dstslots=<slot-selection> volume=<volume-name> scan ]"), true, true },
   { NT_("gui"), gui_cmd, _("Non-interactive gui mode"),
     NT_("on | off"), false, false },
   { NT_("help"), help_cmd, _("Print help on specific command"),
     NT_("add autodisplay automount cancel create delete disable\n"
         "\tenable estimate exit gui label list llist\n"
         "\tmessages memory mount prune purge quit query\n"
         "\trestore relabel release reload run status\n"
         "\tsetbandwidth setdebug setip show sqlquery time trace unmount\n"
         "\tumount update use var version wait"), false, false },
   { NT_("import"), import_cmd, _("Import volumes from import/export slots to normal slots"),
     NT_("storage=<storage-name> [ srcslots=<slot-selection> dstslots=<slot-selection> volume=<volume-name> scan ]"), true, true },
   { NT_("label"), label_cmd, _("Label a tape"),
     NT_("storage=<storage-name> volume=<volume-name> pool=<pool-name> slot=<slot> [ barcodes ] [ encrypt ]"), false, true },
   { NT_("list"), list_cmd, _("List objects from catalog"),
     NT_("jobs | jobid=<jobid> | ujobid=<complete_name> | job=<job-name> | jobmedia jobid=<jobid> |\n"
         "\tjobmedia ujobid=<complete_name> | joblog jobid=<jobid> | joblog ujobid=<complete_name> |\n"
         "\tbasefiles jobid=<jobid> | basefiles ujobid=<complete_name> |\n"
         "\tfiles jobid=<jobid> | files ujobid=<complete_name> | pools | jobtotals |\n"
         "\tvolumes [ jobid=<jobid> ujobid=<complete_name> pool=<pool-name> ] |\n"
         "\tmedia [ jobid=<jobid> ujobid=<complete_name> pool=<pool-name> ] | clients |\n"
         "\tnextvol job=<job-name> | nextvolume ujobid=<complete_name> | copies jobid=<jobid>"), true, true },
   { NT_("llist"), llist_cmd, _("Full or long list like list command"),
     NT_("jobs | jobid=<jobid> | ujobid=<complete_name> | job=<job-name> | jobmedia jobid=<jobid> |\n"
         "\tjobmedia ujobid=<complete_name> | joblog jobid=<jobid> | joblog ujobid=<complete_name> |\n"
         "\tbasefiles jobid=<jobid> | basefiles ujobid=<complete_name> |\n"
         "\tfiles jobid=<jobid> | files ujobid=<complete_name> | pools | jobtotals |\n"
         "\tvolumes [ jobid=<jobid> ujobid=<complete_name> pool=<pool-name> ] |\n"
         "\tmedia [ jobid=<jobid> ujobid=<complete_name> pool=<pool-name> ] | clients |\n"
         "\tnextvol job=<job-name> | nextvolume ujobid=<complete_name> | copies jobid=<jobid>"), true, true },
   { NT_("messages"), messages_cmd, _("Display pending messages"),
     NT_(""), false, false },
   { NT_("memory"), memory_cmd, _("Print current memory usage"),
     NT_(""), true, false },
   { NT_("mount"), mount_cmd, _("Mount storage"),
     NT_("storage=<storage-name> slot=<num> drive=<num>\n"
         "\tjobid=<jobid> | job=<job-name> | ujobid=<complete_name>"), false, true },
   { NT_("move"), move_cmd, _("Move slots in an autochanger"),
     NT_("storage=<storage-name> srcslots=<slot-selection> dstslots=<slot-selection>"), true, true },
   { NT_("prune"), prune_cmd, _("Prune records from catalog"),
     NT_("files | jobs | jobtype=<jobtype> | pool=<pool-name> | client=<client-name> | volume=<volume-name> | directory=<directory> | recursive"), true, true },
   { NT_("purge"), purge_cmd, _("Purge records from catalog"),
     NT_("files jobs volume=<volume-name> [ action=<action> devicetype=<type> pool=<pool-name>\n"
         "\tallpools storage=<storage-name> drive=<num> ]"), true, true },
   { NT_("quit"), quit_cmd, _("Terminate Bconsole session"),
     NT_(""), false, false },
   { NT_("query"), query_cmd, _("Query catalog"),
     NT_(""), false, true },
   { NT_("restore"), restore_cmd, _("Restore files"),
     NT_("where=</path> client=<client-name> storage=<storage-name> bootstrap=<file>\n"
         "\trestorejob=<job-name> comment=<text> jobid=<jobid> fileset=<fileset-name>\n"
         "\treplace=<always|never|ifolder|ifnewer> pluginoptions=<plugin-options-string>\n"
         "\tregexwhere=<regex> restoreclient=<client-name> backupformat=<format>\n"
         "\tpool=<pool-name> file=<filename> directory=<directory> before=<date>\n"
         "\tstrip_prefix=<prefix> add_prefix=<prefix> add_suffix=<suffix>\n"
         "\tselect=<date> select before current copies done all"), false, true },
   { NT_("relabel"), relabel_cmd, _("Relabel a tape"),
     NT_("storage=<storage-name> oldvolume=<old-volume-name>\n"
         "\tvolume=<new-volume-name> pool=<pool-name> [ encrypt ]"), false, true },
   { NT_("release"), release_cmd, _("Release storage"),
     NT_("storage=<storage-name> [ drive=<num> ] [ alldrives ]"), false, true },
   { NT_("reload"), reload_cmd, _("Reload conf file"),
     NT_(""), true, true },
   { NT_("rerun"), rerun_cmd, _("Rerun a job"),
     NT_("jobid=<jobid> | since_jobid=<jobid> [ until_jobid=<jobid> ] | days=<nr_days> | hours=<nr_hours> | yes"), false, true },
   { NT_("resolve"), resolve_cmd, _("Resolve a hostname"),
     NT_("client=<client-name> | storage=<storage-name> <host-name>"), false, true },
   { NT_("run"), run_cmd, _("Run a job"),
     NT_("job=<job-name> client=<client-name> fileset=<fileset-name> level=<level>\n"
         "\tstorage=<storage-name> where=<directory-prefix> when=<universal-time-specification>\n"
         "\tpool=<pool-name> pluginoptions=<plugin-options-string> accurate=<yes|no> comment=<text>\n"
         "\tspooldata=<yes|no> priority=<number> jobid=<jobid> catalog=<catalog> migrationjob=<>\n"
         "\tbackupclient=<client-name> backupformat=<format> nextpool=<pool-name>\n"
         "\tsince=<universal-time-specification> verifyjob=<job-name> verifylist=<verify-list>\n"
         "\tmigrationjob=<complete_name> yes"), false, true },
   { NT_("status"), status_cmd, _("Report status"),
     NT_("all | dir=<dir-name> | director | scheduler | schedule=<schedule-name> | client=<client-name> |\n"
         "\tstorage=<storage-name> slots | days=<nr_days> | job=<job-name> | schedule=<schedule-name> |\n"
         "\tsubscriptions" ), true, true },
   { NT_("setbandwidth"), setbwlimit_cmd,  _("Sets bandwidth"),
     NT_("client=<client-name> | storage=<storage-name> | jobid=<jobid> |\n"
         "\tjob=<job-name> | ujobid=<unique-jobid> state=<job_state> | all\n"
         "\tlimit=<nn-kbs> yes"), true, true },
   { NT_("setdebug"), setdebug_cmd, _("Sets debug level"),
     NT_("level=<nn> trace=0/1 timestamp=0/1 client=<client-name> | dir | storage=<storage-name> | all"), true, true },
   { NT_("setip"), setip_cmd, _("Sets new client address -- if authorized"),
     NT_(""), false, true },
   { NT_("show"), show_cmd, _("Show resource records"),
     NT_("jobdefs=<job-defaults>| job=<job-name> | pool=<pool-name> | fileset=<fileset-name> |\n"
         "\tschedule=<schedule-name> | client=<client-name> | message=<message-resource-name> |\n"
         "\tprofile=<profile-name> | jobdefs | jobs | pools | filesets | schedules | clients |\n"
         "\tmessages | profiles | consoles | disabled | all"), true, true },
   { NT_("sqlquery"), sqlquery_cmd, _("Use SQL to query catalog"),
     NT_(""), false, true },
   { NT_("time"), time_cmd, _("Print current time"),
     NT_(""), true, false },
   { NT_("trace"), trace_cmd, _("Turn on/off trace to file"),
     NT_("on | off"), true, true },
   { NT_("unmount"), unmount_cmd, _("Unmount storage"),
     NT_("storage=<storage-name> [ drive=<num> ]\n"
         "\tjobid=<jobid> | job=<job-name> | ujobid=<complete_name>"), false, true },
   { NT_("umount"), unmount_cmd, _("Umount - for old-time Unix guys, see unmount"),
     NT_("storage=<storage-name> [ drive=<num> ]\n"
         "\tjobid=<jobid> | job=<job-name> | ujobid=<complete_name>"), false, true },
   { NT_("update"), update_cmd, _("Update volume, pool or stats"),
     NT_("stats\n"
         "\tpool=<pool-name>\n"
         "\tslots storage=<storage-name> scan\n"
         "\tvolume=<volume-name> volstatus=<status> volretention=<time-def>\n"
         "\tpool=<pool-name> recycle=<yes/no> slot=<number>\n"
         "\tinchanger=<yes/no>\n"
         "\tmaxvolbytes=<size> maxvolfiles=<nb> maxvoljobs=<nb>\n"
         "\tenabled=<yes/no> recyclepool=<pool-name> actiononpurge=<action>"), true, true },
   { NT_("use"), use_cmd, _("Use specific catalog"),
     NT_("catalog=<catalog>"), false, true },
   { NT_("var"), var_cmd, _("Does variable expansion"),
     NT_(""), false, true },
   { NT_("version"), version_cmd, _("Print Director version"),
     NT_(""), true, false },
   { NT_("wait"), wait_cmd, _("Wait until no jobs are running"),
     NT_("jobname=<name> | jobid=<jobid> | ujobid=<complete_name>"), false, false }
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
   for (i = 0; i < comsize; i++) { /* search for command */
      if (bstrncasecmp(ua->argk[0], commands[i].key, len)) {
         /*
          * Check if command permitted, but "quit" is always OK
          */
         if (!bstrcmp(ua->argk[0], NT_("quit")) &&
             !acl_access_ok(ua, Command_ACL, ua->argk[0], len, true)) {
            break;
         }

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

         if (ua->api) {
            user->signal(BNET_CMD_BEGIN);
         }
         ok = (*commands[i].func)(ua, ua->cmd);   /* go execute command */
         if (ua->api) {
            user->signal(ok ? BNET_CMD_OK : BNET_CMD_FAILED);
         }

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
   mr->MinBlocksize = pr->MinBlocksize;
   mr->MaxBlocksize = pr->MaxBlocksize;
   mr->Enabled = VOL_ENABLED;
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
   STORERES *store;
   int Slot = 0, InChanger = 0;

   ua->send_msg(_("You probably don't want to be using this command since it\n"
                  "creates database records without labeling the Volumes.\n"
                  "You probably want to use the \"label\" command.\n\n"));

   if (!open_client_db(ua)) {
      return 1;
   }

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

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
   if ((store = get_storage_resource(ua)) != NULL) {
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
      mr.Enabled = VOL_ENABLED;
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

   ua->automount = (bstrcasecmp(onoff, NT_("off"))) ? 0 : 1;
   return 1;
}

static inline int cancel_storage_daemon_job(UAContext *ua, const char *cmd)
{
   int i;
   STORERES *store;

   store = get_storage_resource(ua);
   if (store) {
      /*
       * See what JobId to cancel on the storage daemon.
       */
      i = find_arg_with_value(ua, NT_("jobid"));
      if (i >= 0) {
         if (!is_a_number(ua->argv[i])) {
            ua->warning_msg(_("JobId %s not a number\n"), ua->argv[i]);
         }

         cancel_storage_daemon_job(ua, store, ua->argv[i]);
      } else {
         ua->warning_msg(_("Missing jobid=JobId specification\n"));
      }
   }

   return 1;
}

static inline int cancel_jobs(UAContext *ua, const char *cmd)
{
   JCR *jcr;
   JobId_t *JobId;
   alist *selection;

   selection = select_jobs(ua, "cancel");
   if (!selection) {
      return 1;
   }

   /*
    * Loop over the different JobIds selected.
    */
   foreach_alist(JobId, selection) {
      if (!(jcr = get_jcr_by_id(*JobId))) {
         continue;
      }

      cancel_job(ua, jcr);
      free_jcr(jcr);
   }

   delete selection;

   return 1;
}

/*
 * Cancel a job
 */
static int cancel_cmd(UAContext *ua, const char *cmd)
{
   int i;

   /*
    * See if we need to explicitly cancel a storage daemon Job.
    */
   i = find_arg_with_value(ua, NT_("storage"));
   if (i >= 0) {
      return cancel_storage_daemon_job(ua, cmd);
   } else {
      return cancel_jobs(ua, cmd);
   }
}

/*
 * This is a common routine to create or update a
 *   Pool DB base record from a Pool Resource. We handle
 *   the setting of MaxVols and NumVols slightly differently
 *   depending on if we are creating the Pool or we are
 *   simply bringing it into agreement with the resource (update).
 *
 * Caution : RecyclePoolId isn't setup in this function.
 *           You can use set_pooldbr_recyclepoolid();
 */
void set_pooldbr_from_poolres(POOL_DBR *pr, POOLRES *pool, e_pool_op op)
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
   pr->ActionOnPurge = pool->action_on_purge;
   pr->MinBlocksize = pool->MinBlocksize;
   pr->MaxBlocksize = pool->MaxBlocksize;
   pr->Recycle = pool->Recycle;
   if (pool->label_format) {
      bstrncpy(pr->LabelFormat, pool->label_format, sizeof(pr->LabelFormat));
   } else {
      bstrncpy(pr->LabelFormat, "*", sizeof(pr->LabelFormat));    /* none */
   }
}

/* set/update Pool.RecyclePoolId and Pool.ScratchPoolId in Catalog */
int update_pool_references(JCR *jcr, B_DB *db, POOLRES *pool)
{
   POOL_DBR pr;

   if (!pool->RecyclePool && !pool->ScratchPool) {
      return 1;
   }

   memset(&pr, 0, sizeof(pr));
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
bool set_pooldbr_references(JCR *jcr, B_DB *db, POOL_DBR *pr, POOLRES *pool)
{
   POOL_DBR rpool;
   bool ret = true;

   if (pool->RecyclePool) {
      memset(&rpool, 0, sizeof(rpool));

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
      memset(&rpool, 0, sizeof(rpool));

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

int create_pool(JCR *jcr, B_DB *db, POOLRES *pool, e_pool_op op)
{
   POOL_DBR  pr;

   memset(&pr, 0, sizeof(pr));

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
   POOLRES *pool;

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

static inline int setbwlimit_filed(UAContext *ua, CLIENTRES *client,
                                   int64_t limit, char *Job)
{
   /*
    * Connect to File daemon
    */
   ua->jcr->res.client = client;
   ua->jcr->max_bandwidth = limit;

   /*
    * Try to connect for 15 seconds
    */
   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
                client->name(), client->address, client->FDport);

   if (!connect_to_file_daemon(ua->jcr, 1, 15, false, false)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return 1;
   }

   Dmsg0(120, "Connected to file daemon\n");
   if (!send_bwlimit_to_fd(ua->jcr, Job)) {
      ua->error_msg(_("Failed to set bandwidth limit on Client.\n"));
   } else {
      ua->info_msg(_("OK Limiting bandwidth to %lldkb/s %s\n"), limit / 1024, Job);
   }

   ua->jcr->file_bsock->signal(BNET_TERMINATE);
   ua->jcr->file_bsock->close();
   delete ua->jcr->file_bsock;
   ua->jcr->file_bsock = NULL;
   ua->jcr->res.client = NULL;
   ua->jcr->max_bandwidth = 0;

   return 1;
}

static inline int setbwlimit_stored(UAContext *ua, STORERES *store,
                                    int64_t limit, char *Job)
{
   /*
    * Check the storage daemon protocol.
    */
   switch (store->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      ua->error_msg(_("Storage selected is NDMP storage which cannot have a bandwidth limit\n"));
      return 1;
   default:
      break;
   }

   /*
    * Connect to Storage daemon
    */
   ua->jcr->res.wstore = store;
   ua->jcr->max_bandwidth = limit;

   /*
    * Try to connect for 15 seconds
    */
   ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
                store->name(), store->address, store->SDport);

   if (!connect_to_storage_daemon(ua->jcr, 1, 15, false)) {
      ua->error_msg(_("Failed to connect to Storage daemon.\n"));
      return 1;
   }

   Dmsg0(120, "Connected to Storage daemon\n");
   if (!send_bwlimit_to_fd(ua->jcr, Job)) {
      ua->error_msg(_("Failed to set bandwidth limit on Storage daemon.\n"));
   } else {
      ua->info_msg(_("OK Limiting bandwidth to %lldkb/s %s\n"), limit / 1024, Job);
   }

   ua->jcr->store_bsock->signal(BNET_TERMINATE);
   ua->jcr->store_bsock->close();
   delete ua->jcr->store_bsock;
   ua->jcr->store_bsock = NULL;
   ua->jcr->res.wstore = NULL;
   ua->jcr->max_bandwidth = 0;

   return 1;
}

static int setbwlimit_cmd(UAContext *ua, const char *cmd)
{
   int i;
   int64_t limit = -1;
   CLIENTRES *client = NULL;
   STORERES *store = NULL;
   char Job[MAX_NAME_LENGTH];
   const char *lst[] = {
      "job",
      "jobid",
      "ujobid",
      "all",
      "state",
      NULL
   };

   memset(Job, 0, sizeof(Job));
   i = find_arg_with_value(ua, NT_("limit"));
   if (i >= 0) {
      limit = ((int64_t)atoi(ua->argv[i]) * 1024);
   }

   if (limit < 0) {
      if (!get_pint(ua, _("Enter new bandwidth limit kb/s: "))) {
         return 1;
      }
      limit = ((int64_t)ua->pint32_val * 1024); /* kb/s */
   }

   if (find_arg_keyword(ua, lst) > 0) {
      JCR *jcr;
      JobId_t *JobId;
      alist *selection;

      selection = select_jobs(ua, "limit");
      if (!selection) {
         return 1;
      }

      /*
       * Loop over the different JobIds selected.
       */
      foreach_alist(JobId, selection) {
         if (!(jcr = get_jcr_by_id(*JobId))) {
            continue;
         }

         jcr->max_bandwidth = limit;
         bstrncpy(Job, jcr->Job, sizeof(Job));
         switch (jcr->getJobType()) {
         case JT_COPY:
         case JT_MIGRATE:
            store = jcr->res.rstore;
            break;
         default:
            client = jcr->res.client;
            break;
         }
         free_jcr(jcr);
      }

      delete selection;
   } else if (find_arg(ua, NT_("storage")) >= 0) {
      store = get_storage_resource(ua);
   } else {
      client = get_client_resource(ua);
   }

   if (client) {
      return setbwlimit_filed(ua, client, limit, Job);
   }

   if (store) {
      return setbwlimit_stored(ua, store, limit, Job);
   }

   return 1;
}

/*
 * Set a new address in a Client resource. We do this only
 * if the Console name is the same as the Client name
 * and the Console can access the client.
 */
static int setip_cmd(UAContext *ua, const char *cmd)
{
   CLIENTRES *client;
   char buf[1024];

   if (!ua->cons || !acl_access_ok(ua, Client_ACL, ua->cons->name(), true)) {
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

   sockaddr_to_ascii(&(ua->UA_sock->client_addr), buf, sizeof(buf));
   client->address = bstrdup(buf);
   ua->send_msg(_("Client \"%s\" address set to %s\n"), client->name(), client->address);

get_out:
   UnlockRes();
   return 1;
}

static void do_en_disable_cmd(UAContext *ua, bool setting)
{
   SCHEDRES *sched = NULL;
   CLIENTRES *client = NULL;
   JOBRES *job = NULL;
   int i;

   i = find_arg(ua, NT_("schedule"));
   if (i >= 0) {
      i = find_arg_with_value(ua, NT_("schedule"));
      if (i >= 0) {
         LockRes();
         sched = GetScheduleResWithName(ua->argv[i]);
         UnlockRes();
      } else {
         sched = select_enable_disable_schedule_resource(ua, setting);
         if (!sched) {
            return;
         }
      }

      if (!sched) {
         ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
         return;
      }
   } else {
      i = find_arg(ua, NT_("client"));
      if (i >= 0) {
         i = find_arg_with_value(ua, NT_("client"));
         if (i >= 0) {
            LockRes();
            client = GetClientResWithName(ua->argv[i]);
            UnlockRes();
         } else {
            client = select_enable_disable_client_resource(ua, setting);
            if (!client) {
               return;
            }
         }

         if (!client) {
            ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
            return;
         }
      } else {
         i = find_arg_with_value(ua, NT_("job"));
         if (i >= 0) {
            LockRes();
            job = GetJobResWithName(ua->argv[i]);
            UnlockRes();
         } else {
            job = select_enable_disable_job_resource(ua, setting);
            if (!job) {
               return;
            }
         }

         if (!job) {
            ua->error_msg(_("Job \"%s\" not found.\n"), ua->argv[i]);
            return;
         }
      }
   }

   if (sched) {
      if (!acl_access_ok(ua, Schedule_ACL, sched->name(), true)) {
         ua->error_msg(_("Unauthorized command from this console.\n"));
         return;
      }

      sched->enabled = setting;
      ua->send_msg(_("Schedule \"%s\" %sabled\n"), sched->name(), setting ? "en" : "dis");
   } else if (client) {
      if (!acl_access_ok(ua, Client_ACL, client->name(), true)) {
         ua->error_msg(_("Unauthorized command from this console.\n"));
         return;
      }

      client->enabled = setting;
      ua->send_msg(_("Client \"%s\" %sabled\n"), client->name(), setting ? "en" : "dis");
   } else if (job) {
      if (!acl_access_ok(ua, Job_ACL, job->name(), true)) {
         ua->error_msg(_("Unauthorized command from this console.\n"));
         return;
      }

      job->enabled = setting;
      ua->send_msg(_("Job \"%s\" %sabled\n"), job->name(), setting ? "en" : "dis");
   }
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

static void do_storage_setdebug(UAContext *ua, STORERES *store, int level,
                                int trace_flag, int timestamp_flag)
{
   BSOCK *sd;
   JCR *jcr = ua->jcr;
   USTORERES lstore;

   switch (store->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      return;
   default:
      break;
   }

   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(jcr, &lstore);

   /*
    * Try connecting for up to 15 seconds
    */
   ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
                store->name(), store->address, store->SDport);

   if (!connect_to_storage_daemon(jcr, 1, 15, false)) {
      ua->error_msg(_("Failed to connect to Storage daemon.\n"));
      return;
   }

   Dmsg0(120, _("Connected to storage daemon\n"));
   sd = jcr->store_bsock;
   sd->fsend("setdebug=%d trace=%d timestamp=%d\n", level, trace_flag, timestamp_flag);
   if (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }

   sd->signal(BNET_TERMINATE);
   sd->close();
   delete jcr->store_bsock;
   jcr->store_bsock = NULL;

   return;
}

/*
 * For the client, we have the following values that can be set :
 *
 * level = debug level
 * trace = send debug output to a file
 * hangup = how many records to send to SD before hanging up
 *          obviously this is most useful for testing restarting
 *          failed jobs.
 * timestamp = set debug msg timestamping
 */
static void do_client_setdebug(UAContext *ua, CLIENTRES *client, int level, int trace_flag,
                               int hangup_flag, int timestamp_flag)
{
   BSOCK *fd;

   switch (client->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      return;
   default:
      break;
   }

   /*
    * Connect to File daemon
    */
   ua->jcr->res.client = client;

   /*
    * Try to connect for 15 seconds
    */
   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
                client->name(), client->address, client->FDport);

   if (!connect_to_file_daemon(ua->jcr, 1, 15, false, false)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return;
   }

   Dmsg0(120, "Connected to file daemon\n");
   fd = ua->jcr->file_bsock;
   if (ua->jcr->FDVersion >= FD_VERSION_53) {
      fd->fsend("setdebug=%d trace=%d hangup=%d timestamp=%d\n", level, trace_flag, hangup_flag, timestamp_flag);
   } else {
      fd->fsend("setdebug=%d trace=%d hangup=%d\n", level, trace_flag, hangup_flag);
   }

   if (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }

   fd->signal(BNET_TERMINATE);
   fd->close();
   delete ua->jcr->file_bsock;
   ua->jcr->file_bsock = NULL;

   return;
}

static void do_director_setdebug(UAContext *ua, int level, int trace_flag, int timestamp_flag)
{
   POOL_MEM tracefilename(PM_FNAME);

   debug_level = level;
   set_trace(trace_flag);
   set_timestamp(timestamp_flag);
   Mmsg(tracefilename, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);
   ua->send_msg("level=%d trace=%d hangup=%d timestamp=%d tracefilename=%s\n", level, get_trace(), get_hangup(), get_timestamp(), tracefilename.c_str());
}

static void do_all_setdebug(UAContext *ua, int level, int trace_flag,
                            int hangup_flag, int timestamp_flag)
{
   STORERES *store, **unique_store;
   CLIENTRES *client, **unique_client;
   int i, j, found;

   /* Director */
   do_director_setdebug(ua, level, trace_flag, timestamp_flag);

   /* Count Storage items */
   LockRes();
   store = NULL;
   i = 0;
   foreach_res(store, R_STORAGE) {
      i++;
   }
   unique_store = (STORERES **) malloc(i * sizeof(STORERES));

   /*
    * Find Unique Storage address/port
    */
   store = (STORERES *)GetNextRes(R_STORAGE, NULL);
   i = 0;
   unique_store[i++] = store;
   while ((store = (STORERES *)GetNextRes(R_STORAGE, (RES *)store))) {
      found = 0;
      for (j = 0; j < i; j++) {
         if (bstrcmp(unique_store[j]->address, store->address) &&
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

   /*
    * Call each unique Storage daemon
    */
   for (j = 0;  j < i; j++) {
      do_storage_setdebug(ua, unique_store[j], level, trace_flag, timestamp_flag);
   }
   free(unique_store);

   /*
    * Count Client items
    */
   LockRes();
   client = NULL;
   i = 0;
   foreach_res(client, R_CLIENT) {
      i++;
   }
   unique_client = (CLIENTRES **) malloc(i * sizeof(CLIENTRES));

   /*
    * Find Unique Client address/port
    */
   client = (CLIENTRES *)GetNextRes(R_CLIENT, NULL);
   i = 0;
   unique_client[i++] = client;
   while ((client = (CLIENTRES *)GetNextRes(R_CLIENT, (RES *)client))) {
      found = 0;
      for (j = 0; j < i; j++) {
         if (bstrcmp(unique_client[j]->address, client->address) &&
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

   /*
    * Call each unique File daemon
    */
   for (j = 0; j < i; j++) {
      do_client_setdebug(ua, unique_client[j], level, trace_flag, hangup_flag, timestamp_flag);
   }
   free(unique_client);
}

/*
 * setdebug level=nn all trace=1/0 timestamp=1/0
 */
static int setdebug_cmd(UAContext *ua, const char *cmd)
{
   int i;
   int level;
   int trace_flag;
   int hangup_flag;
   int timestamp_flag;
   STORERES *store;
   CLIENTRES *client;

   Dmsg1(120, "setdebug:%s:\n", cmd);

   level = -1;
   i = find_arg_with_value(ua, NT_("level"));
   if (i >= 0) {
      level = atoi(ua->argv[i]);
   }
   if (level < 0) {
      if (!get_pint(ua, _("Enter new debug level: "))) {
         return 1;
      }
      level = ua->pint32_val;
   }

   /*
    * Look for trace flag. -1 => not change
    */
   i = find_arg_with_value(ua, NT_("trace"));
   if (i >= 0) {
      trace_flag = atoi(ua->argv[i]);
      if (trace_flag > 0) {
         trace_flag = 1;
      }
   } else {
      trace_flag = -1;
   }

   /*
    * Look for hangup (debug only) flag. -1 => not change
    */
   i = find_arg_with_value(ua, NT_("hangup"));
   if (i >= 0) {
      hangup_flag = atoi(ua->argv[i]);
   } else {
      hangup_flag = -1;
   }

   /*
    * Look for timestamp flag. -1 => not change
    */
   i = find_arg_with_value(ua, NT_("timestamp"));
   if (i >= 0) {
      timestamp_flag = atoi(ua->argv[i]);
      if (timestamp_flag > 0) {
         timestamp_flag = 1;
      }
   } else {
      timestamp_flag = -1;
   }

   /*
    * General debug?
    */
   for (i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], "all")) {
         do_all_setdebug(ua, level, trace_flag, hangup_flag, timestamp_flag);
         return 1;
      }
      if (bstrcasecmp(ua->argk[i], "dir") ||
          bstrcasecmp(ua->argk[i], "director")) {
         do_director_setdebug(ua, level, trace_flag, timestamp_flag);
         return 1;
      }
      if (bstrcasecmp(ua->argk[i], "client") ||
          bstrcasecmp(ua->argk[i], "fd")) {
         client = NULL;
         if (ua->argv[i]) {
            client = GetClientResWithName(ua->argv[i]);
            if (client) {
               do_client_setdebug(ua, client, level, trace_flag, hangup_flag, timestamp_flag);
               return 1;
            }
         }
         client = select_client_resource(ua);
         if (client) {
            do_client_setdebug(ua, client, level, trace_flag, hangup_flag, timestamp_flag);
            return 1;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("store")) ||
          bstrcasecmp(ua->argk[i], NT_("storage")) ||
          bstrcasecmp(ua->argk[i], NT_("sd"))) {
         store = NULL;
         if (ua->argv[i]) {
            store = GetStoreResWithName(ua->argv[i]);
            if (store) {
               do_storage_setdebug(ua, store, level, trace_flag, timestamp_flag);
               return 1;
            }
         }
         store = get_storage_resource(ua);
         if (store) {
            switch (store->Protocol) {
            case APT_NDMPV2:
            case APT_NDMPV3:
            case APT_NDMPV4:
               ua->warning_msg(_("Storage has non-native protocol.\n"));
               return 1;
            default:
               break;
            }

            do_storage_setdebug(ua, store, level, trace_flag, timestamp_flag);
            return 1;
         }
      }
   }

   /*
    * We didn't find an appropriate keyword above, so prompt the user.
    */
   start_prompt(ua, _("Available daemons are: \n"));
   add_prompt(ua, _("Director"));
   add_prompt(ua, _("Storage"));
   add_prompt(ua, _("Client"));
   add_prompt(ua, _("All"));

   switch(do_prompt(ua, "", _("Select daemon type to set debug level"), NULL, 0)) {
   case 0:                         /* Director */
      do_director_setdebug(ua, level, trace_flag, timestamp_flag);
      break;
   case 1:
      store = get_storage_resource(ua);
      if (store) {
         switch (store->Protocol) {
         case APT_NDMPV2:
         case APT_NDMPV3:
         case APT_NDMPV4:
            ua->warning_msg(_("Storage has non-native protocol.\n"));
            return 1;
         default:
            break;
         }
         do_storage_setdebug(ua, store, level, trace_flag, timestamp_flag);
      }
      break;
   case 2:
      client = select_client_resource(ua);
      if (client) {
         do_client_setdebug(ua, client, level, trace_flag, hangup_flag, timestamp_flag);
      }
      break;
   case 3:
      do_all_setdebug(ua, level, trace_flag, hangup_flag, timestamp_flag);
      break;
   default:
      break;
   }

   return 1;
}

/*
 * Resolve a hostname.
 */
static int resolve_cmd(UAContext *ua, const char *cmd)
{
   STORERES *storage = NULL;
   CLIENTRES *client = NULL;

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("client")) ||
          bstrcasecmp(ua->argk[i], NT_("fd"))) {
         if (ua->argv[i]) {
            client = GetClientResWithName(ua->argv[i]);
            if (!client) {
               ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }

            if (!acl_access_ok(ua, Client_ACL, client->name(), true)) {
               ua->error_msg(_("No authorization for Client \"%s\"\n"), client->name());
               return 1;
            }

            *ua->argk[i] = 0;         /* zap keyword already visited */
            continue;
         } else {
            ua->error_msg(_("Client name missing.\n"));
            return 1;
         }
      } else if (bstrcasecmp(ua->argk[i], NT_("storage")))  {
         if (ua->argv[i]) {
            storage = GetStoreResWithName(ua->argv[i]);
            if (!storage) {
               ua->error_msg(_("Storage \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }

            if (!acl_access_ok(ua, Storage_ACL, storage->name(), true)) {
               ua->error_msg(_("No authorization for Storage \"%s\"\n"), storage->name());
               return 1;
            }

            *ua->argk[i] = 0;         /* zap keyword already visited */
            continue;
         } else {
            ua->error_msg(_("Storage name missing.\n"));
            return 1;
         }
      }
   }

   if (client) {
      do_client_resolve(ua, client);
   }

   if (storage) {
      do_storage_resolve(ua, storage);
   }

   if (!client && !storage) {
      dlist *addr_list;
      const char *errstr;
      char addresses[2048];

      for (int i = 1; i < ua->argc; i++) {
         if (!*ua->argk[i]) {
            continue;
         }

         if ((addr_list = bnet_host2ipaddrs(ua->argk[i], 0, &errstr)) == NULL) {
            ua->error_msg(_("%s Failed to resolve %s\n"), my_name, ua->argk[i]);
            return 0;
         }
         ua->send_msg(_("%s resolves %s to %s\n"), my_name, ua->argk[i],
                      build_addresses_str(addr_list, addresses, sizeof(addresses), false));
         free_addresses(addr_list);
      }
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


   set_trace((bstrcasecmp(onoff, NT_("off"))) ? false : true);
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
   JOBRES *job = NULL;
   CLIENTRES *client = NULL;
   FILESETRES *fileset = NULL;
   int listing = 0;
   JCR *jcr = ua->jcr;
   int accurate=-1;

   jcr->setJobLevel(L_FULL);
   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("client")) ||
          bstrcasecmp(ua->argk[i], NT_("fd"))) {
         if (ua->argv[i]) {
            client = GetClientResWithName(ua->argv[i]);
            if (!client) {
               ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }
            if (!acl_access_ok(ua, Client_ACL, client->name(), true)) {
               ua->error_msg(_("No authorization for Client \"%s\"\n"), client->name());
               return 1;
            }
            continue;
         } else {
            ua->error_msg(_("Client name missing.\n"));
            return 1;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("job"))) {
         if (ua->argv[i]) {
            job = GetJobResWithName(ua->argv[i]);
            if (!job) {
               ua->error_msg(_("Job \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }
            if (!acl_access_ok(ua, Job_ACL, job->name(), true)) {
               ua->error_msg(_("No authorization for Job \"%s\"\n"), job->name());
               return 1;
            }
            continue;
         } else {
            ua->error_msg(_("Job name missing.\n"));
            return 1;
         }

      }

      if (bstrcasecmp(ua->argk[i], NT_("fileset"))) {
         if (ua->argv[i]) {
            fileset = GetFileSetResWithName(ua->argv[i]);
            if (!fileset) {
               ua->error_msg(_("Fileset \"%s\" not found.\n"), ua->argv[i]);
               return 1;
            }
            if (!acl_access_ok(ua, FileSet_ACL, fileset->name(), true)) {
               ua->error_msg(_("No authorization for FileSet \"%s\"\n"), fileset->name());
               return 1;
            }
            continue;
         } else {
            ua->error_msg(_("Fileset name missing.\n"));
            return 1;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("listing"))) {
         listing = 1;
         continue;
      }

      if (bstrcasecmp(ua->argk[i], NT_("level"))) {
         if (ua->argv[i]) {
            if (!get_level_from_name(jcr, ua->argv[i])) {
               ua->error_msg(_("Level \"%s\" not valid.\n"), ua->argv[i]);
            }
            continue;
         } else {
            ua->error_msg(_("Level value missing.\n"));
            return 1;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("accurate"))) {
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

      if (!acl_access_ok(ua, Job_ACL, job->name(), true)) {
         ua->error_msg(_("No authorization for Job \"%s\"\n"), job->name());
         return 1;
      }
   }

   switch (job->JobType) {
   case JT_BACKUP:
      break;
   default:
      ua->error_msg(_("Wrong job specified of type %s.\n"), job_type_to_str(job->JobType));
      return 1;
   }

   if (!client) {
      client = job->client;
   }

   if (!fileset) {
      fileset = job->fileset;
   }

   if (!client) {
      ua->error_msg(_("No client specified or selected.\n"));
      return 1;
   }

   if (!fileset) {
      ua->error_msg(_("No fileset specified or selected.\n"));
      return 1;
   }

   jcr->res.client = client;
   jcr->res.fileset = fileset;
   close_db(ua);

   if (job->pool->catalog) {
      ua->catalog = job->pool->catalog;
   } else {
      ua->catalog = client->catalog;
   }

   if (!open_db(ua)) {
      return 1;
   }

   jcr->res.job = job;
   jcr->setJobType(JT_BACKUP);
   jcr->start_time = time(NULL);
   init_jcr_job_record(jcr);

   if (!get_or_create_client_record(jcr)) {
      return 1;
   }

   if (!get_or_create_fileset_record(jcr)) {
      return 1;
   }

   get_level_since_time(jcr);

   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
               jcr->res.client->name(), jcr->res.client->address, jcr->res.client->FDport);
   if (!connect_to_file_daemon(jcr, 1, 15, false, true)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return 1;
   }

   /*
    * The level string change if accurate mode is enabled
    */
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
    * If the job is in accurate mode, we send the list of all files to FD.
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
      delete jcr->file_bsock;
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

   bstrftime(sdt, sizeof(sdt), ttime, "%a %d-%b-%Y %H:%M:%S");
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
 * delete pool=<pool-name>
 * delete volume pool=<pool-name> volume=<name>
 * delete jobid=<jobid>
 */
static int delete_cmd(UAContext *ua, const char *cmd)
{
   static const char *keywords[] = {
      NT_("volume"),
      NT_("pool"),
      NT_("jobid"),
      NULL
   };

   if (!open_client_db(ua, true)) {
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

   ua->warning_msg(_("In general it is not a good idea to delete either a\n"
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

      if (j2 > j1) {
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

   memset(&mr, 0, sizeof(mr));
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
   if (!bstrcmp(mr.VolStatus, "Purged")) {
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

static void do_mount_cmd(UAContext *ua, const char *cmd)
{
   USTORERES store;
   int nr_drives, i;
   int drive = -1;
   int slot = -1;
   bool do_alldrives = false;

   if ((bstrcmp(cmd, "release") || bstrcmp(cmd, "unmount")) &&
      find_arg(ua, "alldrives") >= 0) {
      do_alldrives = true;
   }

   if (!open_client_db(ua)) {
      return;
   }
   Dmsg2(120, "%s: %s\n", cmd, ua->UA_sock->msg);

   store.store = get_storage_resource(ua, true, false);
   if (!store.store) {
      return;
   }

   switch (store.store->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      ua->warning_msg(_("Storage has non-native protocol.\n"));
      return;
   default:
      break;
   }

   pm_strcpy(store.store_source, _("unknown source"));
   set_wstorage(ua->jcr, &store);
   if (!do_alldrives) {
      drive = get_storage_drive(ua, store.store);
      if (drive == -1) {
	 return;
      }
   }
   if (bstrcmp(cmd, "mount")) {
      slot = get_storage_slot(ua, store.store);
   }

   Dmsg3(120, "Found storage, MediaType=%s DevName=%s drive=%d\n",
         store.store->media_type, store.store->dev_name(), drive);

   if (!do_alldrives) {
      do_autochanger_volume_operation(ua, store.store, cmd, drive, slot);
   } else {
      nr_drives = get_num_drives_from_SD(ua);
      for (i = 0; i < nr_drives; i++) {
         do_autochanger_volume_operation(ua, store.store, cmd, i, slot);
      }
   }
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
   CATRES *oldcatalog, *catalog;

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
      if (bstrcasecmp(ua->argk[i], "jobid")) {
         if (!ua->argv[i]) {
            break;
         }
         jobid = str_to_int64(ua->argv[i]);
         break;
      } else if (bstrcasecmp(ua->argk[i], "jobname") ||
                 bstrcasecmp(ua->argk[i], "job")) {
         if (!ua->argv[i]) {
            break;
         }
         jcr=get_jcr_by_partial_name(ua->argv[i]) ;
         if (jcr) {
            jobid = jcr->JobId ;
            free_jcr(jcr);
         }
         break;
      } else if (bstrcasecmp(ua->argk[i], "ujobid")) {
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
      } else if (bstrcasecmp(ua->argk[i], "mount")) {
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
   for (i = 0; i < comsize; i++) {
      if (ua->argc == 2) {
         if (bstrcasecmp(ua->argk[1], commands[i].key)) {
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

/*
 * Output the usage string as a machine parseable string.
 * e.g. remove newlines and replace tabs with a single space.
 */
static inline void usage_to_machine(UAContext *ua, struct cmdstruct *cmd)
{
   char *p;
   POOL_MEM usage(PM_MESSAGE);

   pm_strcpy(usage, cmd->usage);
   p = usage.c_str();
   while (*p) {
      switch (*p) {
      case '\n':
         /*
          * Copy the rest of the string over the unwanted character.
          * Use bstrinlinecpy as we are doing an inline copy which
          * isn't officially supported by (b)strcpy.
          */
         bstrinlinecpy(p, p + 1);
         continue;
      case '\t':
         *p = ' ';
         break;
      default:
         break;
      }
      p++;
   }
   ua->send_msg("%s\n", usage.c_str());
}

int qhelp_cmd(UAContext *ua, const char *cmd)
{
   int i, j;

   /*
    * Want to display only commands
    */
   j = find_arg(ua, NT_("all"));
   if (j >= 0) {
      for (i = 0; i < comsize; i++) {
         ua->send_msg("%s\n", commands[i].key);
      }
      return 1;
   }

   /*
    * Want to display a specific help section
    */
   j = find_arg_with_value(ua, NT_("item"));
   if (j >= 0 && ua->argk[j]) {
      for (i = 0; i < comsize; i++) {
         if (bstrcmp(commands[i].key, ua->argv[j])) {
            usage_to_machine(ua, &commands[i]);
            break;
         }
      }
      return 1;
   }

   /*
    * Want to display everything
    */
   for (i = 0; i < comsize; i++) {
      ua->send_msg("%s %s -- ", commands[i].key, commands[i].help);
      usage_to_machine(ua, &commands[i]);
   }
   return 1;
}

#if 1
static int version_cmd(UAContext *ua, const char *cmd)
{
   ua->send_msg(_("%s Version: %s (%s) %s %s %s %s\n"), my_name, VERSION, BDATE,
                HOST_OS, DISTNAME, DISTVER, NPRTB(me->verid));
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
 * This call explicitly checks for a catalog=catalog-name and
 *  if given, opens that catalog.  It also checks for
 *  client=client-name and if found, opens the catalog
 *  corresponding to that client. If we still don't
 *  have a catalog, look for a Job keyword and get the
 *  catalog from its client record.
 */
bool open_client_db(UAContext *ua, bool use_private)
{
   int i;
   CATRES *catalog;
   CLIENTRES *client;
   JOBRES *job;

   /* Try for catalog keyword */
   i = find_arg_with_value(ua, NT_("catalog"));
   if (i >= 0) {
      if (!acl_access_ok(ua, Catalog_ACL, ua->argv[i], true)) {
         ua->error_msg(_("No authorization for Catalog \"%s\"\n"), ua->argv[i]);
         return false;
      }
      catalog = GetCatalogResWithName(ua->argv[i]);
      if (catalog) {
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         ua->catalog = catalog;
         return open_db(ua, use_private);
      }
   }

   /* Try for client keyword */
   i = find_arg_with_value(ua, NT_("client"));
   if (i >= 0) {
      if (!acl_access_ok(ua, Client_ACL, ua->argv[i], true)) {
         ua->error_msg(_("No authorization for Client \"%s\"\n"), ua->argv[i]);
         return false;
      }
      client = GetClientResWithName(ua->argv[i]);
      if (client) {
         catalog = client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         if (!acl_access_ok(ua, Catalog_ACL, catalog->name(), true)) {
            ua->error_msg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return open_db(ua, use_private);
      }
   }

   /* Try for Job keyword */
   i = find_arg_with_value(ua, NT_("job"));
   if (i >= 0) {
      if (!acl_access_ok(ua, Job_ACL, ua->argv[i], true)) {
         ua->error_msg(_("No authorization for Job \"%s\"\n"), ua->argv[i]);
         return false;
      }
      job = GetJobResWithName(ua->argv[i]);
      if (job && job->client) {
         catalog = job->client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         if (!acl_access_ok(ua, Catalog_ACL, catalog->name(), true)) {
            ua->error_msg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return open_db(ua, use_private);
      }
   }

   return open_db(ua, use_private);
}

/*
 * Open the catalog database.
 */
bool open_db(UAContext *ua, bool use_private)
{
   bool mult_db_conn;

   /*
    * See if we need to do any work at all.
    * Point the current used db e.g. ua->db to the correct database connection.
    */
   if (use_private) {
      if (ua->private_db) {
         ua->db = ua->private_db;
         return true;
      }
   } else if (ua->shared_db) {
      ua->db = ua->shared_db;
      return true;
   }

   /*
    * Select the right catalog to use.
    */
   if (!ua->catalog) {
      ua->catalog = get_catalog_resource(ua);
      if (!ua->catalog) {
         ua->error_msg( _("Could not find a Catalog resource\n"));
         return false;
      }
   }

   /*
    * Some modules like bvfs need their own private catalog connection
    */
   mult_db_conn = ua->catalog->mult_db_connections;
   if (use_private) {
      mult_db_conn = true;
   }

   ua->jcr->res.catalog = ua->catalog;
   Dmsg0(100, "UA Open database\n");
   ua->db = db_sql_get_pooled_connection(ua->jcr,
                                         ua->catalog->db_driver,
                                         ua->catalog->db_name,
                                         ua->catalog->db_user,
                                         ua->catalog->db_password.value,
                                         ua->catalog->db_address,
                                         ua->catalog->db_port,
                                         ua->catalog->db_socket,
                                         mult_db_conn,
                                         ua->catalog->disable_batch_insert,
                                         use_private);
   if (ua->db == NULL) {
      ua->error_msg(_("Could not open catalog database \"%s\".\n"), ua->catalog->db_name);
      return false;
   }
   ua->jcr->db = ua->db;

   /*
    * Save the new database connection under the right label e.g. shared or private.
    */
   if (use_private) {
      ua->private_db = ua->db;
   } else {
      ua->shared_db = ua->db;
   }

   if (!ua->api) {
      ua->send_msg(_("Using Catalog \"%s\"\n"), ua->catalog->name());
   }

   Dmsg1(150, "DB %s opened\n", ua->catalog->db_name);
   return true;
}

void close_db(UAContext *ua)
{
   if (ua->jcr) {
      ua->jcr->db = NULL;
   }

   if (ua->shared_db) {
      db_sql_close_pooled_connection(ua->jcr, ua->shared_db);
      ua->shared_db = NULL;
   }

   if (ua->private_db) {
      db_sql_close_pooled_connection(ua->jcr, ua->private_db);
      ua->private_db = NULL;
   }
}
