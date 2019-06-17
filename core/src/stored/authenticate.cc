/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Authenticate caller
 */

#include "include/bareos.h"
#include "include/make_unique.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "lib/bsock.h"
#include "lib/bsock_network_dump.h"
#include "lib/util.h"

namespace storagedaemon {

const int debuglevel = 50;

static char Dir_sorry[] = "3999 No go\n";
static char OK_hello[] = "3000 OK Hello\n";

/**
 * Initiate the message channel with the Director.
 * It has made a connection to our server.
 *
 * Basic tasks done here:
 *   - Assume the Hello message is already in the input buffer.
 *   - Authenticate
 *   - Get device
 *   - Get media
 *   - Get pool information
 *
 * This is the channel across which we will send error
 * messages and job status information.
 */
bool AuthenticateDirector(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  POOLMEM* dirname;
  DirectorResource* director = NULL;

  /*
   * Sanity check.
   */
  if (dir->message_length < 25 || dir->message_length > 500) {
    Dmsg2(debuglevel, "Bad Hello command from Director at %s. Len=%d.\n",
          dir->who(), dir->message_length);
    Jmsg2(jcr, M_FATAL, 0,
          _("Bad Hello command from Director at %s. Len=%d.\n"), dir->who(),
          dir->message_length);
    return false;
  }
  dirname = GetPoolMemory(PM_MESSAGE);
  dirname = CheckPoolMemorySize(dirname, dir->message_length);

  if (sscanf(dir->msg, "Hello Director %127s calling", dirname) != 1) {
    dir->msg[100] = 0;
    Dmsg2(debuglevel, "Bad Hello command from Director at %s: %s\n", dir->who(),
          dir->msg);
    Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"),
          dir->who(), dir->msg);
    FreePoolMemory(dirname);
    return false;
  }

  UnbashSpaces(dirname);
  director = (DirectorResource*)my_config->GetResWithName(R_DIRECTOR, dirname);
  jcr->director = director;

  if (!director) {
    Dmsg2(debuglevel, "Connection from unknown Director %s at %s rejected.\n",
          dirname, dir->who());
    Jmsg(jcr, M_FATAL, 0,
         _("Connection from unknown Director %s at %s rejected.\n"), dirname,
         dir->who());
    FreePoolMemory(dirname);
    return false;
  }

  if (!dir->AuthenticateInboundConnection(jcr, "Director",
                                          director->resource_name_,
                                          director->password_, director)) {
    dir->fsend("%s", Dir_sorry);
    Dmsg2(debuglevel, "Unable to authenticate Director \"%s\" at %s.\n",
          director->resource_name_, dir->who());
    Jmsg1(jcr, M_ERROR, 0, _("Unable to authenticate Director at %s.\n"),
          dir->who());
    Bmicrosleep(5, 0);
    FreePoolMemory(dirname);
    return false;
  }

  FreePoolMemory(dirname);
  return dir->fsend("%s", OK_hello);
}

/**
 * Authenticate a remote storage daemon.
 *
 * This is used for SD-SD replication of data.
 */
bool AuthenticateStoragedaemon(JobControlRecord* jcr)
{
  BareosSocket* sd = jcr->store_bsock;
  s_password password;

  password.encoding = p_encoding_md5;
  password.value = jcr->sd_auth_key;
  const char* identity = "* replicate *";

  Dmsg2(debuglevel, "AuthenticateStoragedaemon %s %s\n", identity,
        (unsigned char*)password.value);
  if (!sd->AuthenticateInboundConnection(jcr, "Storage daemon", identity,
                                         password, me)) {
    Jmsg1(jcr, M_FATAL, 0,
          _("Authorization problem: Two way security handshake failed with "
            "Storage daemon at %s\n"),
          sd->who());
    return false;
  }

  return true;
}

/**
 * Authenticate with a remote storage daemon.
 *
 * This is used for SD-SD replication of data.
 */
bool AuthenticateWithStoragedaemon(JobControlRecord* jcr)
{
  BareosSocket* sd = jcr->store_bsock;
  s_password password;

  const char* identity = "* replicate *";
  password.encoding = p_encoding_md5;
  password.value = jcr->sd_auth_key;

  if (!sd->AuthenticateOutboundConnection(jcr, "Storage daemon", identity,
                                          password, me)) {
    Jmsg1(jcr, M_FATAL, 0,
          _("Authorization problem: Two way security handshake failed with "
            "Storage daemon at %s\n"),
          sd->who());
    return false;
  }

  return true;
}

/**
 * Authenticate a remote File daemon.
 *
 * This is used for FD backups or restores.
 */
bool AuthenticateFiledaemon(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  s_password password;

  password.encoding = p_encoding_md5;
  password.value = jcr->sd_auth_key;

  fd->SetNwdump(BnetDump::Create(
      my_config->own_resource_, R_CLIENT,
      *my_config->GetQualifiedResourceNameTypeConverter()));

  if (!fd->AuthenticateInboundConnection(jcr, "File daemon", jcr->client_name,
                                         password, me)) {
    Jmsg1(jcr, M_FATAL, 0,
          _("Authorization problem: Two way security handshake failed with "
            "File daemon at %s\n"),
          fd->who());
    return false;
  }

  return true;
}

/**
 * Authenticate with a remote file daemon.
 *
 * This is used for passive FD backups or restores.
 */
bool AuthenticateWithFiledaemon(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  s_password password;

  password.encoding = p_encoding_md5;
  password.value = jcr->sd_auth_key;

  if (!fd->AuthenticateOutboundConnection(jcr, "File daemon", jcr->client_name,
                                          password, me)) {
    Jmsg1(jcr, M_FATAL, 0,
          _("Authorization problem: Two way security handshake failed with "
            "File daemon at %s\n"),
          fd->who());
    return false;
  }

  return true;
}

} /* namespace storagedaemon */
