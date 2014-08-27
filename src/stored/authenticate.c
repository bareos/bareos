/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 * Authenticate caller
 *
 * Kern Sibbald, October 2000
 */

#include "bareos.h"
#include "stored.h"

const int dbglvl = 50;

static char Dir_sorry[] =
   "3999 No go\n";
static char OK_hello[] =
   "3000 OK Hello\n";

/*
 * See who is connecting and lookup the authentication information.
 * First make him prove his identity and then prove our identity to the Remote daemon.
 */
static inline bool two_way_authenticate(int rcode, BSOCK *bs, JCR* jcr)
{
   POOLMEM *dirname;
   DIRRES *director = NULL;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;                 /* require md5 compatible DIR */
   bool auth_success = false;
   alist *verify_list = NULL;

   if (rcode != R_DIRECTOR) {
      Dmsg1(dbglvl, "I only authenticate Directors, not %d\n", rcode);
      Jmsg1(jcr, M_FATAL, 0, _("I only authenticate Directors, not %d\n"), rcode);
      return 0;
   }

   /*
    * Sanity check.
    */
   if (bs->msglen < 25 || bs->msglen > 500) {
      Dmsg2(dbglvl, "Bad Hello command from Director at %s. Len=%d.\n",
            bs->who(), bs->msglen);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s. Len=%d.\n"),
            bs->who(), bs->msglen);
      return 0;
   }
   dirname = get_pool_memory(PM_MESSAGE);
   dirname = check_pool_memory_size(dirname, bs->msglen);

   if (sscanf(bs->msg, "Hello Director %127s calling", dirname) != 1) {
      bs->msg[100] = 0;
      Dmsg2(dbglvl, "Bad Hello command from Director at %s: %s\n",
            bs->who(), bs->msg);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"),
            bs->who(), bs->msg);
      return 0;
   }

   director = NULL;
   unbash_spaces(dirname);
   foreach_res(director, rcode) {
      if (bstrcmp(director->hdr.name, dirname)) {
         break;
      }
   }

   if (!director) {
      Dmsg2(dbglvl, "Connection from unknown Director %s at %s rejected.\n",
            dirname, bs->who());
      Jmsg(jcr, M_FATAL, 0, _("Connection from unknown Director %s at %s rejected.\n"
                              "Please see %s for help.\n"),
           dirname, bs->who(), MANUAL_AUTH_URL);
      free_pool_memory(dirname);
      return 0;
   }

   /*
    * TLS Requirement
    */
   if (director->tls_enable) {
      if (director->tls_require) {
         tls_local_need = BNET_TLS_REQUIRED;
      } else {
         tls_local_need = BNET_TLS_OK;
      }
   }

   if (director->tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   if (director->tls_verify_peer) {
      verify_list = director->tls_allowed_cns;
   }

   ASSERT(director->password.encoding == p_encoding_md5);

   /*
    * Timeout Hello after 10 mins
    */
   btimer_t *tid = start_bsock_timer(bs, AUTH_TIMEOUT);
   auth_success = cram_md5_challenge(bs, director->password.value, tls_local_need, compatible);
   if (auth_success) {
      auth_success = cram_md5_respond(bs, director->password.value, &tls_remote_need, &compatible);
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_get_auth failed with %s\n", bs->who());
      }
   } else {
      Dmsg1(dbglvl, "cram_auth failed with %s\n", bs->who());
   }

   if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0, _("Incorrect password given by Director.\n"
                              "Please see %s for help.\n"), MANUAL_AUTH_URL);
      auth_success = false;
      goto auth_fatal;
   }

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg0(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not"
                               " advertize required TLS support.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   /*
    * Verify that we are willing to meet the remote host's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg0(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      if (!bnet_tls_server(director->tls_ctx, bs, verify_list)) {
         Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with DIR at \"%s:%d\"\n"),
              bs->host(), bs->port());
         auth_success = false;
         goto auth_fatal;
      }
      if (director->tls_authenticate) {     /* authenticate with tls only? */
         bs->free_tls();                    /* yes, shut it down */
      }
   }

auth_fatal:
   stop_bsock_timer(tid);
   free_pool_memory(dirname);
   jcr->director = director;

   return auth_success;
}

/*
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote daemon.
 * - First prove our identity to the Remote daemon and then make him prove his identity.
 */
static inline bool two_way_authenticate(BSOCK *bs, JCR *jcr, bool initiate, const char *what)
{
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;                /* require md5 compatible FD/SD */
   bool auth_success = false;
   alist *verify_list = NULL;
   btimer_t *tid = NULL;

   /*
    * TLS Requirement
    */
   if (me->tls_enable) {
      if (me->tls_require) {
         tls_local_need = BNET_TLS_REQUIRED;
      } else {
         tls_local_need = BNET_TLS_OK;
      }
   }

   if (me->tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   if (me->tls_verify_peer) {
      verify_list = me->tls_allowed_cns;
   }

   /*
    * Timeout Hello after 5 mins
    */
   tid = start_bsock_timer(bs, AUTH_TIMEOUT);

   /*
    * See if we initiate the challenge or respond to a challenge.
    */
   if (initiate) {
      /*
       * Challenge FD/SD
       */
      auth_success = cram_md5_challenge(bs, jcr->sd_auth_key, tls_local_need, compatible);
      if (auth_success) {
          /*
           * Respond to his challenge
           */
          auth_success = cram_md5_respond(bs, jcr->sd_auth_key, &tls_remote_need, &compatible);
          if (!auth_success) {
             Dmsg1(dbglvl, "Respond cram-get-auth failed with %s\n", bs->who());
          }
      } else {
         Dmsg1(dbglvl, "Challenge cram-auth failed with %s\n", bs->who());
      }
   } else {
      /*
       * Respond to his challenge
       */
      auth_success = cram_md5_respond(bs, jcr->sd_auth_key, &tls_remote_need, &compatible);
      if (auth_success) {
         /*
          * Challenge FD/SD
          */
         auth_success = cram_md5_challenge(bs, jcr->sd_auth_key, tls_local_need, compatible);
         if (!auth_success) {
            Dmsg1(dbglvl, "Challenge cram-auth failed with %s\n", bs->who());
         }
      } else {
         Dmsg1(dbglvl, "Respond cram-get-auth failed with %s\n", bs->who());
      }
   }

   if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0, _("Incorrect authorization key from %s daemon at %s rejected.\n"
                              "Please see %s for help.\n"), what, bs->who(), MANUAL_AUTH_URL);
      auth_success = false;
      goto auth_fatal;
   }

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not"
                              " advertize required TLS support.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   /*
    * Verify that we are willing to meet the remote host's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /*
       * Check if we need to be client or server.
       */
      if (initiate) {
         if (!bnet_tls_server(me->tls_ctx, bs, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with %s daemon at \"%s:%d\"\n"),
                 what, bs->host(), bs->port());
            auth_success = false;
            goto auth_fatal;
         }
      } else {
         if (!bnet_tls_client(me->tls_ctx, bs, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with %s daemon at \"%s:%d\"\n"),
                 what, bs->host(), bs->port());
            auth_success = false;
            goto auth_fatal;
         }
      }

      if (me->tls_authenticate) {          /* tls authenticate only? */
         bs->free_tls();                   /* yes, shut it down */
      }
   }

auth_fatal:
   stop_bsock_timer(tid);
   jcr->authenticated = auth_success;

   return auth_success;
}
/*
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

   if (!two_way_authenticate(R_DIRECTOR, dir, jcr)) {
      dir->fsend("%s", Dir_sorry);
      Dmsg1(dbglvl, "Unable to authenticate Director at %s.\n", dir->who());
      Jmsg1(jcr, M_ERROR, 0, _("Unable to authenticate Director at %s.\n"), dir->who());
      bmicrosleep(5, 0);
      return false;
   }

   return dir->fsend("%s", OK_hello);
}

/*
 * Authenticate a remote storage daemon.
 *
 * This is used for SD-SD replication of data.
 */
bool authenticate_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   if (!two_way_authenticate(sd, jcr, true, "Storage")) {
      Jmsg1(jcr, M_FATAL, 0,
            _("Authorization problem: Two way security handshake failed with Storage daemon at %s\n"),
            sd->who());
      return false;
   }

   return true;
}

/*
 * Authenticate with a remote storage daemon.
 *
 * This is used for SD-SD replication of data.
 */
bool authenticate_with_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   if (!two_way_authenticate(sd, jcr, false, "Storage")) {
      Jmsg1(jcr, M_FATAL, 0,
            _("Authorization problem: Two way security handshake failed with Storage daemon at %s\n"),
            sd->who());
      return false;
   }

   return true;
}

/*
 * Authenticate a remote File daemon.
 *
 * This is used for FD backups or restores.
 */
bool authenticate_filedaemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   if (!two_way_authenticate(fd, jcr, true, "File")) {
      Jmsg1(jcr, M_FATAL, 0,
            _("Authorization problem: Two way security handshake failed with File daemon at %s\n"),
            fd->who());
      return false;
   }

   return true;
}

/*
 * Authenticate with a remote file daemon.
 *
 * This is used for passive FD backups or restores.
 */
bool authenticate_with_filedaemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   if (!two_way_authenticate(fd, jcr, false, "File")) {
      Jmsg1(jcr, M_FATAL, 0,
            _("Authorization problem: Two way security handshake failed with File daemon at %s\n"),
            fd->who());
      return false;
   }

   return true;
}
