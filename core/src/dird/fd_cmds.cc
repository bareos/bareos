/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, October MM
 */
/**
 * fd_cmds.c -- send commands to File daemon
 *
 * This routine is run as a separate thread.  There may be more
 * work to be done to make it totally reentrant!!!!
 *
 * Utility functions for sending info to File Daemon.
 * These functions are used by both backup and verify.
 */

#include <algorithm>
#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "findlib/find.h"
#include "dird/authenticate.h"
#include "dird/fd_cmds.h"
#include "dird/getmsg.h"
#include "dird/msgchan.h"
#include "lib/berrno.h"
#include "lib/bsock_tcp.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/watchdog.h"


namespace directordaemon {

const int debuglevel = 400;

/* Commands sent to File daemon */
static char filesetcmd[] = "fileset%s\n"; /* set full fileset */
static char jobcmd[] = "JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s\n";
static char jobcmdssl[] =
    "JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s ssl=%d\n";
/* Note, mtime_only is not used here -- implemented as file option */
static char levelcmd[] = "level = %s%s%s mtime_only=%d %s%s\n";
static char runscriptcmd[] =
    "Run OnSuccess=%u OnFailure=%u AbortOnError=%u When=%u Command=%s\n";
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

/* Forward referenced functions */
static bool SendListItem(JobControlRecord* jcr,
                         const char* code,
                         char* item,
                         BareosSocket* fd);

/* External functions */
extern DirectorResource* director;

#define INC_LIST 0
#define EXC_LIST 1

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
    Dmsg1(120, "connecting to client \"%s\" is not allowed.\n",
          jcr->res.client->resource_name_);
    return false;
  }

  fd = new BareosSocketTCP;
  if (me->nokeepalive) { fd->ClearKeepalive(); }
  heart_beat = get_heartbeat_interval(jcr->res.client);

  char name[MAX_NAME_LENGTH + 100];
  bstrncpy(name, _("Client: "), sizeof(name));
  bstrncat(name, jcr->res.client->resource_name_, sizeof(name));

  fd->SetSourceAddress(me->DIRsrc_addr);
  if (!fd->connect(jcr, retry_interval, max_retry_time, heart_beat, name,
                   jcr->res.client->address, NULL, jcr->res.client->FDport,
                   verbose)) {
    delete fd;
    fd = NULL;
    jcr->setJobStatus(JS_ErrorTerminated);
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

  if (jcr->res.client->connection_successful_handshake_ ==
          ClientConnectionHandshakeMode::kUndefined ||
      jcr->res.client->connection_successful_handshake_ ==
          ClientConnectionHandshakeMode::kFailed) {
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
  std::string str;
  jcr->file_bsock->GetCipherMessageString(str);
  str += '\n';
  if (jcr->JobId != 0) { Jmsg(jcr, M_INFO, 0, str.c_str()); }
  if (ua) { /* only whith console connection */
    ua->SendRawMsg(str.c_str());
  }
}

static void SendInfoSuccess(JobControlRecord* jcr, UaContext* ua)
{
  std::string m;
  if (jcr->res.client->connection_successful_handshake_ ==
      ClientConnectionHandshakeMode::kUndefined) {
    m += "\r\v";
  }
  bool add_newline_in_joblog = false;
  switch (jcr->connection_handshake_try_) {
    case ClientConnectionHandshakeMode::kTlsFirst:
      m += " Handshake: Immediate TLS,";
      break;
    case ClientConnectionHandshakeMode::kCleartextFirst:
      m += " Handshake: Cleartext,";
      add_newline_in_joblog = true;
      break;
    default:
      m += " unknown mode\n";
      break;
  }

  if (jcr && jcr->JobId != 0) {
    std::string m1 = m;
    std::replace(m1.begin(), m1.end(), '\r', ' ');
    std::replace(m1.begin(), m1.end(), '\v', ' ');
    std::replace(m1.begin(), m1.end(), ',', ' ');
    if (add_newline_in_joblog) { m1 += std::string("\n"); }
    Jmsg(jcr, M_INFO, 0, m1.c_str());
  }
  if (ua) { /* only whith console connection */
    ua->SendRawMsg(m.c_str());
  }
}

bool ConnectToFileDaemon(JobControlRecord* jcr,
                         int retry_interval,
                         int max_retry_time,
                         bool verbose,
                         UaContext* ua)
{
  bool success = false;
  bool tcp_connect_failed = false;
  int connect_tries =
      3; /* as a finish-hook for the UseWaitingClient mechanism */

  /* try the connection modes starting with tls directly,
   * in case there is a client that cannot do Tls immediately then
   * fall back to cleartext md5-handshake */
  OutputMessageForConnectionTry(jcr, ua);
  if (jcr->res.client->connection_successful_handshake_ ==
          ClientConnectionHandshakeMode::kUndefined ||
      jcr->res.client->connection_successful_handshake_ ==
          ClientConnectionHandshakeMode::kFailed) {
    if (jcr->res.client->IsTlsConfigured()) {
      jcr->connection_handshake_try_ = ClientConnectionHandshakeMode::kTlsFirst;
    } else {
      jcr->connection_handshake_try_ =
          ClientConnectionHandshakeMode::kCleartextFirst;
    }
    jcr->is_passive_client_connection_probing = true;
  } else {
    /* if there is a stored mode from a previous connection then use this */
    jcr->connection_handshake_try_ =
        jcr->res.client->connection_successful_handshake_;
    jcr->is_passive_client_connection_probing = false;
  }

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
      jcr->setJobStatus(JS_Running);
      if (AuthenticateWithFileDaemon(jcr)) {
        success = true;
        SendInfoSuccess(jcr, ua);
        SendInfoChosenCipher(jcr, ua);
        jcr->is_passive_client_connection_probing = false;
        jcr->res.client->connection_successful_handshake_ =
            jcr->connection_handshake_try_;
      } else {
        /* authentication failed due to
         * - tls mismatch or
         * - if an old client cannot do tls- before md5-handshake
         * */
        switch (jcr->connection_handshake_try_) {
          case ClientConnectionHandshakeMode::kTlsFirst:
            if (jcr->file_bsock) {
              jcr->file_bsock->close();
              delete jcr->file_bsock;
              jcr->file_bsock = nullptr;
            }
            jcr->resetJobStatus(JS_Running);
            jcr->connection_handshake_try_ =
                ClientConnectionHandshakeMode::kCleartextFirst;
            break;
          case ClientConnectionHandshakeMode::kCleartextFirst:
            jcr->connection_handshake_try_ =
                ClientConnectionHandshakeMode::kFailed;
            break;
          case ClientConnectionHandshakeMode::kFailed:
          default: /* should bei one of class ClientConnectionHandshakeMode */
            ASSERT(false);
            break;
        }
      }
    } else {
      Jmsg(jcr, M_FATAL, 0, "\nFailed to connect to client \"%s\".\n",
           jcr->res.client->resource_name_);
    }
    connect_tries--;
  } while (!tcp_connect_failed && connect_tries && !success &&
           jcr->connection_handshake_try_ !=
               ClientConnectionHandshakeMode::kFailed);

  if (!success) { jcr->setJobStatus(JS_ErrorTerminated); }

  return success;
}

int SendJobInfoToFileDaemon(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;

  if (jcr->sd_auth_key == NULL) { jcr->sd_auth_key = strdup("dummy"); }

  if (jcr->res.client->connection_successful_handshake_ ==
      ClientConnectionHandshakeMode::kTlsFirst) {
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
        if (jcr->res.write_storage) {
          storage = jcr->res.write_storage;
        } else if (jcr->res.read_storage) {
          storage = jcr->res.read_storage;
        } else {
          Jmsg(jcr, M_FATAL, 0, _("No read or write storage defined\n"));
          jcr->setJobStatus(JS_ErrorTerminated);
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
  } else { /* jcr->res.client->connection_successful_handshake_ !=
              ClientConnectionHandshakeMode::kTlsFirst */
    /* client protocol before Bareos 18.2 */
    char ed1[30];
    fd->fsend(jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job, jcr->VolSessionId,
              jcr->VolSessionTime, jcr->sd_auth_key);
  } /* if (jcr->res.client->connection_successful_handshake_ ==
       ClientConnectionHandshakeMode::kTlsFirst) */

  if (!jcr->keep_sd_auth_key && !bstrcmp(jcr->sd_auth_key, "dummy")) {
    memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
  }

  Dmsg1(100, ">filed: %s", fd->msg);
  if (BgetDirmsg(fd) > 0) {
    Dmsg1(110, "<filed: %s", fd->msg);
    if (!bstrncmp(fd->msg, OKjob, strlen(OKjob))) {
      Jmsg(jcr, M_FATAL, 0, _("File daemon \"%s\" rejected Job command: %s\n"),
           jcr->res.client->resource_name_, fd->msg);
      jcr->setJobStatus(JS_ErrorTerminated);
      return 0;
    } else if (jcr->db) {
      ClientDbRecord cr;

      memset(&cr, 0, sizeof(cr));
      bstrncpy(cr.Name, jcr->res.client->resource_name_, sizeof(cr.Name));
      cr.AutoPrune = jcr->res.client->AutoPrune;
      cr.FileRetention = jcr->res.client->FileRetention;
      cr.JobRetention = jcr->res.client->JobRetention;
      bstrncpy(cr.Uname, fd->msg + strlen(OKjob) + 1, sizeof(cr.Uname));
      if (!jcr->db->UpdateClientRecord(jcr, &cr)) {
        Jmsg(jcr, M_WARNING, 0, _("Error updating Client record. ERR=%s\n"),
             jcr->db->strerror());
      }
    }
  } else {
    Jmsg(jcr, M_FATAL, 0, _("FD gave bad response to JobId command: %s\n"),
         BnetStrerror(fd));
    jcr->setJobStatus(JS_ErrorTerminated);
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
      if (jcr->previous_jr.JobId > 0) {
        if (!SendRestoreObjects(jcr, jcr->previous_jr.JobId, false)) {
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

  if (jcr->FDVersion >= FD_VERSION_4) {
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

  if (!jcr->FDSecureEraseCmd) {
    jcr->FDSecureEraseCmd = GetPoolMemory(PM_NAME);
  }

  if (jcr->FDVersion > FD_VERSION_53) {
    fd->fsend(getSecureEraseCmd);
    while ((n = BgetDirmsg(fd)) >= 0) {
      jcr->FDSecureEraseCmd =
          CheckPoolMemorySize(jcr->FDSecureEraseCmd, fd->message_length);
      if (sscanf(fd->msg, OKgetSecureEraseCmd, jcr->FDSecureEraseCmd) == 1) {
        Dmsg1(400, "Got FD Secure Erase Cmd: %s\n", jcr->FDSecureEraseCmd);
        break;
      } else {
        Jmsg(jcr, M_WARNING, 0, _("Unexpected Client Secure Erase Cmd: %s\n"),
             fd->msg);
        PmStrcpy(jcr->FDSecureEraseCmd, "*None*");
        return false;
      }
    }
  } else {
    PmStrcpy(jcr->FDSecureEraseCmd, "*None*");
  }

  return true;
}

static void SendSinceTime(JobControlRecord* jcr)
{
  char ed1[50];
  utime_t stime;
  BareosSocket* fd = jcr->file_bsock;

  stime = StrToUtime(jcr->stime);
  fd->fsend(levelcmd, "", NT_("since_utime "), edit_uint64(stime, ed1), 0,
            NT_("prev_job="), jcr->PrevJob);

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

  /*
   * Send Level command to File daemon
   */
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
      Jmsg2(jcr, M_FATAL, 0, _("Unimplemented backup level %d %c\n"), JobLevel,
            JobLevel);
      break;
    default:
      Jmsg2(jcr, M_FATAL, 0, _("Unimplemented backup level %d %c\n"), JobLevel,
            JobLevel);
      return 0;
  }

  Dmsg1(120, ">filed: %s", fd->msg);

  if (!response(jcr, fd, OKlevel, "Level", DISPLAY_ERROR)) { return false; }

  return true;
}

/**
 * Send either an Included or an Excluded list to FD
 */
static bool SendFileset(JobControlRecord* jcr)
{
  FilesetResource* fileset = jcr->res.fileset;
  BareosSocket* fd = jcr->file_bsock;
  StorageResource* store = jcr->res.write_storage;
  int num;
  bool include = true;

  while (1) {
    if (include) {
      num = fileset->include_items.size();
    } else {
      num = fileset->exclude_items.size();
    }
    for (int i = 0; i < num; i++) {
      char* item;
      IncludeExcludeItem* ie;

      if (include) {
        ie = fileset->include_items[i];
        fd->fsend("I\n");
      } else {
        ie = fileset->exclude_items[i];
        fd->fsend("E\n");
      }

      for (int j = 0; j < ie->ignoredir.size(); j++) {
        fd->fsend("Z %s\n", ie->ignoredir.get(j));
      }

      for (std::size_t j = 0; j < ie->opts_list.size(); j++) {
        FileOptions* fo = ie->opts_list[j];
        bool enhanced_wild = false;

        for (int k = 0; fo->opts[k] != '\0'; k++) {
          if (fo->opts[k] == 'W') {
            enhanced_wild = true;
            break;
          }
        }

        /*
         * Strip out compression option Zn if disallowed for this Storage
         */
        if (store && !store->AllowCompress) {
          char newopts[MAX_FOPTS];
          bool done =
              false; /* print warning only if compression enabled in FS */
          int l = 0;

          for (int k = 0; fo->opts[k] != '\0'; k++) {
            /*
             * Z compress option is followed by the single-digit compress level
             * or 'o' For fastlz its Zf with a single char selecting the actual
             * compression algo.
             */
            if (fo->opts[k] == 'Z' && fo->opts[k + 1] == 'f') {
              done = true;
              k += 2; /* skip option */
            } else if (fo->opts[k] == 'Z') {
              done = true;
              k++; /* skip option and level */
            } else {
              newopts[l] = fo->opts[k];
              l++;
            }
          }
          newopts[l] = '\0';

          if (done) {
            Jmsg(jcr, M_INFO, 0,
                 _("FD compression disabled for this Job because "
                   "AllowCompress=No in Storage resource.\n"));
          }

          /*
           * Send the new trimmed option set without overwriting fo->opts
           */
          fd->fsend("O %s\n", newopts);
        } else {
          /*
           * Send the original options
           */
          fd->fsend("O %s\n", fo->opts);
        }

        for (int k = 0; k < fo->regex.size(); k++) {
          fd->fsend("R %s\n", fo->regex.get(k));
        }

        for (int k = 0; k < fo->regexdir.size(); k++) {
          fd->fsend("RD %s\n", fo->regexdir.get(k));
        }

        for (int k = 0; k < fo->regexfile.size(); k++) {
          fd->fsend("RF %s\n", fo->regexfile.get(k));
        }

        for (int k = 0; k < fo->wild.size(); k++) {
          fd->fsend("W %s\n", fo->wild.get(k));
        }

        for (int k = 0; k < fo->wilddir.size(); k++) {
          fd->fsend("WD %s\n", fo->wilddir.get(k));
        }

        for (int k = 0; k < fo->wildfile.size(); k++) {
          fd->fsend("WF %s\n", fo->wildfile.get(k));
        }

        for (int k = 0; k < fo->wildbase.size(); k++) {
          fd->fsend("W%c %s\n", enhanced_wild ? 'B' : 'F', fo->wildbase.get(k));
        }

        for (int k = 0; k < fo->base.size(); k++) {
          fd->fsend("B %s\n", fo->base.get(k));
        }

        for (int k = 0; k < fo->fstype.size(); k++) {
          fd->fsend("X %s\n", fo->fstype.get(k));
        }

        for (int k = 0; k < fo->Drivetype.size(); k++) {
          fd->fsend("XD %s\n", fo->Drivetype.get(k));
        }

        if (fo->plugin) { fd->fsend("G %s\n", fo->plugin); }

        if (fo->reader) { fd->fsend("D %s\n", fo->reader); }

        if (fo->writer) { fd->fsend("T %s\n", fo->writer); }

        fd->fsend("N\n");
      }

      for (int j = 0; j < ie->name_list.size(); j++) {
        item = (char*)ie->name_list.get(j);
        if (!SendListItem(jcr, "F ", item, fd)) { goto bail_out; }
      }
      fd->fsend("N\n");

      for (int j = 0; j < ie->plugin_list.size(); j++) {
        item = (char*)ie->plugin_list.get(j);
        if (!SendListItem(jcr, "P ", item, fd)) { goto bail_out; }
      }
      fd->fsend("N\n");
    }

    if (!include) { /* If we just did excludes */
      break;        /*   all done */
    }

    include = false; /* Now do excludes */
  }

  fd->signal(BNET_EOD); /* end of data */
  if (!response(jcr, fd, OKinc, "Include", DISPLAY_ERROR)) { goto bail_out; }
  return true;

bail_out:
  jcr->setJobStatus(JS_ErrorTerminated);
  return false;
}

static bool SendListItem(JobControlRecord* jcr,
                         const char* code,
                         char* item,
                         BareosSocket* fd)
{
  Bpipe* bpipe;
  FILE* ffd;
  char buf[2000];
  int optlen, status;
  char* p = item;

  switch (*p) {
    case '|':
      p++; /* skip over the | */
      fd->msg = edit_job_codes(jcr, fd->msg, p, "");
      bpipe = OpenBpipe(fd->msg, 0, "r");
      if (!bpipe) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, _("Cannot run program: %s. ERR=%s\n"), p,
             be.bstrerror());
        return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf + optlen, sizeof(buf) - optlen, bpipe->rfd)) {
        fd->message_length = Mmsg(fd->msg, "%s", buf);
        Dmsg2(500, "Inc/exc len=%d: %s", fd->message_length, fd->msg);
        if (!BnetSend(fd)) {
          Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
          return false;
        }
      }
      if ((status = CloseBpipe(bpipe)) != 0) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, _("Error running program: %s. ERR=%s\n"), p,
             be.bstrerror(status));
        return false;
      }
      break;
    case '<':
      p++; /* skip over < */
      if ((ffd = fopen(p, "rb")) == NULL) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, _("Cannot open included file: %s. ERR=%s\n"), p,
             be.bstrerror());
        return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf + optlen, sizeof(buf) - optlen, ffd)) {
        fd->message_length = Mmsg(fd->msg, "%s", buf);
        if (!BnetSend(fd)) {
          Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
          fclose(ffd);
          return false;
        }
      }
      fclose(ffd);
      break;
    case '\\':
      p++; /* skip over \ */
           /* Note, fall through wanted */
    default:
      PmStrcpy(fd->msg, code);
      fd->message_length = PmStrcat(fd->msg, p);
      Dmsg1(500, "Inc/Exc name=%s\n", fd->msg);
      if (!fd->send()) {
        Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
        return false;
      }
      break;
  }
  return true;
}

/**
 * Send include list to File daemon
 */
bool SendIncludeList(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  if (jcr->res.fileset->new_include) {
    fd->fsend(filesetcmd, jcr->res.fileset->enable_vss ? " vss=1" : "");
    return SendFileset(jcr);
  }
  return true;
}

/**
 * Send exclude list to File daemon
 *   Under the new scheme, the Exclude list
 *   is part of the FileSet sent with the
 *   "include_list" above.
 */
bool SendExcludeList(JobControlRecord* jcr) { return true; }

/*
 * This checks to see if there are any non local runscripts for this job.
 */
static bool HaveClientRunscripts(alist* RunScripts)
{
  RunScript* cmd = nullptr;
  bool retval = false;

  if (RunScripts->empty()) { return false; }

  foreach_alist (cmd, RunScripts) {
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
  RunScript* cmd = nullptr;
  POOLMEM *msg, *ehost;
  BareosSocket* fd = jcr->file_bsock;
  bool has_before_jobs = false;

  /*
   * See if there are any runscripts that need to be ran on the client.
   */
  if (!HaveClientRunscripts(jcr->res.job->RunScripts)) { return 1; }

  Dmsg0(120, "dird: sending runscripts to fd\n");

  msg = GetPoolMemory(PM_FNAME);
  ehost = GetPoolMemory(PM_FNAME);
  foreach_alist (cmd, jcr->res.job->RunScripts) {
    if (cmd->CanRunAtLevel(jcr->getJobLevel()) && cmd->target) {
      ehost = edit_job_codes(jcr, ehost, cmd->target, "");
      Dmsg2(200, "dird: runscript %s -> %s\n", cmd->target, ehost);

      if (bstrcmp(ehost, jcr->res.client->resource_name_)) {
        PmStrcpy(msg, cmd->command);
        BashSpaces(msg);

        Dmsg1(120, "dird: sending runscripts to fd '%s'\n", cmd->command);

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

    /*
     * See if this is a ClientRunBeforeJob.
     */
    if (cmd->when & SCRIPT_Before || cmd->when & SCRIPT_AfterVSS) {
      has_before_jobs = true;
    }
  }

  /*
   * Tell the FD to execute the ClientRunBeforeJob
   */
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
  Jmsg(jcr, M_FATAL, 0, _("Client \"%s\" RunScript failed.\n"), ehost);
  FreePoolMemory(msg);
  FreePoolMemory(ehost);

  return 0;
}

struct RestoreObjectContext {
  JobControlRecord* jcr;
  int count;
};

/**
 * RestoreObjectHandler is called for each file found
 */
static int RestoreObjectHandler(void* ctx, int num_fields, char** row)
{
  BareosSocket* fd;
  bool is_compressed;
  RestoreObjectContext* octx = (RestoreObjectContext*)ctx;
  JobControlRecord* jcr = octx->jcr;

  fd = jcr->file_bsock;
  if (jcr->IsJobCanceled()) { return 1; }

  /*
   * Old File Daemon doesn't handle restore objects
   */
  if (jcr->FDVersion < FD_VERSION_3) {
    Jmsg(jcr, M_WARNING, 0,
         _("Client \"%s\" may not be used to restore "
           "this job. Please upgrade your client.\n"),
         jcr->res.client->resource_name_);
    return 1;
  }

  if (jcr->FDVersion < FD_VERSION_5) { /* Old version without PluginName */
    fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s\n", row[0], row[1],
              row[2], row[3], row[4], row[5], row[6]);
  } else {
    /*
     * bash spaces from PluginName
     */
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

  /*
   * Don't try to print compressed objects.
   */
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
  POOLMEM* msg;

  if (jcr->plugin_options) {
    msg = GetPoolMemory(PM_FNAME);
    PmStrcpy(msg, jcr->plugin_options);
    BashSpaces(msg);

    fd->fsend(pluginoptionscmd, msg);
    FreePoolMemory(msg);

    if (!response(jcr, fd, OKPluginOptions, "PluginOptions", DISPLAY_ERROR)) {
      Jmsg(jcr, M_FATAL, 0, _("Plugin options failed.\n"));
      return false;
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

  /*
   * Send restore objects for all jobs involved
   */
  jcr->db->FillQuery(query, BareosDb::SQL_QUERY_get_restore_objects,
                     jcr->JobIds, FT_RESTORE_FIRST);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);

  jcr->db->FillQuery(query, BareosDb::SQL_QUERY_get_restore_objects,
                     jcr->JobIds, FT_PLUGIN_CONFIG);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);

  /*
   * Send config objects for the current restore job
   */
  jcr->db->FillQuery(query, BareosDb::SQL_QUERY_get_restore_objects,
                     edit_uint64(jcr->JobId, ed1), FT_PLUGIN_CONFIG_FILLED);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);
}

static void SendJobSpecificRestoreObjects(JobControlRecord* jcr,
                                          JobId_t JobId,
                                          RestoreObjectContext* octx)
{
  char ed1[50];
  PoolMem query(PM_MESSAGE);

  /*
   * Send restore objects for specific JobId.
   */
  jcr->db->FillQuery(query, BareosDb::SQL_QUERY_get_restore_objects,
                     edit_uint64(JobId, ed1), FT_RESTORE_FIRST);
  jcr->db->SqlQuery(query.c_str(), RestoreObjectHandler, (void*)octx);

  jcr->db->FillQuery(query, BareosDb::SQL_QUERY_get_restore_objects,
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

  /*
   * Send to FD only if we have at least one restore object.
   * This permits backward compatibility with older FDs.
   */
  if (octx.count > 0) {
    fd = jcr->file_bsock;
    fd->fsend(restoreobjectendcmd);
    if (!response(jcr, fd, OKRestoreObject, "RestoreObject", DISPLAY_ERROR)) {
      Jmsg(jcr, M_FATAL, 0, _("RestoreObject failed.\n"));
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
  jcr->jr.FirstIndex = 1;
  jcr->FileIndex = 0;

  /*
   * Start transaction allocates jcr->attr and jcr->ar if needed
   */
  jcr->db->StartTransaction(jcr); /* start transaction if not already open */
  ar = jcr->ar;

  Dmsg0(120, "dird: waiting to receive file attributes\n");

  /*
   * Pickup file attributes and digest
   */
  while (!fd->errors && (n = BgetDirmsg(fd)) > 0) {
    uint32_t file_index;
    int stream, len;
    char *p, *fn;
    PoolMem Digest(PM_MESSAGE); /* Either Verify opts or MD5/SHA1 digest */

    if ((len = sscanf(fd->msg, "%ld %d %s", &file_index, &stream,
                      Digest.c_str())) != 3) {
      Jmsg(jcr, M_FATAL, 0,
           _("<filed: bad attributes, expected 3 fields got %d\n"
             "message_length=%d msg=%s\n"),
           len, fd->message_length, fd->msg);
      jcr->setJobStatus(JS_ErrorTerminated);
      return 0;
    }
    p = fd->msg;

    /*
     * The following three fields were sscanf'ed above so skip them
     */
    SkipNonspaces(&p); /* skip FileIndex */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip Stream */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip Opts_Digest */
    p++;               /* skip space */
    Dmsg1(debuglevel, "Stream=%d\n", stream);
    if (stream == STREAM_UNIX_ATTRIBUTES ||
        stream == STREAM_UNIX_ATTRIBUTES_EX) {
      if (jcr->cached_attribute) {
        Dmsg3(debuglevel, "Cached attr. Stream=%d fname=%s\n", ar->Stream,
              ar->fname, ar->attr);
        if (!jcr->db->CreateFileAttributesRecord(jcr, ar)) {
          Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"),
                jcr->db->strerror());
        }
      }

      /*
       * Any cached attr is flushed so we can reuse jcr->attr and jcr->ar
       */
      fn = jcr->fname = CheckPoolMemorySize(jcr->fname, fd->message_length);
      while (*p != 0) { *fn++ = *p++; /* copy filename */ }
      *fn = *p++;             /* term filename and point p to attribs */
      PmStrcpy(jcr->attr, p); /* save attributes */
      jcr->JobFiles++;
      jcr->FileIndex = file_index;
      ar->attr = jcr->attr;
      ar->fname = jcr->fname;
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

      Dmsg2(debuglevel, "dird<filed: stream=%d %s\n", stream, jcr->fname);
      Dmsg1(debuglevel, "dird<filed: attr=%s\n", ar->attr);
      jcr->FileId = ar->FileId;
    } else if (CryptoDigestStreamType(stream) != CRYPTO_DIGEST_NONE) {
      size_t length;

      /*
       * First, get STREAM_UNIX_ATTRIBUTES and fill AttributesDbRecord structure
       * Next, we CAN have a CRYPTO_DIGEST, so we fill AttributesDbRecord with
       * it (or not) When we get a new STREAM_UNIX_ATTRIBUTES, we known that we
       * can add file to the catalog At the end, we have to add the last file
       */
      if (jcr->FileIndex != (uint32_t)file_index) {
        Jmsg3(jcr, M_ERROR, 0, _("%s index %d not same as attributes %d\n"),
              stream_to_ascii(stream), file_index, jcr->FileIndex);
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
    jcr->jr.JobFiles = jcr->JobFiles = file_index;
    jcr->jr.LastIndex = file_index;
  }

  if (IsBnetError(fd)) {
    Jmsg1(jcr, M_FATAL, 0,
          _("<filed: Network error getting attributes. ERR=%s\n"),
          fd->bstrerror());
    return 0;
  }

  if (jcr->cached_attribute) {
    Dmsg3(debuglevel, "Cached attr with digest. Stream=%d fname=%s attr=%s\n",
          ar->Stream, ar->fname, ar->attr);
    if (!jcr->db->CreateFileAttributesRecord(jcr, ar)) {
      Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"),
            jcr->db->strerror());
    }
    jcr->cached_attribute = false;
  }

  jcr->setJobStatus(JS_Terminated);

  return 1;
}

/**
 * Cancel a job running in the File daemon
 */
bool CancelFileDaemonJob(UaContext* ua, JobControlRecord* jcr)
{
  BareosSocket* fd;

  ua->jcr->res.client = jcr->res.client;
  if (!ConnectToFileDaemon(ua->jcr, 10, me->FDConnectTimeout, true, ua)) {
    ua->ErrorMsg(_("\nFailed to connect to File daemon.\n"));
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

/**
 * Get the status of a remote File Daemon.
 */
void DoNativeClientStatus(UaContext* ua, ClientResource* client, char* cmd)
{
  BareosSocket* fd;

  /*
   * Connect to File daemon
   */
  ua->jcr->res.client = client;

  /*
   * Try to connect for 15 seconds
   */
  if (!ua->api) {
    ua->SendMsg(_("Connecting to Client %s at %s:%d\n"), client->resource_name_,
                client->address, client->FDport);
  }

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false, ua)) {
    ua->SendMsg(_("\nFailed to connect to Client %s.\n====\n"),
                client->resource_name_);
    if (ua->jcr->file_bsock) {
      ua->jcr->file_bsock->close();
      delete ua->jcr->file_bsock;
      ua->jcr->file_bsock = NULL;
    }
    return;
  }

  Dmsg0(20, _("Connected to file daemon\n"));
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

/**
 * resolve a host on a filedaemon
 */
void DoClientResolve(UaContext* ua, ClientResource* client)
{
  BareosSocket* fd;

  /*
   * Connect to File daemon
   */
  ua->jcr->res.client = client;

  /*
   * Try to connect for 15 seconds
   */
  if (!ua->api) {
    ua->SendMsg(_("Connecting to Client %s at %s:%d\n"), client->resource_name_,
                client->address, client->FDport);
  }

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false, ua)) {
    ua->SendMsg(_("\nFailed to connect to Client %s.\n====\n"),
                client->resource_name_);
    if (ua->jcr->file_bsock) {
      ua->jcr->file_bsock->close();
      delete ua->jcr->file_bsock;
      ua->jcr->file_bsock = NULL;
    }
    return;
  }

  Dmsg0(20, _("Connected to file daemon\n"));
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

/**
 * After receiving a connection (in socket_server.c) if it is
 * from the File daemon, this routine is called.
 */
void* HandleFiledConnection(ConnectionPool* connections,
                            BareosSocket* fd,
                            char* client_name,
                            int fd_protocol_version)
{
  ClientResource* client_resource;
  Connection* connection = NULL;

  client_resource =
      (ClientResource*)my_config->GetResWithName(R_CLIENT, client_name);
  if (!client_resource) {
    Emsg1(M_WARNING, 0,
          "Client \"%s\" tries to connect, "
          "but no matching resource is defined.\n",
          client_name);
    goto getout;
  }

  if (!IsConnectFromClientAllowed(client_resource)) {
    Emsg1(M_WARNING, 0,
          "Client \"%s\" tries to connect, "
          "but does not have the required permission.\n",
          client_name);
    goto getout;
  }

  if (!AuthenticateFileDaemon(fd, client_name)) { goto getout; }

  Dmsg1(20, "Connected to file daemon %s\n", client_name);

  connection =
      connections->add_connection(client_name, fd_protocol_version, fd, true);
  if (!connection) {
    Emsg0(M_ERROR, 0, "Failed to add connection to pool.\n");
    goto getout;
  }

  /*
   * The connection is now kept in connection_pool.
   * This thread is no longer required and will end now.
   */
  return NULL;

getout:
  fd->close();
  delete (fd);
  return NULL;
}
} /* namespace directordaemon */
