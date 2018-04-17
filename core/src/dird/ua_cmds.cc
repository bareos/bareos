/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * User Agent Commands
 */

#include "bareos.h"
#include "dird.h"
#include "dird/backup.h"
#include "dird/expand.h"
#include "dird/fd_cmds.h"
#include "dird/job.h"
#include "dird/next_vol.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "dird/ua_db.h"
#include "dird/ua_impexp.h"
#include "dird/ua_input.h"
#include "dird/ua_label.h"
#include "dird/ua_select.h"
#include "dird/ua_status.h"
#include "dird/ua_purge.h"
#include "dird/ua_run.h"

/* Imported subroutines */

/* Imported variables */

/*
 * Imported functions
 */
/* dird.c */
extern bool do_reload_config();

/* ua_cmds.c */
extern bool autodisplay_cmd(UaContext *ua, const char *cmd);
extern bool configure_cmd(UaContext *ua, const char *cmd);
extern bool gui_cmd(UaContext *ua, const char *cmd);
extern bool label_cmd(UaContext *ua, const char *cmd);
extern bool list_cmd(UaContext *ua, const char *cmd);
extern bool llist_cmd(UaContext *ua, const char *cmd);
extern bool messages_cmd(UaContext *ua, const char *cmd);
extern bool prune_cmd(UaContext *ua, const char *cmd);
extern bool purge_cmd(UaContext *ua, const char *cmd);
extern bool query_cmd(UaContext *ua, const char *cmd);
extern bool relabel_cmd(UaContext *ua, const char *cmd);
extern bool restore_cmd(UaContext *ua, const char *cmd);
extern bool show_cmd(UaContext *ua, const char *cmd);
extern bool sqlquery_cmd(UaContext *ua, const char *cmd);
extern bool status_cmd(UaContext *ua, const char *cmd);
extern bool update_cmd(UaContext *ua, const char *cmd);

/* ua_dotcmds.c */
extern bool dot_catalogs_cmd(UaContext *ua, const char *cmd);
extern bool dot_admin_cmds(UaContext *ua, const char *cmd);
extern bool dot_jobdefs_cmd(UaContext *ua, const char *cmd);
extern bool dot_jobs_cmd(UaContext *ua, const char *cmd);
extern bool dot_jobstatus_cmd(UaContext *ua, const char *cmd);
extern bool dot_filesets_cmd(UaContext *ua, const char *cmd);
extern bool dot_clients_cmd(UaContext *ua, const char *cmd);
extern bool dot_consoles_cmd(UaContext *ua, const char *cmd);
extern bool dot_msgs_cmd(UaContext *ua, const char *cmd);
extern bool dot_pools_cmd(UaContext *ua, const char *cmd);
extern bool dot_schedule_cmd(UaContext *ua, const char *cmd);
extern bool dot_storage_cmd(UaContext *ua, const char *cmd);
extern bool dot_defaults_cmd(UaContext *ua, const char *cmd);
extern bool dot_types_cmd(UaContext *ua, const char *cmd);
extern bool dot_levels_cmd(UaContext *ua, const char *cmd);
extern bool dot_getmsgs_cmd(UaContext *ua, const char *cmd);
extern bool dot_volstatus_cmd(UaContext *ua, const char *cmd);
extern bool dot_mediatypes_cmd(UaContext *ua, const char *cmd);
extern bool dot_locations_cmd(UaContext *ua, const char *cmd);
extern bool dot_media_cmd(UaContext *ua, const char *cmd);
extern bool dot_profiles_cmd(UaContext *ua, const char *cmd);
extern bool dot_aop_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_lsdirs_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_lsfiles_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_update_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_get_jobids_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_versions_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_restore_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_cleanup_cmd(UaContext *ua, const char *cmd);
extern bool dot_bvfs_clear_cache_cmd(UaContext *ua, const char *cmd);
extern bool dot_api_cmd(UaContext *ua, const char *cmd);
extern bool dot_sql_cmd(UaContext *ua, const char *cmd);
extern bool dot_authorized_cmd(UaContext *ua, const char *cmd);

/* ua_status.c */
extern bool dot_status_cmd(UaContext *ua, const char *cmd);

/* Forward referenced functions */
static bool add_cmd(UaContext *ua, const char *cmd);
static bool automount_cmd(UaContext *ua, const char *cmd);
static bool cancel_cmd(UaContext *ua, const char *cmd);
static bool create_cmd(UaContext *ua, const char *cmd);
static bool delete_cmd(UaContext *ua, const char *cmd);
static bool disable_cmd(UaContext *ua, const char *cmd);
static bool enable_cmd(UaContext *ua, const char *cmd);
static bool estimate_cmd(UaContext *ua, const char *cmd);
static bool help_cmd(UaContext *ua, const char *cmd);
static bool dot_help_cmd(UaContext *ua, const char *cmd);
static bool memory_cmd(UaContext *ua, const char *cmd);
static bool mount_cmd(UaContext *ua, const char *cmd);
static bool noop_cmd(UaContext *ua, const char *cmd);
static bool release_cmd(UaContext *ua, const char *cmd);
static bool reload_cmd(UaContext *ua, const char *cmd);
static bool resolve_cmd(UaContext *ua, const char *cmd);
static bool setdebug_cmd(UaContext *ua, const char *cmd);
static bool setbwlimit_cmd(UaContext *ua, const char *cmd);
static bool setip_cmd(UaContext *ua, const char *cmd);
static bool time_cmd(UaContext *ua, const char *cmd);
static bool trace_cmd(UaContext *ua, const char *cmd);
static bool truncate_cmd(UaContext *ua, const char *cmd);
static bool unmount_cmd(UaContext *ua, const char *cmd);
static bool use_cmd(UaContext *ua, const char *cmd);
static bool var_cmd(UaContext *ua, const char *cmd);
static bool version_cmd(UaContext *ua, const char *cmd);
static bool wait_cmd(UaContext *ua, const char *cmd);

static void do_job_delete(UaContext *ua, JobId_t JobId);
static bool delete_job_id_range(UaContext *ua, char *tok);
static bool delete_volume(UaContext *ua);
static bool delete_pool(UaContext *ua);
static void delete_job(UaContext *ua);
static bool do_truncate(UaContext *ua, MediaDbRecord &mr);

bool quit_cmd(UaContext *ua, const char *cmd);

/**
 * Not all in alphabetical order.
 * New commands are added after existing commands with similar letters
 * to prevent breakage of existing user scripts.
 */
static struct ua_cmdstruct commands[] = {
   { NT_("."), noop_cmd, _("no op"),
     NULL, true, false },
   { NT_(".actiononpurge"), dot_aop_cmd, _("List possible actions on purge"),
     NULL, true, false },
   { NT_(".api"), dot_api_cmd, _("Switch between different api modes"),
     NT_("[ 0 | 1 | 2 | off | on | json ] [compact=<yes|no>]"), false, false },
   { NT_(".authorized"), dot_authorized_cmd, _("Check for authorization"),
     NT_("job=<job-name> | client=<client-name> | storage=<storage-name | \n"
         "schedule=<schedule-name> | pool=<pool-name> | cmd=<command> | \n"
         "fileset=<fileset-name> | catalog=<catalog>"), false, false },
   { NT_(".catalogs"), dot_catalogs_cmd, _("List all catalog resources"),
     NULL, false, false },
   { NT_(".clients"), dot_clients_cmd, _("List all client resources"),
     NT_("[enabled | disabled]"), true, false },
   { NT_(".consoles"), dot_consoles_cmd, _("List all console resources"),
     NULL, true, false },
   { NT_(".defaults"), dot_defaults_cmd, _("Get default settings"),
     NT_("job=<job-name> | client=<client-name> | storage=<storage-name | pool=<pool-name>"), false, false },
#ifdef DEVELOPER
   { NT_(".die"), dot_admin_cmds, _("Generate Segmentation Fault"),
     NT_("[dir | director] [client=<client>] [storage=<storage>]"), false, true },
   { NT_(".dump"), dot_admin_cmds, _("Dump memory statistics"),
     NT_("[dir | director] [client=<client>] [storage=<storage>]"), false, true },
   { NT_(".memcheck"), dot_admin_cmds, _("Checks for internal memory leaks"),
     NT_("[dir | director] [client=<client>] [storage=<storage>]"), false, true },
   { NT_(".exit"), dot_admin_cmds, _("Close connection"),
     NT_("[dir | director] [client=<client>] [storage=<storage>]"), false, true },
#endif
   { NT_(".filesets"), dot_filesets_cmd, _("List all filesets"),
     NULL, false, false },
   { NT_(".help"), dot_help_cmd, _("Print parsable information about a command"),
     NT_("[ all | item=cmd ]"), false, false },
   { NT_(".jobdefs"), dot_jobdefs_cmd, _("List all job defaults resources"),
     NULL, true, false },
   { NT_(".jobs"), dot_jobs_cmd, _("List all job resources"),
     NT_("[type=<jobtype>] | [enabled | disabled]"), true, false },
   { NT_(".jobstatus"), dot_jobstatus_cmd, _("List jobstatus information"),
     NT_("[ =<jobstatus> ]"), true, false },
   { NT_(".levels"), dot_levels_cmd, _("List all backup levels"),
     NULL, false, false },
   { NT_(".locations"), dot_locations_cmd, NULL, NULL, true, false },
   { NT_(".messages"), dot_getmsgs_cmd, _("Display pending messages"),
     NULL, false, false },
   { NT_(".media"), dot_media_cmd, _("List all medias"),
     NULL, true, false },
   { NT_(".mediatypes"), dot_mediatypes_cmd, _("List all media types"),
     NULL, true, false },
   { NT_(".msgs"), dot_msgs_cmd, _("List all message resources"),
     NULL, false, false },
   { NT_(".pools"), dot_pools_cmd, _("List all pool resources"),
     NT_("type=<pooltype>"), true, false },
   { NT_(".profiles"), dot_profiles_cmd, _("List all profile resources"),
     NULL, true, false },
   { NT_(".quit"), quit_cmd, _("Close connection"),
     NULL, false, false },
   { NT_(".sql"), dot_sql_cmd, _("Send an arbitrary SQL command"),
     NT_("query=<sqlquery>"), false, true },
   { NT_(".schedule"), dot_schedule_cmd, _("List all schedule resources"),
     NT_("[enabled | disabled]"), false, false },
   { NT_(".status"), dot_status_cmd, _("Report status"),
     NT_("dir ( current | last | header | scheduled | running | terminated ) |\n"
         "storage=<storage> [ header | waitreservation | devices | volumes | spooling | running | terminated ] |\n"
         "client=<client> [ header | terminated | running ]"),
     false, true },
   { NT_(".storages"), dot_storage_cmd, _("List all storage resources"),
     NT_("[enabled | disabled]"), true, false },
   { NT_(".types"), dot_types_cmd, _("List all job types"),
     NULL, false, false },
   { NT_(".volstatus"), dot_volstatus_cmd, _("List all volume status"),
     NULL, true, false },
   { NT_(".bvfs_lsdirs"), dot_bvfs_lsdirs_cmd, _("List directories using BVFS"),
     NT_("jobid=<jobid> path=<path> | pathid=<pathid> [limit=<limit>] [offset=<offset>]"), true, true },
   { NT_(".bvfs_lsfiles"),dot_bvfs_lsfiles_cmd, _("List files using BVFS"),
     NT_("jobid=<jobid> path=<path> | pathid=<pathid> [limit=<limit>] [offset=<offset>]"), true, true },
   { NT_(".bvfs_update"), dot_bvfs_update_cmd, _("Update BVFS cache"),
     NT_("[jobid=<jobid>]"), true, true },
   { NT_(".bvfs_get_jobids"), dot_bvfs_get_jobids_cmd, _("Get jobids required for a restore"),
     NT_("jobid=<jobid> | ujobid=<unique-jobid> [all]"), true, true },
   { NT_(".bvfs_versions"), dot_bvfs_versions_cmd, _("List versions of a file"),
     NT_("jobid=0 client=<client-name> pathid=<path-id> filename=<file-name> [copies] [versions]"), true, true },
   { NT_(".bvfs_restore"), dot_bvfs_restore_cmd, _("Mark BVFS files/directories for restore. Stored in handle."),
     NT_("path=<handle> jobid=<jobid> [fileid=<file-id>] [dirid=<dirid>] [hardlink=<hardlink>]"), true, true },
   { NT_(".bvfs_cleanup"), dot_bvfs_cleanup_cmd, _("Cleanup BVFS cache for a certain handle"),
     NT_("path=<handle>"), true, true },
   { NT_(".bvfs_clear_cache"), dot_bvfs_clear_cache_cmd, _("Clear BVFS cache"),
     NT_("yes"), false, true },
   { NT_("add"), add_cmd, _("Add media to a pool"),
     NT_("pool=<pool-name> storage=<storage-name> jobid=<jobid>"), false, true },
   { NT_("autodisplay"), autodisplay_cmd,_("Autodisplay console messages"),
     NT_("on | off"), false, false },
   { NT_("automount"), automount_cmd, _("Automount after label"),
     NT_("on | off"), false, true },
   { NT_("cancel"), cancel_cmd, _("Cancel a job"),
     NT_("storage=<storage-name> | jobid=<jobid> | job=<job-name> | ujobid=<unique-jobid> | state=<job_state> | all yes"), false, true },
   { NT_("configure"), configure_cmd, _("Configure director resources"),
     NT_(get_configure_usage_string()), false, true },
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
   { NT_("gui"), gui_cmd, _("Switch between interactive (gui off) and non-interactive (gui on) mode"),
     NT_("on | off"), false, false },
   { NT_("help"), help_cmd, _("Print help on specific command"),
     NT_("add autodisplay automount cancel configure create delete disable\n"
         "\tenable estimate exit gui label list llist\n"
         "\tmessages memory mount prune purge quit query\n"
         "\trestore relabel release reload run status\n"
         "\tsetbandwidth setdebug setip show sqlquery time trace truncate unmount\n"
         "\tumount update use var version wait"), false, false },
   { NT_("import"), import_cmd, _("Import volumes from import/export slots to normal slots"),
     NT_("storage=<storage-name> [ srcslots=<slot-selection> dstslots=<slot-selection> volume=<volume-name> scan ]"), true, true },
   { NT_("label"), label_cmd, _("Label a tape"),
     NT_("storage=<storage-name> volume=<volume-name> pool=<pool-name> slot=<slot> [ drive = <drivenum>] [ barcodes ] [ encrypt ] [ yes ]"), false, true },
   { NT_("list"), list_cmd, _("List objects from catalog"),
     NT_("basefiles jobid=<jobid> | basefiles ujobid=<complete_name> |\n"
         "backups client=<client-name> [fileset=<fileset-name>] [jobstatus=<status>] [level=<level>] [order=<asc|desc>] [limit=<number>] |\n"
         "clients | copies jobid=<jobid> |\n"
         "files jobid=<jobid> | files ujobid=<complete_name> |\n"
         "filesets |\n"
         "fileset [ jobid=<jobid> ] | fileset [ ujobid=<complete_name> ] |\n"
         "fileset [ filesetid=<filesetid> ] | fileset [ jobid=<jobid> ] |\n"
         "jobs [job=<job-name>] [client=<client-name>] [jobstatus=<status>] [joblevel=<joblevel>] [volume=<volumename>] [days=<number>] [hours=<number>] [last] [count] |\n"
         "job=<job-name> [client=<client-name>] [jobstatus=<status>] [volume=<volumename>] [days=<number>] [hours=<number>] |\n"
         "jobid=<jobid> | ujobid=<complete_name> |\n"
         "joblog jobid=<jobid> | joblog ujobid=<complete_name> |\n"
         "jobmedia jobid=<jobid> | jobmedia ujobid=<complete_name> |\n"
         "jobtotals |\n"
         "jobstatistics jobid=<jobid> |\n"
         "log [ limit=<number> [ offset=<number> ] ] [reverse]|\n"
         "media [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | all ] |\n"
         "media=<media-name> |\n"
         "nextvol job=<job-name> | nextvolume ujobid=<complete_name> |\n"
         "pools |\n"
         "pool=<pool-name> |\n"
         "storages |\n"
         "volumes [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | all ] [count] |\n"
         "volume=<volume-name> |\n"
         "[current] | [enabled | disabled] |\n"
         "[limit=<number> [offset=<number>]]"), true, true },
   { NT_("llist"), llist_cmd, _("Full or long list like list command"),
     NT_("basefiles jobid=<jobid> | basefiles ujobid=<complete_name> |\n"
         "backups client=<client-name> [fileset=<fileset-name>] [jobstatus=<status>] [level=<level>] [order=<asc|desc>] [limit=<number>] [days=<number>] [hours=<number>]|\n"
         "clients | copies jobid=<jobid> |\n"
         "files jobid=<jobid> | files ujobid=<complete_name> |\n"
         "filesets |\n"
         "fileset jobid=<jobid> | fileset ujobid=<complete_name> |\n"
         "fileset [ filesetid=<filesetid> ] | fileset [ jobid=<jobid> ] |\n"
         "jobs [job=<job-name>] [client=<client-name>] [jobstatus=<status>] [volume=<volumename>] [days=<number>] [hours=<number>] [last] [count] |\n"
         "job=<job-name> [client=<client-name>] [jobstatus=<status>] [joblevel=<joblevel>] [volume=<volumename>] [days=<number>] [hours=<number>] |\n"
         "jobid=<jobid> | ujobid=<complete_name> |\n"
         "joblog jobid=<jobid> [count] | joblog ujobid=<complete_name> [count] |\n"
         "jobmedia jobid=<jobid> | jobmedia ujobid=<complete_name> |\n"
         "jobtotals |\n"
         "media [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | all ] |\n"
         "media=<media-name> |\n"
         "nextvol job=<job-name> | nextvolume ujobid=<complete_name> |\n"
         "pools |\n"
         "pool=<pool-name> |\n"
         "volumes [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | all ] |\n"
         "volume=<volume-name> |\n"
         "[ current ] | [ enabled ] | [disabled] |\n"
         "[ limit=<num> [ offset=<number> ] ]"), true, true },
   { NT_("messages"), messages_cmd, _("Display pending messages"),
     NT_(""), false, false },
   { NT_("memory"), memory_cmd, _("Print current memory usage"),
     NT_(""), true, false },
   { NT_("mount"), mount_cmd, _("Mount storage"),
     NT_("storage=<storage-name> slot=<num> drive=<drivenum>\n"
         "\tjobid=<jobid> | job=<job-name> | ujobid=<complete_name>"), false, true },
   { NT_("move"), move_cmd, _("Move slots in an autochanger"),
     NT_("storage=<storage-name> srcslots=<slot-selection> dstslots=<slot-selection>"), true, true },
   { NT_("prune"), prune_cmd, _("Prune records from catalog"),
     NT_("files [client=<client>] [pool=<pool>] [yes] |\n"
         "jobs [client=<client>] [pool=<pool>] [jobtype=<jobtype>] [yes] |\n"
         "volume [=volume] [pool=<pool>] [yes] |\n"
         "stats [yes] |\n"
         "directory [=directory] [client=<client>] [recursive] [yes]"), true, true },
   { NT_("purge"), purge_cmd, _("Purge records from catalog"),
     NT_("[files [job=<job> | jobid=<jobid> | client=<client> | volume=<volume>]] |\n"
         "[jobs [client=<client> | volume=<volume>]] |\n"
         "[volume[=<volume>] [storage=<storage>] [pool=<pool> | allpools] [devicetype=<type>] [drive=<drivenum>] [action=<action>]] |\n"
         "[quota [client=<client>]]"), true, true },
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
     NT_("storage=<storage-name> [ drive=<drivenum> ] [ alldrives ]"), false, true },
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
         "\tspooldata=<yes|no> priority=<number> jobid=<jobid> catalog=<catalog> migrationjob=<job-name>\n"
         "\tbackupclient=<client-name> backupformat=<format> nextpool=<pool-name>\n"
         "\tsince=<universal-time-specification> verifyjob=<job-name> verifylist=<verify-list>\n"
         "\tmigrationjob=<complete_name> yes"), false, true },
   { NT_("status"), status_cmd, _("Report status"),
     NT_("all | dir=<dir-name> | director | scheduler | schedule=<schedule-name> | client=<client-name> |\n"
         "\tstorage=<storage-name> slots | days=<nr_days> | job=<job-name> |\n"
         "\tsubscriptions"), true, true },
   { NT_("setbandwidth"), setbwlimit_cmd,  _("Sets bandwidth"),
     NT_("client=<client-name> | storage=<storage-name> | jobid=<jobid> |\n"
         "\tjob=<job-name> | ujobid=<unique-jobid> state=<job_state> | all\n"
         "\tlimit=<nn-kbs> [ yes ]"), true, true },
   { NT_("setdebug"), setdebug_cmd, _("Sets debug level"),
     NT_("level=<nn> trace=0/1 timestamp=0/1 client=<client-name> | dir | storage=<storage-name> | all"), true, true },
   { NT_("setip"), setip_cmd, _("Sets new client address -- if authorized"),
     NT_(""), false, true },
   { NT_("show"), show_cmd, _("Show resource records"),
     NT_("catalog=<catalog-name> | client=<client-name> | console=<console-name> | "
         "director=<director-name> | fileset=<fileset-name> | "
         "jobdefs=<job-defaults> | job=<job-name> | message=<message-resource-name> | "
         "pool=<pool-name> | profile=<profile-name> | "
         "schedule=<schedule-name> | storage=<storage-name> "
         "|\n"
         "catalog | clients | consoles | directors | filesets | jobdefs | jobs | "
         "messages | pools | profiles | schedules | storages "
         "|\n"
         "disabled [ clients | jobs | schedules ] "
         "|\n"
         "all [verbose]"), true, true },
   { NT_("sqlquery"), sqlquery_cmd, _("Use SQL to query catalog"),
     NT_(""), false, true },
   { NT_("time"), time_cmd, _("Print current time"),
     NT_(""), true, false },
   { NT_("trace"), trace_cmd, _("Turn on/off trace to file"),
     NT_("on | off"), true, true },
   { NT_("truncate"), truncate_cmd, _("Truncate purged volumes"),
     NT_("volstatus=Purged [storage=<storage>] [pool=<pool>] [volume=<volume>] [yes]"), true, true },
   { NT_("unmount"), unmount_cmd, _("Unmount storage"),
     NT_("storage=<storage-name> [ drive=<drivenum> ]\n"
         "\tjobid=<jobid> | job=<job-name> | ujobid=<complete_name>"), false, true },
   { NT_("umount"), unmount_cmd, _("Umount - for old-time Unix guys, see unmount"),
     NT_("storage=<storage-name> [ drive=<drivenum> ]\n"
         "\tjobid=<jobid> | job=<job-name> | ujobid=<complete_name>"), false, true },
   { NT_("update"), update_cmd, _("Update volume, pool, slots, job or statistics"),
     NT_("[volume=<volume-name> "
         "[volstatus=<status>] [volretention=<time-def>] actiononpurge=<action>] "
         "[pool=<pool-name>] [recycle=<yes/no>] [slot=<number>] [inchanger=<yes/no>]] "
         "|\n"
         "[pool=<pool-name> "
         "[maxvolbytes=<size>] [maxvolfiles=<nb>] [maxvoljobs=<nb>]"
         "[enabled=<yes/no>] [recyclepool=<pool-name>] [actiononpurge=<action>] |\n"
         "slots [storage=<storage-name>] [scan]] "
         "|\n"
         "[jobid=<jobid> [jobname=<name>] [starttime=<time-def>] [client=<client-name>]\n"
         "[filesetid=<fileset-id>] [jobtype=<job-type>]] "
         "|\n"
         "[stats "
         "[days=<number>]]"), true, true },
   { NT_("use"), use_cmd, _("Use specific catalog"),
     NT_("catalog=<catalog>"), false, true },
   { NT_("var"), var_cmd, _("Does variable expansion"),
     NT_(""), false, true },
   { NT_("version"), version_cmd, _("Print Director version"),
     NT_(""), true, false },
   { NT_("wait"), wait_cmd, _("Wait until no jobs are running"),
     NT_("jobname=<name> | jobid=<jobid> | ujobid=<complete_name> | mount [timeout=<number>]"), false, false }
};

#define comsize ((int)(sizeof(commands)/sizeof(struct ua_cmdstruct)))

bool UaContext::execute(ua_cmdstruct *cmd)
{
   set_command_definition(cmd);
   return (cmd->func)(this, this->cmd);
}

/**
 * Execute a command from the UA
 */
bool do_a_command(UaContext *ua)
{
   int i;
   int len;
   bool ok = false;
   bool found = false;
   BareosSocket *user = ua->UA_sock;

   Dmsg1(900, "Command: %s\n", ua->argk[0]);
   if (ua->argc == 0) {
      return false;
   }

   while (ua->jcr->res.wstorage->size()) {
      ua->jcr->res.wstorage->remove(0);
   }

   len = strlen(ua->argk[0]);
   for (i = 0; i < comsize; i++) { /* search for command */
      if (bstrncasecmp(ua->argk[0], commands[i].key, len)) {
         /*
          * Check if command permitted, but "quit" is always OK
          */
         if (!bstrcmp(ua->argk[0], NT_("quit")) &&
             !ua->acl_access_ok(Command_ACL, ua->argk[0], true)) {
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
         if (ua->audit_event_wanted(commands[i].audit_event)) {
            ua->log_audit_event_cmdline();
         }

         /*
          * Some commands alter the JobStatus of the JobControlRecord.
          * As the console JobControlRecord keeps running,
          * we set it to running state again.
          * ua->jcr->setJobStatus(JS_Running)
          * isn't enough, as it does not overwrite error states.
          */
         ua->jcr->JobStatus = JS_Running;

         if (ua->api) {
            user->signal(BNET_CMD_BEGIN);
         }
         ua->send->set_mode(ua->api);
         ok = ua->execute(&commands[i]);
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
   ua->send->finalize_result(ok);

   return ok;
}

static bool is_dot_command(const char *cmd)
{
   if (cmd && (strlen(cmd) > 0) && (cmd[0] == '.')) {
      return true;
   }
   return false;
}

/**
 * Add Volumes to an existing Pool
 */
static bool add_cmd(UaContext *ua, const char *cmd)
{
   PoolDbRecord pr;
   MediaDbRecord mr;
   int num, i, max, startnum;
   char name[MAX_NAME_LENGTH];
   StoreResource *store;
   slot_number_t Slot = 0;
   int8_t InChanger = 0;

   ua->send_msg(_("You probably don't want to be using this command since it\n"
                  "creates database records without labeling the Volumes.\n"
                  "You probably want to use the \"label\" command.\n\n"));

   if (!open_client_db(ua)) {
      return true;
   }

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

   if (!get_pool_dbr(ua, &pr)) {
      return true;
   }

   Dmsg4(120, "id=%d Num=%d Max=%d type=%s\n", pr.PoolId, pr.NumVols,
      pr.MaxVols, pr.PoolType);

   while (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
      ua->warning_msg(_("Pool already has maximum volumes=%d\n"), pr.MaxVols);
      if (!get_pint(ua, _("Enter new maximum (zero for unlimited): "))) {
         return true;
      }
      pr.MaxVols = ua->pint32_val;
   }

   /*
    * Get media type
    */
   if ((store = get_storage_resource(ua)) != NULL) {
      bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));
   } else if (!get_media_type(ua, mr.MediaType, sizeof(mr.MediaType))) {
      return true;
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
         return true;
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
            return true;
         }
      } else {
         if (!get_cmd(ua, _("Enter base volume name: "))) {
            return true;
         }
      }

      /*
       * Don't allow | in Volume name because it is the volume separator character
       */
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
            return true;
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
         return true;
      }
      Slot = (slot_number_t)ua->pint32_val;
      if (!get_yesno(ua, _("InChanger? yes/no: "))) {
         return true;
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
      if (!ua->db->create_media_record(ua->jcr, &mr)) {
         ua->error_msg("%s", ua->db->strerror());
         return true;
      }
   }
   pr.NumVols += num;
   Dmsg0(200, "Update pool record.\n");
   if (ua->db->update_pool_record(ua->jcr, &pr) != 1) {
      ua->warning_msg("%s", ua->db->strerror());
      return true;
   }
   ua->send_msg(_("%d Volumes created in pool %s\n"), num, pr.Name);

   return true;
}

/**
 * Turn auto mount on/off
 *
 * automount on
 * automount off
 */
static bool automount_cmd(UaContext *ua, const char *cmd)
{
   char *onoff;

   if (ua->argc != 2) {
      if (!get_cmd(ua, _("Turn on or off? "))) {
            return true;
      }
      onoff = ua->cmd;
   } else {
      onoff = ua->argk[1];
   }

   ua->automount = (bstrcasecmp(onoff, NT_("off"))) ? 0 : 1;
   return true;
}

static inline bool cancel_storage_daemon_job(UaContext *ua, const char *cmd)
{
   int i;
   StoreResource *store;

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

   return true;
}

static inline bool cancel_jobs(UaContext *ua, const char *cmd)
{
   JobControlRecord *jcr;
   JobId_t *JobId;
   alist *selection;

   selection = select_jobs(ua, "cancel");
   if (!selection) {
      return true;
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

   return true;
}

/**
 * Cancel a job
 */
static bool cancel_cmd(UaContext *ua, const char *cmd)
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

/**
 * Create a Pool Record in the database.
 * It is always created from the Resource record.
 */
static bool create_cmd(UaContext *ua, const char *cmd)
{
   PoolResource *pool;

   if (!open_client_db(ua)) {
      return true;
   }

   pool = get_pool_resource(ua);
   if (!pool) {
      return true;
   }

   switch (create_pool(ua->jcr, ua->db, pool, POOL_OP_CREATE)) {
   case 0:
      ua->error_msg(_("Error: Pool %s already exists.\n"
               "Use update to change it.\n"), pool->name());
      break;

   case -1:
      ua->error_msg("%s", ua->db->strerror());
      break;

   default:
     break;
   }
   ua->send_msg(_("Pool %s created.\n"), pool->name());
   return true;
}

static inline bool setbwlimit_filed(UaContext *ua, ClientResource *client, int64_t limit, char *Job)
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

   if (!connect_to_file_daemon(ua->jcr, 1, 15, false)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return true;
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

   return true;
}

static inline bool setbwlimit_stored(UaContext *ua, StoreResource *store,
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
      return true;
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
      return true;
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

   return true;
}

static bool setbwlimit_cmd(UaContext *ua, const char *cmd)
{
   int i;
   int64_t limit = -1;
   ClientResource *client = NULL;
   StoreResource *store = NULL;
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
         return true;
      }
      limit = ((int64_t)ua->pint32_val * 1024); /* kb/s */
   }

   if (find_arg_keyword(ua, lst) > 0) {
      JobControlRecord *jcr;
      JobId_t *JobId;
      alist *selection;

      selection = select_jobs(ua, "limit");
      if (!selection) {
         return true;
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

   return true;
}

/**
 * Set a new address in a Client resource. We do this only
 * if the Console name is the same as the Client name
 * and the Console can access the client.
 */
static bool setip_cmd(UaContext *ua, const char *cmd)
{
   ClientResource *client;
   char buf[1024];

   client = ua->GetClientResWithName(ua->cons->name());
   if (!client) {
      ua->error_msg(_("Client \"%s\" not found.\n"), ua->cons->name());
      return false;
   }

   if (client->address) {
      free(client->address);
   }

   sockaddr_to_ascii(&(ua->UA_sock->client_addr), buf, sizeof(buf));
   client->address = bstrdup(buf);
   ua->send_msg(_("Client \"%s\" address set to %s\n"), client->name(), client->address);

   return true;
}

static void do_en_disable_cmd(UaContext *ua, bool setting)
{
   ScheduleResource *sched = NULL;
   ClientResource *client = NULL;
   JobResource *job = NULL;
   int i;

   i = find_arg(ua, NT_("schedule"));
   if (i >= 0) {
      i = find_arg_with_value(ua, NT_("schedule"));
      if (i >= 0) {
         sched = ua->GetScheduleResWithName(ua->argv[i]);
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
            client = ua->GetClientResWithName(ua->argv[i]);
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
            job = ua->GetJobResWithName(ua->argv[i]);
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
      sched->enabled = setting;
      ua->send_msg(_("Schedule \"%s\" %sabled\n"), sched->name(), setting ? "en" : "dis");
   } else if (client) {
      client->enabled = setting;
      ua->send_msg(_("Client \"%s\" %sabled\n"), client->name(), setting ? "en" : "dis");
   } else if (job) {
      job->enabled = setting;
      ua->send_msg(_("Job \"%s\" %sabled\n"), job->name(), setting ? "en" : "dis");
   }

   ua->warning_msg(_("%sabling is a temporary operation until the director reloads\n"), setting ? "En" : "Dis");
}

static bool enable_cmd(UaContext *ua, const char *cmd)
{
   do_en_disable_cmd(ua, true);
   return true;
}

static bool disable_cmd(UaContext *ua, const char *cmd)
{
   do_en_disable_cmd(ua, false);
   return true;
}

static void do_storage_setdebug(UaContext *ua, StoreResource *store, int level,
                                int trace_flag, int timestamp_flag)
{
   BareosSocket *sd;
   JobControlRecord *jcr = ua->jcr;
   UnifiedStoreResource lstore;

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

/**
 * For the client, we have the following values that can be set :
 *
 * level = debug level
 * trace = send debug output to a file
 * hangup = how many records to send to SD before hanging up
 *          obviously this is most useful for testing restarting
 *          failed jobs.
 * timestamp = set debug msg timestamping
 */
static void do_client_setdebug(UaContext *ua, ClientResource *client, int level, int trace_flag,
                               int hangup_flag, int timestamp_flag)
{
   BareosSocket *fd;

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

   if (!connect_to_file_daemon(ua->jcr, 1, 15, false)) {
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

static void do_director_setdebug(UaContext *ua, int level, int trace_flag, int timestamp_flag)
{
   PoolMem tracefilename(PM_FNAME);

   debug_level = level;
   set_trace(trace_flag);
   set_timestamp(timestamp_flag);
   Mmsg(tracefilename, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);
   ua->send_msg("level=%d trace=%d hangup=%d timestamp=%d tracefilename=%s\n", level, get_trace(), get_hangup(), get_timestamp(), tracefilename.c_str());
}

static void do_all_setdebug(UaContext *ua, int level, int trace_flag,
                            int hangup_flag, int timestamp_flag)
{
   StoreResource *store, **unique_store;
   ClientResource *client, **unique_client;
   int i, j, found;

   /*
    * Director
    */
   do_director_setdebug(ua, level, trace_flag, timestamp_flag);

   /*
    * Count Storage items
    */
   LockRes();
   store = NULL;
   i = 0;
   foreach_res(store, R_STORAGE) {
      i++;
   }
   unique_store = (StoreResource **) malloc(i * sizeof(StoreResource));

   /*
    * Find Unique Storage address/port
    */
   store = (StoreResource *)GetNextRes(R_STORAGE, NULL);
   i = 0;
   unique_store[i++] = store;
   while ((store = (StoreResource *)GetNextRes(R_STORAGE, (CommonResourceHeader *)store))) {
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
   unique_client = (ClientResource **) malloc(i * sizeof(ClientResource));

   /*
    * Find Unique Client address/port
    */
   client = (ClientResource *)GetNextRes(R_CLIENT, NULL);
   i = 0;
   unique_client[i++] = client;
   while ((client = (ClientResource *)GetNextRes(R_CLIENT, (CommonResourceHeader *)client))) {
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

/**
 * setdebug level=nn all trace=1/0 timestamp=1/0
 */
static bool setdebug_cmd(UaContext *ua, const char *cmd)
{
   int i;
   int level;
   int trace_flag;
   int hangup_flag;
   int timestamp_flag;
   StoreResource *store;
   ClientResource *client;

   Dmsg1(120, "setdebug:%s:\n", cmd);

   level = -1;
   i = find_arg_with_value(ua, NT_("level"));
   if (i >= 0) {
      level = atoi(ua->argv[i]);
   }
   if (level < 0) {
      if (!get_pint(ua, _("Enter new debug level: "))) {
         return true;
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
         return true;
      }
      if (bstrcasecmp(ua->argk[i], "dir") ||
          bstrcasecmp(ua->argk[i], "director")) {
         do_director_setdebug(ua, level, trace_flag, timestamp_flag);
         return true;
      }
      if (bstrcasecmp(ua->argk[i], "client") ||
          bstrcasecmp(ua->argk[i], "fd")) {
         client = NULL;
         if (ua->argv[i]) {
            client = ua->GetClientResWithName(ua->argv[i]);
            if (client) {
               do_client_setdebug(ua, client, level, trace_flag, hangup_flag, timestamp_flag);
               return true;
            }
         }
         client = select_client_resource(ua);
         if (client) {
            do_client_setdebug(ua, client, level, trace_flag, hangup_flag, timestamp_flag);
            return true;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("store")) ||
          bstrcasecmp(ua->argk[i], NT_("storage")) ||
          bstrcasecmp(ua->argk[i], NT_("sd"))) {
         store = NULL;
         if (ua->argv[i]) {
            store = ua->GetStoreResWithName(ua->argv[i]);
            if (store) {
               do_storage_setdebug(ua, store, level, trace_flag, timestamp_flag);
               return true;
            }
         }
         store = get_storage_resource(ua);
         if (store) {
            switch (store->Protocol) {
            case APT_NDMPV2:
            case APT_NDMPV3:
            case APT_NDMPV4:
               ua->warning_msg(_("Storage has non-native protocol.\n"));
               return true;
            default:
               break;
            }

            do_storage_setdebug(ua, store, level, trace_flag, timestamp_flag);
            return true;
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
   case 0:
      /*
       * Director
       */
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
            return true;
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

   return true;
}

/**
 * Resolve a hostname.
 */
static bool resolve_cmd(UaContext *ua, const char *cmd)
{
   StoreResource *storage = NULL;
   ClientResource *client = NULL;

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("client")) ||
          bstrcasecmp(ua->argk[i], NT_("fd"))) {
         if (ua->argv[i]) {
            client = ua->GetClientResWithName(ua->argv[i]);
            if (!client) {
               ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
               return true;
            }

            *ua->argk[i] = 0;         /* zap keyword already visited */
            continue;
         } else {
            ua->error_msg(_("Client name missing.\n"));
            return true;
         }
      } else if (bstrcasecmp(ua->argk[i], NT_("storage")))  {
         if (ua->argv[i]) {
            storage = ua->GetStoreResWithName(ua->argv[i]);
            if (!storage) {
               ua->error_msg(_("Storage \"%s\" not found.\n"), ua->argv[i]);
               return true;
            }

            *ua->argk[i] = 0;         /* zap keyword already visited */
            continue;
         } else {
            ua->error_msg(_("Storage name missing.\n"));
            return true;
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
            return false;
         }
         ua->send_msg(_("%s resolves %s to %s\n"), my_name, ua->argk[i],
                      build_addresses_str(addr_list, addresses, sizeof(addresses), false));
         free_addresses(addr_list);
      }
   }

   return true;
}

/**
 * Turn debug tracing to file on/off
 */
static bool trace_cmd(UaContext *ua, const char *cmd)
{
   char *onoff;

   if (ua->argc != 2) {
      if (!get_cmd(ua, _("Turn on or off? "))) {
            return true;
      }
      onoff = ua->cmd;
   } else {
      onoff = ua->argk[1];
   }


   set_trace((bstrcasecmp(onoff, NT_("off"))) ? false : true);
   return true;

}

static bool var_cmd(UaContext *ua, const char *cmd)
{
   POOLMEM *val = get_pool_memory(PM_FNAME);
   char *var;

   if (!open_client_db(ua)) {
      return true;
   }
   for (var=ua->cmd; *var != ' '; ) {    /* skip command */
      var++;
   }
   while (*var == ' ') {                 /* skip spaces */
      var++;
   }
   Dmsg1(100, "Var=%s:\n", var);
   variable_expansion(ua->jcr, var, val);
   ua->send_msg("%s\n", val);
   free_pool_memory(val);
   return true;
}

static bool estimate_cmd(UaContext *ua, const char *cmd)
{
   JobResource *job = NULL;
   ClientResource *client = NULL;
   FilesetResource *fileset = NULL;
   int listing = 0;
   JobControlRecord *jcr = ua->jcr;
   bool accurate_set = false;
   bool accurate = false;

   jcr->setJobLevel(L_FULL);
   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("client")) ||
          bstrcasecmp(ua->argk[i], NT_("fd"))) {
         if (ua->argv[i]) {
            client = ua->GetClientResWithName(ua->argv[i]);
            if (!client) {
               ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
               return false;
            }
            continue;
         } else {
            ua->error_msg(_("Client name missing.\n"));
            return false;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("job"))) {
         if (ua->argv[i]) {
            job = ua->GetJobResWithName(ua->argv[i]);
            if (!job) {
               ua->error_msg(_("Job \"%s\" not found.\n"), ua->argv[i]);
               return false;
            }
            continue;
         } else {
            ua->error_msg(_("Job name missing.\n"));
            return false;
         }

      }

      if (bstrcasecmp(ua->argk[i], NT_("fileset"))) {
         if (ua->argv[i]) {
            fileset = ua->GetFileSetResWithName(ua->argv[i]);
            if (!fileset) {
               ua->error_msg(_("Fileset \"%s\" not found.\n"), ua->argv[i]);
               return false;
            }
            continue;
         } else {
            ua->error_msg(_("Fileset name missing.\n"));
            return false;
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
            return false;
         }
      }

      if (bstrcasecmp(ua->argk[i], NT_("accurate"))) {
         if (ua->argv[i]) {
            if (!is_yesno(ua->argv[i], &accurate)) {
               ua->error_msg(_("Invalid value for accurate. "
                               "It must be yes or no.\n"));
            }
            accurate_set = true;
            continue;
         } else {
            ua->error_msg(_("Accurate value missing.\n"));
            return false;
         }
      }
   }

   if (!job && !(client && fileset)) {
      if (!(job = select_job_resource(ua))) {
         return false;
      }
   }

   if (!job) {
      job = ua->GetJobResWithName(ua->argk[1]);
      if (!job) {
         ua->error_msg(_("No job specified.\n"));
         return false;
      }
   }

   switch (job->JobType) {
   case JT_BACKUP:
      break;
   default:
      ua->error_msg(_("Wrong job specified of type %s.\n"), job_type_to_str(job->JobType));
      return false;
   }

   if (!client) {
      client = job->client;
   }

   if (!fileset) {
      fileset = job->fileset;
   }

   if (!client) {
      ua->error_msg(_("No client specified or selected.\n"));
      return false;
   }

   if (!fileset) {
      ua->error_msg(_("No fileset specified or selected.\n"));
      return false;
   }

   jcr->res.client = client;
   jcr->res.fileset = fileset;
   close_db(ua);

   switch (client->Protocol) {
   case APT_NATIVE:
      break;
   default:
      ua->error_msg(_("Estimate is only supported on native clients.\n"));
      return false;
   }

   if (job->pool->catalog) {
      ua->catalog = job->pool->catalog;
   } else {
      ua->catalog = client->catalog;
   }

   if (!open_db(ua)) {
      return false;
   }

   jcr->res.job = job;
   jcr->setJobType(JT_BACKUP);
   jcr->start_time = time(NULL);
   init_jcr_job_record(jcr);

   if (!get_or_create_client_record(jcr)) {
      return true;
   }

   if (!get_or_create_fileset_record(jcr)) {
      return true;
   }

   get_level_since_time(jcr);

   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
               jcr->res.client->name(), jcr->res.client->address, jcr->res.client->FDport);
   if (!connect_to_file_daemon(jcr, 1, 15, false)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return false;
   }

   if (!send_job_info(jcr)) {
      ua->error_msg(_("Failed to connect to Client.\n"));
      return false;
   }

   /*
    * The level string change if accurate mode is enabled
    */
   if (accurate_set) {
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
   return true;
}

/**
 * Print time
 */
static bool time_cmd(UaContext *ua, const char *cmd)
{
   char sdt[50];
   time_t ttime = time(NULL);

   ua->send->object_start("time");

   bstrftime(sdt, sizeof(sdt), ttime, "%a %d-%b-%Y %H:%M:%S");
   ua->send->object_key_value("full", sdt, "%s\n");

   bstrftime(sdt, sizeof(sdt), ttime, "%Y");
   ua->send->object_key_value("year", sdt);
   bstrftime(sdt, sizeof(sdt), ttime, "%m");
   ua->send->object_key_value("month", sdt);
   bstrftime(sdt, sizeof(sdt), ttime, "%d");
   ua->send->object_key_value("day", sdt);
   bstrftime(sdt, sizeof(sdt), ttime, "%H");
   ua->send->object_key_value("hour", sdt);
   bstrftime(sdt, sizeof(sdt), ttime, "%M");
   ua->send->object_key_value("minute", sdt);
   bstrftime(sdt, sizeof(sdt), ttime, "%S");
   ua->send->object_key_value("second", sdt);

   ua->send->object_end("time");

   return true;
}


/**
 * truncate command. Truncates volumes (volume files) on the storage daemon.
 *
 * usage:
 * truncate volstatus=Purged [storage=<storage>] [pool=<pool>] [volume=<volume>] [yes]
 */
static bool truncate_cmd(UaContext *ua, const char *cmd)
{
   bool result = false;
   int i = -1;
   int parsed_args = 1; /* start at 1, as command itself is also counted */
   char esc[MAX_NAME_LENGTH * 2 + 1];
   PoolMem tmp(PM_MESSAGE);
   PoolMem volumes(PM_MESSAGE);
   dbid_list mediaIds;
   MediaDbRecord mr;
   PoolDbRecord pool_dbr;
   StorageDbRecord storage_dbr;

   memset(&pool_dbr, 0, sizeof(pool_dbr));
   memset(&storage_dbr, 0, sizeof(storage_dbr));

   /*
    * Look for volumes that can be recycled,
    * are enabled and have used more than the first block.
    */
   memset(&mr, 0, sizeof(mr));
   mr.Recycle = 1;
   mr.Enabled = VOL_ENABLED;
   mr.VolBytes = (512 * 126);  /* search volumes with more than 64,512 bytes (DEFAULT_BLOCK_SIZE) */

   if (!(ua->argc > 1)) {
      ua->send_cmd_usage(_("missing parameter"));
      return false;
   }

   /* arg: volstatus=Purged */
   i = find_arg_with_value(ua, "volstatus");
   if (i < 0) {
      ua->send_cmd_usage(_("required parameter 'volstatus' missing"));
      return false;
   }
   if (!(bstrcasecmp(ua->argv[i], "Purged"))) {
      ua->send_cmd_usage(_("Invalid parameter. 'volstatus' must be 'Purged'."));
      return false;
   }
   parsed_args++;

   /*
    * Look for Purged volumes.
    */
   bstrncpy(mr.VolStatus, "Purged", sizeof(mr.VolStatus));

   if (!open_db(ua)) {
      ua->error_msg("Failed to open db.\n");
      goto bail_out;
   }

   ua->send->add_acl_filter_tuple(2, Pool_ACL);
   ua->send->add_acl_filter_tuple(3, Storage_ACL);

   /*
    * storage parameter is only required
    * if ACL forbids access to all storages.
    * Otherwise the user should not be asked for this parameter.
    */
   i = find_arg_with_value(ua, "storage");
   if ((i >= 0) || ua->acl_has_restrictions(Storage_ACL)) {
      if (!select_storage_dbr(ua, &storage_dbr, "storage")) {
         goto bail_out;
      }
      mr.StorageId = storage_dbr.StorageId;
      parsed_args++;
   }

   /*
    * pool parameter is only required
    * if ACL forbids access to all pools.
    * Otherwise the user should not be asked for this parameter.
    */
   i = find_arg_with_value(ua, "pool");
   if ((i >= 0) || ua->acl_has_restrictions(Pool_ACL)) {
      if (!select_pool_dbr(ua, &pool_dbr, "pool")) {
         goto bail_out;
      }
      mr.PoolId = pool_dbr.PoolId;
      if (i >= 0) {
         parsed_args++;
      }
   }

   /*
    * parse volume parameter.
    * Currently only support one volume parameter
    * (multiple volume parameter have been intended before,
    * but this causes problems with parsing and ACL handling).
    */
   i = find_arg_with_value(ua, "volume");
   if (i >= 0) {
      if (is_name_valid(ua->argv[i])) {
         ua->db->escape_string(ua->jcr, esc, ua->argv[i], strlen(ua->argv[i]));
         if (!*volumes.c_str()) {
            Mmsg(tmp, "'%s'", esc);
         } else {
            Mmsg(tmp, ",'%s'", esc);
         }
         volumes.strcat(tmp.c_str());
         parsed_args++;
      }
   }

   if (find_arg(ua, NT_("yes")) >= 0) {
      /* parameter yes is evaluated at 'get_confirmation' */
      parsed_args++;
   }

   if (parsed_args != ua->argc) {
      ua->send_cmd_usage(_("Invalid parameter."));
      goto bail_out;
   }

   /* create sql query string (in ua->db->cmd) */
   if (!ua->db->prepare_media_sql_query(ua->jcr, &mr, &tmp , volumes)) {
      ua->error_msg(_("Invalid parameter (failed to create sql query).\n"));
      goto bail_out;
   }

   /* execute query and display result */
   ua->db->list_sql_query(ua->jcr, tmp.c_str(),
                     ua->send, HORZ_LIST, "volumes", true);

   /*
    * execute query to get media ids.
    * Second execution is only required,
    * because function is also used in other contextes.
    */
   //tmp.strcpy(ua->db->cmd);
   if (!ua->db->get_query_dbids(ua->jcr, tmp, mediaIds)) {
      Dmsg0(100, "No results from db_get_query_dbids\n");
      goto bail_out;
   }

   if (!mediaIds.size()) {
      Dmsg0(100, "Results are empty\n");
      goto bail_out;
   }

   if (!ua->db->verify_media_ids_from_single_storage(ua->jcr, mediaIds)) {
      ua->error_msg("Selected volumes are from different storages. "
                    "This is not supported. Please choose only volumes from a single storage.\n");
      goto bail_out;
   }

   mr.MediaId = mediaIds.get(0);
   if (!ua->db->get_media_record(ua->jcr, &mr)) {
      goto bail_out;
   }

   if (ua->GetStoreResWithId(mr.StorageId)->Protocol != APT_NATIVE) {
      ua->warning_msg(_("Storage uses a non-native protocol. Truncate is only supported for native protocols.\n"));
      goto bail_out;
   }

   if (!get_confirmation(ua, _("Truncate listed volumes (yes/no)? "))) {
      goto bail_out;
   }

   /*
    * Loop over the candidate Volumes and actually truncate them
    */
   for (int i = 0; i < mediaIds.size(); i++) {
      memset(&mr, 0, sizeof(mr));
      mr.MediaId = mediaIds.get(i);
      if (!ua->db->get_media_record(ua->jcr, &mr)) {
         Dmsg1(0, "Can't find MediaId=%lld\n", (uint64_t) mr.MediaId);
      } else {
         do_truncate(ua, mr);
      }
   }

bail_out:
   close_db(ua);
   if (ua->jcr->store_bsock) {
      ua->jcr->store_bsock->signal(BNET_TERMINATE);
      ua->jcr->store_bsock->close();
      delete ua->jcr->store_bsock;
      ua->jcr->store_bsock = NULL;
   }
   return result;
}

static bool do_truncate(UaContext *ua, MediaDbRecord &mr)
{
   bool retval = false;
   StorageDbRecord storage_dbr;
   PoolDbRecord pool_dbr;

   memset(&storage_dbr, 0, sizeof(storage_dbr));
   memset(&pool_dbr, 0, sizeof(pool_dbr));

   storage_dbr.StorageId = mr.StorageId;
   if (!ua->db->get_storage_record(ua->jcr, &storage_dbr)) {
      ua->error_msg("failed to determine storage for id %lld\n", mr.StorageId);
      goto bail_out;
   }

   pool_dbr.PoolId = mr.PoolId;
   if (!ua->db->get_pool_record(ua->jcr, &pool_dbr)) {
      ua->error_msg("failed to determine pool for id %lld\n", mr.PoolId);
      goto bail_out;
   }

   /*
    * Choose storage
    */
   ua->jcr->res.wstore = ua->GetStoreResWithName(storage_dbr.Name);
   if (!ua->jcr->res.wstore) {
      ua->error_msg("failed to determine storage resource by name %s\n", storage_dbr.Name);
      goto bail_out;
   }

   if (send_label_request(ua, ua->jcr->res.wstore,
                          &mr, &mr,
                          &pool_dbr,
                          /* bool media_record_exists */
                          true,
                          /* bool relabel */
                          true,
                          /* drive_number_t drive */
                          0,
                          /* slot_number_t slot */
                          0)) {
      ua->send_msg(_("The volume '%s' has been truncated.\n"), mr.VolumeName);
      retval = true;
   }

bail_out:
   ua->jcr->res.wstore = NULL;
   return retval;
}

/**
 * Reload the conf file
 */
static bool reload_cmd(UaContext *ua, const char *cmd)
{
   bool result;

   result = do_reload_config();

   ua->send->object_start("reload");
   if (result) {
      ua->send->object_key_value_bool("success", result, "reloaded\n");
   } else {
      ua->send->object_key_value_bool("success", result, "failed to reload\n");
   }
   ua->send->object_end("reload");

   return result;
}

/**
 * Delete Pool records (should purge Media with it).
 *
 * delete pool=<pool-name>
 * delete volume pool=<pool-name> volume=<name>
 * delete jobid=<jobid>
 */
static bool delete_cmd(UaContext *ua, const char *cmd)
{
   static const char *keywords[] = {
      NT_("volume"),
      NT_("pool"),
      NT_("jobid"),
      NULL
   };

   if (!open_client_db(ua, true)) {
      return true;
   }

   switch (find_arg_keyword(ua, keywords)) {
   case 0:
      delete_volume(ua);
      return true;
   case 1:
      delete_pool(ua);
      return true;
   case 2:
      int i;
      while ((i = find_arg(ua, "jobid")) > 0) {
         delete_job(ua);
         *ua->argk[i] = 0;         /* zap keyword already visited */
      }
      return true;
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
      return true;
   default:
      ua->warning_msg(_("Nothing done.\n"));
      break;
   }
   return true;
}

/**
 * delete_job has been modified to parse JobID lists like the following:
 * delete JobID=3,4,6,7-11,14
 *
 * Thanks to Phil Stracchino for the above addition.
 */
static void delete_job(UaContext *ua)
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

/**
 * We call delete_job_id_range to parse range tokens and iterate over ranges
 */
static bool delete_job_id_range(UaContext *ua, char *tok)
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
                      _("Are you sure you want to delete %d JobIds ? (yes/no): "), j2 - j1);
            if (!get_yesno(ua, buf) || !ua->pint32_val) {
               return true;
            }
         }
         for (j = j1; j <= j2; j++) {
            do_job_delete(ua, j);
         }
      } else {
         ua->warning_msg(_("Illegal JobId range %s - %s should define increasing JobIds, ignored\n"), tok, tok2);
      }
   } else {
      ua->warning_msg(_("Illegal JobId range %s - %s, ignored\n"), tok, tok2);
   }

   return true;
}

/**
 * do_job_delete now performs the actual delete operation atomically
 */
static void do_job_delete(UaContext *ua, JobId_t JobId)
{
   char ed1[50];

   edit_int64(JobId, ed1);
   purge_jobs_from_catalog(ua, ed1);
   ua->send_msg(_("Jobid %s and associated records deleted from the catalog.\n"), ed1);
}

/**
 * Delete media records from database -- dangerous
 */
static bool delete_volume(UaContext *ua)
{
   MediaDbRecord mr;
   char buf[1000];
   db_list_ctx lst;

   memset(&mr, 0, sizeof(mr));
   if (!select_media_dbr(ua, &mr)) {
      return true;
   }
   ua->warning_msg(_("\nThis command will delete volume %s\n"
      "and all Jobs saved on that volume from the Catalog\n"),
      mr.VolumeName);

   if (find_arg(ua, "yes") >= 0) {
      ua->pint32_val = 1; /* Have "yes" on command line already" */
   } else {
      bsnprintf(buf, sizeof(buf), _("Are you sure you want to delete Volume \"%s\"? (yes/no): "), mr.VolumeName);
      if (!get_yesno(ua, buf)) {
         return true;
      }
   }
   if (!ua->pint32_val) {
      return true;
   }

   /*
    * If not purged, do it
    */
   if (!bstrcmp(mr.VolStatus, "Purged")) {
      if (!ua->db->get_volume_jobids(ua->jcr, &mr, &lst)) {
         ua->error_msg(_("Can't list jobs on this volume\n"));
         return true;
      }
      if (lst.count) {
         purge_jobs_from_catalog(ua, lst.list);
      }
   }

   ua->db->delete_media_record(ua->jcr, &mr);
   return true;
}

/**
 * Delete a pool record from the database -- dangerous
 */
static bool delete_pool(UaContext *ua)
{
   PoolDbRecord  pr;
   char buf[200];

   memset(&pr, 0, sizeof(pr));

   if (!get_pool_dbr(ua, &pr)) {
      return true;
   }
   bsnprintf(buf, sizeof(buf), _("Are you sure you want to delete Pool \"%s\"? (yes/no): "),
      pr.Name);
   if (!get_yesno(ua, buf)) {
      return true;
   }
   if (ua->pint32_val) {
      ua->db->delete_pool_record(ua->jcr, &pr);
   }
   return true;
}

static bool memory_cmd(UaContext *ua, const char *cmd)
{
   garbage_collect_memory();
   list_dir_status_header(ua);
   sm_dump(false, true);
   return true;
}

static void do_mount_cmd(UaContext *ua, const char *cmd)
{
   UnifiedStoreResource store;
   drive_number_t nr_drives;
   drive_number_t drive = -1;
   slot_number_t slot = -1;
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

   Dmsg3(120, "Found storage, MediaType=%s DevName=%s drive=%hd\n",
         store.store->media_type, store.store->dev_name(), drive);

   if (!do_alldrives) {
      do_autochanger_volume_operation(ua, store.store, cmd, drive, slot);
   } else {
      nr_drives = get_num_drives(ua, ua->jcr->res.wstore);
      for (drive_number_t i = 0; i < nr_drives; i++) {
         do_autochanger_volume_operation(ua, store.store, cmd, i, slot);
      }
   }

   invalidate_vol_list(store.store);
}

/**
 * mount [storage=<name>] [drive=nn] [slot=mm]
 */
static bool mount_cmd(UaContext *ua, const char *cmd)
{
   do_mount_cmd(ua, "mount");          /* mount */
   return true;
}

/**
 * unmount [storage=<name>] [drive=nn]
 */
static bool unmount_cmd(UaContext *ua, const char *cmd)
{
   do_mount_cmd(ua, "unmount");          /* unmount */
   return true;
}

/**
 * Perform a NO-OP.
 */
static bool noop_cmd(UaContext *ua, const char *cmd)
{
   if (ua->api) {
      ua->signal(BNET_CMD_BEGIN);
   }

   if (ua->api) {
      ua->signal(BNET_CMD_OK);
   }

   return true;                    /* no op */
}

/**
 * release [storage=<name>] [drive=nn]
 */
static bool release_cmd(UaContext *ua, const char *cmd)
{
   do_mount_cmd(ua, "release");          /* release */
   return true;
}

/**
 * Switch databases
 * use catalog=<name>
 */
static bool use_cmd(UaContext *ua, const char *cmd)
{
   CatalogResource *oldcatalog, *catalog;

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
   return true;
}

bool quit_cmd(UaContext *ua, const char *cmd)
{
   ua->quit = true;

   return true;
}

/**
 * Handler to get job status
 */
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

/**
 * Wait until no job is running
 */
static bool wait_cmd(UaContext *ua, const char *cmd)
{
   int i;
   JobControlRecord *jcr;
   int status;
   char ed1[50];
   uint32_t JobId = 0;
   time_t stop_time = 0;
   char jobstatus = '?';        /* Unknown by default */
   PoolMem temp(PM_MESSAGE);

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
      return true;
   }

   i = find_arg_with_value(ua, NT_("timeout"));
   if (i > 0 && ua->argv[i]) {
      stop_time = time(NULL) + str_to_int64(ua->argv[i]);
   }

   /*
    * We have jobid, jobname or ujobid argument
    */
   if (!open_client_db(ua)) {
      ua->error_msg(_("ERR: Can't open db\n"));
      return true;
   }

   for (int i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], "jobid")) {
         if (!ua->argv[i]) {
            break;
         }
         JobId = str_to_int64(ua->argv[i]);
         break;
      } else if (bstrcasecmp(ua->argk[i], "jobname") ||
                 bstrcasecmp(ua->argk[i], "job")) {
         if (!ua->argv[i]) {
            break;
         }
         jcr = get_jcr_by_partial_name(ua->argv[i]);
         if (jcr) {
            JobId = jcr->JobId;
            free_jcr(jcr);
         }
         break;
      } else if (bstrcasecmp(ua->argk[i], "ujobid")) {
         if (!ua->argv[i]) {
            break;
         }
         jcr = get_jcr_by_full_name(ua->argv[i]);
         if (jcr) {
            JobId = jcr->JobId;
            free_jcr(jcr);
         }
         break;
      } else if (bstrcasecmp(ua->argk[i], "mount")) {
         /*
          * Wait for a mount request
          */
         for (bool waiting = false; !waiting; ) {
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
               return true;
            }
            bmicrosleep(1, 0);
         }
         return true;
      }
   }

   if (JobId == 0) {
      ua->error_msg(_("ERR: Job was not found\n"));
      return true;
   }

   /*
    * We wait the end of a specific job
    */
   bmicrosleep(0, 200000);            /* let job actually start */
   for (bool running = true; running; ) {
      running = false;

      jcr = get_jcr_by_id(JobId);
      if (jcr) {
         running = true;
         free_jcr(jcr);
      }

      if (running) {
         bmicrosleep(1, 0);
      }
   }

   /*
    * We have to get JobStatus
    */
   Mmsg(temp, "SELECT JobStatus FROM Job WHERE JobId='%s'", edit_int64(JobId, ed1));
   ua->db->sql_query(temp.c_str(), status_handler, (void *)&jobstatus);

   switch (jobstatus) {
   case JS_Error:
      status = 1;          /* Warning */
      break;
   case JS_FatalError:
   case JS_ErrorTerminated:
   case JS_Canceled:
      status = 2;          /* Critical */
      break;
   case JS_Warnings:
   case JS_Terminated:
      status = 0;          /* Ok */
      break;
   default:
      status = 3;          /* Unknown */
      break;
   }

   Mmsg(temp, "%c", jobstatus);
   ua->send->object_start("Job");
   ua->send->object_key_value("JobId", "%s=", JobId, "%i\n");
   ua->send->object_key_value("JobStatusLong", job_status_to_str(jobstatus), "JobStatus=%s ");
   ua->send->object_key_value("JobStatus",  temp.c_str(), "(%s)\n");
   ua->send->object_key_value("ExitStatus", status);
   ua->send->object_end("Job");

   return true;
}

static bool help_cmd(UaContext *ua, const char *cmd)
{
   int i;

   ua->send->decoration("%s", _("  Command            Description\n  =======            ===========\n"));
   for (i = 0; i < comsize; i++) {
      if (ua->argc == 2) {
         if (bstrcasecmp(ua->argk[1], commands[i].key)) {
            ua->send->object_start(commands[i].key);
            ua->send->object_key_value("command", commands[i].key, "  %-18s");
            ua->send->object_key_value("description", commands[i].help, " %s\n\n");
            ua->send->object_key_value("arguments", "Arguments:\n\t", commands[i].usage, "%s\n", 40);
            ua->send->object_key_value_bool("permission", ua->acl_access_ok(Command_ACL, commands[i].key));
            ua->send->object_end(commands[i].key);
            break;
         }
      } else {
         if (ua->acl_access_ok(Command_ACL, commands[i].key) && (!is_dot_command(commands[i].key))) {
            ua->send->object_start(commands[i].key);
            ua->send->object_key_value("command", commands[i].key, "  %-18s");
            ua->send->object_key_value("description", commands[i].help, " %s\n");
            ua->send->object_key_value("arguments", commands[i].usage, 0);
            ua->send->object_key_value_bool("permission", true);
            ua->send->object_end(commands[i].key);
         }
      }
   }
   if (i == comsize && ua->argc == 2) {
      ua->send_msg(_("\nCan't find %s command.\n\n"), ua->argk[1]);
   }
   ua->send->decoration(_("\nWhen at a prompt, entering a period cancels the command.\n\n"));

   return true;
}

static bool dot_help_cmd(UaContext *ua, const char *cmd)
{
   int i, j;

   /*
    * Implement dot_help_cmd here instead of ua_dotcmds.c,
    * because comsize and commands are defined here.
    */

   /*
    * Want to display a specific help section
    */
   j = find_arg_with_value(ua, NT_("item"));
   if (j >= 0 && ua->argk[j]) {
      for (i = 0; i < comsize; i++) {
         if (bstrcmp(commands[i].key, ua->argv[j])) {
            ua->send->object_start(commands[i].key);
            ua->send->object_key_value("command", commands[i].key);
            ua->send->object_key_value("description", commands[i].help);
            ua->send->object_key_value("arguments", commands[i].usage, "%s\n", 0);
            ua->send->object_key_value_bool("permission", ua->acl_access_ok(Command_ACL, commands[i].key));
            ua->send->object_end(commands[i].key);
            break;
         }
      }
      return true;
   }

   j = find_arg(ua, NT_("all"));
   if (j >= 0) {
      /*
       * Want to display only user commands (except dot commands)
       */
      for (i = 0; i < comsize; i++) {
         if (ua->acl_access_ok(Command_ACL, commands[i].key) && (!is_dot_command(commands[i].key))) {
            ua->send->object_start(commands[i].key);
            ua->send->object_key_value("command", commands[i].key, "%s\n");
            ua->send->object_key_value("description", commands[i].help);
            ua->send->object_key_value("arguments", commands[i].usage, NULL, 0);
            ua->send->object_key_value_bool("permission", true);
            ua->send->object_end(commands[i].key);
         }
      }
   } else {
      /*
       * Want to display everything
       */
      for (i = 0; i < comsize; i++) {
         if (ua->acl_access_ok(Command_ACL, commands[i].key)) {
            ua->send->object_start(commands[i].key);
            ua->send->object_key_value("command", commands[i].key, "%s ");
            ua->send->object_key_value("description", commands[i].help, "%s -- ");
            ua->send->object_key_value("arguments", commands[i].usage, "%s\n", 0);
            ua->send->object_key_value_bool("permission", true);
            ua->send->object_end(commands[i].key);
         }
      }
   }

   return true;
}

#if 1
static bool version_cmd(UaContext *ua, const char *cmd)
{
   ua->send->object_start("version");
   ua->send->object_key_value("name", my_name, "%s ");
   ua->send->object_key_value("type", "bareos-director");
   ua->send->object_key_value("Version", "%s: ", VERSION, "%s ");
   ua->send->object_key_value("bdate", BDATE, "(%s) ");
   ua->send->object_key_value("operatingsystem", HOST_OS, "%s ");
   ua->send->object_key_value("distname", DISTNAME, "%s ");
   ua->send->object_key_value("distversion", DISTVER, "%s ");
#if defined(IS_BUILD_ON_OBS)
   ua->send->object_key_value("obsdistribution", OBS_DISTRIBUTION, "%s ");
   ua->send->object_key_value("obsarch", OBS_ARCH, "%s ");
#endif
   ua->send->object_key_value("CustomVersionId", NPRTB(me->verid), "%s\n");
   ua->send->object_end("version");

   return true;
}
#else
/**
 *  Test code -- turned on only for debug testing
 */
static bool version_cmd(UaContext *ua, const char *cmd)
{
   dbid_list ids;
   PoolMem query(PM_MESSAGE);
   open_db(ua);
   Mmsg(query, "select MediaId from Media,Pool where Pool.PoolId=Media.PoolId and Pool.Name='Full'");
   get_query_dbids(ua->jcr, ua->db, query, ids);
   ua->send_msg("num_ids=%d max_ids=%d tot_ids=%d\n", ids.num_ids, ids.max_ids, ids.tot_ids);
   for (int i=0; i < ids.num_ids; i++) {
      ua->send_msg("id=%d\n", ids.DBId[i]);
   }
   close_db(ua);

   return true;
}
#endif
