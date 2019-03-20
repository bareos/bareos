/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Kern Sibbald, October 2000
 */
/**
 * @file
 * Authenticate Director who is attempting to connect.
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/restore.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

namespace filedaemon {

const int debuglevel = 50;

/* Version at end of Hello
 *   prior to 10Mar08 no version
 *   1 10Mar08
 *   2 13Mar09 - Added the ability to restore from multiple storages
 *   3 03Sep10 - Added the restore object command for vss plugin 4.0
 *   4 25Nov10 - Added bandwidth command 5.1
 *   5 24Nov11 - Added new restore object command format (pluginname) 6.0
 *
 *  51 21Mar13 - Added reverse datachannel initialization
 *  52 13Jul13 - Added plugin options
 *  53 02Apr15 - Added setdebug timestamp
 *  54 29Oct15 - Added getSecureEraseCmd
 */
static char OK_hello_compat[] = "2000 OK Hello 5\n";
static char OK_hello[] = "2000 OK Hello 54\n";

static char Dir_sorry[] = "2999 Authentication failed.\n";

/**
 * To prevent DOS attacks,
 * wait a bit in case of an
 * authentication failure of a (remotely) initiated connection.
 */
static inline void delay()
{
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  /*
   * Single thread all failures to avoid DOS
   */
  P(mutex);
  Bmicrosleep(6, 0);
  V(mutex);
}

static inline void AuthenticateFailed(JobControlRecord* jcr, PoolMem& message)
{
  Dmsg0(debuglevel, message.c_str());
  Jmsg0(jcr, M_FATAL, 0, message.c_str());
  delay();
}

/**
 * Initiate the communications with the Director.
 * He has made a connection to our server.
 *
 * Basic tasks done here:
 * We read Director's initial message and authorize him.
 */
bool AuthenticateDirector(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;

  PoolMem errormsg(PM_MESSAGE);
  PoolMem dirname(PM_MESSAGE);
  DirectorResource* director = NULL;

  if (dir->message_length < 25 || dir->message_length > 500) {
    char addr[64];
    char* who = BnetGetPeer(dir, addr, sizeof(addr)) ? dir->who() : addr;
    errormsg.bsprintf(_("Bad Hello command from Director at %s. Len=%d.\n"),
                      who, dir->message_length);
    AuthenticateFailed(jcr, errormsg);
    return false;
  }

  if (sscanf(dir->msg, "Hello Director %s calling",
             dirname.check_size(dir->message_length)) != 1) {
    char addr[64];
    char* who = BnetGetPeer(dir, addr, sizeof(addr)) ? dir->who() : addr;
    dir->msg[100] = 0;
    errormsg.bsprintf(_("Bad Hello command from Director at %s: %s\n"), who,
                      dir->msg);
    AuthenticateFailed(jcr, errormsg);
    return false;
  }

  UnbashSpaces(dirname.c_str());
  director =
      (DirectorResource*)my_config->GetResWithName(R_DIRECTOR, dirname.c_str());

  if (!director) {
    char addr[64];
    char* who = BnetGetPeer(dir, addr, sizeof(addr)) ? dir->who() : addr;
    errormsg.bsprintf(
        _("Connection from unknown Director %s at %s rejected.\n"),
        dirname.c_str(), who);
    AuthenticateFailed(jcr, errormsg);
    return false;
  }

  if (!director->conn_from_dir_to_fd) {
    errormsg.bsprintf(_("Connection from Director %s rejected.\n"),
                      dirname.c_str());
    AuthenticateFailed(jcr, errormsg);
    return false;
  }

  if (!dir->AuthenticateInboundConnection(jcr, "Director", dirname.c_str(),
                                          director->password_, director)) {
    dir->fsend("%s", Dir_sorry);
    errormsg.bsprintf(_("Unable to authenticate Director %s.\n"),
                      dirname.c_str());
    AuthenticateFailed(jcr, errormsg);
    return false;
  }

  jcr->director = director;

  return dir->fsend("%s", (me->compatible) ? OK_hello_compat : OK_hello);
}

/**
 * Authenticate with a remote director.
 */
bool AuthenticateWithDirector(JobControlRecord* jcr, DirectorResource* director)
{
  return jcr->dir_bsock->AuthenticateOutboundConnection(
      jcr, "Director", me->resource_name_, director->password_, director);
}

/**
 * Authenticate a remote storage daemon.
 */
bool AuthenticateStoragedaemon(JobControlRecord* jcr)
{
  bool result = false;
  BareosSocket* sd = jcr->store_bsock;
  s_password password;

  password.encoding = p_encoding_md5;
  password.value = jcr->sd_auth_key;
  result = sd->AuthenticateInboundConnection(jcr, "Storage daemon",
                                             jcr->client_name, password, me);

  /*
   * Destroy session key
   */
  memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
  if (!result) { delay(); }

  return result;
}

/**
 * Authenticate with a remote storage daemon.
 */
bool AuthenticateWithStoragedaemon(JobControlRecord* jcr)
{
  bool result = false;
  BareosSocket* sd = jcr->store_bsock;
  s_password password;

  password.encoding = p_encoding_md5;
  password.value = jcr->sd_auth_key;
  result = sd->AuthenticateOutboundConnection(
      jcr, "Storage daemon", (char*)jcr->client_name, password, me);

  /*
   * Destroy session key
   */
  memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

  return result;
}
} /* namespace filedaemon */
