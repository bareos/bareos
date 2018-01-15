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

#include "bareos.h"
#include "stored.h"

const int dbglvl = 50;

static char Dir_sorry[] =
   "3999 No go\n";
static char OK_hello[] =
   "3000 OK Hello\n";

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
bool authenticate_director(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *dirname;
   DIRRES *director = NULL;

   /*
    * Sanity check.
    */
   if (dir->msglen < 25 || dir->msglen > 500) {
      Dmsg2(dbglvl, "Bad Hello command from Director at %s. Len=%d.\n",
            dir->who(), dir->msglen);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s. Len=%d.\n"),
            dir->who(), dir->msglen);
      return false;
   }
   dirname = get_pool_memory(PM_MESSAGE);
   dirname = check_pool_memory_size(dirname, dir->msglen);

   if (sscanf(dir->msg, "Hello Director %127s calling", dirname) != 1) {
      dir->msg[100] = 0;
      Dmsg2(dbglvl, "Bad Hello command from Director at %s: %s\n",
            dir->who(), dir->msg);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"),
            dir->who(), dir->msg);
      free_pool_memory(dirname);
      return false;
   }

   unbash_spaces(dirname);
   director = (DIRRES *)GetResWithName(R_DIRECTOR, dirname);
   jcr->director = director;

   if (!director) {
      Dmsg2(dbglvl, "Connection from unknown Director %s at %s rejected.\n",
            dirname, dir->who());
      Jmsg(jcr, M_FATAL, 0, _("Connection from unknown Director %s at %s rejected.\n"
                              "Please see %s for help.\n"),
           dirname, dir->who(), MANUAL_AUTH_URL);
      free_pool_memory(dirname);
      return false;
   }

   std::vector<std::reference_wrapper<tls_base_t>> tls_resources{director->tls_cert, director->tls_psk};
   if (!dir->authenticate_inbound_connection(jcr, "Director",
                                             director->name(), director->password,
                                             tls_resources)) {
      dir->fsend("%s", Dir_sorry);
      Dmsg2(dbglvl, "Unable to authenticate Director \"%s\" at %s.\n", director->name(), dir->who());
      Jmsg1(jcr, M_ERROR, 0, _("Unable to authenticate Director at %s.\n"), dir->who());
      bmicrosleep(5, 0);
      free_pool_memory(dirname);
      return false;
   }

   free_pool_memory(dirname);
   return dir->fsend("%s", OK_hello);
}

/**
 * Authenticate a remote storage daemon.
 *
 * This is used for SD-SD replication of data.
 */
bool authenticate_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;
   s_password password;

   password.encoding = p_encoding_md5;
   password.value    = jcr->sd_auth_key;
   const char *identity =
       "* replicate *";

   Dmsg2(dbglvl, "authenticate_storagedaemon %s %s\n", identity, (unsigned char *)password.value);
   std::vector<std::reference_wrapper<tls_base_t>> tls_resources{me->tls_cert, me->tls_psk};
   if (!sd->authenticate_inbound_connection(jcr, "Storage daemon", identity, password, tls_resources)) {
      Jmsg1(
          jcr,
          M_FATAL,
          0,
          _("Authorization problem: Two way security handshake failed with Storage daemon at %s\n"),
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
bool authenticate_with_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;
   s_password password;

   const char *identity =
       "* replicate *";
   password.encoding = p_encoding_md5;
   password.value = jcr->sd_auth_key;

   std::vector<std::reference_wrapper<tls_base_t>> tls_resources{me->tls_cert, me->tls_psk};
   if (!sd->authenticate_outbound_connection(
           jcr, "Storage daemon", identity, password, tls_resources)) {
      Jmsg1(
          jcr,
          M_FATAL,
          0,
          _("Authorization problem: Two way security handshake failed with Storage daemon at %s\n"),
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
bool authenticate_filedaemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   s_password password;

   password.encoding = p_encoding_md5;
   password.value = jcr->sd_auth_key;

   std::vector<std::reference_wrapper<tls_base_t>> tls_resources{me->tls_cert, me->tls_psk};
   if (!fd->authenticate_inbound_connection(jcr, "File daemon", jcr->client_name, password, tls_resources)) {
      Jmsg1(jcr,
            M_FATAL,
            0,
            _("Authorization problem: Two way security handshake failed with File daemon at %s\n"),
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
bool authenticate_with_filedaemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   s_password password;

   password.encoding = p_encoding_md5;
   password.value = jcr->sd_auth_key;

   std::vector<std::reference_wrapper<tls_base_t>> tls_resources{me->tls_cert, me->tls_psk};
   if (!fd->authenticate_outbound_connection(
           jcr, "File daemon", jcr->client_name, password, tls_resources)) {
      Jmsg1(jcr,
            M_FATAL,
            0,
            _("Authorization problem: Two way security handshake failed with File daemon at %s\n"),
            fd->who());
      return false;
   }

   return true;
}
