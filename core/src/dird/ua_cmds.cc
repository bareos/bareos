/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
// Kern Sibbald, September MM
/**
 * @file
 * User Agent Commands
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/backup.h"
#include "dird/ua_cmds.h"
#include "dird/ua_cmdstruct.h"
#include "dird/expand.h"
#include "dird/fd_cmds.h"
#include "dird/director_jcr_impl.h"
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
#include "include/auth_protocol_types.h"
#include "lib/bnet.h"
#include "lib/bool_string.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/version.h"

#include "dird/dbcheck_utils.h"

#include "dird/reload.h"

namespace directordaemon {

/* Imported subroutines */

/* Imported variables */

// Imported functions

/* ua_cmds.c */
extern bool AutodisplayCmd(UaContext* ua, const char* cmd);
extern bool ConfigureCmd(UaContext* ua, const char* cmd);
extern bool gui_cmd(UaContext* ua, const char* cmd);
extern bool LabelCmd(UaContext* ua, const char* cmd);
extern bool list_cmd(UaContext* ua, const char* cmd);
extern bool LlistCmd(UaContext* ua, const char* cmd);
extern bool MessagesCmd(UaContext* ua, const char* cmd);
extern bool PruneCmd(UaContext* ua, const char* cmd);
extern bool PurgeCmd(UaContext* ua, const char* cmd);
extern bool QueryCmd(UaContext* ua, const char* cmd);
extern bool RelabelCmd(UaContext* ua, const char* cmd);
extern bool RestoreCmd(UaContext* ua, const char* cmd);
extern bool show_cmd(UaContext* ua, const char* cmd);
extern bool SqlqueryCmd(UaContext* ua, const char* cmd);
extern bool StatusCmd(UaContext* ua, const char* cmd);
extern bool UpdateCmd(UaContext* ua, const char* cmd);

/* ua_dotcmds.c */
extern bool DotCatalogsCmd(UaContext* ua, const char* cmd);
extern bool DotJobdefsCmd(UaContext* ua, const char* cmd);
extern bool DotJobsCmd(UaContext* ua, const char* cmd);
extern bool DotJobstatusCmd(UaContext* ua, const char* cmd);
extern bool DotFilesetsCmd(UaContext* ua, const char* cmd);
extern bool DotClientsCmd(UaContext* ua, const char* cmd);
extern bool DotConsolesCmd(UaContext* ua, const char* cmd);
extern bool DotUsersCmd(UaContext* ua, const char* cmd);
extern bool DotMsgsCmd(UaContext* ua, const char* cmd);
extern bool DotPoolsCmd(UaContext* ua, const char* cmd);
extern bool DotScheduleCmd(UaContext* ua, const char* cmd);
extern bool DotStorageCmd(UaContext* ua, const char* cmd);
extern bool DotDefaultsCmd(UaContext* ua, const char* cmd);
extern bool DotTypesCmd(UaContext* ua, const char* cmd);
extern bool DotLevelsCmd(UaContext* ua, const char* cmd);
extern bool DotGetmsgsCmd(UaContext* ua, const char* cmd);
extern bool DotVolstatusCmd(UaContext* ua, const char* cmd);
extern bool DotMediatypesCmd(UaContext* ua, const char* cmd);
extern bool DotLocationsCmd(UaContext* ua, const char* cmd);
extern bool DotMediaCmd(UaContext* ua, const char* cmd);
extern bool DotProfilesCmd(UaContext* ua, const char* cmd);
extern bool DotAopCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsLsdirsCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsLsfilesCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsUpdateCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsGetJobidsCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsVersionsCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsRestoreCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsCleanupCmd(UaContext* ua, const char* cmd);
extern bool DotBvfsClearCacheCmd(UaContext* ua, const char* cmd);
extern bool DotApiCmd(UaContext* ua, const char* cmd);
extern bool DotSqlCmd(UaContext* ua, const char* cmd);
extern bool DotAuthorizedCmd(UaContext* ua, const char* cmd);

/* ua_status.c */
extern bool DotStatusCmd(UaContext* ua, const char* cmd);

/* Forward referenced functions */
static bool add_cmd(UaContext* ua, const char* cmd);
static bool AutomountCmd(UaContext* ua, const char* cmd);
static bool CancelCmd(UaContext* ua, const char* cmd);
static bool CreateCmd(UaContext* ua, const char* cmd);
static bool DeleteCmd(UaContext* ua, const char* cmd);
static bool DisableCmd(UaContext* ua, const char* cmd);
static bool EnableCmd(UaContext* ua, const char* cmd);
static bool EstimateCmd(UaContext* ua, const char* cmd);
static bool help_cmd(UaContext* ua, const char* cmd);
static bool DotHelpCmd(UaContext* ua, const char* cmd);
static bool MemoryCmd(UaContext* ua, const char* cmd);
static bool MountCmd(UaContext* ua, const char* cmd);
static bool noop_cmd(UaContext* ua, const char* cmd);
static bool ReleaseCmd(UaContext* ua, const char* cmd);
static bool ReloadCmd(UaContext* ua, const char* cmd);
static bool ResolveCmd(UaContext* ua, const char* cmd);
static bool SetdebugCmd(UaContext* ua, const char* cmd);
static bool SetbwlimitCmd(UaContext* ua, const char* cmd);
static bool SetipCmd(UaContext* ua, const char* cmd);
static bool time_cmd(UaContext* ua, const char* cmd);
static bool TraceCmd(UaContext* ua, const char* cmd);
static bool TruncateCmd(UaContext* ua, const char* cmd);
static bool UnmountCmd(UaContext* ua, const char* cmd);
static bool use_cmd(UaContext* ua, const char* cmd);
static bool var_cmd(UaContext* ua, const char* cmd);
static bool VersionCmd(UaContext* ua, const char* cmd);
static bool wait_cmd(UaContext* ua, const char* cmd);
static bool WhoAmICmd(UaContext* ua, const char* cmd);

static void DoJobDelete(UaContext* ua, JobId_t JobId);
static bool DeleteJobIdRange(UaContext* ua, char* tok);
static bool DeleteVolume(UaContext* ua);
static bool DeletePool(UaContext* ua);
static void DeleteJob(UaContext* ua);
static void DeleteStorage(UaContext* ua);
static bool DoTruncate(UaContext* ua,
                       MediaDbRecord& mr,
                       drive_number_t drive_number);

bool quit_cmd(UaContext* ua, const char* cmd);

const char list_cmd_usage[] = NT_(
    "backups client=<client-name> [fileset=<fileset-name>] "
    "[jobstatus=<status>] [level=<level>] [order=<asc|desc>] "
    "[limit=<number>] [days=<number>] [hours=<number>] | "
    "client=<client-name> | "
    "clients | "
    "copies jobid=<jobid> | "
    "files jobid=<jobid> | files ujobid=<complete_name> | "
    "filesets | "
    "fileset [ filesetid=<filesetid> ] | "
    "fileset [ jobid=<jobid> ] | "
    "fileset [ ujobid=<complete_name> ] | "
    "jobs [job=<job-name>] [client=<client-name>] [jobstatus=<status>] "
    "[jobtype=<jobtype>] [joblevel=<joblevel>] [volume=<volumename>] "
    "[pool=<pool>] "
    "[days=<number>] [hours=<number>] [last] [count] | "
    "job=<job-name> [client=<client-name>] [jobstatus=<status>] "
    "[jobtype=<jobtype>] [joblevel=<joblevel>] [volume=<volumename>] "
    "[days=<number>] [hours=<number>] | "
    "jobid=<jobid> | "
    "ujobid=<complete_name> | "
    "joblog jobid=<jobid> [count] | "
    "joblog ujobid=<complete_name> [count] | "
    "jobmedia jobid=<jobid> | "
    "jobmedia ujobid=<complete_name> | "
    "jobtotals | "
    "jobstatistics jobid=<jobid> | "
    "log [client=<client-name>] [reverse] | "
    "media [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | "
    "all ] | "
    "media=<media-name> | "
    "nextvolume job=<job-name> [days=<number>] | "
    "pools | "
    "pool=<pool-name> | "
    "poolid=<poolid> | "
    "storages | "
    "volumes [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> "
    "| all ] [count] | "
    "volume=<volume-name> | "
    "volumeid=<volumeid> | "
    "mediaid=<volumeid> | "
    "[current] | [enabled | disabled] | "
    "[limit=<number> [offset=<number>]]");

/**
 * Not all in alphabetical order.
 * New commands are added after existing commands with similar letters
 * to prevent breakage of existing user scripts.
 */
static struct ua_cmdstruct commands[] = {
    {NT_("."), noop_cmd, T_("no op"), NULL, true, false},
    {NT_(".actiononpurge"), DotAopCmd, T_("List possible actions on purge"),
     NULL, true, false},
    {NT_(".api"), DotApiCmd, T_("Switch between different api modes"),
     NT_("[ 0 | 1 | 2 | off | on | json ] [compact=<yes|no>]"), false, false},
    {NT_(".authorized"), DotAuthorizedCmd, T_("Check for authorization"),
     NT_("job=<job-name> | client=<client-name> | storage=<storage-name> "
         "| schedule=<schedule-name> | pool=<pool-name> | cmd=<command> "
         "| fileset=<fileset-name> | catalog=<catalog>"),
     false, false},
    {NT_(".catalogs"), DotCatalogsCmd, T_("List all catalog resources"), NULL,
     false, false},
    {NT_(".clients"), DotClientsCmd, T_("List all client resources"),
     NT_("[enabled | disabled]"), true, false},
    {NT_(".consoles"), DotConsolesCmd, T_("List all console resources"), NULL,
     true, false},
    {NT_(".users"), DotUsersCmd, T_("List all user resources"), NULL, true,
     false},
    {NT_(".defaults"), DotDefaultsCmd, T_("Get default settings"),
     NT_("job=<job-name> | client=<client-name> | storage=<storage-name> | "
         "pool=<pool-name>"),
     false, false},
    {NT_(".filesets"), DotFilesetsCmd, T_("List all filesets"), NULL, false,
     false},
    {NT_(".help"), DotHelpCmd, T_("Print parsable information about a command"),
     NT_("[ all | item=cmd ]"), false, false},
    {NT_(".jobdefs"), DotJobdefsCmd, T_("List all job defaults resources"),
     NULL, true, false},
    {NT_(".jobs"), DotJobsCmd, T_("List all job resources"),
     NT_("[type=<jobtype>] | [enabled | disabled]"), true, false},
    {NT_(".jobstatus"), DotJobstatusCmd, T_("List jobstatus information"),
     NT_("[ =<jobstatus> ]"), true, false},
    {NT_(".levels"), DotLevelsCmd, T_("List all backup levels"), NULL, false,
     false},
    {NT_(".locations"), DotLocationsCmd, NULL, NULL, true, false},
    {NT_(".messages"), DotGetmsgsCmd, T_("Display pending messages"), NULL,
     false, false},
    {NT_(".media"), DotMediaCmd, T_("List all medias"), NULL, true, false},
    {NT_(".mediatypes"), DotMediatypesCmd, T_("List all media types"), NULL,
     true, false},
    {NT_(".msgs"), DotMsgsCmd, T_("List all message resources"), NULL, false,
     false},
    {NT_(".pools"), DotPoolsCmd, T_("List all pool resources"),
     NT_("type=<pooltype>"), true, false},
    {NT_(".profiles"), DotProfilesCmd, T_("List all profile resources"), NULL,
     true, false},
    {NT_(".quit"), quit_cmd, T_("Close connection"), NULL, false, false},
    {NT_(".sql"), DotSqlCmd, T_("Send an arbitrary SQL command"),
     NT_("query=<sqlquery>"), false, true},
    {NT_(".schedule"), DotScheduleCmd, T_("List all schedule resources"),
     NT_("[enabled | disabled]"), false, false},
    {NT_(".status"), DotStatusCmd, T_("Report status"),
     NT_("dir ( current | last | header | scheduled | running | terminated ) "
         "| "
         "storage=<storage> [ header | waitreservation | devices | volumes | "
         "spooling | running | terminated ] | "
         "client=<client> [ header | terminated | running ]"),
     false, true},
    {NT_(".storages"), DotStorageCmd, T_("List all storage resources"),
     NT_("[enabled | disabled]"), true, false},
    {NT_(".types"), DotTypesCmd, T_("List all job types"), NULL, false, false},
    {NT_(".volstatus"), DotVolstatusCmd, T_("List all volume status"), NULL,
     true, false},
    {NT_(".bvfs_lsdirs"), DotBvfsLsdirsCmd, T_("List directories using BVFS"),
     NT_("jobid=<jobid> path=<path> | pathid=<pathid> [limit=<limit>] "
         "[offset=<offset>]"),
     true, true},
    {NT_(".bvfs_lsfiles"), DotBvfsLsfilesCmd, T_("List files using BVFS"),
     NT_("jobid=<jobid> path=<path> | pathid=<pathid> [limit=<limit>] "
         "[offset=<offset>]"),
     true, true},
    {NT_(".bvfs_update"), DotBvfsUpdateCmd, T_("Update BVFS cache"),
     NT_("[jobid=<jobid>]"), true, true},
    {NT_(".bvfs_get_jobids"), DotBvfsGetJobidsCmd,
     T_("Get jobids required for a restore"),
     NT_("jobid=<jobid> | ujobid=<unique-jobid> [all]"), true, true},
    {NT_(".bvfs_versions"), DotBvfsVersionsCmd, T_("List versions of a file"),
     NT_("jobid=0 client=<client-name> pathid=<path-id> filename=<file-name> "
         "[copies] [versions]"),
     true, true},
    {NT_(".bvfs_restore"), DotBvfsRestoreCmd,
     T_("Mark BVFS files/directories for restore. Stored in handle."),
     NT_("path=<handle> jobid=<jobid> [fileid=<file-id>] [dirid=<dirid>] "
         "[hardlink=<hardlink>]"),
     true, true},
    {NT_(".bvfs_cleanup"), DotBvfsCleanupCmd,
     T_("Cleanup BVFS cache for a certain handle"), NT_("path=<handle>"), true,
     true},
    {NT_(".bvfs_clear_cache"), DotBvfsClearCacheCmd, T_("Clear BVFS cache"),
     NT_("yes"), false, true},
    {NT_("add"), add_cmd, T_("Add media to a pool"),
     NT_("pool=<pool-name> storage=<storage-name> jobid=<jobid>"), false, true},
    {NT_("autodisplay"), AutodisplayCmd, T_("Autodisplay console messages"),
     NT_("on | off"), false, false},
    {NT_("automount"), AutomountCmd, T_("Automount after label"),
     NT_("on | off"), false, true},
    {NT_("cancel"), CancelCmd, T_("Cancel a job"),
     NT_("storage=<storage-name> | jobid=<jobid> | job=<job-name> | "
         "ujobid=<unique-jobid> | state=<job_state> | all yes"),
     false, true},
    {NT_("configure"), ConfigureCmd, T_("Configure director resources"),
     NT_(GetUsageStringForConsoleConfigureCommand()), false, true},
    {NT_("create"), CreateCmd, T_("Create DB Pool from resource"),
     NT_("pool=<pool-name>"), false, true},
    {NT_("delete"), DeleteCmd, T_("Delete volume, pool or job"),
     NT_("[volume=<vol-name> [yes] | volume pool=<pool-name> [yes] | "
         "storage storage=<storage-name> | pool=<pool-name> | jobid=<jobid> | "
         "jobid=<jobid1,jobid2,...> | jobid=<jobid1-jobid9>]"),
     true, true},
    {NT_("disable"), DisableCmd, T_("Disable a job/client/schedule"),
     NT_("job=<job-name> client=<client-name> schedule=<schedule-name>"), true,
     true},
    {NT_("enable"), EnableCmd, T_("Enable a job/client/schedule"),
     NT_("job=<job-name> client=<client-name> schedule=<schedule-name>"), true,
     true},
    {NT_("estimate"), EstimateCmd,
     T_("Performs FileSet estimate, listing gives full listing"),
     NT_("fileset=<fileset-name> client=<client-name> level=<level> "
         "accurate=<yes/no> job=<job-name> listing"),
     true, true},
    {NT_("exit"), quit_cmd, T_("Terminate Bconsole session"), NT_(""), false,
     false},
    {NT_("export"), ExportCmd,
     T_("Export volumes from normal slots to import/export slots"),
     NT_("storage=<storage-name> srcslots=<slot-selection> [ "
         "dstslots=<slot-selection> volume=<volume-name> scan ]"),
     true, true},
    {NT_("gui"), gui_cmd,
     T_("Switch between interactive (gui off) and non-interactive (gui on) "
        "mode"),
     NT_("on | off"), false, false},
    {NT_("help"), help_cmd, T_("Print help on specific command"),
     NT_("add autodisplay automount cancel configure create delete disable "
         "enable estimate exit gui label list llist "
         "messages memory mount prune purge quit query "
         "restore relabel release reload run status "
         "setbandwidth setdebug setip show sqlquery time trace truncate "
         "unmount umount update use var version wait"),
     false, false},
    {NT_("import"), ImportCmd,
     T_("Import volumes from import/export slots to normal slots"),
     NT_("storage=<storage-name> [ srcslots=<slot-selection> "
         "dstslots=<slot-selection> volume=<volume-name> scan ]"),
     true, true},
    {NT_("label"), LabelCmd, T_("Label a tape"),
     NT_("storage=<storage-name> volume=<volume-name> pool=<pool-name> "
         "slot=<slot> [ drive = <drivenum>] [ barcodes ] [ encrypt ] [ yes ]"),
     false, true},
    {NT_("list"), list_cmd, T_("List objects from catalog"), list_cmd_usage,
     true, true},
    {NT_("llist"), LlistCmd, T_("Full or long list like list command"),
     list_cmd_usage, true, true},
    {NT_("messages"), MessagesCmd, T_("Display pending messages"), NT_(""),
     false, false},
    {NT_("memory"), MemoryCmd, T_("Print current memory usage"), NT_(""), true,
     false},
    {NT_("mount"), MountCmd, T_("Mount storage"),
     NT_("storage=<storage-name> slot=<num> drive=<drivenum> "
         "jobid=<jobid> | job=<job-name> | ujobid=<complete_name>"),
     false, true},
    {NT_("move"), move_cmd, T_("Move slots in an autochanger"),
     NT_("storage=<storage-name> srcslots=<slot-selection> "
         "dstslots=<slot-selection>"),
     true, true},
    {NT_("prune"), PruneCmd, T_("Prune records from catalog"),
     NT_("files [client=<client>] [pool=<pool>] [yes] | "
         "jobs [client=<client>] [pool=<pool>] [yes] | "
         "volume [all] [=volume] [pool=<pool>] [yes] | "
         "stats [yes] | "
         "directory [=directory] [client=<client>] [recursive] [yes]"),
     true, true},
    {NT_("purge"), PurgeCmd, T_("Purge records from catalog"),
     NT_("[files [job=<job> | jobid=<jobid> | client=<client> | "
         "volume=<volume>]] | "
         "[jobs [client=<client> | volume=<volume>] | pool=<pool>] "
         "[jobstatus=<status>]] | "
         "[volume[=<volume>] [storage=<storage>] [pool=<pool> | allpools] "
         "[devicetype=<type>] [drive=<drivenum>] [action=<action>]] | "
         "[quota [client=<client>]]"),
     true, true},
    {NT_("quit"), quit_cmd, T_("Terminate Bconsole session"), NT_(""), false,
     false},
    {NT_("query"), QueryCmd, T_("Query catalog"), NT_(""), false, true},
    {NT_("restore"), RestoreCmd, T_("Restore files"),
     NT_("where=</path> client=<client-name> storage=<storage-name> "
         "bootstrap=<file> "
         "restorejob=<job-name> comment=<text> jobid=<jobid> "
         "fileset=<fileset-name> "
         "replace=<always|never|ifolder|ifnewer> "
         "pluginoptions=<plugin-options-string> "
         "regexwhere=<regex> fileregex=<regex> "
         "restoreclient=<client-name> backupformat=<format> "
         "pool=<pool-name> file=<filename> directory=<directory> "
         "before=<date> "
         "strip_prefix=<prefix> add_prefix=<prefix> add_suffix=<suffix> "
         "select=<date> select before current copies done all"),
     false, true},
    {NT_("relabel"), RelabelCmd, T_("Relabel a tape"),
     NT_("storage=<storage-name> oldvolume=<old-volume-name> "
         " volume=<new-volume-name> pool=<pool-name> [ encrypt ]"),
     false, true},
    {NT_("release"), ReleaseCmd, T_("Release storage"),
     NT_("storage=<storage-name> [ drive=<drivenum> ] [ alldrives ]"), true,
     true},
    {NT_("reload"), ReloadCmd, T_("Reload conf file"), NT_(""), true, true},
    {NT_("rerun"), reRunCmd, T_("Rerun a job"),
     NT_("jobid=<jobid> | since_jobid=<jobid> [ until_jobid=<jobid> ] | "
         "days=<nr_days> | hours=<nr_hours> | yes"),
     false, true},
    {NT_("resolve"), ResolveCmd, T_("Resolve a hostname"),
     NT_("client=<client-name> | storage=<storage-name> <host-name>"), false,
     true},
    {NT_("run"), RunCmd, T_("Run a job"),
     NT_("job=<job-name> client=<client-name> fileset=<fileset-name> "
         "level=<level> "
         "storage=<storage-name> where=<directory-prefix> "
         "when=<universal-time-specification> "
         "pool=<pool-name> pluginoptions=<plugin-options-string> "
         "accurate=<yes|no> comment=<text> "
         "spooldata=<yes|no> priority=<number> jobid=<jobid> "
         "catalog=<catalog> migrationjob=<job-name> "
         "backupclient=<client-name> backupformat=<format> "
         "nextpool=<pool-name> "
         "since=<universal-time-specification> verifyjob=<job-name> "
         "verifylist=<verify-list> "
         "migrationjob=<complete_name> [ yes ]"),
     false, true},
    {NT_("status"), StatusCmd, T_("Report status"),
     NT_("all | dir=<dir-name> | director | scheduler | "
         "schedule=<schedule-name> | client=<client-name> | "
         "storage=<storage-name> slots | days=<nr_days> | job=<job-name> | "
         "subscriptions [detail] [unknown] [all] | configuration"),
     true, true},
    {NT_("setbandwidth"), SetbwlimitCmd, T_("Sets bandwidth"),
     NT_("[ client=<client-name> | storage=<storage-name> | jobid=<jobid> "
         "| job=<job-name> | ujobid=<unique-jobid> state=<job_state> | all ] "
         "limit=<nn-kbs> [ yes ]"),
     true, true},
    {NT_("setdebug"), SetdebugCmd, T_("Sets debug level"),
     NT_("level=<nn> trace=0/1 timestamp=0/1 client=<client-name> | dir | "
         "storage=<storage-name> | all"),
     true, true},
    {NT_("setdevice"), SetDeviceCommand::Cmd, T_("Sets device parameter"),
     NT_("storage=<storage-name> device=<device-name> autoselect=<bool>"), true,
     true},
    {NT_("setip"), SetipCmd, T_("Sets new client address -- if authorized"),
     NT_(""), false, true},
    {NT_("show"), show_cmd, T_("Show resource records"),
     NT_("catalog=<catalog-name> | client=<client-name> | "
         "console=<console-name> | "
         "director=<director-name> | fileset=<fileset-name> | "
         "jobdefs=<job-defaults> | job=<job-name> | "
         "message=<message-resource-name> | "
         "pool=<pool-name> | profile=<profile-name> | "
         "schedule=<schedule-name> | storage=<storage-name> "
         "| catalog | clients | consoles | directors | filesets "
         "| jobdefs | jobs "
         "| messages | pools | profiles | schedules | storages "
         "| disabled [ clients | jobs | schedules ] "
         "| all [verbose]"),
     true, true},
    {NT_("sqlquery"), SqlqueryCmd, T_("Use SQL to query catalog"), NT_(""),
     false, true},
    {NT_("time"), time_cmd, T_("Print current time"), NT_(""), true, false},
    {NT_("trace"), TraceCmd, T_("Turn on/off trace to file"), NT_("on | off"),
     true, true},
    {NT_("truncate"), TruncateCmd, T_("Truncate purged volumes"),
     NT_("volstatus=Purged [storage=<storage>] [pool=<pool>] [volume=<volume>] "
         "[drive=<drivenum>] "
         "[yes]"),
     true, true},
    {NT_("unmount"), UnmountCmd, T_("Unmount storage"),
     NT_("storage=<storage-name> [ drive=<drivenum> ] "
         "jobid=<jobid> | job=<job-name> | ujobid=<complete_name>"),
     false, true},
    {NT_("umount"), UnmountCmd,
     T_("Umount - for old-time Unix guys, see unmount"),
     NT_("storage=<storage-name> [ drive=<drivenum> ] "
         "jobid=<jobid> | job=<job-name> | ujobid=<complete_name>"),
     false, true},
    {NT_("update"), UpdateCmd,
     T_("Update volume, pool, slots, job or statistics"),
     NT_("[volume=<volume-name> "
         "[volstatus=<status>] [volretention=<time-def>] "
         "actiononpurge=<action>] "
         "[pool=<pool-name>] [recycle=<yes/no>] [slot=<number>] "
         "[inchanger=<yes/no>]] "
         "| [pool=<pool-name> "
         "[maxvolbytes=<size>] [maxvolfiles=<nb>] [maxvoljobs=<nb>]"
         "[enabled=<yes/no>] [recyclepool=<pool-name>] "
         "[actiononpurge=<action>] | "
         "slots [storage=<storage-name>] [scan]] "
         "| [jobid=<jobid> [jobname=<name>] [starttime=<time-def>] "
         "[client=<client-name>] "
         "[filesetid=<fileset-id>] [jobtype=<job-type>]] "
         "| [stats [days=<number>]]"),
     true, true},
    {NT_("use"), use_cmd, T_("Use specific catalog"), NT_("catalog=<catalog>"),
     false, true},
    {NT_("var"), var_cmd, T_("Does variable expansion"), NT_(""), false, true},
    {NT_("version"), VersionCmd, T_("Print Director version"), NT_(""), true,
     false},
    {NT_("wait"), wait_cmd, T_("Wait until no jobs are running"),
     NT_("jobname=<name> | jobid=<jobid> | ujobid=<complete_name> | mount "
         "[timeout=<number>]"),
     false, false},
    {NT_("whoami"), WhoAmICmd,
     T_("Print the user name associated with this console"), NT_(""), true,
     false}};

#define comsize ((int)(sizeof(commands) / sizeof(struct ua_cmdstruct)))

bool UaContext::execute(ua_cmdstruct* cmdstruct)
{
  SetCommandDefinition(cmdstruct);
  return (cmdstruct->func)(this, cmd);
}

// Execute a command from the UA
bool Do_a_command(UaContext* ua)
{
  int i;
  int len;
  bool ok = false;
  bool found = false;
  BareosSocket* user = ua->UA_sock;

  Dmsg1(900, "Command: %s\n", ua->argk[0]);
  if (ua->argc == 0) { return false; }

  while (ua->jcr->dir_impl->res.write_storage_list
         && ua->jcr->dir_impl->res.write_storage_list->size()) {
    ua->jcr->dir_impl->res.write_storage_list->remove(0);
  }

  len = strlen(ua->argk[0]);
  for (i = 0; i < comsize; i++) { /* search for command */
    if (bstrncasecmp(ua->argk[0], commands[i].key, len)) {
      const char* command = commands[i].key;
      // Check if command permitted, but "exit/quit" and "whoami" is always OK
      if (!bstrcmp(command, NT_("exit")) && !bstrcmp(command, NT_("quit"))
          && !bstrcmp(command, NT_("whoami"))
          && !ua->AclAccessOk(Command_ACL, command, true)) {
        break;
      }

      // Check if this command is authorized in RunScript
      if (ua->runscript && !commands[i].allowed_in_runscript) {
        ua->ErrorMsg(T_("Can't use %s command in a runscript"), command);
        break;
      }

      // If we need to audit this event do it now.
      if (ua->AuditEventWanted(commands[i].audit_event)) {
        ua->LogAuditEventCmdline();
      }

      /* Some commands alter the JobStatus of the JobControlRecord.
       * As the console JobControlRecord keeps running,
       * we set it to running state again.
       * ua->jcr->setJobStatus(JS_Running)
       * isn't enough, as it does not overwrite error states. */
      ua->jcr->setJobStatus(JS_Running);

      if (ua->api) { user->signal(BNET_CMD_BEGIN); }
      ua->send->SetMode(ua->api);
      ok = ua->execute(&commands[i]);
      if (ua->api) { user->signal(ok ? BNET_CMD_OK : BNET_CMD_FAILED); }

      found = true;
      break;
    }
  }

  if (!found) {
    ua->ErrorMsg(T_("%s: is an invalid command.\n"), ua->argk[0]);
    ok = false;
  }
  ua->send->FinalizeResult(ok);

  return ok;
}

static bool IsDotCommand(const char* cmd)
{
  if (cmd && (strlen(cmd) > 0) && (cmd[0] == '.')) { return true; }
  return false;
}

// Add Volumes to an existing Pool
static bool add_cmd(UaContext* ua, const char*)
{
  PoolDbRecord pr;
  MediaDbRecord mr;
  int num, i, max, startnum;
  char name[MAX_NAME_LENGTH];
  StorageResource* store;
  slot_number_t Slot = 0;
  int8_t InChanger = 0;

  ua->SendMsg(
      T_("You probably don't want to be using this command since it\n"
         "creates database records without labeling the Volumes.\n"
         "You probably want to use the \"label\" command.\n\n"));

  if (!OpenClientDb(ua)) { return true; }

  if (!GetPoolDbr(ua, &pr)) { return true; }

  Dmsg4(120, "id=%d Num=%d Max=%d type=%s\n", pr.PoolId, pr.NumVols, pr.MaxVols,
        pr.PoolType);

  while (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
    ua->WarningMsg(T_("Pool already has maximum volumes=%d\n"), pr.MaxVols);
    if (!GetPint(ua, T_("Enter new maximum (zero for unlimited): "))) {
      return true;
    }
    pr.MaxVols = ua->pint32_val;
  }

  // Get media type
  if ((store = get_storage_resource(ua)) != NULL) {
    bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));
  } else if (!GetMediaType(ua, mr.MediaType, sizeof(mr.MediaType))) {
    return true;
  }

  if (pr.MaxVols == 0) {
    max = 1000;
  } else {
    max = pr.MaxVols - pr.NumVols;
  }
  for (;;) {
    char buf[100];

    Bsnprintf(buf, sizeof(buf),
              T_("Enter number of Volumes to create. 0=>fixed name. Max=%d: "),
              max);
    if (!GetPint(ua, buf)) { return true; }
    num = ua->pint32_val;
    if (num < 0 || num > max) {
      ua->WarningMsg(T_("The number must be between 0 and %d\n"), max);
      continue;
    }
    break;
  }

  for (;;) {
    if (num == 0) {
      if (!GetCmd(ua, T_("Enter Volume name: "))) { return true; }
    } else {
      if (!GetCmd(ua, T_("Enter base volume name: "))) { return true; }
    }

    // Don't allow | in Volume name because it is the volume separator character
    if (!IsVolumeNameLegal(ua, ua->cmd)) { continue; }
    if (strlen(ua->cmd) >= MAX_NAME_LENGTH - 10) {
      ua->WarningMsg(T_("Volume name too long.\n"));
      continue;
    }
    if (strlen(ua->cmd) == 0) {
      ua->WarningMsg(T_("Volume name must be at least one character long.\n"));
      continue;
    }
    break;
  }

  bstrncpy(name, ua->cmd, sizeof(name));
  if (num > 0) {
    bstrncat(name, "%04d", sizeof(name));

    for (;;) {
      if (!GetPint(ua, T_("Enter the starting number: "))) { return true; }
      startnum = ua->pint32_val;
      if (startnum < 1) {
        ua->WarningMsg(T_("Start number must be greater than zero.\n"));
        continue;
      }
      break;
    }
  } else {
    startnum = 1;
    num = 1;
  }

  if (store && store->autochanger) {
    if (!GetPint(ua, T_("Enter slot (0 for none): "))) { return true; }
    Slot = static_cast<slot_number_t>(ua->pint32_val);
    if (!GetYesno(ua, T_("InChanger? yes/no: "))) { return true; }
    InChanger = ua->pint32_val;
  }

  SetPoolDbrDefaultsInMediaDbr(&mr, &pr);
  for (i = startnum; i < num + startnum; i++) {
    Bsnprintf(mr.VolumeName, sizeof(mr.VolumeName), name, i);
    mr.Slot = Slot++;
    mr.InChanger = InChanger;
    mr.Enabled = VOL_ENABLED;
    SetStorageidInMr(store, &mr);
    Dmsg1(200, "Create Volume %s\n", mr.VolumeName);
    if (DbLocker _{ua->db}; !ua->db->CreateMediaRecord(ua->jcr, &mr)) {
      ua->ErrorMsg("%s", ua->db->strerror());
      return true;
    }
  }
  pr.NumVols += num;
  Dmsg0(200, "Update pool record.\n");
  if (DbLocker _{ua->db}; ua->db->UpdatePoolRecord(ua->jcr, &pr) != 1) {
    ua->WarningMsg("%s", ua->db->strerror());
    return true;
  }
  ua->SendMsg(T_("%d Volumes created in pool %s\n"), num, pr.Name);

  return true;
}

/**
 * Turn auto mount on/off
 *
 * automount on
 * automount off
 */
static bool AutomountCmd(UaContext* ua, const char*)
{
  char* onoff;

  if (ua->argc != 2) {
    if (!GetCmd(ua, T_("Turn on or off? "))) { return true; }
    onoff = ua->cmd;
  } else {
    onoff = ua->argk[1];
  }

  ua->automount = (Bstrcasecmp(onoff, NT_("off"))) ? 0 : 1;
  return true;
}

static inline bool CancelStorageDaemonJob(UaContext* ua, const char*)
{
  int i;
  StorageResource* store;

  store = get_storage_resource(ua);
  if (store) {
    // See what JobId to cancel on the storage daemon.
    i = FindArgWithValue(ua, NT_("jobid"));
    if (i >= 0) {
      if (!Is_a_number(ua->argv[i])) {
        ua->WarningMsg(T_("JobId %s not a number\n"), ua->argv[i]);
      }

      CancelStorageDaemonJob(ua, store, ua->argv[i]);
    } else {
      ua->WarningMsg(T_("Missing jobid=JobId specification\n"));
    }
  }

  return true;
}

static inline bool CancelJobs(UaContext* ua, const char*)
{
  JobControlRecord* jcr;
  alist<JobId_t*>* selection;

  selection = select_jobs(ua, "cancel");
  if (!selection) { return true; }

  // Loop over the different JobIds selected.
  for (auto* JobId : selection) {
    if (!(jcr = get_jcr_by_id(*JobId))) { continue; }

    CancelJob(ua, jcr);
    FreeJcr(jcr);
  }

  delete selection;

  return true;
}

// Cancel a job
static bool CancelCmd(UaContext* ua, const char* cmd)
{
  int i;

  // See if we need to explicitly cancel a storage daemon Job.
  i = FindArgWithValue(ua, NT_("storage"));
  if (i >= 0) {
    return CancelStorageDaemonJob(ua, cmd);
  } else {
    return CancelJobs(ua, cmd);
  }
}

/**
 * Create a Pool Record in the database.
 * It is always created from the Resource record.
 */
static bool CreateCmd(UaContext* ua, const char*)
{
  PoolResource* pool;

  if (!OpenClientDb(ua)) { return true; }

  pool = get_pool_resource(ua);
  if (!pool) { return true; }

  DbLocker _{ua->db};

  switch (CreatePool(ua->jcr, ua->db, pool, POOL_OP_CREATE)) {
    case 0:
      ua->ErrorMsg(T_("Error: Pool %s already exists.\n"
                      "Use update to change it.\n"),
                   pool->resource_name_);
      break;

    case -1:
      ua->ErrorMsg("%s", ua->db->strerror());
      break;

    default:
      break;
  }
  ua->SendMsg(T_("Pool %s created.\n"), pool->resource_name_);
  return true;
}

static inline bool SetbwlimitFiled(UaContext* ua,
                                   ClientResource* client,
                                   int64_t limit,
                                   char* Job)
{
  // Connect to File daemon
  ua->jcr->dir_impl->res.client = client;
  ua->jcr->max_bandwidth = limit;

  // Try to connect for 15 seconds
  ua->SendMsg(T_("Connecting to Client %s at %s:%d\n"), client->resource_name_,
              client->address, client->FDport);

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false)) {
    ua->ErrorMsg(T_("Failed to connect to Client.\n"));
    return true;
  }

  Dmsg0(120, "Connected to file daemon\n");
  if (!SendBwlimitToFd(ua->jcr, Job)) {
    ua->ErrorMsg(T_("Failed to set bandwidth limit on Client.\n"));
  } else {
    ua->InfoMsg(T_("OK Limiting bandwidth to %" PRId64 "kb/s %s\n"),
                limit / 1024, Job);
  }

  ua->jcr->file_bsock->signal(BNET_TERMINATE);
  ua->jcr->file_bsock->close();
  delete ua->jcr->file_bsock;
  ua->jcr->file_bsock = NULL;
  ua->jcr->dir_impl->res.client = NULL;
  ua->jcr->max_bandwidth = 0;

  return true;
}

static inline bool setbwlimit_stored(UaContext* ua,
                                     StorageResource* store,
                                     int64_t limit,
                                     char* Job)
{
  // Check the storage daemon protocol.
  switch (store->Protocol) {
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      ua->ErrorMsg(
          T_("Storage selected is NDMP storage which cannot have a bandwidth "
             "limit\n"));
      return true;
    default:
      break;
  }

  // Connect to Storage daemon
  ua->jcr->dir_impl->res.write_storage = store;
  ua->jcr->max_bandwidth = limit;

  // Try to connect for 15 seconds
  ua->SendMsg(T_("Connecting to Storage daemon %s at %s:%d\n"),
              store->resource_name_, store->address, store->SDport);

  if (!ConnectToStorageDaemon(ua->jcr, 1, 15, false)) {
    ua->ErrorMsg(T_("Failed to connect to Storage daemon.\n"));
    return true;
  }

  Dmsg0(120, "Connected to Storage daemon\n");
  if (!SendBwlimitToFd(ua->jcr, Job)) {
    ua->ErrorMsg(T_("Failed to set bandwidth limit on Storage daemon.\n"));
  } else {
    ua->InfoMsg(T_("OK Limiting bandwidth to %" PRId64 "kb/s %s\n"),
                limit / 1024, Job);
  }

  ua->jcr->store_bsock->signal(BNET_TERMINATE);
  ua->jcr->store_bsock->close();
  delete ua->jcr->store_bsock;
  ua->jcr->store_bsock = NULL;
  ua->jcr->dir_impl->res.write_storage = NULL;
  ua->jcr->max_bandwidth = 0;

  return true;
}

static bool SetbwlimitCmd(UaContext* ua, const char*)
{
  int i;
  int64_t limit = -1;
  ClientResource* client = NULL;
  StorageResource* store = NULL;
  char Job[MAX_NAME_LENGTH];
  const char* lst[] = {"job", "jobid", "ujobid", "all", "state", NULL};

  i = FindArgWithValue(ua, NT_("limit"));
  if (i >= 0) { limit = ((int64_t)atoi(ua->argv[i]) * 1024); }

  if (limit < 0) {
    if (!GetPint(ua, T_("Enter new bandwidth limit kb/s: "))) { return true; }
    limit = ((int64_t)ua->pint32_val * 1024); /* kb/s */
  }

  if (FindArgKeyword(ua, lst) > 0) {
    JobControlRecord* jcr;
    alist<JobId_t*>* selection;

    selection = select_jobs(ua, "limit");
    if (!selection) { return true; }

    // Loop over the different JobIds selected.
    for (auto* JobId : selection) {
      if (!(jcr = get_jcr_by_id(*JobId))) { continue; }

      jcr->max_bandwidth = limit;
      bstrncpy(Job, jcr->Job, sizeof(Job));
      switch (jcr->getJobType()) {
        case JT_COPY:
        case JT_MIGRATE:
          store = jcr->dir_impl->res.read_storage;
          break;
        default:
          client = jcr->dir_impl->res.client;
          break;
      }
      FreeJcr(jcr);
    }

    delete selection;
  } else if (FindArg(ua, NT_("storage")) >= 0) {
    store = get_storage_resource(ua);
  } else {
    client = get_client_resource(ua);
  }

  if (client) { return SetbwlimitFiled(ua, client, limit, Job); }

  if (store) { return setbwlimit_stored(ua, store, limit, Job); }

  return true;
}

/**
 * Set a new address in a Client resource. We do this only
 * if the Console name is the same as the Client name
 * and the Console can access the client.
 */
static bool SetipCmd(UaContext* ua, const char*)
{
  ClientResource* client;
  char buf[1024];

  if ((!ua->user_acl) || (!ua->user_acl->corresponding_resource)) {
    ua->ErrorMsg(T_("No corresponding client found.\n"));
    return false;
  }
  client = ua->GetClientResWithName(
      ua->user_acl->corresponding_resource->resource_name_);
  if (!client) {
    ua->ErrorMsg(T_("Client \"%s\" not found.\n"),
                 ua->user_acl->corresponding_resource->resource_name_);
    return false;
  }

  if (client->address) { free(client->address); }

  SockaddrToAscii(&ua->UA_sock->client_addr, buf, sizeof(buf));
  client->address = strdup(buf);
  ua->SendMsg(T_("Client \"%s\" address set to %s\n"), client->resource_name_,
              client->address);

  return true;
}

static void DoEnDisableCmd(UaContext* ua, bool setting)
{
  ScheduleResource* sched = NULL;
  ClientResource* client = NULL;
  JobResource* job = NULL;
  std::string action(setting ? "enable" : "disable");

  int i = FindArg(ua, NT_("schedule"));
  if (i >= 0) {
    i = FindArgWithValue(ua, NT_("schedule"));
    if (i >= 0) {
      sched = ua->GetScheduleResWithName(ua->argv[i]);
    } else {
      sched = select_enable_disable_schedule_resource(ua, setting);
      if (!sched) { return; }
    }

    if (!sched) {
      ua->ErrorMsg(T_("Schedule \"%s\" not found.\n"), ua->argv[i]);
      return;
    }
  } else {
    i = FindArg(ua, NT_("client"));
    if (i >= 0) {
      i = FindArgWithValue(ua, NT_("client"));
      if (i >= 0) {
        client = ua->GetClientResWithName(ua->argv[i]);
      } else {
        client = select_enable_disable_client_resource(ua, setting);
        if (!client) { return; }
      }

      if (!client) {
        ua->ErrorMsg(T_("Client \"%s\" not found.\n"), ua->argv[i]);
        return;
      }
    } else {
      i = FindArgWithValue(ua, NT_("job"));
      if (i >= 0) {
        job = ua->GetJobResWithName(ua->argv[i]);
      } else {
        job = select_enable_disable_job_resource(ua, setting);
        if (!job) { return; }
      }

      if (!job) {
        ua->ErrorMsg(T_("Job \"%s\" not found.\n"), ua->argv[i]);
        return;
      }
    }
  }

  ua->send->ObjectStart(action.c_str());
  if (sched) {
    sched->enabled = setting;
    ua->SendMsg(T_("Schedule \"%s\" %sd\n"), sched->resource_name_,
                action.c_str());
    ua->send->ArrayStart("schedules");
    ua->send->ArrayItem(sched->resource_name_);
    ua->send->ArrayEnd("schedules");
  } else if (client) {
    client->enabled = setting;
    ua->SendMsg(T_("Client \"%s\" %sd\n"), client->resource_name_,
                action.c_str());
    ua->send->ArrayStart("clients");
    ua->send->ArrayItem(client->resource_name_);
    ua->send->ArrayEnd("clients");
  } else if (job) {
    job->enabled = setting;
    ua->SendMsg(T_("Job \"%s\" %sd\n"), job->resource_name_, action.c_str());
    ua->send->ArrayStart("jobs");
    ua->send->ArrayItem(job->resource_name_);
    ua->send->ArrayEnd("jobs");
  }
  ua->send->ObjectEnd(action.c_str());

  ua->WarningMsg(
      T_("%sabling is a temporary operation until the director reloads.\n"
         "For a permanent setting, please set the value of the \"Enabled\"\n"
         "directive in the relevant configuration resource file.\n"),
      setting ? "En" : "Dis");
}

static bool EnableCmd(UaContext* ua, const char*)
{
  DoEnDisableCmd(ua, true);
  return true;
}

static bool DisableCmd(UaContext* ua, const char*)
{
  DoEnDisableCmd(ua, false);
  return true;
}

static void DoStorageSetdebug(UaContext* ua,
                              StorageResource* store,
                              int level,
                              int trace_flag,
                              int timestamp_flag)
{
  BareosSocket* sd;
  JobControlRecord* jcr = ua->jcr;
  UnifiedStorageResource lstore;

  switch (store->Protocol) {
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      return;
    default:
      break;
  }

  lstore.store = store;
  PmStrcpy(lstore.store_source, T_("unknown source"));
  SetWstorage(jcr, &lstore);

  // Try connecting for up to 15 seconds
  ua->SendMsg(T_("Connecting to Storage daemon %s at %s:%d\n"),
              store->resource_name_, store->address, store->SDport);

  if (!ConnectToStorageDaemon(jcr, 1, 15, false)) {
    ua->ErrorMsg(T_("Failed to connect to Storage daemon.\n"));
    return;
  }

  Dmsg0(120, T_("Connected to storage daemon\n"));
  sd = jcr->store_bsock;
  sd->fsend("setdebug=%d trace=%d timestamp=%d\n", level, trace_flag,
            timestamp_flag);
  if (sd->recv() >= 0) { ua->SendMsg("%s", sd->msg); }

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
static void DoClientSetdebug(UaContext* ua,
                             ClientResource* client,
                             int level,
                             int trace_flag,
                             int hangup_flag,
                             int timestamp_flag)
{
  BareosSocket* fd;

  switch (client->Protocol) {
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      return;
    default:
      break;
  }

  // Connect to File daemon
  ua->jcr->dir_impl->res.client = client;

  // Try to connect for 15 seconds
  ua->SendMsg(T_("Connecting to Client %s at %s:%d\n"), client->resource_name_,
              client->address, client->FDport);

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false)) {
    ua->ErrorMsg(T_("Failed to connect to Client.\n"));
    return;
  }

  Dmsg0(120, "Connected to file daemon\n");
  fd = ua->jcr->file_bsock;
  if (ua->jcr->dir_impl->FDVersion >= FD_VERSION_53) {
    fd->fsend("setdebug=%d trace=%d hangup=%d timestamp=%d\n", level,
              trace_flag, hangup_flag, timestamp_flag);
  } else {
    fd->fsend("setdebug=%d trace=%d hangup=%d\n", level, trace_flag,
              hangup_flag);
  }

  if (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }

  fd->signal(BNET_TERMINATE);
  fd->close();
  delete ua->jcr->file_bsock;
  ua->jcr->file_bsock = NULL;

  return;
}

static std::function<void(UaContext* ua,
                          StorageResource* store,
                          int level,
                          int trace_flag,
                          int timestamp_flag)>
    DoStorageSetdebugFunction = DoStorageSetdebug;

void SetDoStorageSetdebugFunction(std::function<void(UaContext* ua,
                                                     StorageResource* store,
                                                     int level,
                                                     int trace_flag,
                                                     int timestamp_flag)> f)
{
  // dependency injection for testing
  DoStorageSetdebugFunction = f;
}

static void AllStorageSetdebug(UaContext* ua,
                               int level,
                               int trace_flag,
                               int timestamp_flag)
{
  std::vector<StorageResource*> storages_with_unique_address;
  StorageResource* storage_in_config = nullptr;

  ResLocker _{my_config};
  do {
    storage_in_config = static_cast<StorageResource*>(
        my_config->GetNextRes(R_STORAGE, storage_in_config));

    if (storage_in_config) {
      bool is_duplicate_address = false;
      for (StorageResource* compared_store : storages_with_unique_address) {
        if (bstrcmp(compared_store->address, storage_in_config->address)
            && compared_store->SDport == storage_in_config->SDport) {
          is_duplicate_address = true;
          break;
        }
      }
      if (!is_duplicate_address) {
        storages_with_unique_address.push_back(storage_in_config);
        Dmsg2(140, "Stuffing: %s:%d\n", storage_in_config->address,
              storage_in_config->SDport);
      }
    }
  } while (storage_in_config);

  for (StorageResource* store : storages_with_unique_address) {
    DoStorageSetdebugFunction(ua, store, level, trace_flag, timestamp_flag);
  }
}

static std::function<void(UaContext* ua,
                          ClientResource* client,
                          int level,
                          int trace_flag,
                          int hangup_flag,
                          int timestamp_flag)>
    DoClientSetdebugFunction = DoClientSetdebug;

void SetDoClientSetdebugFunction(std::function<void(UaContext* ua,
                                                    ClientResource* client,
                                                    int level,
                                                    int trace_flag,
                                                    int hangup_flag,
                                                    int timestamp_flag)> f)
{
  // dependency injection for testing
  DoClientSetdebugFunction = f;
}

static void AllClientSetdebug(UaContext* ua,
                              int level,
                              int trace_flag,
                              int hangup_flag,
                              int timestamp_flag)
{
  std::vector<ClientResource*> clients_with_unique_address;
  ClientResource* client_in_config = nullptr;

  ResLocker _{my_config};
  do {
    client_in_config = static_cast<ClientResource*>(
        my_config->GetNextRes(R_CLIENT, client_in_config));

    if (client_in_config) {
      bool is_duplicate_address = false;
      for (ClientResource* compared_client : clients_with_unique_address) {
        if (bstrcmp(compared_client->address, client_in_config->address)
            && compared_client->FDport == client_in_config->FDport) {
          is_duplicate_address = true;
          break;
        }
      }
      if (!is_duplicate_address) {
        clients_with_unique_address.push_back(client_in_config);
        Dmsg2(140, "Stuffing: %s:%d\n", client_in_config->address,
              client_in_config->FDport);
      }
    }
  } while (client_in_config);

  for (ClientResource* client : clients_with_unique_address) {
    DoClientSetdebugFunction(ua, client, level, trace_flag, hangup_flag,
                             timestamp_flag);
  }
}  // namespace directordaemon

static void DoDirectorSetdebug(UaContext* ua,
                               int level,
                               int trace_flag,
                               int timestamp_flag)
{
  PoolMem tracefilename(PM_FNAME);

  debug_level = level;
  SetTrace(trace_flag);
  SetTimestamp(timestamp_flag);
  Mmsg(tracefilename, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);
  if (ua) {
    ua->SendMsg("level=%d trace=%d hangup=%d timestamp=%d tracefilename=%s\n",
                level, GetTrace(), GetHangup(), GetTimestamp(),
                tracefilename.c_str());
  }
}

void DoAllSetDebug(UaContext* ua,
                   int level,
                   int trace_flag,
                   int hangup_flag,
                   int timestamp_flag)
{
  DoDirectorSetdebug(ua, level, trace_flag, timestamp_flag);
  AllStorageSetdebug(ua, level, trace_flag, timestamp_flag);
  AllClientSetdebug(ua, level, trace_flag, hangup_flag, timestamp_flag);
}

// setdebug level=nn all trace=1/0 timestamp=1/0
static bool SetdebugCmd(UaContext* ua, const char* cmd)
{
  int i;
  int level;
  int trace_flag;
  int hangup_flag;
  int timestamp_flag;
  StorageResource* store;
  ClientResource* client;

  Dmsg1(120, "setdebug:%s:\n", cmd);

  level = -1;
  i = FindArgWithValue(ua, NT_("level"));
  if (i >= 0) { level = atoi(ua->argv[i]); }
  if (level < 0) {
    if (!GetPint(ua, T_("Enter new debug level: "))) { return true; }
    level = ua->pint32_val;
  }

  // Look for trace flag. -1 => not change
  i = FindArgWithValue(ua, NT_("trace"));
  if (i >= 0) {
    trace_flag = atoi(ua->argv[i]);
    if (trace_flag > 0) { trace_flag = 1; }
  } else {
    trace_flag = -1;
  }

  // Look for hangup (debug only) flag. -1 => not change
  i = FindArgWithValue(ua, NT_("hangup"));
  if (i >= 0) {
    hangup_flag = atoi(ua->argv[i]);
  } else {
    hangup_flag = -1;
  }

  // Look for timestamp flag. -1 => not change
  i = FindArgWithValue(ua, NT_("timestamp"));
  if (i >= 0) {
    timestamp_flag = atoi(ua->argv[i]);
    if (timestamp_flag > 0) { timestamp_flag = 1; }
  } else {
    timestamp_flag = -1;
  }

  // General debug?
  for (i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], "all")) {
      DoAllSetDebug(ua, level, trace_flag, hangup_flag, timestamp_flag);
      return true;
    }
    if (Bstrcasecmp(ua->argk[i], "dir")
        || Bstrcasecmp(ua->argk[i], "director")) {
      DoDirectorSetdebug(ua, level, trace_flag, timestamp_flag);
      return true;
    }
    if (Bstrcasecmp(ua->argk[i], "client") || Bstrcasecmp(ua->argk[i], "fd")) {
      client = NULL;
      if (ua->argv[i]) {
        client = ua->GetClientResWithName(ua->argv[i]);
        if (client) {
          DoClientSetdebug(ua, client, level, trace_flag, hangup_flag,
                           timestamp_flag);
          return true;
        }
      }
      client = select_client_resource(ua);
      if (client) {
        DoClientSetdebug(ua, client, level, trace_flag, hangup_flag,
                         timestamp_flag);
        return true;
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("store"))
        || Bstrcasecmp(ua->argk[i], NT_("storage"))
        || Bstrcasecmp(ua->argk[i], NT_("sd"))) {
      store = NULL;
      if (ua->argv[i]) {
        store = ua->GetStoreResWithName(ua->argv[i]);
        if (store) {
          DoStorageSetdebug(ua, store, level, trace_flag, timestamp_flag);
          return true;
        }
      }
      store = get_storage_resource(ua);
      if (store) {
        switch (store->Protocol) {
          case APT_NDMPV2:
          case APT_NDMPV3:
          case APT_NDMPV4:
            ua->WarningMsg(T_("Storage has non-native protocol.\n"));
            return true;
          default:
            break;
        }

        DoStorageSetdebug(ua, store, level, trace_flag, timestamp_flag);
        return true;
      }
    }
  }

  // We didn't find an appropriate keyword above, so prompt the user.
  StartPrompt(ua, T_("Available daemons are: \n"));
  AddPrompt(ua, T_("Director"));
  AddPrompt(ua, T_("Storage"));
  AddPrompt(ua, T_("Client"));
  AddPrompt(ua, T_("All"));

  switch (
      DoPrompt(ua, "", T_("Select daemon type to set debug level"), NULL, 0)) {
    case 0:
      // Director
      DoDirectorSetdebug(ua, level, trace_flag, timestamp_flag);
      break;
    case 1:
      store = get_storage_resource(ua);
      if (store) {
        switch (store->Protocol) {
          case APT_NDMPV2:
          case APT_NDMPV3:
          case APT_NDMPV4:
            ua->WarningMsg(T_("Storage has non-native protocol.\n"));
            return true;
          default:
            break;
        }
        DoStorageSetdebug(ua, store, level, trace_flag, timestamp_flag);
      }
      break;
    case 2:
      client = select_client_resource(ua);
      if (client) {
        DoClientSetdebug(ua, client, level, trace_flag, hangup_flag,
                         timestamp_flag);
      }
      break;
    case 3:
      DoAllSetDebug(ua, level, trace_flag, hangup_flag, timestamp_flag);
      break;
    default:
      break;
  }

  return true;
}

SetDeviceCommand::ArgumentsList SetDeviceCommand::ScanCommandLine(UaContext* ua)
{
  ArgumentsList arguments{{"storage", ""}, {"device", ""}, {"autoselect", ""}};

  for (int i = 1; i < ua->argc; i++) {
    try {
      auto& value = arguments.at(ua->argk[i]);
      int idx = FindArgWithValue(ua, NT_(ua->argk[i]));
      if (idx >= 0) { value = ua->argv[idx]; }
    } catch (std::out_of_range& e) {
      ua->ErrorMsg("Wrong argument: %s\n", ua->argk[i]);
      return ArgumentsList();
    }
  }

  bool argument_missing = false;
  for (const auto& arg : arguments) {
    if (arg.second.empty()) {  // value
      ua->ErrorMsg("Argument missing: %s\n", arg.first.c_str());
      argument_missing = true;
    }
  }
  if (argument_missing) { return ArgumentsList(); }

  try {
    BoolString s{arguments["autoselect"].data()};  // throws
    arguments["autoselect"].clear();
    arguments["autoselect"] = s.get<bool>() == true ? "1" : "0";
  } catch (const std::out_of_range& e) {
    ua->ErrorMsg("Wrong argument: %s\n", arguments["autoselect"].c_str());
    return ArgumentsList();
  }

  return arguments;
}

bool SetDeviceCommand::SendToSd(UaContext* ua,
                                StorageResource* store,
                                const ArgumentsList& arguments)
{
  switch (store->Protocol) {
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      return true;
    default:
      break;
  }

  UnifiedStorageResource lstore;
  lstore.store = store;
  PmStrcpy(lstore.store_source, T_("unknown source"));
  SetWstorage(ua->jcr, &lstore);

  // Try connecting for up to 15 seconds
  ua->SendMsg(T_("Connecting to Storage daemon %s at %s:%d\n"),
              store->resource_name_, store->address, store->SDport);

  if (!ConnectToStorageDaemon(ua->jcr, 1, 15, false)) {
    ua->ErrorMsg(T_("Failed to connect to Storage daemon.\n"));
    return false;
  }

  Dmsg0(120, T_("Connected to storage daemon\n"));
  ua->jcr->store_bsock->fsend("setdevice device=%s autoselect=%d",
                              arguments.at("device").c_str(),
                              std::stoi(arguments.at("autoselect")));
  if (ua->jcr->store_bsock->recv() >= 0) {
    ua->SendMsg("%s", ua->jcr->store_bsock->msg);
  }

  ua->jcr->store_bsock->signal(BNET_TERMINATE);
  ua->jcr->store_bsock->close();
  delete ua->jcr->store_bsock;
  ua->jcr->store_bsock = nullptr;

  return true;
}


// setdevice storage=<storage-name> device=<device-name> autoselect=<bool>
bool SetDeviceCommand::Cmd(UaContext* ua, const char*)
{
  auto arguments = ScanCommandLine(ua);

  if (arguments.empty()) {
    ua->SendCmdUsage("");
    return false;
  }

  if (ua->AclHasRestrictions(Storage_ACL)) {
    if (!ua->AclAccessOk(Storage_ACL, arguments["storage"].c_str())) {
      std::string err{"Access to storage "};
      err += "\"" + arguments["storage"] + "\" forbidden\n";
      ua->ErrorMsg("%s", err.c_str());
      return false;
    }
  }

  StorageResource* sd = ua->GetStoreResWithName(arguments["storage"].c_str());

  if (sd == nullptr) {
    ua->ErrorMsg(T_("Storage \"%s\" not found.\n"),
                 arguments["storage"].c_str());
    return false;
  }

  return SendToSd(ua, sd, arguments);
}

// Resolve a hostname.
static bool ResolveCmd(UaContext* ua, const char*)
{
  StorageResource* storage = NULL;
  ClientResource* client = NULL;

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("client"))
        || Bstrcasecmp(ua->argk[i], NT_("fd"))) {
      if (ua->argv[i]) {
        client = ua->GetClientResWithName(ua->argv[i]);
        if (!client) {
          ua->ErrorMsg(T_("Client \"%s\" not found.\n"), ua->argv[i]);
          return true;
        }

        *ua->argk[i] = 0; /* zap keyword already visited */
        continue;
      } else {
        ua->ErrorMsg(T_("Client name missing.\n"));
        return true;
      }
    } else if (Bstrcasecmp(ua->argk[i], NT_("storage"))) {
      if (ua->argv[i]) {
        storage = ua->GetStoreResWithName(ua->argv[i]);
        if (!storage) {
          ua->ErrorMsg(T_("Storage \"%s\" not found.\n"), ua->argv[i]);
          return true;
        }

        *ua->argk[i] = 0; /* zap keyword already visited */
        continue;
      } else {
        ua->ErrorMsg(T_("Storage name missing.\n"));
        return true;
      }
    }
  }

  if (client) { DoClientResolve(ua, client); }

  if (storage) { DoStorageResolve(ua, storage); }

  if (!client && !storage) {
    dlist<IPADDR>* addr_list;
    const char* errstr;
    char addresses[2048];

    for (int i = 1; i < ua->argc; i++) {
      if (!*ua->argk[i]) { continue; }

      if ((addr_list = BnetHost2IpAddrs(ua->argk[i], 0, &errstr)) == NULL) {
        ua->ErrorMsg(T_("%s Failed to resolve %s\n"), my_name, ua->argk[i]);
        return false;
      }
      ua->SendMsg(
          T_("%s resolves %s to %s\n"), my_name, ua->argk[i],
          BuildAddressesString(addr_list, addresses, sizeof(addresses), false));
      FreeAddresses(addr_list);
    }
  }

  return true;
}

// Turn debug tracing to file on/off
static bool TraceCmd(UaContext* ua, const char*)
{
  char* onoff;

  if (ua->argc != 2) {
    if (!GetCmd(ua, T_("Turn on or off? "))) { return true; }
    onoff = ua->cmd;
  } else {
    onoff = ua->argk[1];
  }


  SetTrace((Bstrcasecmp(onoff, NT_("off"))) ? false : true);
  return true;
}

static bool var_cmd(UaContext* ua, const char*)
{
  if (!OpenClientDb(ua)) { return true; }

  char* var;
  for (var = ua->cmd; *var != ' ';) { /* skip command */
    var++;
  }
  while (*var == ' ') { /* skip spaces */
    var++;
  }
  Dmsg1(100, "Var=%s:\n", var);
  POOLMEM* val = GetPoolMemory(PM_FNAME);
  VariableExpansion(ua->jcr, var, val);
  ua->SendMsg("%s\n", val);
  FreePoolMemory(val);
  return true;
}

static bool EstimateCmd(UaContext* ua, const char*)
{
  JobResource* job = NULL;
  ClientResource* client = NULL;
  FilesetResource* fileset = NULL;
  int listing = 0;
  JobControlRecord* jcr = ua->jcr;
  bool accurate_set = false;
  bool accurate = false;

  jcr->setJobLevel(L_FULL);
  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("client"))
        || Bstrcasecmp(ua->argk[i], NT_("fd"))) {
      if (ua->argv[i]) {
        client = ua->GetClientResWithName(ua->argv[i]);
        if (!client) {
          ua->ErrorMsg(T_("Client \"%s\" not found.\n"), ua->argv[i]);
          return false;
        }
        continue;
      } else {
        ua->ErrorMsg(T_("Client name missing.\n"));
        return false;
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("job"))) {
      if (ua->argv[i]) {
        job = ua->GetJobResWithName(ua->argv[i]);
        if (!job) {
          ua->ErrorMsg(T_("Job \"%s\" not found.\n"), ua->argv[i]);
          return false;
        }
        continue;
      } else {
        ua->ErrorMsg(T_("Job name missing.\n"));
        return false;
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("fileset"))) {
      if (ua->argv[i]) {
        fileset = ua->GetFileSetResWithName(ua->argv[i]);
        if (!fileset) {
          ua->ErrorMsg(T_("Fileset \"%s\" not found.\n"), ua->argv[i]);
          return false;
        }
        continue;
      } else {
        ua->ErrorMsg(T_("Fileset name missing.\n"));
        return false;
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("listing"))) {
      listing = 1;
      continue;
    }

    if (Bstrcasecmp(ua->argk[i], NT_("level"))) {
      if (ua->argv[i]) {
        if (!GetLevelFromName(jcr, ua->argv[i])) {
          ua->ErrorMsg(T_("Level \"%s\" not valid.\n"), ua->argv[i]);
        }
        continue;
      } else {
        ua->ErrorMsg(T_("Level value missing.\n"));
        return false;
      }
    }

    if (Bstrcasecmp(ua->argk[i], NT_("accurate"))) {
      if (ua->argv[i]) {
        if (!IsYesno(ua->argv[i], &accurate)) {
          ua->ErrorMsg(
              T_("Invalid value for accurate. "
                 "It must be yes or no.\n"));
        }
        accurate_set = true;
        continue;
      } else {
        ua->ErrorMsg(T_("Accurate value missing.\n"));
        return false;
      }
    }
  }

  if (!job && !(client && fileset)) {
    if (!(job = select_job_resource(ua))) { return false; }
  }

  if (!job) {
    job = ua->GetJobResWithName(ua->argk[1]);
    if (!job) {
      ua->ErrorMsg(T_("No job specified.\n"));
      return false;
    }
  }

  switch (job->JobType) {
    case JT_BACKUP:
      break;
    default:
      ua->ErrorMsg(T_("Wrong job specified of type %s.\n"),
                   job_type_to_str(job->JobType));
      return false;
  }

  if (!client) { client = job->client; }

  if (!fileset) { fileset = job->fileset; }

  if (!client) {
    ua->ErrorMsg(T_("No client specified or selected.\n"));
    return false;
  }

  if (!fileset) {
    ua->ErrorMsg(T_("No fileset specified or selected.\n"));
    return false;
  }

  jcr->dir_impl->res.client = client;
  jcr->dir_impl->res.fileset = fileset;
  CloseDb(ua);

  switch (client->Protocol) {
    case APT_NATIVE:
      break;
    default:
      ua->ErrorMsg(T_("Estimate is only supported on native clients.\n"));
      return false;
  }

  if (job->pool->catalog) {
    ua->catalog = job->pool->catalog;
  } else {
    ua->catalog = client->catalog;
  }

  if (!OpenDb(ua)) { return false; }

  jcr->dir_impl->res.job = job;
  jcr->setJobType(JT_BACKUP);
  jcr->start_time = time(NULL);
  InitJcrJobRecord(jcr);

  if (!GetOrCreateClientRecord(jcr)) { return true; }

  if (!GetOrCreateFilesetRecord(jcr)) { return true; }

  GetLevelSinceTime(jcr);

  ua->SendMsg(T_("Connecting to Client %s at %s:%d\n"),
              jcr->dir_impl->res.client->resource_name_,
              jcr->dir_impl->res.client->address,
              jcr->dir_impl->res.client->FDport);
  if (!ConnectToFileDaemon(jcr, 1, 15, false)) {
    ua->ErrorMsg(T_("Failed to connect to Client.\n"));
    return false;
  }

  if (!SendJobInfoToFileDaemon(jcr)) {
    ua->ErrorMsg(T_("Failed to connect to Client.\n"));
    return false;
  }

  // The level string change if accurate mode is enabled
  if (accurate_set) {
    jcr->accurate = accurate;
  } else {
    jcr->accurate = job->accurate;
  }

  if (!SendLevelCommand(jcr)) { goto bail_out; }

  if (!SendIncludeExcludeLists(jcr)) {
    ua->ErrorMsg(T_("Error sending include and exclude lists.\n"));
    goto bail_out;
  }

  // If the job is in accurate mode, we send the list of all files to FD.
  Dmsg1(40, "estimate accurate=%d\n", jcr->accurate);
  if (!SendAccurateCurrentFiles(jcr)) { goto bail_out; }

  jcr->file_bsock->fsend("estimate listing=%d\n", listing);
  while (jcr->file_bsock->recv() >= 0) {
    ua->SendMsg("%s", jcr->file_bsock->msg);
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

// Print time
static bool time_cmd(UaContext* ua, const char*)
{
  char sdt[50];
  time_t ttime = time(NULL);

  ua->send->ObjectStart("time");

  bstrftime(sdt, sizeof(sdt), ttime, "%a %d-%b-%Y %H:%M:%S");
  ua->send->ObjectKeyValue("full", sdt, "%s\n");

  bstrftime(sdt, sizeof(sdt), ttime, "%Y");
  ua->send->ObjectKeyValue("year", sdt);
  bstrftime(sdt, sizeof(sdt), ttime, "%m");
  ua->send->ObjectKeyValue("month", sdt);
  bstrftime(sdt, sizeof(sdt), ttime, "%d");
  ua->send->ObjectKeyValue("day", sdt);
  bstrftime(sdt, sizeof(sdt), ttime, "%H");
  ua->send->ObjectKeyValue("hour", sdt);
  bstrftime(sdt, sizeof(sdt), ttime, "%M");
  ua->send->ObjectKeyValue("minute", sdt);
  bstrftime(sdt, sizeof(sdt), ttime, "%S");
  ua->send->ObjectKeyValue("second", sdt);

  ua->send->ObjectEnd("time");

  return true;
}

StorageResource* UaContext::GetStoreResWithId(DBId_t id,
                                              bool audit_event,
                                              bool lock)
{
  StorageDbRecord storage_dbr;

  storage_dbr.StorageId = id;
  if (db->GetStorageRecord(jcr, &storage_dbr)) {
    return GetStoreResWithName(storage_dbr.Name, audit_event, lock);
  }
  return NULL;
}


/**
 * truncate command. Truncates volumes (volume files) on the storage daemon.
 *
 * usage:
 * truncate volstatus=Purged [storage=<storage>] [pool=<pool>]
 * [volume=<volume>] [drive=<drivenum>] [yes]
 */
static bool TruncateCmd(UaContext* ua, const char*)
{
  bool result = false;
  int parsed_args = 1; /* start at 1, as command itself is also counted */
  char esc[MAX_NAME_LENGTH * 2 + 1];
  PoolMem tmp(PM_MESSAGE);
  PoolMem volumes(PM_MESSAGE);
  dbid_list mediaIds;
  MediaDbRecord mr;
  PoolDbRecord pool_dbr;
  StorageDbRecord storage_dbr;
  drive_number_t drive_number = 0;
  /* Look for volumes that can be recycled,
   * are enabled and have used more than the first block. */
  mr.Recycle = 1;
  mr.Enabled = VOL_ENABLED;
  mr.VolBytes = (512 * 126); /* search volumes with more than 64,512 bytes
                                (DEFAULT_BLOCK_SIZE) */

  if (!(ua->argc > 1)) {
    ua->SendCmdUsage(T_("missing parameter"));
    return false;
  }

  /* arg: volstatus=Purged */
  if (int i = FindArgWithValue(ua, "volstatus"); i < 0) {
    ua->SendCmdUsage(T_("required parameter 'volstatus' missing"));
    return false;
  } else if (!(Bstrcasecmp(ua->argv[i], "Purged"))) {
    ua->SendCmdUsage(T_("Invalid parameter. 'volstatus' must be 'Purged'."));
    return false;
  }
  parsed_args++;

  // Look for Purged volumes.
  bstrncpy(mr.VolStatus, "Purged", sizeof(mr.VolStatus));

  if (!OpenDb(ua)) {
    ua->ErrorMsg("Failed to open db.\n");
    goto bail_out;
  }

  ua->send->AddAclFilterTuple(2, Pool_ACL);
  ua->send->AddAclFilterTuple(3, Storage_ACL);

  /* storage parameter is only required
   * if ACL forbids access to all storages.
   * Otherwise the user should not be asked for this parameter. */
  if (int i = FindArgWithValue(ua, "storage");
      (i >= 0) || ua->AclHasRestrictions(Storage_ACL)) {
    if (!SelectStorageDbr(ua, &storage_dbr, "storage")) { goto bail_out; }
    mr.StorageId = storage_dbr.StorageId;
    parsed_args++;
  }

  /* pool parameter is only required
   * if ACL forbids access to all pools.
   * Otherwise the user should not be asked for this parameter. */
  if (int i = FindArgWithValue(ua, "pool");
      (i >= 0) || ua->AclHasRestrictions(Pool_ACL)) {
    if (!SelectPoolDbr(ua, &pool_dbr, "pool")) { goto bail_out; }
    mr.PoolId = pool_dbr.PoolId;
    if (i >= 0) { parsed_args++; }
  }

  /* parse volume parameter.
   * Currently only support one volume parameter
   * (multiple volume parameter have been intended before,
   * but this causes problems with parsing and ACL handling). */
  if (int i = FindArgWithValue(ua, "volume"); i >= 0) {
    if (IsNameValid(ua->argv[i])) {
      ua->db->EscapeString(ua->jcr, esc, ua->argv[i], strlen(ua->argv[i]));
      if (!*volumes.c_str()) {
        Mmsg(tmp, "'%s'", esc);
      } else {
        Mmsg(tmp, ",'%s'", esc);
      }
      volumes.strcat(tmp.c_str());
      parsed_args++;
    }
  }

  if (int i = FindArgWithValue(ua, "drive"); i >= 0) {
    if (!IsAnInteger(ua->argv[i])) {
      PoolMem msg;
      msg.bsprintf(T_("Drive number must be integer but was : %s\n"),
                   ua->argv[i]);
      ua->SendCmdUsage(msg.c_str());
    } else {
      drive_number = atoi(ua->argv[i]);
      parsed_args++;
    }
  }

  if (FindArg(ua, NT_("yes")) >= 0) {
    /* parameter yes is evaluated at 'GetConfirmation' */
    parsed_args++;
  }

  if (parsed_args != ua->argc) {
    ua->SendCmdUsage(T_("Invalid parameter."));
    goto bail_out;
  }

  /* create sql query string (in ua->db->cmd) */
  if (!ua->db->PrepareMediaSqlQuery(ua->jcr, &mr, tmp, volumes)) {
    ua->ErrorMsg(T_("Invalid parameter (failed to create sql query).\n"));
    goto bail_out;
  }

  /* execute query and display result */
  ua->db->ListSqlQuery(ua->jcr, tmp.c_str(), ua->send, HORZ_LIST, "volumes",
                       true);

  /* execute query to get media ids.
   * Second execution is only required,
   * because function is also used in other contextes. */
  // tmp.strcpy(ua->db->cmd);
  if (!ua->db->GetQueryDbids(ua->jcr, tmp, mediaIds)) {
    Dmsg0(100, "No results from db_get_query_dbids\n");
    goto bail_out;
  }

  if (!mediaIds.size()) {
    Dmsg0(100, "Results are empty\n");
    // no results should not count as an error
    result = true;
    goto bail_out;
  }

  if (!ua->db->VerifyMediaIdsFromSingleStorage(ua->jcr, mediaIds)) {
    ua->ErrorMsg(
        "Selected volumes are from different storages. "
        "This is not supported. Please choose only volumes from a single "
        "storage.\n");
    goto bail_out;
  }

  mr.MediaId = mediaIds.get(0);
  if (!ua->db->GetMediaRecord(ua->jcr, &mr)) { goto bail_out; }

  if (ua->GetStoreResWithId(mr.StorageId)->Protocol != APT_NATIVE) {
    ua->WarningMsg(
        T_("Storage uses a non-native protocol. Truncate is only supported for "
           "native protocols.\n"));
    goto bail_out;
  }

  if (!GetConfirmation(ua, T_("Truncate listed volumes (yes/no)? "))) {
    goto bail_out;
  }

  // Loop over the candidate Volumes and actually truncate them
  for (int i = 0; i < mediaIds.size(); i++) {
    mr.MediaId = mediaIds.get(i);
    if (!ua->db->GetMediaRecord(ua->jcr, &mr)) {
      Dmsg1(0, "Can't find MediaId=%" PRIdbid "\n", mr.MediaId);
    } else {
      DoTruncate(ua, mr, drive_number);
    }
  }
  result = true;

bail_out:
  CloseDb(ua);
  if (ua->jcr->store_bsock) {
    ua->jcr->store_bsock->signal(BNET_TERMINATE);
    ua->jcr->store_bsock->close();
    delete ua->jcr->store_bsock;
    ua->jcr->store_bsock = NULL;
  }
  return result;
}

static bool DoTruncate(UaContext* ua,
                       MediaDbRecord& mr,
                       drive_number_t drive_number)
{
  bool retval = false;
  StorageDbRecord storage_dbr;
  PoolDbRecord pool_dbr;

  storage_dbr.StorageId = mr.StorageId;
  if (!ua->db->GetStorageRecord(ua->jcr, &storage_dbr)) {
    ua->ErrorMsg("failed to determine storage for id %" PRIdbid "\n",
                 mr.StorageId);
    goto bail_out;
  }

  pool_dbr.PoolId = mr.PoolId;
  if (!ua->db->GetPoolRecord(ua->jcr, &pool_dbr)) {
    ua->ErrorMsg("failed to determine pool for id %" PRIdbid "\n", mr.PoolId);
    goto bail_out;
  }

  // Choose storage
  ua->jcr->dir_impl->res.write_storage
      = ua->GetStoreResWithName(storage_dbr.Name);
  if (!ua->jcr->dir_impl->res.write_storage) {
    ua->ErrorMsg("failed to determine storage resource by name %s\n",
                 storage_dbr.Name);
    goto bail_out;
  }

  if (SendLabelRequest(ua, ua->jcr->dir_impl->res.write_storage, &mr, &mr,
                       &pool_dbr,
                       /* bool media_record_exists */
                       true,
                       /* bool relabel */
                       true,
                       /* drive_number_t drive */
                       drive_number,
                       /* slot_number_t slot */
                       0)) {
    ua->SendMsg(T_("The volume '%s' has been truncated.\n"), mr.VolumeName);
    retval = true;
  }

bail_out:
  ua->jcr->dir_impl->res.write_storage = NULL;
  return retval;
}

// Reload the conf file
static bool ReloadCmd(UaContext* ua, const char*)
{
  bool result;

  result = DoReloadConfig();

  ua->send->ObjectStart("reload");
  if (result) {
    ua->send->ObjectKeyValueBool("success", result, "reloaded\n");
  } else {
    ua->send->ObjectKeyValueBool("success", result, "failed to reload\n");
  }
  ua->send->ObjectEnd("reload");

  return result;
}

/**
 * Delete Pool records (should purge Media with it).
 *
 * delete pool=<pool-name>
 * delete volume pool=<pool-name> volume=<name>
 * delete jobid=<jobid>
 */
static bool DeleteCmd(UaContext* ua, const char*)
{
  static const char* keywords[]
      = {NT_("volume"), NT_("pool"), NT_("jobid"), NT_("storage"), NULL};

  if (!OpenClientDb(ua, true)) { return true; }
  ua->send->ObjectStart("deleted");

  int keyword = FindArgKeyword(ua, keywords);
  if (keyword < 0) {
    ua->WarningMsg(
        T_("In general it is not a good idea to delete either a\n"
           "Pool or a Volume since they may contain data.\n\n"));

    keyword
        = DoKeywordPrompt(ua, T_("Choose catalog item to delete"), keywords);
  }

  switch (keyword) {
    case 0:
      DeleteVolume(ua);
      break;
    case 1:
      DeletePool(ua);
      break;
    case 2:
      DeleteJob(ua);
      break;
    case 3:
      DeleteStorage(ua);
      break;
    default:
      ua->WarningMsg(T_("Nothing done.\n"));
      break;
  }
  ua->send->ObjectEnd("deleted");
  return true;
}

static void DeleteStorage(UaContext* ua)
{
  std::string given_storagename;
  if (FindArgWithValue(ua, NT_("storage")) <= 0) {
    if (GetCmd(ua, T_("Enter storage to delete: "))) {
      given_storagename = ua->cmd;
    }
  } else {
    given_storagename = ua->argv[FindArgWithValue(ua, NT_("storage"))];
  }

  std::vector<std::string> orphaned_storages
      = get_orphaned_storages_names(ua->db);

  ua->SendMsg(T_("Found %" PRIuz " orphaned Storage records.\n"),
              orphaned_storages.size());

  if (std::find(orphaned_storages.begin(), orphaned_storages.end(),
                given_storagename)
      == orphaned_storages.end()) {
    ua->ErrorMsg(
        T_("The given storage '%s' either does not exist at all, or still "
           "exists in the configuration. In order to remove a storage "
           "from the catalog, its configuration must be removed first.\n"),
        given_storagename.c_str());
    return;
  }

  std::vector<int> storages_to_be_deleted;
  if (orphaned_storages.size() > 0) {
    for (int i = orphaned_storages.size() - 1; i >= 0; i--) {
      if (orphaned_storages[i] != given_storagename) {
        orphaned_storages.erase(orphaned_storages.begin() + i);
      }
    }
    storages_to_be_deleted
        = get_deletable_storageids(ua->db, orphaned_storages);
  }

  if (storages_to_be_deleted.size() > 0) {
    ua->SendMsg(T_("Deleting %" PRIuz " orphaned storage record(s).\n"),
                storages_to_be_deleted.size());
    delete_storages(ua->db, storages_to_be_deleted);
  }
}

/**
 * DeleteJob has been modified to parse JobID lists like the following:
 * delete JobID=3,4,6,7-11,14
 *
 * Thanks to Phil Stracchino for the above addition.
 */
static void DeleteJob(UaContext* ua)
{
  int i;
  JobId_t JobId;
  char *s, *sep, *tok;

  if (FindArgWithValue(ua, NT_("jobid")) <= 0) {
    if (GetPint(ua, T_("Enter JobId to delete: "))) {
      JobId = ua->int64_val;
      DoJobDelete(ua, JobId);
    }
    return;
  }

  ua->send->ArrayStart("jobids");
  while ((i = FindArgWithValue(ua, NT_("jobid"))) > 0) {
    *ua->argk[i] = 0; /* zap keyword already visited */
    if (strchr(ua->argv[i], ',') || strchr(ua->argv[i], '-')) {
      s = strdup(ua->argv[i]);
      tok = s;

      /* We could use strtok() here.  But we're not going to, because:
       * (a) strtok() is deprecated, having been replaced by strsep();
       * (b) strtok() is broken in significant ways.
       * we could use strsep() instead, but it's not universally available.
       * so we grow our own using strchr(). */
      sep = strchr(tok, ',');
      while (sep != NULL) {
        *sep = '\0';
        if (!DeleteJobIdRange(ua, tok)) {
          if (Is_a_number(tok)) {
            JobId = (JobId_t)str_to_uint64(tok);
            DoJobDelete(ua, JobId);
          } else {
            ua->ErrorMsg(T_("Illegal JobId %s ignored\n"), tok);
          }
        }
        tok = ++sep;
        sep = strchr(tok, ',');
      }

      // Pick up the last token
      if (!DeleteJobIdRange(ua, tok)) {
        if (Is_a_number(tok)) {
          JobId = (JobId_t)str_to_uint64(tok);
          DoJobDelete(ua, JobId);
        } else {
          ua->ErrorMsg(T_("Illegal JobId %s ignored\n"), tok);
        }
      }

      free(s);
    } else {
      if (Is_a_number(ua->argv[i])) {
        JobId = (JobId_t)str_to_uint64(ua->argv[i]);
        DoJobDelete(ua, JobId);
      } else {
        ua->ErrorMsg(T_("Illegal JobId %s ignored\n"), ua->argv[i]);
      }
    }
  }
  ua->send->ArrayEnd("jobids");
}

// We call DeleteJobIdRange to parse range tokens and iterate over ranges
static bool DeleteJobIdRange(UaContext* ua, char* tok)
{
  char buf[64];
  char* tok2;
  JobId_t j, j1, j2;

  tok2 = strchr(tok, '-');
  if (!tok2) { return false; }

  *tok2 = '\0';
  tok2++;

  if (Is_a_number(tok) && Is_a_number(tok2)) {
    j1 = (JobId_t)str_to_uint64(tok);
    j2 = (JobId_t)str_to_uint64(tok2);

    if (j2 > j1) {
      /* See if the range is big if more then 25 Jobs are deleted
       * ask the user for confirmation. */
      if ((!ua->batch) && ((j2 - j1) > 25)) {
        Bsnprintf(buf, sizeof(buf),
                  T_("Are you sure you want to delete %d JobIds ? (yes/no): "),
                  j2 - j1);
        if (!GetYesno(ua, buf) || !ua->pint32_val) { return true; }
      }
      for (j = j1; j <= j2; j++) { DoJobDelete(ua, j); }
    } else {
      ua->ErrorMsg(T_("Illegal JobId range %s - %s should define increasing "
                      "JobIds, ignored\n"),
                   tok, tok2);
    }
  } else {
    ua->ErrorMsg(T_("Illegal JobId range %s - %s, ignored\n"), tok, tok2);
  }

  return true;
}

// DoJobDelete now performs the actual delete operation atomically
static void DoJobDelete(UaContext* ua, JobId_t JobId)
{
  char ed1[50];

  edit_int64(JobId, ed1);
  PurgeJobsFromCatalog(ua, ed1);
  ua->send->ArrayItem(
      ed1, T_("Jobid %s and associated records deleted from the catalog.\n"));
}

// Delete media records from database -- dangerous
static bool DeleteVolume(UaContext* ua)
{
  MediaDbRecord mr;
  char buf[1000];
  db_list_ctx lst;

  if (!SelectMediaDbr(ua, &mr)) { return true; }
  ua->WarningMsg(T_("\nThis command will delete volume %s\n"
                    "and all Jobs saved on that volume from the Catalog\n"),
                 mr.VolumeName);

  if ((!ua->batch) && (FindArg(ua, "yes") < 0)) {
    Bsnprintf(buf, sizeof(buf),
              T_("Are you sure you want to delete Volume \"%s\"? (yes/no): "),
              mr.VolumeName);
    if (!GetYesno(ua, buf)) { return true; }
    if (!ua->pint32_val) { return true; }
  }

  // If not purged, do it
  if (!bstrcmp(mr.VolStatus, "Purged")) {
    if (!ua->db->GetVolumeJobids(&mr, &lst)) {
      ua->ErrorMsg(T_("Can't list jobs on this volume\n"));
      return true;
    }
    if (!lst.empty()) {
      PurgeJobsFromCatalog(ua, lst.GetAsString().c_str());
      ua->send->ArrayStart("jobids");
      for (const std::string& item : lst) { ua->send->ArrayItem(item.c_str()); }
      ua->send->ArrayEnd("jobids");
      ua->InfoMsg(T_("Deleted %" PRIuz
                     " jobs and associated records deleted from the catalog "
                     "(jobids: %s).\n"),
                  lst.size(), lst.GetAsString().c_str());
    }
  }

  ua->db->DeleteMediaRecord(ua->jcr, &mr);
  ua->send->ArrayStart("volumes");
  ua->send->ArrayItem(mr.VolumeName, T_("Volume %s deleted.\n"));
  ua->send->ArrayEnd("volumes");

  return true;
}

// Delete a pool record from the database -- dangerous.
static bool DeletePool(UaContext* ua)
{
  PoolDbRecord pr;
  char buf[200];

  if (ua->batch) {
    ua->ErrorMsg(T_(
        "Deleting pools from the catalog is not supported in batch mode.\n"));
    return false;
  }

  if (!GetPoolDbr(ua, &pr)) { return true; }
  Bsnprintf(buf, sizeof(buf),
            T_("Are you sure you want to delete Pool \"%s\"? (yes/no): "),
            pr.Name);
  if (!GetYesno(ua, buf)) { return true; }
  if (ua->pint32_val) { ua->db->DeletePoolRecord(ua->jcr, &pr); }
  return true;
}

static bool MemoryCmd(UaContext* ua, const char*)
{
  ListDirStatusHeader(ua);
  return true;
}

static void DoMountCmd(UaContext* ua, const char* cmd)
{
  UnifiedStorageResource store;
  drive_number_t nr_drives;
  drive_number_t drive = kInvalidDriveNumber;
  slot_number_t slot = kInvalidSlotNumber;
  bool do_alldrives = false;

  if ((bstrcmp(cmd, "release") || bstrcmp(cmd, "unmount"))
      && FindArg(ua, "alldrives") >= 0) {
    do_alldrives = true;
  }

  if (!OpenClientDb(ua)) { return; }
  Dmsg2(120, "%s: %s\n", cmd, ua->UA_sock->msg);

  store.store = get_storage_resource(ua, true, false);
  if (!store.store) { return; }

  PmStrcpy(store.store_source, T_("unknown source"));
  SetWstorage(ua->jcr, &store);
  if (!do_alldrives) {
    drive = GetStorageDrive(ua, store.store);
    if (drive == kInvalidDriveNumber) { return; }
  }
  if (bstrcmp(cmd, "mount")) { slot = GetStorageSlot(ua, store.store); }

  Dmsg3(120, "Found storage, MediaType=%s DevName=%s drive=%hd\n",
        store.store->media_type, store.store->dev_name(), drive);

  if (!do_alldrives) {
    DoAutochangerVolumeOperation(ua, store.store, cmd, drive, slot);
  } else {
    nr_drives = GetNumDrives(ua, ua->jcr->dir_impl->res.write_storage);
    for (drive_number_t i = 0; i < nr_drives; i++) {
      DoAutochangerVolumeOperation(ua, store.store, cmd, i, slot);
    }
  }

  InvalidateVolList(store.store);
}

// mount [storage=<name>] [drive=nn] [slot=mm]
static bool MountCmd(UaContext* ua, const char*)
{
  DoMountCmd(ua, "mount"); /* mount */
  return true;
}

// unmount [storage=<name>] [drive=nn]
static bool UnmountCmd(UaContext* ua, const char*)
{
  DoMountCmd(ua, "unmount"); /* unmount */
  return true;
}

// Perform a NO-OP.
static bool noop_cmd(UaContext* ua, const char*)
{
  if (ua->api) { ua->signal(BNET_CMD_BEGIN); }

  if (ua->api) { ua->signal(BNET_CMD_OK); }

  return true; /* no op */
}

// release [storage=<name>] [drive=nn]
static bool ReleaseCmd(UaContext* ua, const char*)
{
  DoMountCmd(ua, "release"); /* release */
  return true;
}

/**
 * Switch databases
 * use catalog=<name>
 */
static bool use_cmd(UaContext* ua, const char*)
{
  CatalogResource *oldcatalog, *catalog;

  CloseDb(ua); /* close any previously open db */
  oldcatalog = ua->catalog;

  if (!(catalog = get_catalog_resource(ua))) {
    ua->catalog = oldcatalog;
  } else {
    ua->catalog = catalog;
  }
  if (OpenDb(ua)) {
    ua->SendMsg(T_("Using Catalog name=%s DB=%s\n"),
                ua->catalog->resource_name_, ua->catalog->db_name);
  }
  return true;
}

bool quit_cmd(UaContext* ua, const char*)
{
  ua->quit = true;

  return true;
}

// Handler to get job status
static int StatusHandler(void* ctx, int, char** row)
{
  char* val = (char*)ctx;

  if (row[0]) {
    *val = row[0][0];
  } else {
    *val = '?'; /* Unknown by default */
  }

  return 0;
}

// Wait until no job is running
static bool wait_cmd(UaContext* ua, const char*)
{
  JobControlRecord* jcr;
  int status;
  char ed1[50];
  uint32_t JobId = 0;
  time_t stop_time = 0;
  char jobstatus = '?'; /* Unknown by default */
  PoolMem temp(PM_MESSAGE);

  /* no args
   * Wait until no job is running */
  if (ua->argc == 1) {
    Bmicrosleep(0, 200000); /* let job actually start */
    for (bool running = true; running;) {
      running = false;
      foreach_jcr (jcr) {
        if (jcr->JobId != 0) {
          running = true;
          break;
        }
      }
      endeach_jcr(jcr);

      if (running) { Bmicrosleep(1, 0); }
    }
    return true;
  }

  if (int i = FindArgWithValue(ua, NT_("timeout")); i > 0 && ua->argv[i]) {
    stop_time = time(NULL) + str_to_int64(ua->argv[i]);
  }

  // We have jobid, jobname or ujobid argument
  if (!OpenClientDb(ua)) {
    ua->ErrorMsg(T_("ERR: Can't open db\n"));
    return true;
  }

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], "jobid")) {
      if (!ua->argv[i]) { break; }
      JobId = str_to_int64(ua->argv[i]);
      break;
    } else if (Bstrcasecmp(ua->argk[i], "jobname")
               || Bstrcasecmp(ua->argk[i], "job")) {
      if (!ua->argv[i]) { break; }
      jcr = get_jcr_by_partial_name(ua->argv[i]);
      if (jcr) {
        JobId = jcr->JobId;
        FreeJcr(jcr);
      }
      break;
    } else if (Bstrcasecmp(ua->argk[i], "ujobid")) {
      if (!ua->argv[i]) { break; }
      jcr = get_jcr_by_full_name(ua->argv[i]);
      if (jcr) {
        JobId = jcr->JobId;
        FreeJcr(jcr);
      }
      break;
    } else if (Bstrcasecmp(ua->argk[i], "mount")) {
      // Wait for a mount request
      for (bool waiting = false; !waiting;) {
        foreach_jcr (jcr) {
          if (jcr->JobId != 0
              && (jcr->getJobStatus() == JS_WaitMedia
                  || jcr->getJobStatus() == JS_WaitMount)) {
            waiting = true;
            break;
          }
        }
        endeach_jcr(jcr);
        if (waiting) { break; }
        if (stop_time && (time(NULL) >= stop_time)) {
          ua->WarningMsg(T_("Wait on mount timed out\n"));
          return true;
        }
        Bmicrosleep(1, 0);
      }
      return true;
    }
  }

  if (JobId == 0) {
    ua->ErrorMsg(T_("ERR: Job was not found\n"));
    return true;
  }

  // We wait the end of a specific job
  Bmicrosleep(0, 200000); /* let job actually start */
  for (bool running = true; running;) {
    running = false;

    jcr = get_jcr_by_id(JobId);
    if (jcr) {
      running = true;
      FreeJcr(jcr);
    }

    if (running) { Bmicrosleep(1, 0); }
  }

  // We have to get JobStatus
  Mmsg(temp, "SELECT JobStatus FROM Job WHERE JobId='%s'",
       edit_int64(JobId, ed1));
  ua->db->SqlQuery(temp.c_str(), StatusHandler, (void*)&jobstatus);

  switch (jobstatus) {
    case JS_Error:
      status = 1; /* Warning */
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
    case JS_Canceled:
      status = 2; /* Critical */
      break;
    case JS_Warnings:
    case JS_Terminated:
      status = 0; /* Ok */
      break;
    default:
      status = 3; /* Unknown */
      break;
  }

  Mmsg(temp, "%c", jobstatus);
  ua->send->ObjectStart("Job");
  ua->send->ObjectKeyValue("JobId", "%s=", JobId, "%i\n");
  ua->send->ObjectKeyValue("JobStatusLong", job_status_to_str(jobstatus),
                           "JobStatus=%s ");
  ua->send->ObjectKeyValue("JobStatus", temp.c_str(), "(%s)\n");
  ua->send->ObjectKeyValue("ExitStatus", status);
  ua->send->ObjectEnd("Job");

  return true;
}

static bool WhoAmICmd(UaContext* ua, const char*)
{
  std::string message;
  message = ua->user_acl ? ua->user_acl->corresponding_resource->resource_name_
                         : "root";
  ua->send->ObjectKeyValue("whoami", message.c_str(), "%s\n");
  return true;
}

static bool help_cmd(UaContext* ua, const char*)
{
  int i;

  ua->send->Decoration("%s",
                       T_("  Command            Description\n  =======    "
                          "        ===========\n"));
  for (i = 0; i < comsize; i++) {
    if (ua->argc == 2) {
      if (Bstrcasecmp(ua->argk[1], commands[i].key)) {
        ua->send->ObjectStart(commands[i].key);
        ua->send->ObjectKeyValue("command", commands[i].key, "  %-18s");
        ua->send->ObjectKeyValue("description", commands[i].help, " %s\n\n");
        ua->send->ObjectKeyValue("arguments", "Arguments:\n\t",
                                 commands[i].usage, "%s\n", 40);
        ua->send->ObjectKeyValueBool(
            "permission", ua->AclAccessOk(Command_ACL, commands[i].key));
        ua->send->ObjectEnd(commands[i].key);
        break;
      }
    } else {
      if (ua->AclAccessOk(Command_ACL, commands[i].key)
          && (!IsDotCommand(commands[i].key))) {
        ua->send->ObjectStart(commands[i].key);
        ua->send->ObjectKeyValue("command", commands[i].key, "  %-18s");
        ua->send->ObjectKeyValue("description", commands[i].help, " %s\n");
        ua->send->ObjectKeyValue("arguments", commands[i].usage, 0);
        ua->send->ObjectKeyValueBool("permission", true);
        ua->send->ObjectEnd(commands[i].key);
      }
    }
  }
  if (i == comsize && ua->argc == 2) {
    ua->SendMsg(T_("\nCan't find %s command.\n\n"), ua->argk[1]);
  }
  ua->send->Decoration(
      T_("\nWhen at a prompt, entering a period (.) cancels the command.\n\n"));

  return true;
}

static bool DotHelpCmd(UaContext* ua, const char*)
{
  int i, j;

  /* Implement DotHelpCmd here instead of ua_dotcmds.c,
   * because comsize and commands are defined here. */

  // Want to display a specific help section
  j = FindArgWithValue(ua, NT_("item"));
  if (j >= 0 && ua->argk[j]) {
    for (i = 0; i < comsize; i++) {
      if (bstrcmp(commands[i].key, ua->argv[j])) {
        ua->send->ObjectStart(commands[i].key);
        ua->send->ObjectKeyValue("command", commands[i].key);
        ua->send->ObjectKeyValue("description", commands[i].help);
        ua->send->ObjectKeyValue("arguments", commands[i].usage, "%s\n", 0);
        ua->send->ObjectKeyValueBool(
            "permission", ua->AclAccessOk(Command_ACL, commands[i].key));
        ua->send->ObjectEnd(commands[i].key);
        break;
      }
    }
    return true;
  }

  j = FindArg(ua, NT_("all"));
  if (j >= 0) {
    // Want to display only user commands (except dot commands)
    for (i = 0; i < comsize; i++) {
      if (ua->AclAccessOk(Command_ACL, commands[i].key)
          && (!IsDotCommand(commands[i].key))) {
        ua->send->ObjectStart(commands[i].key);
        ua->send->ObjectKeyValue("command", commands[i].key, "%s\n");
        ua->send->ObjectKeyValue("description", commands[i].help);
        ua->send->ObjectKeyValue("arguments", commands[i].usage, NULL, 0);
        ua->send->ObjectKeyValueBool("permission", true);
        ua->send->ObjectEnd(commands[i].key);
      }
    }
  } else {
    // Want to display everything
    for (i = 0; i < comsize; i++) {
      if (ua->AclAccessOk(Command_ACL, commands[i].key)) {
        ua->send->ObjectStart(commands[i].key);
        ua->send->ObjectKeyValue("command", commands[i].key, "%s ");
        ua->send->ObjectKeyValue("description", commands[i].help, "%s -- ");
        ua->send->ObjectKeyValue("arguments", commands[i].usage, "%s\n", 0);
        ua->send->ObjectKeyValueBool("permission", true);
        ua->send->ObjectEnd(commands[i].key);
      }
    }
  }

  return true;
}

static bool VersionCmd(UaContext* ua, const char*)
{
  ua->send->ObjectStart("version");
  ua->send->ObjectKeyValue("name", my_name, "%s ");
  ua->send->ObjectKeyValue("type", "bareos-director");
  ua->send->ObjectKeyValue("Version", "%s: ", kBareosVersionStrings.Full,
                           "%s ");
  ua->send->ObjectKeyValue("bdate", kBareosVersionStrings.Date, "(%s) ");
  ua->send->ObjectKeyValue("operatingsystem", kBareosVersionStrings.GetOsInfo(),
                           "%s ");
  ua->send->ObjectKeyValue("distname", BAREOS_PLATFORM, "%s ");
  ua->send->ObjectKeyValue("distversion", kBareosVersionStrings.GetOsInfo(),
                           "%s ");
  ua->send->ObjectKeyValue("CustomVersionId", NPRTB(me->verid), "%s\n");
  ua->send->ObjectEnd("version");

  return true;
}

} /* namespace directordaemon */
