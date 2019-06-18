/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October MM
 */
/**
 * @file
 * This file handles accepting Director Commands
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "include/ch.h"
#include "filed/authenticate.h"
#include "filed/dir_cmd.h"
#include "filed/estimate.h"
#include "filed/evaluate_job_command.h"
#include "filed/heartbeat.h"
#include "filed/fileset.h"
#include "filed/socket_server.h"
#include "filed/restore.h"
#include "filed/verify.h"
#include "findlib/enable_priv.h"
#include "findlib/shadowing.h"
#include "include/make_unique.h"
#include "lib/berrno.h"
#include "lib/bget_msg.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/path_list.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/tls_conf.h"
#include "lib/parse_conf.h"
#include "lib/bsock_tcp.h"
#include "lib/bsock_network_dump.h"
#include "lib/watchdog.h"
#include "lib/util.h"

#if defined(WIN32_VSS)
#include "win32/findlib/win32.h"
#include "vss.h"
#endif

namespace filedaemon {

extern bool backup_only_mode;
extern bool restore_only_mode;

/**
 * As Windows saves ACLs as part of the standard backup stream
 * we just pretend here that is has implicit acl support.
 */
#if defined(HAVE_ACL) || defined(HAVE_WIN32)
const bool have_acl = true;
#else
const bool have_acl = false;
#endif

#if defined(HAVE_XATTR)
const bool have_xattr = true;
#else
const bool have_xattr = false;
#endif

const bool have_encryption = true;

/* Global variables to handle Client Initiated Connections */
static bool quit_client_initiate_connection = false;
static alist* client_initiated_connection_threads = nullptr;

/* Imported functions */
extern bool AccurateCmd(JobControlRecord* jcr);
extern bool StatusCmd(JobControlRecord* jcr);
extern bool QstatusCmd(JobControlRecord* jcr);
extern "C" char* job_code_callback_filed(JobControlRecord* jcr,
                                         const char* param);

/* Forward referenced functions */
static bool BackupCmd(JobControlRecord* jcr);
static bool BootstrapCmd(JobControlRecord* jcr);
static bool CancelCmd(JobControlRecord* jcr);
static bool EndRestoreCmd(JobControlRecord* jcr);
static bool EstimateCmd(JobControlRecord* jcr);
#ifdef DEVELOPER
static bool exit_cmd(JobControlRecord* jcr);
#endif
static bool FilesetCmd(JobControlRecord* jcr);
static bool job_cmd(JobControlRecord* jcr);
static bool LevelCmd(JobControlRecord* jcr);
static bool PluginoptionsCmd(JobControlRecord* jcr);
static bool RunafterCmd(JobControlRecord* jcr);
static bool RunbeforenowCmd(JobControlRecord* jcr);
static bool RunbeforeCmd(JobControlRecord* jcr);
static bool RunscriptCmd(JobControlRecord* jcr);
static bool ResolveCmd(JobControlRecord* jcr);
static bool RestoreObjectCmd(JobControlRecord* jcr);
static bool RestoreCmd(JobControlRecord* jcr);
static bool SessionCmd(JobControlRecord* jcr);
static bool SecureerasereqCmd(JobControlRecord* jcr);
static bool SetauthorizationCmd(JobControlRecord* jcr);
static bool SetbandwidthCmd(JobControlRecord* jcr);
static bool SetdebugCmd(JobControlRecord* jcr);
static bool StorageCmd(JobControlRecord* jcr);
static bool VerifyCmd(JobControlRecord* jcr);

static BareosSocket* connect_to_director(JobControlRecord* jcr,
                                         DirectorResource* dir_res,
                                         bool verbose);
static bool response(JobControlRecord* jcr,
                     BareosSocket* sd,
                     char* resp,
                     const char* cmd);
static void FiledFreeJcr(JobControlRecord* jcr);
static bool OpenSdReadSession(JobControlRecord* jcr);
static void SetStorageAuthKeyAndTlsPolicy(JobControlRecord* jcr,
                                          char* key,
                                          TlsPolicy policy);

/* Exported functions */

struct s_cmds {
  const char* cmd;
  bool (*func)(JobControlRecord*);
  bool monitoraccess; /* specify if monitors have access to this function */
};

/**
 * The following are the recognized commands from the Director.
 *
 * Keywords are sorted first longest match when the keywords start with the same
 * string.
 */
static struct s_cmds cmds[] = {
    {"accurate", AccurateCmd, false},
    {"backup", BackupCmd, false},
    {"bootstrap", BootstrapCmd, false},
    {"cancel", CancelCmd, false},
    {"endrestore", EndRestoreCmd, false},
    {"estimate", EstimateCmd, false},
#ifdef DEVELOPER
    {"exit", exit_cmd, false},
#endif
    {"fileset", FilesetCmd, false},
    {"JobId=", job_cmd, false},
    {"level = ", LevelCmd, false},
    {"pluginoptions", PluginoptionsCmd, false},
    {"RunAfterJob", RunafterCmd, false},
    {"RunBeforeNow", RunbeforenowCmd, false},
    {"RunBeforeJob", RunbeforeCmd, false},
    {"Run", RunscriptCmd, false},
    {"restoreobject", RestoreObjectCmd, false},
    {"restore ", RestoreCmd, false},
    {"resolve ", ResolveCmd, false},
    {"getSecureEraseCmd", SecureerasereqCmd, false},
    {"session", SessionCmd, false},
    {"setauthorization", SetauthorizationCmd, false},
    {"setbandwidth=", SetbandwidthCmd, false},
    {"setdebug=", SetdebugCmd, false},
    {"status", StatusCmd, true},
    {".status", QstatusCmd, true},
    {"storage ", StorageCmd, false},
    {"verify", VerifyCmd, false},
    {nullptr, nullptr, false} /* list terminator */
};

/**
 * Commands send to director
 */
static char hello_client[] = "Hello Client %s FdProtocolVersion=%d calling\n";

/**
 * Responses received from the director
 */
static const char OKversion[] = "1000 OK: %s Version: %s (%u %s %u)";

/**
 * Commands received from director that need scanning
 */
static char setauthorizationcmd[] = "setauthorization Authorization=%100s";
static char setbandwidthcmd[] = "setbandwidth=%lld Job=%127s";
static char setdebugv0cmd[] = "setdebug=%d trace=%d";
static char setdebugv1cmd[] = "setdebug=%d trace=%d hangup=%d";
static char setdebugv2cmd[] = "setdebug=%d trace=%d hangup=%d timestamp=%d";
static char storaddrv0cmd[] = "storage address=%s port=%d ssl=%d";
static char storaddrv1cmd[] =
    "storage address=%s port=%d ssl=%d Authorization=%100s";
static char sessioncmd[] = "session %127s %ld %ld %ld %ld %ld %ld\n";
static char restorecmd[] = "restore replace=%c prelinks=%d where=%s\n";
static char restorecmd1[] = "restore replace=%c prelinks=%d where=\n";
static char restorecmdR[] = "restore replace=%c prelinks=%d regexwhere=%s\n";
static char restoreobjcmd[] = "restoreobject JobId=%u %d,%d,%d,%d,%d,%d,%s";
static char restoreobjcmd1[] = "restoreobject JobId=%u %d,%d,%d,%d,%d,%d\n";
static char endrestoreobjectcmd[] = "restoreobject end\n";
static char pluginoptionscmd[] = "pluginoptions %s";
static char verifycmd[] = "verify level=%30s";
static char Estimatecmd[] = "estimate listing=%d";
static char runbeforecmd[] = "RunBeforeJob %s";
static char runaftercmd[] = "RunAfterJob %s";
static char runscriptcmd[] =
    "Run OnSuccess=%d OnFailure=%d AbortOnError=%d When=%d Command=%s";
static char resolvecmd[] = "resolve %s";

/**
 * Responses sent to Director
 */
static char errmsg[] = "2999 Invalid command\n";
static char invalid_cmd[] =
    "2997 Invalid command for a Director with Monitor directive enabled.\n";
static char OkAuthorization[] = "2000 OK Authorization\n";
static char OKBandwidth[] = "2000 OK Bandwidth\n";
static char OKinc[] = "2000 OK include\n";
static char OKest[] = "2000 OK estimate files=%s bytes=%s\n";
static char OKlevel[] = "2000 OK level\n";
static char OKbackup[] = "2000 OK backup\n";
static char OKbootstrap[] = "2000 OK bootstrap\n";
static char OKverify[] = "2000 OK verify\n";
static char OKrestore[] = "2000 OK restore\n";
static char OKsecureerase[] = "2000 OK FDSecureEraseCmd %s\n";
static char OKsession[] = "2000 OK session\n";
static char OKstore[] = "2000 OK storage\n";
static char OKstoreend[] = "2000 OK storage end\n";
static char OKjob[] =
#if !defined(IS_BUILD_ON_OBS)
    "2000 OK Job %s (%s) %s,%s,%s";
#else
    "2000 OK Job %s (%s) %s,%s,%s,%s,%s";
#endif
static char OKsetdebugv0[] =
    "2000 OK setdebug=%d trace=%d hangup=%d tracefile=%s\n";
static char OKsetdebugv1[] =
    "2000 OK setdebug=%d trace=%d hangup=%d timestamp=%d tracefile=%s\n";
static char BADjob[] = "2901 Bad Job\n";
static char EndJob[] =
    "2800 End Job TermCode=%d JobFiles=%u ReadBytes=%s"
    " JobBytes=%s Errors=%u VSS=%d Encrypt=%d\n";
static char OKRunBefore[] = "2000 OK RunBefore\n";
static char OKRunBeforeNow[] = "2000 OK RunBeforeNow\n";
static char OKRunAfter[] = "2000 OK RunAfter\n";
static char OKRunScript[] = "2000 OK RunScript\n";
static char BadRunBeforeJob[] = "2905 Bad RunBeforeJob command.\n";
static char FailedRunScript[] = "2905 Failed RunScript\n";
static char BadRunAfterJob[] = "2905 Bad RunAfterJob command.\n";
static char BADcmd[] = "2902 Bad %s\n";
static char OKRestoreObject[] = "2000 OK ObjectRestored\n";
static char OKPluginOptions[] = "2000 OK PluginOptions\n";
static char BadPluginOptions[] = "2905 Bad PluginOptions command.\n";

/**
 * Responses received from Storage Daemon
 */
static char OK_end[] = "3000 OK end\n";
static char OK_close[] = "3000 OK close Status = %d\n";
static char OK_open[] = "3000 OK open ticket = %d\n";
static char OK_data[] = "3000 OK data\n";
static char OK_append[] = "3000 OK append data\n";

/**
 * Commands sent to Storage Daemon
 */
static char append_open[] = "append open session\n";
static char append_data[] = "append data %d\n";
static char append_end[] = "append end session %d\n";
static char append_close[] = "append close session %d\n";
static char read_open[] = "read open session = %s %ld %ld %ld %ld %ld %ld\n";
static char read_data[] = "read data %d\n";
static char read_close[] = "read close session %d\n";

/**
 * See if we are allowed to execute the command issued.
 */
static bool ValidateCommand(JobControlRecord* jcr,
                            const char* cmd,
                            alist* allowed_job_cmds)
{
  char* allowed_job_cmd = nullptr;
  bool allowed = false;

  /*
   * If there is no explicit list of allowed cmds allow all cmds.
   */
  if (!allowed_job_cmds) { return true; }

  foreach_alist (allowed_job_cmd, allowed_job_cmds) {
    if (Bstrcasecmp(cmd, allowed_job_cmd)) {
      allowed = true;
      break;
    }
  }

  if (!allowed) {
    Jmsg(jcr, M_FATAL, 0,
         _("Illegal \"%s\" command not allowed by Allowed Job Cmds setting of "
           "this filed.\n"),
         cmd);
  }

  return allowed;
}

static inline void CleanupFileset(JobControlRecord* jcr)
{
  findFILESET* fileset;
  findIncludeExcludeItem* incexe;
  findFOPTS* fo;

  fileset = jcr->ff->fileset;
  if (fileset) {
    /*
     * Delete FileSet Include lists
     */
    for (int i = 0; i < fileset->include_list.size(); i++) {
      incexe = (findIncludeExcludeItem*)fileset->include_list.get(i);
      for (int j = 0; j < incexe->opts_list.size(); j++) {
        fo = (findFOPTS*)incexe->opts_list.get(j);
        if (fo->plugin) { free(fo->plugin); }
        for (int k = 0; k < fo->regex.size(); k++) {
          regfree((regex_t*)fo->regex.get(k));
        }
        for (int k = 0; k < fo->regexdir.size(); k++) {
          regfree((regex_t*)fo->regexdir.get(k));
        }
        for (int k = 0; k < fo->regexfile.size(); k++) {
          regfree((regex_t*)fo->regexfile.get(k));
        }
        if (fo->size_match) { free(fo->size_match); }
        fo->regex.destroy();
        fo->regexdir.destroy();
        fo->regexfile.destroy();
        fo->wild.destroy();
        fo->wilddir.destroy();
        fo->wildfile.destroy();
        fo->wildbase.destroy();
        fo->base.destroy();
        fo->fstype.destroy();
        fo->Drivetype.destroy();
      }
      incexe->opts_list.destroy();
      incexe->name_list.destroy();
      incexe->plugin_list.destroy();
      incexe->ignoredir.destroy();
    }
    fileset->include_list.destroy();

    /*
     * Delete FileSet Exclude lists
     */
    for (int i = 0; i < fileset->exclude_list.size(); i++) {
      incexe = (findIncludeExcludeItem*)fileset->exclude_list.get(i);
      for (int j = 0; j < incexe->opts_list.size(); j++) {
        fo = (findFOPTS*)incexe->opts_list.get(j);
        if (fo->size_match) { free(fo->size_match); }
        fo->regex.destroy();
        fo->regexdir.destroy();
        fo->regexfile.destroy();
        fo->wild.destroy();
        fo->wilddir.destroy();
        fo->wildfile.destroy();
        fo->wildbase.destroy();
        fo->base.destroy();
        fo->fstype.destroy();
        fo->Drivetype.destroy();
      }
      incexe->opts_list.destroy();
      incexe->name_list.destroy();
      incexe->plugin_list.destroy();
      incexe->ignoredir.destroy();
    }
    fileset->exclude_list.destroy();
    free(fileset);
  }
  jcr->ff->fileset = nullptr;
}

static inline bool AreMaxConcurrentJobsExceeded()
{
  JobControlRecord* jcr;
  unsigned int cnt = 0;

  foreach_jcr (jcr) {
    cnt++;
  }
  endeach_jcr(jcr);

  return (cnt >= me->MaxConcurrentJobs) ? true : false;
}

JobControlRecord* create_new_director_session(BareosSocket* dir)
{
  JobControlRecord* jcr;
  const char jobname[12] = "*Director*";

  jcr = new_jcr(sizeof(JobControlRecord),
                FiledFreeJcr); /* create JobControlRecord */
  jcr->dir_bsock = dir;
  jcr->ff = init_find_files();
  jcr->start_time = time(nullptr);
  jcr->RunScripts = new alist(10, not_owned_by_alist);
  jcr->last_fname = GetPoolMemory(PM_FNAME);
  jcr->last_fname[0] = 0;
  jcr->client_name = GetMemory(strlen(my_name) + 1);
  PmStrcpy(jcr->client_name, my_name);
  bstrncpy(jcr->Job, jobname, sizeof(jobname)); /* dummy */
  jcr->crypto.pki_sign = me->pki_sign;
  jcr->crypto.pki_encrypt = me->pki_encrypt;
  jcr->crypto.pki_keypair = me->pki_keypair;
  jcr->crypto.pki_signers = me->pki_signers;
  jcr->crypto.pki_recipients = me->pki_recipients;
  if (dir) { dir->SetJcr(jcr); }
  SetJcrInTsd(jcr);

  EnableBackupPrivileges(nullptr, 1 /* ignore_errors */);

  return jcr;
}

void* process_director_commands(void* p_jcr)
{
  JobControlRecord* jcr = (JobControlRecord*)p_jcr;
  return process_director_commands(jcr, jcr->dir_bsock);
}

void* process_director_commands(JobControlRecord* jcr, BareosSocket* dir)
{
  bool found;
  bool quit = false;

  /**********FIXME******* add command handler error code */

  while (jcr->authenticated && (!quit)) {
    /*
     * Read command
     */
    if (dir->recv() < 0) { break; /* connection terminated */ }

    dir->msg[dir->message_length] = 0;
    Dmsg1(100, "<dird: %s\n", dir->msg);
    found = false;
    for (int i = 0; cmds[i].cmd; i++) {
      if (bstrncmp(cmds[i].cmd, dir->msg, strlen(cmds[i].cmd))) {
        found = true; /* indicate command found */
        if ((!cmds[i].monitoraccess) && (jcr->director->monitor)) {
          Dmsg1(100, "Command \"%s\" is invalid.\n", cmds[i].cmd);
          dir->fsend(invalid_cmd);
          dir->signal(BNET_EOD);
          break;
        }
        Dmsg1(100, "Executing %s command.\n", cmds[i].cmd);
        if (!cmds[i].func(jcr)) { /* do command */
          quit = true;            /* error or fully terminated, get out */
          Dmsg1(100, "Quit command loop. Canceled=%d\n", JobCanceled(jcr));
        }
        break;
      }
    }
    if (!found) { /* command not found */
      dir->fsend(errmsg);
      quit = true;
      break;
    }
  }

  /*
   * Inform Storage daemon that we are done
   */
  if (jcr->store_bsock) { jcr->store_bsock->signal(BNET_TERMINATE); }

  /*
   * Run the after job
   */
  if (jcr->RunScripts) {
    RunScripts(jcr, jcr->RunScripts, "ClientAfterJob",
               (jcr->director && jcr->director->allowed_script_dirs)
                   ? jcr->director->allowed_script_dirs
                   : me->allowed_script_dirs);
  }

  if (jcr->JobId) { /* send EndJob if running a job */
    char ed1[50], ed2[50];
    /*
     * Send termination status back to Dir
     */
    dir->fsend(EndJob, jcr->JobStatus, jcr->JobFiles,
               edit_uint64(jcr->ReadBytes, ed1),
               edit_uint64(jcr->JobBytes, ed2), jcr->JobErrors, jcr->enable_vss,
               jcr->crypto.pki_encrypt);
    Dmsg1(110, "End FD msg: %s\n", dir->msg);
  }

  GeneratePluginEvent(jcr, bEventJobEnd);

  DequeueMessages(jcr); /* send any queued messages */

  /*
   * Inform Director that we are done
   */
  dir->signal(BNET_TERMINATE);

  FreePlugins(jcr); /* release instantiated plugins */
  FreeAndNullPoolMemory(jcr->job_metadata);

  /*
   * Clean up fileset
   */
  CleanupFileset(jcr);

  FreeJcr(jcr); /* destroy JobControlRecord record */
  Dmsg0(100, "Done with FreeJcr\n");
  GarbageCollectMemoryPool();

#ifdef HAVE_WIN32
  AllowOsSuspensions();
#endif

  return nullptr;
}

/**
 * Create a new thread to handle director connection.
 */
static bool StartProcessDirectorCommands(JobControlRecord* jcr)
{
  int result = 0;
  pthread_t thread;

  if ((result = pthread_create(&thread, nullptr, process_director_commands,
                               (void*)jcr)) != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, _("Cannot create Director connect thread: %s\n"),
          be.bstrerror(result));
  }

  return (result == 0);
}

/**
 * Connection request from an director.
 *
 * Accept commands one at a time from the Director and execute them.
 *
 * Concerning ClientRunBefore/After, the sequence of events is rather critical.
 * If they are not done in the right order one can easily get FD->SD timeouts
 * if the script runs a long time.
 *
 * The current sequence of events is:
 *  1. Dir starts job with FD
 *  2. Dir connects to SD
 *  3. Dir connects to FD
 *  4. FD connects to SD
 *  5. FD gets/runs ClientRunBeforeJob and sends ClientRunAfterJob
 *  6. Dir sends include/exclude
 *  7. FD sends data to SD
 *  8. SD/FD disconnects while SD despools data and attributes (optional)
 *  9. FD runs ClientRunAfterJob
 */
void* handle_director_connection(BareosSocket* dir)
{
  JobControlRecord* jcr;

#ifdef HAVE_WIN32
  PreventOsSuspensions();
#endif

  if (AreMaxConcurrentJobsExceeded()) {
    Emsg0(
        M_ERROR, 0,
        _("Number of Jobs exhausted, please increase MaximumConcurrentJobs\n"));
    return nullptr;
  }

  jcr = create_new_director_session(dir);

  Dmsg0(120, "Calling Authenticate\n");
  if (AuthenticateDirector(jcr)) { Dmsg0(120, "OK Authenticate\n"); }

  return process_director_commands(jcr, dir);
}

static bool ParseOkVersion(const char* string)
{
  char name[MAX_NAME_LENGTH];
  char version[MAX_NAME_LENGTH];
  unsigned int day = 0;
  char month[100];
  unsigned int year = 0;
  int number = 0;

  number = sscanf(string, OKversion, &name, &version, &day, &month, &year);
  Dmsg2(120, "OK message: %s, Version: %s\n", name, version);
  return (number == 5);
}

static void* handle_connection_to_director(void* director_resource)
{
  DirectorResource* dir_res = (DirectorResource*)director_resource;
  BareosSocket* dir_bsock = nullptr;
  JobControlRecord* jcr = nullptr;
  int data_available = 0;
  int retry_period = 60;
  const int timeout_data = 60;

  while (!quit_client_initiate_connection) {
    if (jcr) {
      FreeJcr(jcr);
      jcr = nullptr;
    }

    jcr = create_new_director_session(nullptr);
    dir_bsock = connect_to_director(jcr, dir_res, true);
    if (!dir_bsock) {
      Emsg2(M_ERROR, 0, "Failed to connect to Director \"%s\". Retry in %ds.\n",
            dir_res->resource_name_, retry_period);
      sleep(retry_period);
    } else {
      Dmsg1(120, "Connected to \"%s\".\n", dir_res->resource_name_);

      /*
       * Returns: 1 if data available, 0 if timeout, -1 if error
       */
      data_available = 0;
      while ((data_available == 0) && (!quit_client_initiate_connection)) {
        Dmsg2(120, "Waiting for data from Director \"%s\" (timeout: %ds)\n",
              dir_res->resource_name_, timeout_data);
        data_available = dir_bsock->WaitDataIntr(timeout_data);
      }
      if (!quit_client_initiate_connection) {
        if (data_available < 0) {
          Emsg1(M_ABORT, 0,
                _("Failed while waiting for data from Director \"%s\"\n"),
                dir_res->resource_name_);
        } else {
          /*
           * data is available
           */
          dir_bsock->SetJcr(jcr);
          jcr->dir_bsock = dir_bsock;
          if (StartProcessDirectorCommands(jcr)) {
            /*
             * jcr (and dir_bsock) are now used by another thread.
             */
            dir_bsock = nullptr;
            jcr = nullptr;
          }
        }
      }
    }
  }

  Dmsg1(100, "Exiting Client Initiated Connection thread for %s\n",
        dir_res->resource_name_);
  if (jcr) {
    /*
     * cleanup old data structures
     */
    FreeJcr(jcr);
  }

  return nullptr;
}

bool StartConnectToDirectorThreads()
{
  bool result = false;
  DirectorResource* dir_res = nullptr;
  int pthread_create_result = 0;
  if (!client_initiated_connection_threads) {
    client_initiated_connection_threads = new alist();
  }
  pthread_t* thread;

  foreach_res (dir_res, R_DIRECTOR) {
    if (dir_res->conn_from_fd_to_dir) {
      if (!dir_res->address) {
        Emsg1(M_ERROR, 0,
              "Failed to connect to Director \"%s\". The address config "
              "directive is missing.\n",
              dir_res->resource_name_);
      } else if (!dir_res->port) {
        Emsg1(M_ERROR, 0,
              "Failed to connect to Director \"%s\". The port config directive "
              "is missing.\n",
              dir_res->resource_name_);
      } else {
        Dmsg3(120, "Connecting to Director \"%s\", address %s:%d.\n",
              dir_res->resource_name_, dir_res->address, dir_res->port);
        thread = (pthread_t*)malloc(sizeof(pthread_t));
        if ((pthread_create_result =
                 pthread_create(thread, nullptr, handle_connection_to_director,
                                (void*)dir_res)) == 0) {
          client_initiated_connection_threads->append(thread);
        } else {
          BErrNo be;
          Emsg1(M_ABORT, 0, _("Cannot create Director connect thread: %s\n"),
                be.bstrerror(pthread_create_result));
        }
      }
    }
  }

  return result;
}

bool StopConnectToDirectorThreads(bool wait)
{
  bool result = true;
  pthread_t* thread = nullptr;
  quit_client_initiate_connection = true;
  if (client_initiated_connection_threads) {
    while (!client_initiated_connection_threads->empty()) {
      thread = (pthread_t*)client_initiated_connection_threads->remove(0);
      if (thread) {
        pthread_kill(*thread, TIMEOUT_SIGNAL);
        if (wait) {
          if (pthread_join(*thread, nullptr) != 0) { result = false; }
        }
        free(thread);
      }
    }
    delete (client_initiated_connection_threads);
  }
  return result;
}

/**
 * Resolve a hostname
 */
static bool ResolveCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  dlist* addr_list;
  const char* errstr;
  char addresses[2048];
  char hostname[2048];

  sscanf(dir->msg, resolvecmd, &hostname);

  if ((addr_list = BnetHost2IpAddrs(hostname, 0, &errstr)) == nullptr) {
    dir->fsend(_("%s: Failed to resolve %s\n"), my_name, hostname);
    goto bail_out;
  }

  dir->fsend(
      _("%s resolves %s to %s\n"), my_name, hostname,
      BuildAddressesString(addr_list, addresses, sizeof(addresses), false));
  FreeAddresses(addr_list);

bail_out:
  dir->signal(BNET_EOD);
  return true;
}

static bool SecureerasereqCmd(JobControlRecord* jcr)
{
  const char* setting;
  BareosSocket* dir = jcr->dir_bsock;

  setting = me->secure_erase_cmdline ? me->secure_erase_cmdline : "*None*";
  Dmsg1(200, "Secure Erase Cmd Request: %s\n", setting);
  return dir->fsend(OKsecureerase, setting);
}

#ifdef DEVELOPER
static bool exit_cmd(JobControlRecord* jcr)
{
  jcr->dir_bsock->fsend("2000 exit OK\n");
  // terminate_filed(0);
  StopSocketServer();
  return false;
}
#endif

/**
 * Cancel a Job
 */
static bool CancelCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  char Job[MAX_NAME_LENGTH];
  JobControlRecord* cjcr;

  if (sscanf(dir->msg, "cancel Job=%127s", Job) == 1) {
    if (!(cjcr = get_jcr_by_full_name(Job))) {
      dir->fsend(_("2901 Job %s not found.\n"), Job);
    } else {
      GeneratePluginEvent(cjcr, bEventCancelCommand, nullptr);
      cjcr->setJobStatus(JS_Canceled);
      if (cjcr->store_bsock) {
        cjcr->store_bsock->SetTimedOut();
        cjcr->store_bsock->SetTerminated();
      }
      cjcr->MyThreadSendSignal(TIMEOUT_SIGNAL);
      FreeJcr(cjcr);
      dir->fsend(_("2001 Job %s marked to be canceled.\n"), Job);
    }
  } else {
    dir->fsend(_("2902 Error scanning cancel command.\n"));
  }
  dir->signal(BNET_EOD);
  return true;
}

/**
 * Set new authorization key as requested by the Director
 */
static bool SetauthorizationCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  PoolMem sd_auth_key(PM_MESSAGE);

  sd_auth_key.check_size(dir->message_length);
  if (sscanf(dir->msg, setauthorizationcmd, sd_auth_key.c_str()) != 1) {
    dir->fsend(BADcmd, "setauthorization");
    return false;
  }

  SetStorageAuthKeyAndTlsPolicy(jcr, sd_auth_key.c_str(), jcr->sd_tls_policy);
  Dmsg2(120, "JobId=%d Auth=%s\n", jcr->JobId, jcr->sd_auth_key);

  return dir->fsend(OkAuthorization);
}

/**
 * Set bandwidth limit as requested by the Director
 */
static bool SetbandwidthCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  int64_t bw = 0;
  JobControlRecord* cjcr;
  char Job[MAX_NAME_LENGTH];

  *Job = 0;
  if (sscanf(dir->msg, setbandwidthcmd, &bw, Job) != 2 || bw < 0) {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(_("2991 Bad setbandwidth command: %s\n"), jcr->errmsg);
    return false;
  }

  if (*Job) {
    if (!(cjcr = get_jcr_by_full_name(Job))) {
      dir->fsend(_("2901 Job %s not found.\n"), Job);
    } else {
      cjcr->max_bandwidth = bw;
      if (cjcr->store_bsock) {
        cjcr->store_bsock->SetBwlimit(bw);
        if (me->allow_bw_bursting) { cjcr->store_bsock->SetBwlimitBursting(); }
      }
      FreeJcr(cjcr);
    }
  } else {                          /* No job requested, apply globally */
    me->max_bandwidth_per_job = bw; /* Overwrite directive */
  }

  return dir->fsend(OKBandwidth);
}

/**
 * Set debug level as requested by the Director
 */
static bool SetdebugCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  int32_t level, trace_flag, hangup_flag, timestamp_flag;
  int scan;

  Dmsg1(50, "SetdebugCmd: %s", dir->msg);
  scan = sscanf(dir->msg, setdebugv2cmd, &level, &trace_flag, &hangup_flag,
                &timestamp_flag);
  if (scan != 4) {
    scan = sscanf(dir->msg, setdebugv1cmd, &level, &trace_flag, &hangup_flag);
  }
  if (scan != 3 && scan != 4) {
    scan = sscanf(dir->msg, setdebugv0cmd, &level, &trace_flag);
    if (scan != 2) {
      PmStrcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("2991 Bad setdebug command: %s\n"), jcr->errmsg);
      return false;
    } else {
      hangup_flag = -1;
    }
  }

  PoolMem tracefilename(PM_FNAME);
  Mmsg(tracefilename, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);

  if (level >= 0) { debug_level = level; }

  SetTrace(trace_flag);
  SetHangup(hangup_flag);
  if (scan == 4) {
    SetTimestamp(timestamp_flag);
    Dmsg5(50, "level=%d trace=%d hangup=%d timestamp=%d tracefilename=%s\n",
          level, GetTrace(), GetHangup(), GetTimestamp(),
          tracefilename.c_str());
    return dir->fsend(OKsetdebugv1, level, GetTrace(), GetHangup(),
                      GetTimestamp(), tracefilename.c_str());
  } else {
    Dmsg4(50, "level=%d trace=%d hangup=%d tracefilename=%s\n", level,
          GetTrace(), GetHangup(), tracefilename.c_str());
    return dir->fsend(OKsetdebugv0, level, GetTrace(), GetHangup(),
                      tracefilename.c_str());
  }
}

static bool EstimateCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  char ed1[50], ed2[50];

  /*
   * See if we are allowed to run estimate cmds.
   */
  if (!ValidateCommand(jcr, "estimate",
                       (jcr->director && jcr->director->allowed_job_cmds)
                           ? jcr->director->allowed_job_cmds
                           : me->allowed_job_cmds)) {
    dir->fsend(_("2992 Bad estimate command.\n"));
    return 0;
  }

  if (sscanf(dir->msg, Estimatecmd, &jcr->listing) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg(jcr, M_FATAL, 0, _("Bad estimate command: %s"), jcr->errmsg);
    dir->fsend(_("2992 Bad estimate command.\n"));
    return false;
  }

  MakeEstimate(jcr);

  dir->fsend(OKest, edit_uint64_with_commas(jcr->num_files_examined, ed1),
             edit_uint64_with_commas(jcr->JobBytes, ed2));
  dir->signal(BNET_EOD);

  return true;
}

/**
 * Get JobId and Storage Daemon Authorization key from Director
 */
static bool job_cmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;

  JobCommand command(dir->msg);

  if (!command.EvaluationSuccesful()) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg(jcr, M_FATAL, 0, _("Bad Job Command: %s"), jcr->errmsg);
    dir->fsend(BADjob);
    return false;
  }

  jcr->JobId = command.job_id_;
  strncpy(jcr->Job, command.job_, sizeof(jcr->Job));
  jcr->VolSessionId = command.vol_session_id_;
  jcr->VolSessionTime = command.vol_session_time_;

  TlsPolicy tls_policy =
      command.protocol_version_ ==
              JobCommand::ProtocolVersion::KVersionBefore_18_2
          ? TlsPolicy::kBnetTlsNone
          : command.tls_policy_;

  SetStorageAuthKeyAndTlsPolicy(jcr, command.sd_auth_key_, tls_policy);
  Dmsg3(120, "JobId=%d Auth=%s TlsPolicy=%d\n", jcr->JobId, jcr->sd_auth_key,
        tls_policy);
  Mmsg(jcr->errmsg, "JobId=%d Job=%s", jcr->JobId, jcr->Job);
  NewPlugins(jcr); /* instantiate plugins for this jcr */
  GeneratePluginEvent(jcr, bEventJobStart, (void*)jcr->errmsg);

  const char* os_version;
#ifdef HAVE_WIN32
  os_version = win_os;
#else
  os_version = HOST_OS;
#endif

#if !defined(IS_BUILD_ON_OBS)
  return dir->fsend(OKjob, VERSION, LSMDATE, os_version, DISTNAME, DISTVER);
#else
  return dir->fsend(OKjob, VERSION, LSMDATE, os_version, DISTNAME, DISTVER,
                    OBS_DISTRIBUTION, OBS_ARCH);
#endif
}

static bool RunbeforeCmd(JobControlRecord* jcr)
{
  bool ok;
  POOLMEM* cmd;
  RunScript* script;
  BareosSocket* dir = jcr->dir_bsock;

  if (!me->compatible) {
    dir->fsend(BadRunBeforeJob);
    return false;
  }

  Dmsg1(100, "RunbeforeCmd: %s", dir->msg);
  cmd = GetMemory(dir->message_length + 1);
  if (sscanf(dir->msg, runbeforecmd, cmd) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, _("Bad RunBeforeJob command: %s\n"), jcr->errmsg);
    dir->fsend(BadRunBeforeJob);
    FreeMemory(cmd);
    return false;
  }
  UnbashSpaces(cmd);

  /*
   * Run the command now
   */

  Dmsg0(500, "runscript: creating new RunScript object\n");
  script = new RunScript;
  script->SetJobCodeCallback(job_code_callback_filed);
  script->SetCommand(cmd);
  script->when = SCRIPT_Before;
  FreeMemory(cmd);

  ok = script->Run(jcr, "ClientRunBeforeJob");
  FreeRunscript(script);

  if (ok) {
    dir->fsend(OKRunBefore);
    return true;
  } else {
    dir->fsend(BadRunBeforeJob);
    return false;
  }
}

static bool RunbeforenowCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;

  RunScripts(jcr, jcr->RunScripts, "ClientBeforeJob",
             (jcr->director && jcr->director->allowed_script_dirs)
                 ? jcr->director->allowed_script_dirs
                 : me->allowed_script_dirs);

  if (JobCanceled(jcr)) {
    dir->fsend(FailedRunScript);
    Dmsg0(100, "Back from RunScripts ClientBeforeJob now: FAILED\n");
    return false;
  } else {
    dir->fsend(OKRunBeforeNow);
    Dmsg0(100, "Back from RunScripts ClientBeforeJob now: OK\n");
    return true;
  }
}

static bool RunafterCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  POOLMEM* cmd;
  RunScript* script;

  if (!me->compatible) {
    dir->fsend(BadRunAfterJob);
    return false;
  }

  Dmsg1(100, "RunafterCmd: %s", dir->msg);
  cmd = GetMemory(dir->message_length + 1);
  if (sscanf(dir->msg, runaftercmd, cmd) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, _("Bad RunAfter command: %s\n"), jcr->errmsg);
    dir->fsend(BadRunAfterJob);
    FreeMemory(cmd);
    return false;
  }
  UnbashSpaces(cmd);

  Dmsg0(500, "runscript: creating new RunScript object\n");
  script = new RunScript;
  script->SetJobCodeCallback(job_code_callback_filed);
  script->SetCommand(cmd);
  script->on_success = true;
  script->on_failure = false;
  script->when = SCRIPT_After;
  FreeMemory(cmd);

  jcr->RunScripts->append(script);

  return dir->fsend(OKRunAfter);
}

static bool RunscriptCmd(JobControlRecord* jcr)
{
  POOLMEM* msg;
  RunScript* cmd;
  BareosSocket* dir = jcr->dir_bsock;
  int on_success, on_failure, fail_on_error;

  /*
   * See if we are allowed to run runscript cmds.
   */
  if (!ValidateCommand(jcr, "runscript",
                       (jcr->director && jcr->director->allowed_job_cmds)
                           ? jcr->director->allowed_job_cmds
                           : me->allowed_job_cmds)) {
    dir->fsend(FailedRunScript);
    return 0;
  }

  msg = GetMemory(dir->message_length + 1);
  Dmsg0(500, "runscript: creating new RunScript object\n");
  cmd = new RunScript;
  cmd->SetJobCodeCallback(job_code_callback_filed);

  Dmsg1(100, "RunscriptCmd: '%s'\n", dir->msg);

  /*
   * Note, we cannot sscanf into bools
   */
  if (sscanf(dir->msg, runscriptcmd, &on_success, &on_failure, &fail_on_error,
             &cmd->when, msg) != 5) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, _("Bad RunScript command: %s\n"), jcr->errmsg);
    dir->fsend(FailedRunScript);
    FreeRunscript(cmd);
    FreeMemory(msg);
    return false;
  }

  cmd->on_success = on_success;
  cmd->on_failure = on_failure;
  cmd->fail_on_error = fail_on_error;
  UnbashSpaces(msg);

  cmd->SetCommand(msg);
  cmd->Debug();
  jcr->RunScripts->append(cmd);

  FreePoolMemory(msg);

  return dir->fsend(OKRunScript);
}

/**
 * This passes plugin specific options.
 */
static bool PluginoptionsCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  POOLMEM* msg;

  msg = GetMemory(dir->message_length + 1);
  if (sscanf(dir->msg, pluginoptionscmd, msg) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, _("Bad Plugin Options command: %s\n"), jcr->errmsg);
    dir->fsend(BadPluginOptions);
    FreeMemory(msg);
    return false;
  }

  UnbashSpaces(msg);
  GeneratePluginEvent(jcr, bEventNewPluginOptions, (void*)msg);
  FreeMemory(msg);

  return dir->fsend(OKPluginOptions);
}

/**
 * This reads data sent from the Director from the
 * RestoreObject table that allows us to get objects
 * that were backed up (VSS .xml data) and are needed
 * before starting the restore.
 */
static bool RestoreObjectCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  int32_t FileIndex;
  restore_object_pkt rop;

  memset(&rop, 0, sizeof(rop));
  rop.pkt_size = sizeof(rop);
  rop.pkt_end = sizeof(rop);

  Dmsg1(100, "Enter restoreobject_cmd: %s", dir->msg);
  if (bstrcmp(dir->msg, endrestoreobjectcmd)) {
    GeneratePluginEvent(jcr, bEventRestoreObject, nullptr);
    return dir->fsend(OKRestoreObject);
  }

  rop.plugin_name = (char*)malloc(dir->message_length);
  *rop.plugin_name = 0;

  if (sscanf(dir->msg, restoreobjcmd, &rop.JobId, &rop.object_len,
             &rop.object_full_len, &rop.object_index, &rop.object_type,
             &rop.object_compression, &FileIndex, rop.plugin_name) != 8) {
    /*
     * Old version, no plugin_name
     */
    if (sscanf(dir->msg, restoreobjcmd1, &rop.JobId, &rop.object_len,
               &rop.object_full_len, &rop.object_index, &rop.object_type,
               &rop.object_compression, &FileIndex) != 7) {
      Dmsg0(5, "Bad restore object command\n");
      PmStrcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RestoreObject command: %s\n"), jcr->errmsg);
      goto bail_out;
    }
  }

  UnbashSpaces(rop.plugin_name);

  Dmsg7(100,
        "Recv object: JobId=%u objlen=%d full_len=%d objinx=%d objtype=%d "
        "FI=%d plugin_name=%s\n",
        rop.JobId, rop.object_len, rop.object_full_len, rop.object_index,
        rop.object_type, FileIndex, rop.plugin_name);

  /*
   * Read Object name
   */
  if (dir->recv() < 0) { goto bail_out; }
  Dmsg2(100, "Recv Oname object: len=%d Oname=%s\n", dir->message_length,
        dir->msg);
  rop.object_name = strdup(dir->msg);

  /*
   * Read Object
   */
  if (dir->recv() < 0) { goto bail_out; }

  /*
   * Transfer object from message buffer, and get new message buffer
   */
  rop.object = dir->msg;
  dir->msg = GetPoolMemory(PM_MESSAGE);

  /*
   * If object is compressed, uncompress it
   */
  switch (rop.object_compression) {
    case 1: { /* zlib level 9 */
      int status;
      int out_len = rop.object_full_len + 100;
      POOLMEM* obj = GetMemory(out_len);

      Dmsg2(100, "Inflating from %d to %d\n", rop.object_len,
            rop.object_full_len);
      status = Zinflate(rop.object, rop.object_len, obj, out_len);
      Dmsg1(100, "Zinflate status=%d\n", status);

      if (out_len != rop.object_full_len) {
        Jmsg3(jcr, M_ERROR, 0,
              ("Decompression failed. Len wanted=%d got=%d. Object_name=%s\n"),
              rop.object_full_len, out_len, rop.object_name);
      }

      FreePoolMemory(rop.object); /* Release compressed object */
      rop.object = obj;           /* New uncompressed object */
      rop.object_len = out_len;
      break;
    }
    default:
      break;
  }

  if (debug_level >= 100) {
    PoolMem object_content(PM_MESSAGE);

    /*
     * Convert the object into a null terminated string.
     */
    object_content.check_size(rop.object_len + 1);
    memset(object_content.c_str(), 0, rop.object_len + 1);
    memcpy(object_content.c_str(), rop.object, rop.object_len);

    Dmsg2(100, "Recv Object: len=%d Object=%s\n", rop.object_len,
          object_content.c_str());
  }

  /*
   * We still need to do this to detect a vss restore
   */
  if (bstrcmp(rop.object_name, "job_metadata.xml")) {
    Dmsg0(100, "got job metadata\n");
    jcr->got_metadata = true;
  }

  GeneratePluginEvent(jcr, bEventRestoreObject, (void*)&rop);

  if (rop.object_name) { free(rop.object_name); }

  if (rop.object) { FreePoolMemory(rop.object); }

  if (rop.plugin_name) { free(rop.plugin_name); }

  Dmsg1(100, "Send: %s", OKRestoreObject);
  return true;

bail_out:
  dir->fsend(_("2909 Bad RestoreObject command.\n"));
  return false;
}

#if defined(WIN32_VSS)
static inline int CountIncludeListFileEntries(JobControlRecord* jcr)
{
  int cnt = 0;
  findFILESET* fileset;
  findIncludeExcludeItem* incexe;

  fileset = jcr->ff->fileset;
  if (fileset) {
    for (int i = 0; i < fileset->include_list.size(); i++) {
      incexe = (findIncludeExcludeItem*)fileset->include_list.get(i);
      cnt += incexe->name_list.size();
    }
  }

  return cnt;
}
#endif

/**
 * Director is passing his Fileset
 */
static bool FilesetCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  bool retval;
#if defined(WIN32_VSS)
  int vss = 0;

  sscanf(dir->msg, "fileset vss=%d", &vss);
#endif

  if (!InitFileset(jcr)) { return false; }

  while (dir->recv() >= 0) {
    StripTrailingJunk(dir->msg);
    Dmsg1(500, "Fileset: %s\n", dir->msg);
    AddFileset(jcr, dir->msg);
  }

  if (!TermFileset(jcr)) { return false; }

#if defined(WIN32_VSS)
  jcr->enable_vss =
      (vss && (CountIncludeListFileEntries(jcr) > 0)) ? true : false;
#endif

  retval = dir->fsend(OKinc);
  GeneratePluginEvent(jcr, bEventEndFileSet);
  CheckIncludeListShadowing(jcr, jcr->ff->fileset);

  return retval;
}

static void FreeBootstrap(JobControlRecord* jcr)
{
  if (jcr->RestoreBootstrap) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    FreePoolMemory(jcr->RestoreBootstrap);
    jcr->RestoreBootstrap = nullptr;
  }
}

static pthread_mutex_t bsr_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t bsr_uniq = 0;

/**
 * The Director sends us the bootstrap file, which
 * we will in turn pass to the SD.
 * Deprecated.  The bsr is now sent directly from the
 * Director to the SD.
 */
static bool BootstrapCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  POOLMEM* fname = GetPoolMemory(PM_FNAME);
  FILE* bs;

  FreeBootstrap(jcr);
  P(bsr_mutex);
  bsr_uniq++;
  Mmsg(fname, "%s/%s.%s.%d.bootstrap", me->working_directory,
       me->resource_name_, jcr->Job, bsr_uniq);
  V(bsr_mutex);
  Dmsg1(400, "bootstrap=%s\n", fname);
  jcr->RestoreBootstrap = fname;
  bs = fopen(fname, "a+b"); /* create file */
  if (!bs) {
    BErrNo be;
    Jmsg(jcr, M_FATAL, 0, _("Could not create bootstrap file %s: ERR=%s\n"),
         jcr->RestoreBootstrap, be.bstrerror());
    /*
     * Suck up what he is sending to us so that he will then
     *   read our error message.
     */
    while (dir->recv() >= 0) {}
    FreeBootstrap(jcr);
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  while (dir->recv() >= 0) {
    Dmsg1(200, "filed<dird: bootstrap: %s", dir->msg);
    fputs(dir->msg, bs);
  }
  fclose(bs);
  /*
   * Note, do not free the bootstrap yet -- it needs to be sent to the SD
   */
  return dir->fsend(OKbootstrap);
}

/**
 * Get backup level from Director
 */
static bool LevelCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  POOLMEM *level, *buf = nullptr;
  int mtime_only;

  level = GetMemory(dir->message_length + 1);
  Dmsg1(10, "LevelCmd: %s", dir->msg);

  /*
   * Keep compatibility with older directors
   */
  if (strstr(dir->msg, "accurate")) { jcr->accurate = true; }
  if (strstr(dir->msg, "rerunning")) { jcr->rerunning = true; }
  if (sscanf(dir->msg, "level = %s ", level) != 1) { goto bail_out; }

  if (bstrcmp(level, "base")) {
    /*
     * Base backup requested
     */
    jcr->setJobLevel(L_BASE);
  } else if (bstrcmp(level, "full")) {
    /*
     * Full backup requested
     */
    jcr->setJobLevel(L_FULL);
  } else if (strstr(level, "differential")) {
    jcr->setJobLevel(L_DIFFERENTIAL);
    FreeMemory(level);
    return true;
  } else if (strstr(level, "incremental")) {
    jcr->setJobLevel(L_INCREMENTAL);
    FreeMemory(level);
    return true;
  } else if (bstrcmp(level, "since_utime")) {
    char ed1[50], ed2[50];

    /*
     * We get his UTC since time, then sync the clocks and correct it to agree
     * with our clock.
     */
    buf = GetMemory(dir->message_length + 1);
    utime_t since_time, adj;
    btime_t his_time, bt_start, rt = 0, bt_adj = 0;
    if (jcr->getJobLevel() == L_NONE) {
      jcr->setJobLevel(L_SINCE); /* if no other job level set, do it now */
    }

    if (sscanf(dir->msg, "level = since_utime %s mtime_only=%d prev_job=%127s",
               buf, &mtime_only, jcr->PrevJob) != 3) {
      if (sscanf(dir->msg, "level = since_utime %s mtime_only=%d", buf,
                 &mtime_only) != 2) {
        goto bail_out;
      }
    }

    since_time = str_to_uint64(buf); /* this is the since time */
    Dmsg2(100, "since_time=%lld prev_job=%s\n", since_time, jcr->PrevJob);
    /*
     * Sync clocks by polling him for the time. We take 10 samples of his time
     * throwing out the first two.
     */
    for (int i = 0; i < 10; i++) {
      bt_start = GetCurrentBtime();
      dir->signal(BNET_BTIME); /* poll for time */
      if (dir->recv() <= 0) {  /* get response */
        goto bail_out;
      }
      if (sscanf(dir->msg, "btime %s", buf) != 1) { goto bail_out; }
      if (i < 2) { /* toss first two results */
        continue;
      }
      his_time = str_to_uint64(buf);
      rt = GetCurrentBtime() - bt_start; /* compute round trip time */
      Dmsg2(100, "Dirtime=%s FDtime=%s\n", edit_uint64(his_time, ed1),
            edit_uint64(bt_start, ed2));
      bt_adj += bt_start - his_time - rt / 2;
      Dmsg2(100, "rt=%s adj=%s\n", edit_uint64(rt, ed1),
            edit_uint64(bt_adj, ed2));
    }

    bt_adj = bt_adj / 8; /* compute average time */
    Dmsg2(100, "rt=%s adj=%s\n", edit_uint64(rt, ed1),
          edit_uint64(bt_adj, ed2));
    adj = BtimeToUtime(bt_adj);
    since_time += adj; /* adjust for clock difference */

    /*
     * Don't notify if time within 3 seconds
     */
    if (adj > 3 || adj < -3) {
      int type;
      if (adj > 600 || adj < -600) {
        type = M_WARNING;
      } else {
        type = M_INFO;
      }
      Jmsg(jcr, type, 0,
           _("DIR and FD clocks differ by %lld seconds, FD automatically "
             "compensating.\n"),
           adj);
    }
    dir->signal(BNET_EOD);

    Dmsg2(100, "adj=%lld since_time=%lld\n", adj, since_time);
    jcr->incremental = true; /* set incremental or decremental backup */
    jcr->mtime = since_time; /* set since time */
    GeneratePluginEvent(jcr, bEventSince, (void*)(time_t)jcr->mtime);
  } else {
    Jmsg1(jcr, M_FATAL, 0, _("Unknown backup level: %s\n"), level);
    FreeMemory(level);
    return false;
  }

  FreeMemory(level);
  if (buf) { FreeMemory(buf); }

  GeneratePluginEvent(jcr, bEventLevel, (void*)(intptr_t)jcr->getJobLevel());

  return dir->fsend(OKlevel);

bail_out:
  PmStrcpy(jcr->errmsg, dir->msg);
  Jmsg1(jcr, M_FATAL, 0, _("Bad level command: %s\n"), jcr->errmsg);
  FreeMemory(level);
  if (buf) { FreeMemory(buf); }
  return false;
}

/**
 * Get session parameters from Director -- this is for a Restore command
 * This is deprecated. It is now passed via the bsr.
 */
static bool SessionCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;

  Dmsg1(100, "SessionCmd: %s", dir->msg);
  if (sscanf(dir->msg, sessioncmd, jcr->VolumeName, &jcr->VolSessionId,
             &jcr->VolSessionTime, &jcr->StartFile, &jcr->EndFile,
             &jcr->StartBlock, &jcr->EndBlock) != 7) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg(jcr, M_FATAL, 0, _("Bad session command: %s"), jcr->errmsg);
    return false;
  }

  return dir->fsend(OKsession);
}

static void SetStorageAuthKeyAndTlsPolicy(JobControlRecord* jcr,
                                          char* key,
                                          TlsPolicy policy)
{
  /* if no key don't update anything */
  if (!*key) { return; }

  /**
   * We can be contacting multiple storage daemons.
   * So, make sure that any old jcr->store_bsock is cleaned up.
   */
  if (jcr->store_bsock) {
    delete jcr->store_bsock;
    jcr->store_bsock = nullptr;
  }

  /**
   * We can be contacting multiple storage daemons.
   * So, make sure that any old jcr->sd_auth_key is cleaned up.
   */
  if (jcr->sd_auth_key) {
    /*
     * If we already have a Authorization key, director can do multi storage
     * restore
     */
    Dmsg0(5, "set multi_restore=true\n");
    jcr->multi_restore = true;
    free(jcr->sd_auth_key);
  }

  jcr->sd_auth_key = strdup(key);
  Dmsg0(5, "set sd auth key\n");

  jcr->sd_tls_policy = policy;
  Dmsg1(5, "set sd ssl_policy to %d\n", policy);
}

/**
 * Get address of storage daemon from Director
 */
static bool StorageCmd(JobControlRecord* jcr)
{
  int stored_port;      /* storage daemon port */
  TlsPolicy tls_policy; /* enable ssl to sd */
  char stored_addr[MAX_NAME_LENGTH];
  PoolMem sd_auth_key(PM_MESSAGE);
  BareosSocket* dir = jcr->dir_bsock;
  BareosSocket* storage_daemon_socket = new BareosSocketTCP;

  if (me->nokeepalive) { storage_daemon_socket->ClearKeepalive(); }
  Dmsg1(100, "StorageCmd: %s", dir->msg);
  sd_auth_key.check_size(dir->message_length);
  if (sscanf(dir->msg, storaddrv1cmd, stored_addr, &stored_port, &tls_policy,
             sd_auth_key.c_str()) != 4) {
    if (sscanf(dir->msg, storaddrv0cmd, stored_addr, &stored_port,
               &tls_policy) != 3) {
      PmStrcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad storage command: %s"), jcr->errmsg);
      goto bail_out;
    }
  }

  SetStorageAuthKeyAndTlsPolicy(jcr, sd_auth_key.c_str(), tls_policy);

  Dmsg3(110, "Open storage: %s:%d ssl=%d\n", stored_addr, stored_port,
        tls_policy);

  storage_daemon_socket->SetSourceAddress(me->FDsrc_addr);

  /*
   * TODO: see if we put limit on restore and backup...
   */
  if (!jcr->max_bandwidth) {
    if (jcr->director->max_bandwidth_per_job) {
      jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;
    } else if (me->max_bandwidth_per_job) {
      jcr->max_bandwidth = me->max_bandwidth_per_job;
    }
  }

  storage_daemon_socket->SetBwlimit(jcr->max_bandwidth);
  if (me->allow_bw_bursting) { storage_daemon_socket->SetBwlimitBursting(); }

  /*
   * Open command communications with Storage daemon
   */
  if (!storage_daemon_socket->connect(
          jcr, 10, (int)me->SDConnectTimeout, me->heartbeat_interval,
          _("Storage daemon"), stored_addr, nullptr, stored_port, 1)) {
    Jmsg(jcr, M_FATAL, 0, _("Failed to connect to Storage daemon: %s:%d\n"),
         stored_addr, stored_port);
    Dmsg2(100, "Failed to connect to Storage daemon: %s:%d\n", stored_addr,
          stored_port);
    goto bail_out;
  }
  Dmsg0(110, "Connection OK to SD.\n");

  jcr->store_bsock = storage_daemon_socket;

  if (tls_policy == TlsPolicy::kBnetTlsAuto) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            jcr->Job, R_JOB, qualified_resource_name)) {
      goto bail_out;
    }

    if (!storage_daemon_socket->DoTlsHandshake(
            TlsPolicy::kBnetTlsAuto, me, false, qualified_resource_name.c_str(),
            jcr->sd_auth_key, jcr)) {
      jcr->store_bsock = nullptr;
      goto bail_out;
    }
  }

  storage_daemon_socket->FillBSockWithConnectedDaemonInformation(*my_config,
                                                                 R_STORAGE);

  storage_daemon_socket->fsend("Hello Start Job %s\n", jcr->Job);
  if (!AuthenticateWithStoragedaemon(jcr)) {
    Jmsg(jcr, M_FATAL, 0, _("Failed to authenticate Storage daemon.\n"));
    goto bail_out;
  }
  Dmsg0(110, "Authenticated with SD.\n");

  /*
   * Send OK to Director
   */
  return dir->fsend(OKstore);

bail_out:
  delete storage_daemon_socket;
  jcr->store_bsock = nullptr;
  dir->fsend(BADcmd, "storage");
  return false;
}

#ifndef HAVE_WIN32
static void LogFlagStatus(JobControlRecord* jcr,
                          int flag,
                          const char* flag_text)
{
  findFILESET* fileset = jcr->ff->fileset;
  bool found = false;
  if (fileset) {
    for (int i = 0; i < fileset->include_list.size() && !found; i++) {
      findIncludeExcludeItem* incexe =
          (findIncludeExcludeItem*)fileset->include_list.get(i);

      for (int j = 0; j < incexe->opts_list.size() && !found; j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);

        if (BitIsSet(flag, fo->flags)) { found = true; }
      }
    }
  }

  std::string m = flag_text;
  m += found ? "is enabled\n" : "is disabled\n";
  Jmsg(jcr, M_INFO, 0, m.c_str());
}
#endif

/**
 * Clear a flag in the find options.
 *
 * We walk the list of include blocks and for each option block
 * check if a certain flag is set and clear that.
 */
static inline void ClearFlagInFileset(JobControlRecord* jcr,
                                      int flag,
                                      const char* warning)
{
  findFILESET* fileset;
  bool cleared_flag = false;

  fileset = jcr->ff->fileset;
  if (fileset) {
    for (int i = 0; i < fileset->include_list.size(); i++) {
      findIncludeExcludeItem* incexe =
          (findIncludeExcludeItem*)fileset->include_list.get(i);

      for (int j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);

        if (BitIsSet(flag, fo->flags)) {
          ClearBit(flag, fo->flags);
          cleared_flag = true;
        }
      }
    }
  }

  if (cleared_flag) { Jmsg(jcr, M_WARNING, 0, warning); }
}

/**
 * Clear a compression flag in the find options.
 *
 * We walk the list of include blocks and for each option block
 * check if a certain compression flag is set and clear that.
 */
static inline void ClearCompressionFlagInFileset(JobControlRecord* jcr)
{
  findFILESET* fileset;

  fileset = jcr->ff->fileset;
  if (fileset) {
    for (int i = 0; i < fileset->include_list.size(); i++) {
      findIncludeExcludeItem* incexe =
          (findIncludeExcludeItem*)fileset->include_list.get(i);

      for (int j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);

        /*
         * See if a compression flag is set in this option block.
         */
        if (BitIsSet(FO_COMPRESS, fo->flags)) {
          switch (fo->Compress_algo) {
#if defined(HAVE_LIBZ)
            case COMPRESS_GZIP:
              break;
#endif
#if defined(HAVE_LZO)
            case COMPRESS_LZO1X:
              break;
#endif
#if defined(HAVE_FASTLZ)
            case COMPRESS_FZFZ:
            case COMPRESS_FZ4L:
            case COMPRESS_FZ4H:
              break;
#endif
            default:
              /*
               * When we get here its because the wanted compression protocol is
               * not supported with the current compile options.
               */
              Jmsg(jcr, M_WARNING, 0,
                   "%s compression support requested in fileset but not "
                   "available on this platform. Disabling "
                   "...\n",
                   cmprs_algo_to_text(fo->Compress_algo));
              ClearBit(FO_COMPRESS, fo->flags);
              fo->Compress_algo = 0;
              break;
          }
        }
      }
    }
  }
}

/**
 * Find out what encryption cipher to use.
 */
static inline bool GetWantedCryptoCipher(JobControlRecord* jcr,
                                         crypto_cipher_t* cipher)
{
  findFILESET* fileset;
  bool force_encrypt = false;
  crypto_cipher_t wanted_cipher = CRYPTO_CIPHER_NONE;

  /*
   * Walk the fileset and check for the FO_FORCE_ENCRYPT flag and any forced
   * crypto cipher.
   */
  fileset = jcr->ff->fileset;
  if (fileset) {
    for (int i = 0; i < fileset->include_list.size(); i++) {
      findIncludeExcludeItem* incexe =
          (findIncludeExcludeItem*)fileset->include_list.get(i);

      for (int j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);

        if (BitIsSet(FO_FORCE_ENCRYPT, fo->flags)) { force_encrypt = true; }

        if (fo->Encryption_cipher != CRYPTO_CIPHER_NONE) {
          /*
           * Make sure we have not found a cipher definition before.
           */
          if (wanted_cipher != CRYPTO_CIPHER_NONE) {
            Jmsg(jcr, M_FATAL, 0,
                 _("Fileset contains multiple cipher settings\n"));
            return false;
          }

          /*
           * See if pki_encrypt is already set for this Job.
           */
          if (!jcr->crypto.pki_encrypt) {
            if (!me->pki_keypair_file) {
              Jmsg(jcr, M_FATAL, 0,
                   _("Fileset contains cipher settings but PKI Key Pair is not "
                     "configured\n"));
              return false;
            }

            /*
             * Enable encryption and signing for this Job.
             */
            jcr->crypto.pki_sign = true;
            jcr->crypto.pki_encrypt = true;
          }

          wanted_cipher = (crypto_cipher_t)fo->Encryption_cipher;
        }
      }
    }
  }

  /*
   * See if fileset forced a certain cipher.
   */
  if (wanted_cipher == CRYPTO_CIPHER_NONE) { wanted_cipher = me->pki_cipher; }

  /*
   * See if we are in compatible mode then we are hardcoded to
   * CRYPTO_CIPHER_AES_128_CBC.
   */
  if (me->compatible) { wanted_cipher = CRYPTO_CIPHER_AES_128_CBC; }

  /*
   * See if FO_FORCE_ENCRYPT is set and encryption is not configured for the
   * filed.
   */
  if (force_encrypt && !jcr->crypto.pki_encrypt) {
    Jmsg(jcr, M_FATAL, 0,
         _("Fileset forces encryption but encryption is not configured\n"));
    return false;
  }

  *cipher = wanted_cipher;

  return true;
}

/**
 * Do a backup.
 */
static bool BackupCmd(JobControlRecord* jcr)
{
  int ok = 0;
  int SDJobStatus;
  int32_t FileIndex;
  BareosSocket* dir = jcr->dir_bsock;
  BareosSocket* sd = jcr->store_bsock;
  crypto_cipher_t cipher = CRYPTO_CIPHER_NONE;

  /*
   * See if we are in restore only mode then we don't allow a backup to be
   * initiated.
   */
  if (restore_only_mode) {
    Jmsg(jcr, M_FATAL, 0,
         _("Filed in restore only mode, backups are not allowed, "
           "aborting...\n"));
    goto cleanup;
  }

  /*
   * See if we are allowed to run backup cmds.
   */
  if (!ValidateCommand(jcr, "backup",
                       (jcr->director && jcr->director->allowed_job_cmds)
                           ? jcr->director->allowed_job_cmds
                           : me->allowed_job_cmds)) {
    goto cleanup;
  }

#if defined(WIN32_VSS)
  if (jcr->enable_vss) { VSSInit(jcr); }
#endif

  if (sscanf(dir->msg, "backup FileIndex=%ld\n", &FileIndex) == 1) {
    jcr->JobFiles = FileIndex;
    Dmsg1(100, "JobFiles=%ld\n", jcr->JobFiles);
  }

  /**
   * Validate some options given to the backup make sense for the compiled in
   * options of this filed.
   */
#ifndef HAVE_WIN32
  if (!have_acl) {
    ClearFlagInFileset(jcr, FO_ACL,
                       _("ACL support requested in fileset but not available "
                         "on this platform. Disabling ...\n"));
  }

  if (!have_xattr) {
    ClearFlagInFileset(jcr, FO_XATTR,
                       _("XATTR support requested in fileset but not available "
                         "on this platform. Disabling ...\n"));
  }
#endif
  if (!have_encryption) {
    ClearFlagInFileset(jcr, FO_ENCRYPT,
                       _("Encryption support requested in fileset but not "
                         "available on this platform. Disabling ...\n"));
  }

  ClearCompressionFlagInFileset(jcr);

#ifndef HAVE_WIN32
  LogFlagStatus(jcr, FO_XATTR, "Extended attribute support ");
  LogFlagStatus(jcr, FO_ACL, "ACL support ");
#endif
  if (!GetWantedCryptoCipher(jcr, &cipher)) {
    dir->fsend(BADcmd, "backup");
    goto cleanup;
  }

  jcr->setJobStatus(JS_Blocked);
  jcr->setJobType(JT_BACKUP);
  Dmsg1(100, "begin backup ff=%p\n", jcr->ff);

  if (sd == nullptr) {
    Jmsg(jcr, M_FATAL, 0, _("Cannot contact Storage daemon\n"));
    dir->fsend(BADcmd, "backup");
    goto cleanup;
  }

  dir->fsend(OKbackup);
  Dmsg1(110, "filed>dird: %s", dir->msg);

  /**
   * Send Append Open Session to Storage daemon
   */
  sd->fsend(append_open);
  Dmsg1(110, ">stored: %s", sd->msg);

  /**
   * Expect to receive back the Ticket number
   */
  if (BgetMsg(sd) >= 0) {
    Dmsg1(110, "<stored: %s", sd->msg);
    if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
      Jmsg(jcr, M_FATAL, 0, _("Bad response to append open: %s\n"), sd->msg);
      goto cleanup;
    }
    Dmsg1(110, "Got Ticket=%d\n", jcr->Ticket);
  } else {
    Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to open command\n"));
    goto cleanup;
  }

  /**
   * Send Append data command to Storage daemon
   */
  sd->fsend(append_data, jcr->Ticket);
  Dmsg1(110, ">stored: %s", sd->msg);

  /**
   * Expect to get OK data
   */
  if (!response(jcr, sd, OK_data, "Append Data")) {
    Dmsg1(110, "<stored: %s", sd->msg);
    goto cleanup;
  }
  Dmsg1(110, "<stored: %s", sd->msg);

  GeneratePluginEvent(jcr, bEventStartBackupJob);

#if defined(WIN32_VSS)
  /*
   * START VSS ON WIN32
   */
  if (jcr->pVSSClient) {
    if (jcr->pVSSClient->InitializeForBackup(jcr)) {
      int drive_count;
      char szWinDriveLetters[27];
      bool onefs_disabled;

      GeneratePluginEvent(jcr, bEventVssBackupAddComponents);

      /*
       * Tell vss which drives to snapshot
       */
      *szWinDriveLetters = 0;

      /*
       * Plugin driver can return drive letters
       */
      GeneratePluginEvent(jcr, bEventVssPrepareSnapshot, szWinDriveLetters);

      drive_count = get_win32_driveletters(jcr->ff->fileset, szWinDriveLetters);

      onefs_disabled = win32_onefs_is_disabled(jcr->ff->fileset);

      if (drive_count > 0) {
        Jmsg(jcr, M_INFO, 0,
             _("Generate VSS snapshots. Driver=\"%s\", Drive(s)=\"%s\"\n"),
             jcr->pVSSClient->GetDriverName(),
             (drive_count) ? szWinDriveLetters : "None");

        if (!jcr->pVSSClient->CreateSnapshots(szWinDriveLetters,
                                              onefs_disabled)) {
          BErrNo be;
          Jmsg(jcr, M_FATAL, 0,
               _("CreateSGenerate VSS snapshots failed. ERR=%s\n"),
               be.bstrerror());
        } else {
          GeneratePluginEvent(jcr, bEventVssCreateSnapshots);

          /*
           * Inform about VMPs if we have them
           */
          jcr->pVSSClient->ShowVolumeMountPointStats(jcr);

          /*
           * Tell user if snapshot creation of a specific drive failed
           */
          for (int i = 0; i < (int)strlen(szWinDriveLetters); i++) {
            if (islower(szWinDriveLetters[i])) {
              Jmsg(jcr, M_FATAL, 0,
                   _("Generate VSS snapshot of drive \"%c:\\\" failed.\n"),
                   szWinDriveLetters[i]);
            }
          }

          /*
           * Inform user about writer states
           */
          for (size_t i = 0; i < jcr->pVSSClient->GetWriterCount(); i++) {
            if (jcr->pVSSClient->GetWriterState(i) < 1) {
              Jmsg(jcr, M_INFO, 0, _("VSS Writer (PrepareForBackup): %s\n"),
                   jcr->pVSSClient->GetWriterInfo(i));
            }
          }
        }

      } else {
        Jmsg(jcr, M_FATAL, 0,
             _("No drive letters found for generating VSS snapshots.\n"));
      }
    } else {
      BErrNo be;

      Jmsg(jcr, M_FATAL, 0, _("VSS was not initialized properly. ERR=%s\n"),
           be.bstrerror());
    }

    RunScripts(jcr, jcr->RunScripts, "ClientAfterVSS",
               (jcr->director && jcr->director->allowed_script_dirs)
                   ? jcr->director->allowed_script_dirs
                   : me->allowed_script_dirs);
  }
#endif

  /**
   * Send Files to Storage daemon
   */
  Dmsg1(110, "begin blast ff=%p\n", (FindFilesPacket*)jcr->ff);
  if (!BlastDataToStorageDaemon(jcr, nullptr, cipher)) {
    jcr->setJobStatus(JS_ErrorTerminated);
    BnetSuppressErrorMessages(sd, 1);
    Dmsg0(110, "Error in blast_data.\n");
  } else {
    jcr->setJobStatus(JS_Terminated);
    /* Note, the above set status will not override an error */
    if (!jcr->IsTerminatedOk()) {
      BnetSuppressErrorMessages(sd, 1);
      goto cleanup; /* bail out now */
    }
    /**
     * Expect to get response to append_data from Storage daemon
     */
    if (!response(jcr, sd, OK_append, "Append Data")) {
      jcr->setJobStatus(JS_ErrorTerminated);
      goto cleanup;
    }

    /**
     * Send Append End Data to Storage daemon
     */
    sd->fsend(append_end, jcr->Ticket);
    /* Get end OK */
    if (!response(jcr, sd, OK_end, "Append End")) {
      jcr->setJobStatus(JS_ErrorTerminated);
      goto cleanup;
    }

    /**
     * Send Append Close to Storage daemon
     */
    sd->fsend(append_close, jcr->Ticket);
    while (BgetMsg(sd) >= 0) { /* stop on signal or error */
      if (sscanf(sd->msg, OK_close, &SDJobStatus) == 1) {
        ok = 1;
        Dmsg2(200, "SDJobStatus = %d %c\n", SDJobStatus, (char)SDJobStatus);
      }
    }
    if (!ok) {
      Jmsg(jcr, M_FATAL, 0, _("Append Close with SD failed.\n"));
      goto cleanup;
    }
    if (!(SDJobStatus == JS_Terminated || SDJobStatus == JS_Warnings)) {
      Jmsg(jcr, M_FATAL, 0, _("Bad status %d returned from Storage Daemon.\n"),
           SDJobStatus);
    }
  }

cleanup:
#if defined(WIN32_VSS)
  if (jcr->pVSSClient) { jcr->pVSSClient->DestroyWriterInfo(); }
#endif

  GeneratePluginEvent(jcr, bEventEndBackupJob);
  return false; /* return and stop command loop */
}

/**
 * Do a Verify for Director
 *
 */
static bool VerifyCmd(JobControlRecord* jcr)
{
  char level[100];
  BareosSocket* dir = jcr->dir_bsock;
  BareosSocket* sd = jcr->store_bsock;

  /*
   * See if we are allowed to run verify cmds.
   */
  if (!ValidateCommand(jcr, "verify",
                       (jcr->director && jcr->director->allowed_job_cmds)
                           ? jcr->director->allowed_job_cmds
                           : me->allowed_job_cmds)) {
    dir->fsend(_("2994 Bad verify command: %s\n"), dir->msg);
    return 0;
  }

  jcr->setJobType(JT_VERIFY);
  if (sscanf(dir->msg, verifycmd, level) != 1) {
    dir->fsend(_("2994 Bad verify command: %s\n"), dir->msg);
    return false;
  }

  if (Bstrcasecmp(level, "init")) {
    jcr->setJobLevel(L_VERIFY_INIT);
  } else if (Bstrcasecmp(level, "catalog")) {
    jcr->setJobLevel(L_VERIFY_CATALOG);
  } else if (Bstrcasecmp(level, "volume")) {
    jcr->setJobLevel(L_VERIFY_VOLUME_TO_CATALOG);
  } else if (Bstrcasecmp(level, "data")) {
    jcr->setJobLevel(L_VERIFY_DATA);
  } else if (Bstrcasecmp(level, "disk_to_catalog")) {
    jcr->setJobLevel(L_VERIFY_DISK_TO_CATALOG);
  } else {
    dir->fsend(_("2994 Bad verify level: %s\n"), dir->msg);
    return false;
  }

  dir->fsend(OKverify);

  GeneratePluginEvent(jcr, bEventLevel, (void*)(intptr_t)jcr->getJobLevel());
  GeneratePluginEvent(jcr, bEventStartVerifyJob);

  Dmsg1(110, "filed>dird: %s", dir->msg);

  switch (jcr->getJobLevel()) {
    case L_VERIFY_INIT:
    case L_VERIFY_CATALOG:
      DoVerify(jcr);
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      if (!OpenSdReadSession(jcr)) { return false; }
      StartDirHeartbeat(jcr);
      DoVerifyVolume(jcr);
      StopDirHeartbeat(jcr);
      /*
       * Send Close session command to Storage daemon
       */
      sd->fsend(read_close, jcr->Ticket);
      Dmsg1(130, "filed>stored: %s", sd->msg);

      /* ****FIXME**** check response */
      BgetMsg(sd); /* get OK */

      /* Inform Storage daemon that we are done */
      sd->signal(BNET_TERMINATE);

      break;
    case L_VERIFY_DISK_TO_CATALOG:
      DoVerify(jcr);
      break;
    default:
      dir->fsend(_("2994 Bad verify level: %s\n"), dir->msg);
      return false;
  }

  dir->signal(BNET_EOD);
  GeneratePluginEvent(jcr, bEventEndVerifyJob);
  return false; /* return and Terminate command loop */
}

/**
 * Open connection to Director.
 */
static BareosSocket* connect_to_director(JobControlRecord* jcr,
                                         DirectorResource* dir_res,
                                         bool verbose)
{
  ASSERT(dir_res != nullptr);

  std::unique_ptr<BareosSocket> director_socket =
      std::make_unique<BareosSocketTCP>();

  if (me->nokeepalive) { director_socket->ClearKeepalive(); }

  director_socket->SetSourceAddress(me->FDsrc_addr);

  int retry_interval = 0;
  int max_retry_time = 0;
  utime_t heart_beat = me->heartbeat_interval;
  if (!director_socket->connect(jcr, retry_interval, max_retry_time, heart_beat,
                                dir_res->resource_name_, dir_res->address,
                                nullptr, dir_res->port, verbose)) {
    return nullptr;
  }

  if (dir_res->IsTlsConfigured()) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            me->resource_name_, my_config->r_own_, qualified_resource_name)) {
      Dmsg0(100,
            "Could not generate qualified resource name for a storage "
            "resource\n");
      return nullptr;
    }

    if (!director_socket->DoTlsHandshake(TlsPolicy::kBnetTlsAuto, dir_res,
                                         false, qualified_resource_name.c_str(),
                                         dir_res->password_.value, jcr)) {
      Dmsg0(100, "Could not DoTlsHandshake() with director\n");
      return nullptr;
    }
  }

  director_socket->FillBSockWithConnectedDaemonInformation(me, dir_res);

  Dmsg1(10, "Opened connection with Director %s\n", dir_res->resource_name_);
  jcr->dir_bsock = director_socket.get();

  director_socket->fsend(hello_client, my_name, FD_PROTOCOL_VERSION);
  if (!AuthenticateWithDirector(jcr, dir_res)) {
    jcr->dir_bsock = nullptr;
    return nullptr;
  }

  director_socket->recv();
  ParseOkVersion(director_socket->msg);

  jcr->director = dir_res;

  return director_socket.release();
}

/**
 * Do a Restore for Director
 */
static bool RestoreCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  BareosSocket* sd = jcr->store_bsock;
  POOLMEM* args;
  bool use_regexwhere = false;
  bool retval;
  int prefix_links;
  char replace;

  /*
   * See if we are in backup only mode then we don't allow a restore to be
   * initiated.
   */
  if (backup_only_mode) {
    Jmsg(jcr, M_FATAL, 0,
         _("Filed in backup only mode, restores are not allowed, "
           "aborting...\n"));
    return false;
  }

  /*
   * See if we are allowed to run restore cmds.
   */
  if (!ValidateCommand(jcr, "restore",
                       (jcr->director && jcr->director->allowed_job_cmds)
                           ? jcr->director->allowed_job_cmds
                           : me->allowed_job_cmds)) {
    return 0;
  }

  jcr->setJobType(JT_RESTORE);

  /**
   * Scan WHERE (base directory for restore) from command
   */
  Dmsg0(100, "restore command\n");

  /*
   * Pickup where string
   */
  args = GetMemory(dir->message_length + 1);
  *args = 0;

  if (sscanf(dir->msg, restorecmd, &replace, &prefix_links, args) != 3) {
    if (sscanf(dir->msg, restorecmdR, &replace, &prefix_links, args) != 3) {
      if (sscanf(dir->msg, restorecmd1, &replace, &prefix_links) != 2) {
        PmStrcpy(jcr->errmsg, dir->msg);
        Jmsg(jcr, M_FATAL, 0, _("Bad replace command. CMD=%s\n"), jcr->errmsg);
        FreePoolMemory(args);
        return false;
      }
      *args = 0;
    }
    use_regexwhere = true;
  }

#if defined(WIN32_VSS)
  /**
   * No need to enable VSS for restore if we do not have plugin data to restore
   */
  jcr->enable_vss = jcr->got_metadata;

  if (jcr->enable_vss) { VSSInit(jcr); }
#endif

  /*
   * Turn / into nothing
   */
  if (IsPathSeparator(args[0]) && args[1] == '\0') { args[0] = '\0'; }

  Dmsg2(150, "Got replace %c, where=%s\n", replace, args);
  UnbashSpaces(args);

  /*
   * Keep track of newly created directories to apply them correct attributes
   */
  if (replace == REPLACE_NEVER) { jcr->keep_path_list = true; }

  if (use_regexwhere) {
    jcr->where_bregexp = get_bregexps(args);
    if (!jcr->where_bregexp) {
      Jmsg(jcr, M_FATAL, 0, _("Bad where regexp. where=%s\n"), args);
      FreePoolMemory(args);
      return false;
    }
  } else {
    jcr->where = strdup(args);
  }

  FreePoolMemory(args);
  jcr->replace = replace;
  jcr->prefix_links = prefix_links;

  dir->fsend(OKrestore);
  Dmsg1(110, "filed>dird: %s", dir->msg);

  jcr->setJobStatus(JS_Blocked);

  if (!OpenSdReadSession(jcr)) {
    jcr->setJobStatus(JS_ErrorTerminated);
    goto bail_out;
  }

  jcr->setJobStatus(JS_Running);

  /**
   * Do restore of files and data
   */
  StartDirHeartbeat(jcr);
  GeneratePluginEvent(jcr, bEventStartRestoreJob);

#if defined(WIN32_VSS)
  /*
   * START VSS ON WIN32
   */
  if (jcr->pVSSClient) {
    if (!jcr->pVSSClient->InitializeForRestore(jcr)) {
      BErrNo be;
      Jmsg(jcr, M_WARNING, 0,
           _("VSS was not initialized properly. VSS support is disabled. "
             "ERR=%s\n"),
           be.bstrerror());
    }

    GeneratePluginEvent(jcr, bEventVssRestoreLoadComponentMetadata);

    RunScripts(jcr, jcr->RunScripts, "ClientAfterVSS",
               (jcr->director && jcr->director->allowed_script_dirs)
                   ? jcr->director->allowed_script_dirs
                   : me->allowed_script_dirs);
  }
#endif

  DoRestore(jcr);
  StopDirHeartbeat(jcr);

  if (jcr->JobWarnings) {
    jcr->setJobStatus(JS_Warnings);
  } else {
    jcr->setJobStatus(JS_Terminated);
  }

  /**
   * Send Close session command to Storage daemon
   */
  sd->fsend(read_close, jcr->Ticket);
  Dmsg1(100, "filed>stored: %s", sd->msg);

  BgetMsg(sd); /* get OK */

  /* Inform Storage daemon that we are done */
  sd->signal(BNET_TERMINATE);

#if defined(WIN32_VSS)
  /*
   * STOP VSS ON WIN32
   * Tell vss to close the restore session
   */
  if (jcr->pVSSClient) {
    Dmsg0(100, "About to call CloseRestore\n");

    GeneratePluginEvent(jcr, bEventVssCloseRestore);

    Dmsg0(100, "Really about to call CloseRestore\n");
    if (jcr->pVSSClient->CloseRestore()) {
      Dmsg0(100, "CloseRestore success\n");
      /*
       * Inform user about writer states
       */
      for (size_t i = 0; i < jcr->pVSSClient->GetWriterCount(); i++) {
        int msg_type = M_INFO;

        if (jcr->pVSSClient->GetWriterState(i) < 1) {
          msg_type = M_WARNING;
          jcr->JobErrors++;
        }
        Jmsg(jcr, msg_type, 0, _("VSS Writer (RestoreComplete): %s\n"),
             jcr->pVSSClient->GetWriterInfo(i));
      }
    } else {
      Dmsg1(100, "CloseRestore fail - %08x\n", errno);
    }
  }
#endif

bail_out:
  BfreeAndNull(jcr->where);

  if (jcr->JobErrors) { jcr->setJobStatus(JS_ErrorTerminated); }

  Dmsg0(100, "Done in job.c\n");

  if (jcr->multi_restore) {
    Dmsg0(100, OKstoreend);
    dir->fsend(OKstoreend);
    retval = true; /* we continue the loop, waiting for next part */
  } else {
    retval = false; /* we stop here */
  }

  if (JobCanceled(jcr)) { retval = false; /* we stop here */ }

  if (!retval) {
    EndRestoreCmd(jcr); /* stopping so send bEventEndRestoreJob */
  }

  return retval;
}

static bool EndRestoreCmd(JobControlRecord* jcr)
{
  Dmsg0(5, "EndRestoreCmd\n");
  GeneratePluginEvent(jcr, bEventEndRestoreJob);
  return false; /* return and Terminate command loop */
}

static bool OpenSdReadSession(JobControlRecord* jcr)
{
  BareosSocket* sd = jcr->store_bsock;

  if (!sd) {
    Jmsg(jcr, M_FATAL, 0, _("Improper calling sequence.\n"));
    return false;
  }
  Dmsg4(120, "VolSessId=%ld VolsessT=%ld SF=%ld EF=%ld\n", jcr->VolSessionId,
        jcr->VolSessionTime, jcr->StartFile, jcr->EndFile);
  Dmsg2(120, "JobId=%d vol=%s\n", jcr->JobId, "DummyVolume");
  /*
   * Open Read Session with Storage daemon
   */
  sd->fsend(read_open, "DummyVolume", jcr->VolSessionId, jcr->VolSessionTime,
            jcr->StartFile, jcr->EndFile, jcr->StartBlock, jcr->EndBlock);
  Dmsg1(110, ">stored: %s", sd->msg);

  /*
   * Get ticket number
   */
  if (BgetMsg(sd) >= 0) {
    Dmsg1(110, "filed<stored: %s", sd->msg);
    if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
      Jmsg(jcr, M_FATAL, 0, _("Bad response to SD read open: %s\n"), sd->msg);
      return false;
    }
    Dmsg1(110, "filed: got Ticket=%d\n", jcr->Ticket);
  } else {
    Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to read open command\n"));
    return false;
  }

  /*
   * Start read of data with Storage daemon
   */
  sd->fsend(read_data, jcr->Ticket);
  Dmsg1(110, ">stored: %s", sd->msg);

  /*
   * Get OK data
   */
  if (!response(jcr, sd, OK_data, "Read Data")) { return false; }

  return true;
}

/**
 * Destroy the Job Control Record and associated resources (sockets).
 */
static void FiledFreeJcr(JobControlRecord* jcr)
{
#if defined(WIN32_VSS)
  if (jcr->pVSSClient) {
    delete jcr->pVSSClient;
    jcr->pVSSClient = nullptr;
  }
#endif

  if (jcr->store_bsock) {
    jcr->store_bsock->close();
    delete jcr->store_bsock;
    jcr->store_bsock = nullptr;
  }

  if (jcr->dir_bsock) {
    jcr->dir_bsock->close();
    delete jcr->dir_bsock;
    jcr->dir_bsock = nullptr;
  }

  if (jcr->last_fname) { FreePoolMemory(jcr->last_fname); }

  FreeBootstrap(jcr);
  FreeRunscripts(jcr->RunScripts);
  delete jcr->RunScripts;
  jcr->RunScripts = nullptr;

  if (jcr->path_list) {
    FreePathList(jcr->path_list);
    jcr->path_list = nullptr;
  }

  TermFindFiles(jcr->ff);
  jcr->ff = nullptr;

  if (jcr->JobId != 0) {
    WriteStateFile(me->working_directory, "bareos-fd",
                   GetFirstPortHostOrder(me->FDaddrs));
  }

  return;
}

/**
 * Get response from Storage daemon to a command we sent.
 * Check that the response is OK.
 *
 * Returns: false on failure
 *          true on success
 */
bool response(JobControlRecord* jcr,
              BareosSocket* sd,
              char* resp,
              const char* cmd)
{
  if (sd->errors) { return false; }
  if (BgetMsg(sd) > 0) {
    Dmsg0(110, sd->msg);
    if (bstrcmp(sd->msg, resp)) { return true; }
  }
  if (JobCanceled(jcr)) {
    return false; /* if canceled avoid useless error messages */
  }
  if (IsBnetError(sd)) {
    Jmsg2(jcr, M_FATAL, 0,
          _("Comm error with SD. bad response to %s. ERR=%s\n"), cmd,
          BnetStrerror(sd));
  } else {
    Jmsg3(jcr, M_FATAL, 0, _("Bad response to %s command. Wanted %s, got %s\n"),
          cmd, resp, sd->msg);
  }
  return false;
}
} /* namespace filedaemon */
