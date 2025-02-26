/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, October MM
/**
 * fd_cmds.c -- send commands to File daemon
 *
 * This routine is run as a separate thread.  There may be more
 * work to be done to make it totally reentrant!!!!
 *
 * Utility functions for sending info to File Daemon.
 * These functions are used by both backup and verify.
 */

#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "cats/cats.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/fd_sendfileset.h"
#include "findlib/find.h"
#include "dird/authenticate.h"
#include "dird/fd_cmds.h"
#include "dird/getmsg.h"
#include "dird/director_jcr_impl.h"
#include "dird/msgchan.h"
#include "dird/run_on_incoming_connect_interval.h"
#include "dird/scheduler.h"
#include "lib/berrno.h"
#include "lib/bsock_tcp.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/runscript.h"
#include "lib/util.h"
#include "lib/watchdog.h"

#include <algorithm>


namespace directordaemon {

const int debuglevel = 400;

/* Commands sent to File daemon */
static char filesetcmd[] = "fileset%s\n"; /* set full fileset */
static char jobcmd[] = "JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s\n";
static char jobcmdssl[]
    = "JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s ssl=%d\n";
/* Note, mtime_only is not used here -- implemented as file option */
static char levelcmd[] = "level = %s%s%s mtime_only=%d %s%s\n";
static char runscriptcmd[]
    = "Run OnSuccess=%u OnFailure=%u AbortOnError=%u When=%u Command=%s\n";
static char runbeforenowcmd[] = "RunBeforeNow\n";
static char restoreobjectendcmd[] = "restoreobject end\n";
static char bandwidthcmd[] = "setbandwidth=%lld Job=%s\n";
static char pluginoptionscmd[] = "pluginoptions %s\n";
static char getSecureEraseCmd[] = "getSecureEraseCmd\n";

/* Responses received from File daemon */
static char OKinc[] = "2000 OK include\n";
static char OKjob[] = "2000 OK Job";
static char OKlevel[] = "2000 OK level\n";
static char OKRunScript[] = "2000 OK RunScript\n";
static char OKRunBeforeNow[] = "2000 OK RunBeforeNow\n";
static char OKRestoreObject[] = "2000 OK ObjectRestored\n";
static char OKBandwidth[] = "2000 OK Bandwidth\n";
static char OKPluginOptions[] = "2000 OK PluginOptions\n";
static char OKgetSecureEraseCmd[] = "2000 OK FDSecureEraseCmd %s\n";

/* External functions */
extern DirectorResource* director;

static utime_t get_heartbeat_interval(ClientResource* res)
{
  utime_t heartbeat;

  if (res->heartbeat_interval) {
    heartbeat = res->heartbeat_interval;
  } else {
    heartbeat = me->heartbeat_interval;
  }

  return heartbeat;
}

/**
 * Open connection to File daemon.
 *
 * Try connecting every retry_interval (default 10 sec), and
 * give up after max_retry_time (default 30 mins).
 */
static bool connect_outbound_to_file_daemon(JobControlRecord* jcr,
                                            int retry_interval,
                                            int max_retry_time,
                                            bool verbose)
{
  bool result = false;
  BareosSocket* fd = NULL;
  utime_t heart_beat;

  if (!IsConnectingToClientAllowed(jcr)) {
    Dmsg1(120, "Connection to client \"%s\" is not allowed.\n",
          jcr->dir_impl->res.client->resource_name_);
    return false;
  }

  fd = new BareosSocketTCP;
  heart_beat = get_heartbeat_interval(jcr->dir_impl->res.client);

  char name[MAX_NAME_LENGTH + 100];
  bstrncpy(name, T_("Client: "), sizeof(name));
  bstrncat(name, jcr->dir_impl->res.client->resource_name_, sizeof(name));

  fd->SetSourceAddress(me->DIRsrc_addr);
  if (!fd->connect(jcr, retry_interval, max_retry_time, heart_beat, name,
                   jcr->dir_impl->res.client->address, NULL,
                   jcr->dir_impl->res.client->FDport, verbose)) {
    delete fd;
    fd = NULL;
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
  } else {
    jcr->file_bsock = fd;
    jcr->authenticated = false;
    Dmsg0(10, "Opened connection with File daemon\n");
    result = true;
  }

  return result;
}

static void OutputMessageForConnectionTry(JobControlRecord* jcr, UaContext* ua)
{
  std::string m;

  if (jcr->dir_impl->res.client->connection_successful_handshake_
          == ClientConnectionHandshakeMode::kUndefined
      || jcr->dir_impl->res.client->connection_successful_handshake_
             == ClientConnectionHandshakeMode::kFailed) {
    m = "Probing client protocol... (result will be saved until config reload)";
  } else {
    return;
  }

  if (jcr && jcr->JobId != 0) {
    std::string m1 = m + "\n";
    Jmsg(jcr, M_INFO, 0, m1.c_str());
  }
  if (ua) { ua->SendMsg(m.c_str()); }
}

static void SendInfoChosenCipher(JobControlRecord* jcr, UaContext* ua)
{
  std::string str = jcr->file_bsock->GetCipherMessageString();
  str += '\n';
  if (jcr->JobId != 0) { Jmsg(jcr, M_INFO, 0, str.c_str()); }
  if (ua) { /* only whith console connection */
    ua->SendRawMsg(str.c_str());
  }
}

static void SendInfoSuccess(JobControlRecord* jcr, UaContext* ua)
{
  std::string m;
  if (jcr->dir_impl->res.client->connection_successful_handshake_
      == ClientConnectionHandshakeMode::kUndefined) {
    m = "\r\v";
  }
  switch (jcr->dir_impl->connection_handshake_try_) {
    case ClientConnectionHandshakeMode::kTlsFirst:
      m += " Handshake: Immediate TLS,";
      break;
    case ClientConnectionHandshakeMode::kCleartextFirst:
      m += " Handshake: Cleartext,";
      break;
    default:
      m += " Handshake: Unknown mode";
      break;
  }

  if (jcr && jcr->JobId != 0) {
    std::string m1 = m;
    std::replace(m1.begin(), m1.end(), '\r', ' ');
    std::replace(m1.begin(), m1.end(), '\v', ' ');
    std::replace(m1.begin(), m1.end(), ',', ' ');
    m1 += std::string("\n");
    Jmsg(jcr, M_INFO, 0, m1.c_str());
  }
  if (ua) { /* only whith console connection */
    ua->SendRawMsg(m.c_str());
  }
}

void UpdateFailedConnectionHandshakeMode(JobControlRecord* jcr)
{
  switch (jcr->dir_impl->connection_handshake_try_) {
    case ClientConnectionHandshakeMode::kTlsFirst:
      if (jcr->file_bsock) {
        jcr->file_bsock->close();
        delete jcr->file_bsock;
        jcr->file_bsock = nullptr;
      }
      jcr->setJobStatus(JS_Running);
      if (!IsClientTlsRequired(jcr)) {
        jcr->dir_impl->connection_handshake_try_
            = ClientConnectionHandshakeMode::kCleartextFirst;
      } else {
        jcr->dir_impl->connection_handshake_try_
            = ClientConnectionHandshakeMode::kFailed;
      }
      break;
    case ClientConnectionHandshakeMode::kCleartextFirst:
      jcr->dir_impl->connection_handshake_try_
          = ClientConnectionHandshakeMode::kFailed;
      break;
    case ClientConnectionHandshakeMode::kFailed:
    default: /* should be one of class ClientConnectionHandshakeMode */
      ASSERT(false);
      break;
  }
}

/* try the connection modes starting with tls directly,
 * in case there is a client that cannot do Tls immediately then
 * fall back to cleartext md5-handshake */
void SetConnectionHandshakeMode(JobControlRecord* jcr, UaContext* ua)
{
  OutputMessageForConnectionTry(jcr, ua);
  if (jcr->dir_impl->res.client->connection_successful_handshake_
          == ClientConnectionHandshakeMode::kUndefined
      || jcr->dir_impl->res.client->connection_successful_handshake_
             == ClientConnectionHandshakeMode::kFailed) {
    if (jcr->dir_impl->res.client->IsTlsConfigured()) {
      jcr->dir_impl->connection_handshake_try_
          = ClientConnectionHandshakeMode::kTlsFirst;
    } else {
      jcr->dir_impl->connection_handshake_try_
          = ClientConnectionHandshakeMode::kCleartextFirst;
    }
    jcr->is_passive_client_connection_probing = true;
  } else {
    /* if there is a stored mode from a previous connection then use this */
    jcr->dir_impl->connection_handshake_try_
        = jcr->dir_impl->res.client->connection_successful_handshake_;
    jcr->is_passive_client_connection_probing = false;
  }
}

bool ConnectToFileDaemon(JobControlRecord* jcr,
                         int retry_interval,
                         int max_retry_time,
                         bool verbose,
                         UaContext* ua)
{
  if (!IsConnectingToClientAllowed(jcr) && !IsConnectFromClientAllowed(jcr)) {
    Emsg1(M_WARNING, 0, T_("Connecting to %s is not allowed.\n"),
          jcr->dir_impl->res.client->resource_name_);
    return false;
  }
  bool success = false;
  bool tcp_connect_failed = false;
  int connect_tries
      = 3; /* as a finish-hook for the UseWaitingClient mechanism */

  SetConnectionHandshakeMode(jcr, ua);

  do { /* while (tcp_connect_failed ...) */
    /* connect the tcp socket */
    if (!jcr->file_bsock) {
      if (!UseWaitingClient(jcr, 0)) {
        if (!connect_outbound_to_file_daemon(jcr, retry_interval,
                                             max_retry_time, verbose)) {
          if (!UseWaitingClient(
                  jcr,
                  max_retry_time)) { /* will set jcr->file_bsock accordingly */
            tcp_connect_failed = true;
          }
        }
      }
    }

    if (jcr->file_bsock) {
      jcr->setJobStatusWithPriorityCheck(JS_Running);
      if (AuthenticateWithFileDaemon(jcr)) {
        success = true;
        SendInfoSuccess(jcr, ua);
        SendInfoChosenCipher(jcr, ua);
        jcr->is_passive_client_connection_probing = false;
        jcr->dir_impl->res.client->connection_successful_handshake_
            = jcr->dir_impl->connection_handshake_try_;
      } else {
        /* authentication failed due to
         * - tls mismatch or
         * - if an old client cannot do tls- before md5-handshake
         * */
        UpdateFailedConnectionHandshakeMode(jcr);
      }
    } else {
      Jmsg(jcr, M_FATAL, 0, "\nFailed to connect to client \"%s\".\n",
           jcr->dir_impl->res.client->resource_name_);
    }
    connect_tries--;
  } while (!tcp_connect_failed && connect_tries && !success
           && jcr->dir_impl->connection_handshake_try_
                  != ClientConnectionHandshakeMode::kFailed);

  if (!success) { jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated); }

  return success;
}

int SendJobInfoToFileDaemon(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;

  if (jcr->sd_auth_key == NULL) { jcr->sd_auth_key = strdup("dummy"); }

  if (jcr->dir_impl->res.client->connection_successful_handshake_
      == ClientConnectionHandshakeMode::kTlsFirst) {
    /* client protocol onwards Bareos 18.2 */
    TlsPolicy tls_policy = kBnetTlsUnknown;
    int JobLevel = jcr->getJobLevel();
    switch (JobLevel) {
      case L_VERIFY_INIT:
      case L_VERIFY_CATALOG:
      case L_VERIFY_DISK_TO_CATALOG:
        tls_policy = kBnetTlsUnknown;
        break;
      default:
        StorageResource* storage = nullptr;
        if (jcr->dir_impl->res.write_storage) {
          storage = jcr->dir_impl->res.write_storage;
        } else if (jcr->dir_impl->res.read_storage) {
          storage = jcr->dir_impl->res.read_storage;
        } else {
          Jmsg(jcr, M_FATAL, 0, T_("No read or write storage defined\n"));
          jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
          return 0;
        }
        if (storage) {
          tls_policy = storage->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                                  : TlsPolicy::kBnetTlsNone;
        }
        break;
    }
    char ed1[30];
    fd->fsend(jobcmdssl, edit_int64(jcr->JobId, ed1), jcr->Job,
              jcr->VolSessionId, jcr->VolSessionTime, jcr->sd_auth_key,
              tls_policy);
  } else { /* jcr->dir_impl_->res.client->connection_successful_handshake_ !=
              ClientConnectionHandshakeMode::kTlsFirst */
    /* client protocol before Bareos 18.2 */
    char ed1[30];
    fd->fsend(jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job, jcr->VolSessionId,
              jcr->VolSessionTime, jcr->sd_auth_key);
  } /* if (jcr->dir_impl_->res.client->connection_successful_handshake_ ==
       ClientConnectionHandshakeMode::kTlsFirst) */

  if (!jcr->dir_impl->keep_sd_auth_key && !bstrcmp(jcr->sd_auth_key, "dummy")) {
    memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
  }

  Dmsg1(100, ">filed: %s", fd->msg);
  if (BgetDirmsg(fd) > 0) {
    Dmsg1(110, "<filed: %s", fd->msg);
    if (!bstrncmp(fd->msg, OKjob, strlen(OKjob))) {
      Jmsg(jcr, M_FATAL, 0, T_("File daemon \"%s\" rejected Job command: %s\n"),
           jcr->dir_impl->res.client->resource_name_, fd->msg);
      jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
      return 0;
    } else if (jcr->db) {
      ClientDbRecord cr;

      bstrncpy(cr.Name, jcr->dir_impl->res.client->resource_name_,
               sizeof(cr.Name));
      cr.AutoPrune = jcr->dir_impl->res.client->AutoPrune;
      cr.FileRetention = jcr->dir_impl->res.client->FileRetention;
      cr.JobRetention = jcr->dir_impl->res.client->JobRetention;
      bstrncpy(cr.Uname, fd->msg + strlen(OKjob) + 1, sizeof(cr.Uname));
      if (!jcr->db->UpdateClientRecord(jcr, &cr)) {
        Jmsg(jcr, M_WARNING, 0, T_("Error updating Client record. ERR=%s\n"),
             jcr->db->strerror());
      }
    }
  } else {
    Jmsg(jcr, M_FATAL, 0, T_("FD gave bad response to JobId command: %s\n"),
         BnetStrerror(fd));
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return 0;
  }

  return 1;
}

bool SendPreviousRestoreObjects(JobControlRecord* jcr)
{
  int JobLevel;

  JobLevel = jcr->getJobLevel();
  switch (JobLevel) {
    case L_DIFFERENTIAL:
    case L_INCREMENTAL:
      if (jcr->dir_impl->previous_jr) {
        if (!SendRestoreObjects(jcr, jcr->dir_impl->previous_jr->JobId,
                                false)) {
          return false;
        }
      }
      break;
    default:
      break;
  }

  return true;
}

bool SendBwlimitToFd(JobControlRecord* jcr, const char* Job)
{
  BareosSocket* fd = jcr->file_bsock;

  if (jcr->dir_impl->FDVersion >= FD_VERSION_4) {
    fd->fsend(bandwidthcmd, jcr->max_bandwidth, Job);
    if (!response(jcr, fd, OKBandwidth, "Bandwidth", DISPLAY_ERROR)) {
      jcr->max_bandwidth = 0; /* can't set bandwidth limit */
      return false;
    }
  }

  return true;
}

bool SendSecureEraseReqToFd(JobControlRecord* jcr)
{
  int32_t n;
  BareosSocket* fd = jcr->file_bsock;

  if (!jcr->dir_impl->FDSecureEraseCmd) {
    jcr->dir_impl->FDSecureEraseCmd = GetPoolMemory(PM_NAME);
  }

  if (jcr->dir_impl->FDVersion > FD_VERSION_53) {
    fd->fsend(getSecureEraseCmd);
    while ((n = BgetDirmsg(fd)) >= 0) {
      jcr->dir_impl->FDSecureEraseCmd = CheckPoolMemorySize(
          jcr->dir_impl->FDSecureEraseCmd, fd->message_length);
      if (sscanf(fd->msg, OKgetSecureEraseCmd, jcr->dir_impl->FDSecureEraseCmd)
          == 1) {
        Dmsg1(400, "Got FD Secure Erase Cmd: %s\n",
              jcr->dir_impl->FDSecureEraseCmd);
        break;
      } else {
        Jmsg(jcr, M_WARNING, 0, T_("Unexpected Client Secure Erase Cmd: %s\n"),
             fd->msg);
        PmStrcpy(jcr->dir_impl->FDSecureEraseCmd, "*None*");
        return false;
      }
    }
  } else {
    PmStrcpy(jcr->dir_impl->FDSecureEraseCmd, "*None*");
  }

  return true;
}

static void SendSinceTime(JobControlRecord* jcr)
{
  char ed1[50];
  utime_t stime;
  BareosSocket* fd = jcr->file_bsock;

  stime = StrToUtime(jcr->starttime_string);
  fd->fsend(levelcmd, "", NT_("since_utime "), edit_uint64(stime, ed1), 0,
            NT_("prev_job="), jcr->dir_impl->PrevJob);

  while (BgetDirmsg(fd) >= 0) { /* allow him to poll us to sync clocks */
    Jmsg(jcr, M_INFO, 0, "%s\n", fd->msg);
  }
}

/**
 * Send level command to FD.
 * Used for backup jobs and estimate command.
 */
bool SendLevelCommand(JobControlRecord* jcr)
{
  int JobLevel;
  BareosSocket* fd = jcr->file_bsock;
  const char* accurate = jcr->accurate ? "accurate_" : "";
  const char* not_accurate = "";
  const char* rerunning = jcr->rerunning ? " rerunning " : " ";

  // Send Level command to File daemon
  JobLevel = jcr->getJobLevel();
  switch (JobLevel) {
    case L_BASE:
      fd->fsend(levelcmd, not_accurate, "base", rerunning, 0, "", "");
      break;
    case L_NONE: /* L_NONE is the console, sending something off to the FD */
    case L_FULL:
      fd->fsend(levelcmd, not_accurate, "full", rerunning, 0, "", "");
      break;
    case L_DIFFERENTIAL:
      fd->fsend(levelcmd, accurate, "differential", rerunning, 0, "", "");
      SendSinceTime(jcr);
      break;
    case L_INCREMENTAL:
      fd->fsend(levelcmd, accurate, "incremental", rerunning, 0, "", "");
      SendSinceTime(jcr);
      break;
    case L_SINCE:
      Jmsg2(jcr, M_FATAL, 0, T_("Unimplemented backup level %d %c\n"), JobLevel,
            JobLevel);
      break;
    default:
      Jmsg2(jcr, M_FATAL, 0, T_("Unimplemented backup level %d %c\n"), JobLevel,
            JobLevel);
      return 0;
  }

  Dmsg1(120, ">filed: %s", fd->msg);

  if (!response(jcr, fd, OKlevel, "Level", DISPLAY_ERROR)) { return false; }

  return true;
}

// Send either an Included or an Excluded list to FD
static bool SendFileset(JobControlRecord* jcr, FilesetResource* fileset)
{
  BareosSocket* fd = jcr->file_bsock;

  if (!SendIncludeExcludeItems(jcr, fileset)) {
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  fd->signal(BNET_EOD); /* end of data */
  if (!response(jcr, fd, OKinc, "Include", DISPLAY_ERROR)) {
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }
  return true;
}

// Send include list to File daemon
bool SendIncludeExcludeLists(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  if (jcr->dir_impl->res.fileset->new_include) {
    fd->fsend(filesetcmd,
              jcr->dir_impl->res.fileset->enable_vss ? " vss=1" : "");
    return SendFileset(jcr, jcr->dir_impl->res.fileset);
  }
  return true;
}

// This checks to see if there are any non local runscripts for this job.
static bool HaveClientRunscripts(alist<RunScript*>* RunScripts)
{
  bool retval = false;

  if (!RunScripts) { return false; }

  if (RunScripts->empty()) { return false; }

  for (auto* cmd : *RunScripts) {
    if (!cmd->IsLocal()) { retval = true; }
  }

  return retval;
}

/**
 * Send RunScripts to File daemon
 * 1) We send all runscript to FD, they can be executed Before, After, or twice
 * 2) Then, we send a "RunBeforeNow" command to the FD to tell him to do the
 *    first run_script() call. (ie ClientRunBeforeJob)
 */
int SendRunscriptsCommands(JobControlRecord* jcr)
{
  int result;
  POOLMEM *msg, *ehost;
  BareosSocket* fd = jcr->file_bsock;
  bool has_before_jobs = false;

  // See if there are any runscripts that need to be ran on the client.
  if (!HaveClientRunscripts(jcr->dir_impl->res.job->RunScripts)) { return 1; }

  Dmsg0(120, "dird: sending runscripts to fd\n");

  msg = GetPoolMemory(PM_FNAME);
  ehost = GetPoolMemory(PM_FNAME);
  for (auto* cmd : *jcr->dir_impl->res.job->RunScripts) {
    if (!cmd->target.empty()) {
      ehost = edit_job_codes(jcr, ehost, cmd->target.c_str(), "");
      Dmsg2(200, "dird: runscript %s -> %s\n", cmd->target.c_str(), ehost);

      if (bstrcmp(ehost, jcr->dir_impl->res.client->resource_name_)) {
        PmStrcpy(msg, cmd->command.c_str());
        BashSpaces(msg);

        Dmsg1(120, "dird: sending runscripts to fd '%s'\n",
              cmd->command.c_str());

        fd->fsend(runscriptcmd, cmd->on_success, cmd->on_failure,
                  cmd->fail_on_error, cmd->when, msg);

        result = response(jcr, fd, OKRunScript, "RunScript", DISPLAY_ERROR);

        if (!result) { goto bail_out; }
      }
      /* TODO : we have to play with other client */
      /*
        else {
        send command to another client
        }
      */
    }

    // See if this is a ClientRunBeforeJob.
    if (cmd->when & SCRIPT_Before || cmd->when & SCRIPT_AfterVSS) {
      has_before_jobs = true;
    }
  }

  // Tell the FD to execute the ClientRunBeforeJob
  if (has_before_jobs) {
    fd->fsend(runbeforenowcmd);
    if (!response(jcr, fd, OKRunBeforeNow, "RunBeforeNow", DISPLAY_ERROR)) {
      goto bail_out;
    }
  }

  FreePoolMemory(msg);
  FreePoolMemory(ehost);

  return 1;

bail_out:
  Jmsg(jcr, M_FATAL, 0, T_("Client \"%s\" RunScript failed.\n"), ehost);
  FreePoolMemory(msg);
  FreePoolMemory(ehost);

  return 0;
}

struct RestoreObjectContext {
  JobControlRecord* jcr;
  int count;
};

// RestoreObjectHandler is called for each file found
static int RestoreObjectHandler(void* ctx, int, char** row)
{
  BareosSocket* fd;
  bool is_compressed;
  RestoreObjectContext* octx = (RestoreObjectContext*)ctx;
  JobControlRecord* jcr = octx->jcr;

  fd = jcr->file_bsock;
  if (jcr->IsJobCanceled()) { return 1; }

  // Old File Daemon doesn't handle restore objects
  if (jcr->dir_impl->FDVersion < FD_VERSION_3) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Client \"%s\" may not be used to restore "
            "this job. Please upgrade your client.\n"),
         jcr->dir_impl->res.client->resource_name_);
    return 1;
  }

  if (jcr->dir_impl->FDVersion
      < FD_VERSION_5) { /* Old version without PluginName */
    fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s\n", row[0], row[1],
              row[2], row[3], row[4], row[5], row[6]);
  } else {
    // bash spaces from PluginName
    BashSpaces(row[9]);
    fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s,%s\n", row[0], row[1],
              row[2], row[3], row[4], row[5], row[6], row[9]);
  }
  Dmsg1(010, "Send obj hdr=%s", fd->msg);

  fd->message_length = PmStrcpy(fd->msg, row[7]);
  fd->send(); /* send Object name */

  Dmsg1(010, "Send obj: %s\n", fd->msg);

  jcr->db->UnescapeObject(jcr, row[8],           /* Object  */
                          str_to_uint64(row[1]), /* Object length */
                          fd->msg, &fd->message_length);
  fd->send(); /* send object */
  octx->count++;

  // Don't try to print compressed objects.
  is_compressed = str_to_uint64(row[5]) > 0;
  if (debug_level >= 100 && !is_compressed) {
    for (int i = 0; i < fd->message_length; i++) {
      if (!fd->msg[i]) { fd->msg[i] = ' '; }
    }

    Dmsg1(100, "Send obj: %s\n", fd->msg);
  }

  return 0;
}

bool SendPluginOptions(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  int i;
  PoolMem cur_plugin_options(PM_MESSAGE);
  const char* plugin_options;
  POOLMEM* msg;

  if (jcr->dir_impl->plugin_options) {
    msg = GetPoolMemory(PM_FNAME);
    PmStrcpy(msg, jcr->dir_impl->plugin_options);
    BashSpaces(msg);

    fd->fsend(pluginoptionscmd, msg);
    FreePoolMemory(msg);

    if (!response(jcr, fd, OKPluginOptions, "PluginOptions", DISPLAY_ERROR)) {
      Jmsg(jcr, M_FATAL, 0, T_("Plugin options failed.\n"));
      return false;
    }
  }
  if (jcr->dir_impl->res.job && jcr->dir_impl->res.job->FdPluginOptions
      && jcr->dir_impl->res.job->FdPluginOptions->size()) {
    Dmsg2(200, "dird: sendpluginoptions found FdPluginOptions in res.job\n");
    foreach_alist_index (i, plugin_options,
                         jcr->dir_impl->res.job->FdPluginOptions) {
      PmStrcpy(cur_plugin_options, plugin_options);
      BashSpaces(cur_plugin_options.c_str());

      fd->fsend(pluginoptionscmd, cur_plugin_options.c_str());
      if (!response(jcr, fd, OKPluginOptions, "PluginOptions", DISPLAY_ERROR)) {
        Jmsg(jcr, M_FATAL, 0, T_("Plugin options failed.\n"));
        return false;
      }
    }
  }

  return true;
}

static void SendGlobalRestoreObjects(JobControlRecord* jcr,
                                     RestoreObjectContext* octx)
{
  char ed1[50];
  PoolMem query(PM_MESSAGE);

  if (!jcr->JobIds || !jcr->JobIds[0]) { return; }

  // Send restore objects for all jobs involved
  jcr->db->FillQuery(query, BareosDb::SQL_QUERY::get_restore_objects,
                     jcr->JobIds, FT_RESTORE_FIRST);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);

  jcr->db->FillQuery(query, BareosDb::SQL_QUERY::get_restore_objects,
                     jcr->JobIds, FT_PLUGIN_CONFIG);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);

  // Send config objects for the current restore job
  jcr->db->FillQuery(query, BareosDb::SQL_QUERY::get_restore_objects,
                     edit_uint64(jcr->JobId, ed1), FT_PLUGIN_CONFIG_FILLED);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);
}

static void SendJobSpecificRestoreObjects(JobControlRecord* jcr,
                                          JobId_t JobId,
                                          RestoreObjectContext* octx)
{
  char ed1[50];
  PoolMem query(PM_MESSAGE);

  // Send restore objects for specific JobId.
  jcr->db->FillQuery(query, BareosDb::SQL_QUERY::get_restore_objects,
                     edit_uint64(JobId, ed1), FT_RESTORE_FIRST);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);

  jcr->db->FillQuery(query, BareosDb::SQL_QUERY::get_restore_objects,
                     edit_uint64(JobId, ed1), FT_PLUGIN_CONFIG);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);
}

bool SendRestoreObjects(JobControlRecord* jcr, JobId_t JobId, bool send_global)
{
  BareosSocket* fd;
  RestoreObjectContext octx;

  octx.jcr = jcr;
  octx.count = 0;

  if (send_global) {
    SendGlobalRestoreObjects(jcr, &octx);
  } else {
    SendJobSpecificRestoreObjects(jcr, JobId, &octx);
  }

  /* Send to FD only if we have at least one restore object.
   * This permits backward compatibility with older FDs. */
  if (octx.count > 0) {
    fd = jcr->file_bsock;
    fd->fsend(restoreobjectendcmd);
    if (!response(jcr, fd, OKRestoreObject, "RestoreObject", DISPLAY_ERROR)) {
      Jmsg(jcr, M_FATAL, 0, T_("RestoreObject failed.\n"));
      return false;
    }
  }

  return true;
}

/**
 * Read the attributes from the File daemon for
 * a Verify job and store them in the catalog.
 */
int GetAttributesAndPutInCatalog(JobControlRecord* jcr)
{
  BareosSocket* fd;
  int n = 0;
  AttributesDbRecord* ar = NULL;
  PoolMem digest(PM_MESSAGE);

  fd = jcr->file_bsock;
  jcr->dir_impl->jr.FirstIndex = 1;
  jcr->dir_impl->FileIndex = 0;

  // Start transaction allocates jcr->attr and jcr->ar if needed
  jcr->db->StartTransaction(jcr); /* start transaction if not already open */
  ar = jcr->ar;

  Dmsg0(120, "dird: waiting to receive file attributes\n");

  // Pickup file attributes and digest
  while (!fd->errors && (n = BgetDirmsg(fd)) > 0) {
    uint32_t file_index;
    int stream, len;
    char *p, *fn;
    PoolMem Digest(PM_MESSAGE); /* Either Verify opts or MD5/SHA1 digest */
    Digest.check_size(fd->message_length);
    if ((len
         = sscanf(fd->msg, "%ld %d %s", &file_index, &stream, Digest.c_str()))
        != 3) {
      Jmsg(jcr, M_FATAL, 0,
           T_("<filed: bad attributes, expected 3 fields got %d\n"
              "message_length=%d msg=%s\n"),
           len, fd->message_length, fd->msg);
      jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
      return 0;
    }
    p = fd->msg;

    // The following three fields were sscanf'ed above so skip them
    SkipNonspaces(&p); /* skip FileIndex */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip Stream */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip Opts_Digest */
    p++;               /* skip space */
    Dmsg1(debuglevel, "Stream=%d\n", stream);
    if (stream == STREAM_UNIX_ATTRIBUTES
        || stream == STREAM_UNIX_ATTRIBUTES_EX) {
      if (jcr->cached_attribute) {
        Dmsg3(debuglevel, "Cached attr. Stream=%d fname=%s\n", ar->Stream,
              ar->fname, ar->attr);
        if (!jcr->db->CreateFileAttributesRecord(jcr, ar)) {
          Jmsg1(jcr, M_FATAL, 0, T_("Attribute create error. %s"),
                jcr->db->strerror());
        }
      }

      // Any cached attr is flushed so we can reuse jcr->attr and jcr->ar
      jcr->dir_impl->fname.check_size(fd->message_length);
      fn = jcr->dir_impl->fname.c_str();
      while (*p != 0) { *fn++ = *p++; /* copy filename */ }
      *fn = *p++;             /* term filename and point p to attribs */
      PmStrcpy(jcr->attr, p); /* save attributes */
      jcr->JobFiles++;
      jcr->dir_impl->FileIndex = file_index;
      ar->attr = jcr->attr;
      ar->fname = jcr->dir_impl->fname.c_str();
      ar->FileIndex = file_index;
      ar->Stream = stream;
      ar->link = NULL;
      ar->JobId = jcr->JobId;
      ar->ClientId = jcr->ClientId;
      ar->PathId = 0;
      ar->Digest = NULL;
      ar->DigestType = CRYPTO_DIGEST_NONE;
      ar->DeltaSeq = 0;
      jcr->cached_attribute = true;

      Dmsg2(debuglevel, "dird<filed: stream=%d %s\n", stream,
            jcr->dir_impl->fname.c_str());
      Dmsg1(debuglevel, "dird<filed: attr=%s\n", ar->attr);
      jcr->FileId = ar->FileId;
    } else if (CryptoDigestStreamType(stream) != CRYPTO_DIGEST_NONE) {
      size_t length;

      /* First, get STREAM_UNIX_ATTRIBUTES and fill AttributesDbRecord structure
       * Next, we CAN have a CRYPTO_DIGEST, so we fill AttributesDbRecord with
       * it (or not) When we get a new STREAM_UNIX_ATTRIBUTES, we known that we
       * can add file to the catalog At the end, we have to add the last file */
      if (jcr->dir_impl->FileIndex != (uint32_t)file_index) {
        Jmsg3(jcr, M_ERROR, 0, T_("%s index %d not same as attributes %d\n"),
              stream_to_ascii(stream), file_index, jcr->dir_impl->FileIndex);
        continue;
      }

      ar->Digest = digest.c_str();
      ar->DigestType = CryptoDigestStreamType(stream);
      length = strlen(Digest.c_str());
      digest.check_size(length * 2 + 1);
      jcr->db->EscapeString(jcr, digest.c_str(), Digest.c_str(), length);
      Dmsg4(debuglevel, "stream=%d DigestLen=%d Digest=%s type=%d\n", stream,
            strlen(digest.c_str()), digest.c_str(), ar->DigestType);
    }
    jcr->dir_impl->jr.JobFiles = jcr->JobFiles = file_index;
    jcr->dir_impl->jr.LastIndex = file_index;
  }

  if (IsBnetError(fd)) {
    Jmsg1(jcr, M_FATAL, 0,
          T_("<filed: Network error getting attributes. ERR=%s\n"),
          fd->bstrerror());
    return 0;
  }

  if (jcr->cached_attribute) {
    Dmsg3(debuglevel, "Cached attr with digest. Stream=%d fname=%s attr=%s\n",
          ar->Stream, ar->fname, ar->attr);
    if (!jcr->db->CreateFileAttributesRecord(jcr, ar)) {
      Jmsg1(jcr, M_FATAL, 0, T_("Attribute create error. %s"),
            jcr->db->strerror());
    }
    jcr->cached_attribute = false;
  }

  jcr->setJobStatusWithPriorityCheck(JS_Terminated);

  return 1;
}

// Cancel a job running in the File daemon
bool CancelFileDaemonJob(UaContext* ua, JobControlRecord* jcr)
{
  BareosSocket* fd;

  ua->jcr->dir_impl->res.client = jcr->dir_impl->res.client;
  if (!ConnectToFileDaemon(ua->jcr, 10, me->FDConnectTimeout, true, ua)) {
    ua->ErrorMsg(T_("\nFailed to connect to File daemon.\n"));
    return false;
  }
  Dmsg0(200, "Connected to file daemon\n");
  fd = ua->jcr->file_bsock;
  fd->fsend("cancel Job=%s\n", jcr->Job);
  while (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }
  fd->signal(BNET_TERMINATE);
  fd->close();
  delete ua->jcr->file_bsock;
  ua->jcr->file_bsock = NULL;
  jcr->file_bsock->SetTerminated();
  jcr->MyThreadSendSignal(TIMEOUT_SIGNAL);
  return true;
}

// Get the status of a remote File Daemon.
void DoNativeClientStatus(UaContext* ua, ClientResource* client, char* cmd)
{
  BareosSocket* fd;

  // Connect to File daemon
  ua->jcr->dir_impl->res.client = client;

  // Try to connect for 15 seconds
  if (!ua->api) {
    ua->SendMsg(T_("Connecting to Client %s at %s:%d\n"),
                client->resource_name_, client->address, client->FDport);
  }

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false, ua)) {
    ua->SendMsg(T_("\nFailed to connect to Client %s.\n====\n"),
                client->resource_name_);
    if (ua->jcr->file_bsock) {
      ua->jcr->file_bsock->close();
      delete ua->jcr->file_bsock;
      ua->jcr->file_bsock = NULL;
    }
    return;
  }

  Dmsg0(20, T_("Connected to file daemon\n"));
  fd = ua->jcr->file_bsock;
  if (cmd) {
    fd->fsend(".status %s", cmd);
  } else {
    fd->fsend("status");
  }

  while (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }

  fd->signal(BNET_TERMINATE);
  fd->close();
  delete ua->jcr->file_bsock;
  ua->jcr->file_bsock = NULL;

  return;
}

// resolve a host on a filedaemon
void DoClientResolve(UaContext* ua, ClientResource* client)
{
  BareosSocket* fd;

  // Connect to File daemon
  ua->jcr->dir_impl->res.client = client;

  // Try to connect for 15 seconds
  if (!ua->api) {
    ua->SendMsg(T_("Connecting to Client %s at %s:%d\n"),
                client->resource_name_, client->address, client->FDport);
  }

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false, ua)) {
    ua->SendMsg(T_("\nFailed to connect to Client %s.\n====\n"),
                client->resource_name_);
    if (ua->jcr->file_bsock) {
      ua->jcr->file_bsock->close();
      delete ua->jcr->file_bsock;
      ua->jcr->file_bsock = NULL;
    }
    return;
  }

  Dmsg0(20, T_("Connected to file daemon\n"));
  fd = ua->jcr->file_bsock;

  for (int i = 1; i < ua->argc; i++) {
    if (!*ua->argk[i]) { continue; }

    fd->fsend("resolve %s", ua->argk[i]);
    while (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }
  }

  fd->signal(BNET_TERMINATE);
  fd->close();
  delete ua->jcr->file_bsock;
  ua->jcr->file_bsock = NULL;

  return;
}

void* HandleFiledConnection(connection_pool& connections,
                            BareosSocket* fd,
                            char* client_name,
                            int fd_protocol_version)
{
  connection conn{client_name, fd_protocol_version, fd};

  auto client_resource
      = (ClientResource*)my_config->GetResWithName(R_CLIENT, client_name);
  if (!client_resource) {
    Emsg1(M_WARNING, 0,
          "Client \"%s\" tries to connect, "
          "but no matching resource is defined.\n",
          client_name);
    return NULL;
  }

  if (!IsConnectFromClientAllowed(client_resource)) {
    Emsg1(M_WARNING, 0,
          "Connection from client \"%s\" to director is not allowed.\n",
          client_name);
    return NULL;
  }

  if (!AuthenticateFileDaemon(fd, client_name)) { return NULL; }

  Dmsg1(20, "Connected to file daemon %s\n", client_name);

  connections.add_authenticated_connection(std::move(conn));

  RunOnIncomingConnectInterval(client_name).Run();

  /* The connection is now kept in the connection_pool.
   * This thread is no longer required and will end now. */
  return NULL;
}
} /* namespace directordaemon */
