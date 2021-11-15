/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
// Kern Sibbald, August MM
/**
 * @file
 * routines to receive network data and
 * handle network signals. These routines handle the connections
 * to the Storage daemon and the File daemon.
 *
 * This routine runs as a thread and must be thread reentrant.
 *
 * Basic tasks done here:
 *    Handle network signals (signals).
 *       Signals always have return status 0 from BnetRecv() and
 *       a zero or negative message length.
 *    Pass appropriate messages back to the caller (responses).
 *       Responses always have a digit as the first character.
 *    Handle requests for message and catalog services (requests).
 *       Requests are any message that does not begin with a digit.
 *       In affect, they are commands.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/catreq.h"
#include "dird/jcr_private.h"
#include "dird/msgchan.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/util.h"

namespace directordaemon {

/* Forward referenced functions */
static char* find_msg_start(char* msg);

static char Job_status[] = "Status Job=%127s JobStatus=%d\n";

static char OK_msg[] = "1000 OK\n";

static void SetJcrSdJobStatus(JobControlRecord* jcr, int SDJobStatus)
{
  bool set_waittime = false;

  Dmsg2(800, "SetJcrSdJobStatus(%s, %c)\n", jcr->Job, SDJobStatus);

  // If wait state is new, we keep current time for watchdog MaxWaitTime
  switch (SDJobStatus) {
    case JS_WaitMedia:
    case JS_WaitMount:
    case JS_WaitMaxJobs:
      set_waittime = true;
    default:
      break;
  }

  if (JobWaiting(jcr)) { set_waittime = false; }

  if (set_waittime) {
    // Set it before JobStatus
    Dmsg0(800, "Setting wait_time\n");
    jcr->wait_time = time(NULL);
  }
  jcr->impl->SDJobStatus = SDJobStatus;

  // Some SD Job status setting are propagated to the controlling Job.
  switch (jcr->impl->SDJobStatus) {
    case JS_Incomplete:
      jcr->setJobStatus(JS_Incomplete);
      break;
    case JS_FatalError:
      jcr->setJobStatus(JS_FatalError);
      break;
    default:
      break;
  }
}

/**
 * Get a message
 *  Call appropriate processing routine
 *  If it is not a Jmsg or a ReqCat message,
 *   return it to the caller.
 *
 *  This routine is called to get the next message from
 *  another daemon. If the message is in canonical message
 *  format and the type is known, it will be dispatched
 *  to the appropriate handler.  If the message is
 *  in any other format, it will be returned.
 *
 *  E.g. any message beginning with a digit will be passed
 *       through to the caller.
 *  All other messages are expected begin with some identifier
 *    -- for the moment only the first character is checked, but
 *    at a later time, the whole identifier (e.g. Jmsg, CatReq, ...)
 *    could be checked. This is followed by Job=Jobname <user-defined>
 *    info. The identifier is used to dispatch the message to the right
 *    place (Job message, catalog request, ...). The Job is used to lookup
 *    the JobControlRecord so that the action is performed on the correct jcr,
 * and the rest of the message is up to the user.  Note, DevUpd uses *System*
 * for the Job name, and hence no JobControlRecord is obtained. This is a *rare*
 * case where a jcr is not really needed.
 *
 */
int BgetDirmsg(BareosSocket* bs, bool allow_any_message)
{
  int32_t n = BNET_TERMINATE;
  char Job[MAX_NAME_LENGTH];
  char MsgType[21];
  int type;
  utime_t mtime; /* message time */
  JobControlRecord* jcr = bs->jcr();
  char* msg;

  for (; !bs->IsStop() && !bs->IsTimedOut();) {
    n = bs->recv();
    Dmsg2(200, "BgetDirmsg %d: %s\n", n, bs->msg);

    if (bs->IsStop() || bs->IsTimedOut()) { return n; /* error or Terminate */ }
    if (n == BNET_SIGNAL) { /* handle signal */
      /* BNET_SIGNAL (-1) return from BnetRecv() => network signal */
      switch (bs->message_length) {
        case BNET_EOD: /* end of data */
          return n;
        case BNET_EOD_POLL:
          bs->fsend(OK_msg); /* send response */
          return n;          /* end of data */
        case BNET_TERMINATE:
          bs->SetTerminated();
          return n;
        case BNET_POLL:
          bs->fsend(OK_msg); /* send response */
          break;
        case BNET_HEARTBEAT:
          //          encode_time(time(NULL), Job);
          //          Dmsg1(100, "%s got heartbeat.\n", Job);
          break;
        case BNET_HB_RESPONSE:
          break;
        case BNET_STATUS:
          /* *****FIXME***** Implement more completely */
          bs->fsend("Status OK\n");
          bs->signal(BNET_EOD);
          break;
        case BNET_BTIME: /* send BAREOS time */
          char ed1[50];
          bs->fsend("btime %s\n", edit_uint64(GetCurrentBtime(), ed1));
          break;
        default:
          Jmsg1(jcr, M_WARNING, 0, _("BgetDirmsg: unknown bnet signal %d\n"),
                bs->message_length);
          return n;
      }
      continue;
    }

    // Handle normal data
    if (n > 0 && B_ISDIGIT(bs->msg[0])) { /* response? */
      return n;                           /* yes, return it */
    }

    /*
     * If we get here, it must be a request.  Either
     *  a message to dispatch, or a catalog request.
     *  Try to fulfill it.
     */
    if (sscanf(bs->msg, "%020s Job=%127s ", MsgType, Job) != 2) {
      /*
       * If the special flag allow_any_message is given ignore
       * the error and just return it as normal data.
       */
      if (allow_any_message) {
        return n;
      } else {
        Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), bs->msg);
        continue;
      }
    }

    // Skip past "Jmsg Job=nnn"
    if (!(msg = find_msg_start(bs->msg))) {
      Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), bs->msg);
      continue;
    }

    /*
     * Here we are expecting a message of the following format:
     *   Jmsg Job=nnn type=nnn level=nnn Message-string
     * Note, level should really be mtime, but that changes
     *   the protocol.
     */
    if (bs->msg[0] == 'J') { /* Job message */
      if (sscanf(bs->msg, "Jmsg Job=%127s type=%d level=%lld", Job, &type,
                 &mtime)
          != 3) {
        Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), bs->msg);
        continue;
      }
      Dmsg1(900, "Got msg: %s\n", bs->msg);
      SkipSpaces(&msg);
      SkipNonspaces(&msg); /* skip type=nnn */
      SkipSpaces(&msg);
      SkipNonspaces(&msg); /* skip level=nnn */
      if (*msg == ' ') { msg++; /* skip leading space */ }
      Dmsg1(900, "Dispatch msg: %s", msg);
      DispatchMessage(jcr, type, mtime, msg);
      continue;
    }
    /*
     * Here we expact a CatReq message
     *   CatReq Job=nn Catalog-Request-Message
     */
    if (bs->msg[0] == 'C') { /* Catalog request */
      Dmsg2(900, "Catalog req jcr 0x%x: %s", jcr, bs->msg);
      CatalogRequest(jcr, bs);
      continue;
    }
    if (bs->msg[0] == 'U') { /* SD sending attributes */
      Dmsg2(900, "Catalog upd jcr 0x%x: %s", jcr, bs->msg);
      CatalogUpdate(jcr, bs);
      continue;
    }
    if (bs->msg[0] == 'B') { /* SD sending file spool attributes */
      Dmsg2(100, "Blast attributes jcr 0x%x: %s", jcr, bs->msg);
      char filename[256];
      if (sscanf(bs->msg, "BlastAttr Job=%127s File=%255s", Job, filename)
          != 2) {
        Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), bs->msg);
        continue;
      }
      UnbashSpaces(filename);
      if (DespoolAttributesFromFile(jcr, filename)) {
        bs->fsend("1000 OK BlastAttr\n");
      } else {
        bs->fsend("1990 ERROR BlastAttr\n");
      }
      continue;
    }
    if (bs->msg[0] == 'S') { /* Status change */
      int JobStatus;
      char Job[MAX_NAME_LENGTH];
      if (sscanf(bs->msg, Job_status, &Job, &JobStatus) == 2) {
        SetJcrSdJobStatus(jcr, JobStatus); /* current status */
      } else {
        Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), bs->msg);
      }
      continue;
    }
    return n;
  }
  return n;
}

static char* find_msg_start(char* msg)
{
  char* p = msg;

  SkipNonspaces(&p); /* skip message type */
  SkipSpaces(&p);
  SkipNonspaces(&p); /* skip Job */
  SkipSpaces(&p);    /* after spaces come the message */
  return p;
}

/**
 * Get response from FD or SD to a command we
 * sent. Check that the response agrees with what we expect.
 *
 *  Returns: false on failure
 *           true  on success
 */
bool response(JobControlRecord* jcr,
              BareosSocket* bs,
              char* resp,
              const char* cmd,
              e_prtmsg PrintMessage)
{
  int n;

  if (IsBnetError(bs)) { return false; }
  if ((n = BgetDirmsg(bs)) >= 0) {
    if (bstrcmp(bs->msg, resp)) { return true; }
    if (PrintMessage == DISPLAY_ERROR) {
      Jmsg(jcr, M_FATAL, 0,
           _("Bad response to %s command: wanted %s, got %s\n"), cmd, resp,
           bs->msg);
    }
    return false;
  }
  Jmsg(jcr, M_FATAL, 0, _("Socket error on %s command: ERR=%s\n"), cmd,
       BnetStrerror(bs));
  return false;
}
} /* namespace directordaemon */
