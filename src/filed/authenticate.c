/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
 * Authenticate Director who is attempting to connect.
 *
 * Kern Sibbald, October 2000
 */

#include "bareos.h"
#include "filed.h"

const int dbglvl = 50;

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
 */
static char OK_hello_compat[] =
   "2000 OK Hello 5\n";
static char OK_hello[] =
   "2000 OK Hello 53\n";

static char Dir_sorry[] =
   "2999 Authentication failed.\n";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * See who is connecting and lookup the authentication information.
 * First make him prove his identity and then prove our identity to the Remote.
 */
static inline bool two_way_authenticate(int rcode, BSOCK *bs, JCR* jcr)
{
   POOLMEM *dirname = get_pool_memory(PM_MESSAGE);
   DIRRES *director = NULL;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;                /* Want md5 compatible DIR */
   bool auth_success = false;
   alist *verify_list = NULL;
   btimer_t *tid = NULL;

   if (rcode != R_DIRECTOR) {
      Dmsg1(dbglvl, "I only authenticate directors, not %d\n", rcode);
      Jmsg1(jcr, M_FATAL, 0, _("I only authenticate directors, not %d\n"), rcode);
      goto auth_fatal;
   }

   if (bs->msglen < 25 || bs->msglen > 500) {
      Dmsg2(dbglvl, "Bad Hello command from Director at %s. Len=%d.\n",
            bs->who(), bs->msglen);
      char addr[64];
      char *who = bnet_get_peer(bs, addr, sizeof(addr)) ? bs->who() : addr;
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s. Len=%d.\n"),
             who, bs->msglen);
      goto auth_fatal;
   }
   dirname = check_pool_memory_size(dirname, bs->msglen);

   if (sscanf(bs->msg, "Hello Director %s calling", dirname) != 1) {
      char addr[64];
      char *who = bnet_get_peer(bs, addr, sizeof(addr)) ? bs->who() : addr;
      bs->msg[100] = 0;
      Dmsg2(dbglvl, "Bad Hello command from Director at %s: %s\n",
            bs->who(), bs->msg);
      Emsg2(M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"), who, bs->msg);
      goto auth_fatal;
   }
   unbash_spaces(dirname);
   director = (DIRRES *)GetResWithName(R_DIRECTOR, dirname);
   if (!director) {
      char addr[64];
      char *who = bnet_get_peer(bs, addr, sizeof(addr)) ? bs->who() : addr;
      Dmsg2(dbglvl, "Connection from unknown Director %s at %s rejected.\n", dirname, who);
      Emsg2(M_FATAL, 0, _("Connection from unknown Director %s at %s rejected.\n"), dirname, who);
      goto auth_fatal;
   }

   if (have_tls) {
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
   }

   /*
    * Timeout Hello after 10 min
    */
   tid = start_bsock_timer(bs, AUTH_TIMEOUT);

   /*
    * Sanity check.
    */
   ASSERT(director->password.encoding == p_encoding_md5);

   /*
    * Challenge the director
    */
   auth_success = cram_md5_challenge(bs, director->password.value, tls_local_need, compatible);
   if (job_canceled(jcr)) {
      auth_success = false;
      goto auth_fatal;                   /* quick exit */
   }

   if (auth_success) {
      auth_success = cram_md5_respond(bs, director->password.value, &tls_remote_need, &compatible);
      if (!auth_success) {
          char addr[64];
          char *who = bnet_get_peer(bs, addr, sizeof(addr)) ? bs->who() : addr;
          Dmsg1(dbglvl, "cram_get_auth failed for %s\n", who);
      }
   } else {
       char addr[64];
       char *who = bnet_get_peer(bs, addr, sizeof(addr)) ? bs->who() : addr;
       Dmsg1(dbglvl, "cram_auth failed for %s\n", who);
   }

   if (!auth_success) {
       Emsg1(M_FATAL, 0, _("Incorrect password given by Director at %s.\n"),
             bs->who());
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
         Jmsg0(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
         auth_success = false;
         goto auth_fatal;
      }
      if (director->tls_authenticate) {         /* authentication only? */
         bs->free_tls();                        /* shutodown tls */
      }
   }

auth_fatal:
   if (tid) {
      stop_bsock_timer(tid);
      tid = NULL;
   }
   free_pool_memory(dirname);
   jcr->director = director;

   /*
    * Single thread all failures to avoid DOS
    */
   if (!auth_success) {
      P(mutex);
      bmicrosleep(6, 0);
      V(mutex);
   }

   return auth_success;
}

/*
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote.
 * - First prove our identity to the Remote and then make him prove his identity.
 */
static inline bool two_way_authenticate(BSOCK *bs, JCR *jcr, bool initiate, const char *what)
{
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;
   bool auth_success = false;
   btimer_t *tid = NULL;

   /*
    * TLS Requirement
    */
   if (have_tls && me->tls_enable) {
      if (me->tls_require) {
         tls_local_need = BNET_TLS_REQUIRED;
      } else {
         tls_local_need = BNET_TLS_OK;
      }
   }

   if (me->tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   if (job_canceled(jcr)) {
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }

   /*
    * Timeout Hello after 10 min
    */
   tid = start_bsock_timer(bs, AUTH_TIMEOUT);

   /*
    * See if we initiate the challenge or respond to a challenge.
    */
   if (initiate) {
      /*
       * Challenge SD
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
       * Respond to challenge
       */
      auth_success = cram_md5_respond(bs, jcr->sd_auth_key, &tls_remote_need, &compatible);
      if (job_canceled(jcr)) {
         auth_success = false;     /* force quick exit */
         goto auth_fatal;
      }
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_respond failed for %s\n", bs->who());
      } else {
         /*
          * Challenge SD.
          */
         auth_success = cram_md5_challenge(bs, jcr->sd_auth_key, tls_local_need, compatible);
         if (!auth_success) {
            Dmsg1(dbglvl, "cram_challenge failed for %s\n", bs->who());
         }
      }
   }

   if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization key rejected by %s.\n"
                              "Please see %s for help.\n"), what, MANUAL_AUTH_URL);
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
      alist *verify_list = NULL;

      if (me->tls_verify_peer) {
         verify_list = me->tls_allowed_cns;
      }

      /*
       * See if we are handshaking a passive client connection.
       */
      if (initiate) {
         if (!bnet_tls_server(me->tls_ctx, bs, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            auth_success = false;
            goto auth_fatal;
         }
      } else {
         if (!bnet_tls_client(me->tls_ctx, bs, me->tls_verify_peer, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            auth_success = false;
            goto auth_fatal;
         }
      }

      if (me->tls_authenticate) {           /* tls authentication only? */
         bs->free_tls();                    /* yes, shutdown tls */
      }
   }

auth_fatal:
   /*
    * Destroy session key
    */
   memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
   stop_bsock_timer(tid);

   /*
    * Single thread all failures to avoid DOS
    */
   if (!auth_success) {
      P(mutex);
      bmicrosleep(6, 0);
      V(mutex);
   }

   return auth_success;
}

/*
 * Inititiate the communications with the Director.
 * He has made a connection to our server.
 *
 * Basic tasks done here:
 *   We read Director's initial message and authorize him.
 */
bool authenticate_director(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   if (!two_way_authenticate(R_DIRECTOR, dir, jcr)) {
      dir->fsend("%s", Dir_sorry);
      Emsg0(M_FATAL, 0, _("Unable to authenticate Director\n"));
      return false;
   }

   return dir->fsend("%s", (me->compatible) ? OK_hello_compat : OK_hello);
}

/*
 * Authenticate a remote storage daemon.
 */
bool authenticate_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   return two_way_authenticate(sd, jcr, true, "Storage daemon");
}

/*
 * Authenticate with a remote storage daemon.
 */
bool authenticate_with_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   return two_way_authenticate(sd, jcr, false, "Storage daemon");
}
