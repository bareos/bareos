/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
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
 * BAREOS Director -- authenticate.c -- handles authorization of Consoles, Storage and File daemons.
 *
 * Kern Sibbald, May MMI
 *
 * This routine runs as a thread and must be thread reentrant.
 */

#include "bareos.h"
#include "dird.h"

static const int dbglvl = 50;

/*
 * Commands sent to Storage daemon and File daemon and received from the User Agent
 */
static char hello[] =
   "Hello Director %s calling\n";

/*
 * Response from Storage daemon
 */
static char OKhello[] =
   "3000 OK Hello\n";
static char FDOKhello[] =
   "2000 OK Hello\n";
static char FDOKnewHello[] =
   "2000 OK Hello %d\n";

/*
 * Sent to User Agent
 */
static char Dir_sorry[] =
   "1999 You are not authorized.\n";

/*
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote.
 * - First prove our identity to the Remote and then make him prove his identity.
 */
static inline bool two_way_authenticate(BSOCK *bs, JCR *jcr, const char *passwd, bool initiate,
                                        bool tls_enable, bool tls_require, bool tls_authenticate,
                                        bool tls_verify_peer, alist *verify_list,
                                        TLS_CONTEXT *tls_ctx, const char *what)
{
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;
   bool auth_success = false;
   btimer_t *tid = NULL;

   /*
    * TLS Requirement
    */
   if (tls_enable) {
      if (tls_require) {
         tls_local_need = BNET_TLS_REQUIRED;
      } else {
         tls_local_need = BNET_TLS_OK;
      }
   }

   if (tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
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
       * Challenge remote.
       */
      auth_success = cram_md5_challenge(bs, passwd, tls_local_need, compatible);
      if (auth_success) {
          /*
           * Respond to his challenge
           */
          auth_success = cram_md5_respond(bs, passwd, &tls_remote_need, &compatible);
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
      auth_success = cram_md5_respond(bs, passwd, &tls_remote_need, &compatible);
      if (auth_success) {
         /*
          * Challenge remote.
          */
         auth_success = cram_md5_challenge(bs, passwd, tls_local_need, compatible);
         if (!auth_success) {
            Dmsg1(dbglvl, "Challenge cram-auth failed with %s\n", bs->who());
         }
      } else {
         Dmsg1(dbglvl, "Respond cram-get-auth failed with %s\n", bs->who());
      }
   }

   if (!auth_success) {
      goto auth_fatal;
   }

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      if (jcr) {
         Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not"
                                 " advertize required TLS support.\n"));
      } else {
         Emsg0(M_FATAL, 0, _("Authorization problem: Remote server did not"
                            " advertize required TLS support.\n"));
      }
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   /*
    * Verify that we are willing to meet the remote host's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      if (jcr) {
         Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      } else {
         Emsg0(M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      }
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /*
       * Check if we need to be client or server.
       */
      if (initiate) {
         if (!bnet_tls_server(tls_ctx, bs, verify_list)) {
            if (jcr) {
               Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
                    what, bs->host(), bs->port());
            } else {
               Emsg3(M_FATAL, 0, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
                    what, bs->host(), bs->port());
            }
            auth_success = false;
            goto auth_fatal;
         }
      } else {
         if (!bnet_tls_client(tls_ctx, bs, tls_verify_peer, verify_list)) {
            if (jcr) {
               Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
                    what, bs->host(), bs->port());
            } else {
               Emsg3(M_FATAL, 0, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
                    what, bs->host(), bs->port());
            }
            auth_success = false;
            goto auth_fatal;
         }
      }

      if (tls_authenticate) {          /* tls authenticate only? */
         bs->free_tls();                   /* yes, shut it down */
      }
   }

auth_fatal:
   stop_bsock_timer(tid);
   if (jcr) {
      jcr->authenticated = auth_success;
   }

   return auth_success;
}

/*
 * Authenticate with a remote Storage daemon
 */
bool authenticate_with_storage_daemon(JCR *jcr, STORERES *store)
{
   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, me->hdr.name, sizeof(dirname));
   bash_spaces(dirname);

   if (!sd->fsend(hello, dirname)) {
      Dmsg1(dbglvl, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return false;
   }

   ASSERT(store->password.encoding == p_encoding_md5);
   auth_success = two_way_authenticate(sd, jcr, store->password.value, false,
                                       store->tls_enable, store->tls_require, store->tls_authenticate,
                                       store->tls_verify_peer, NULL, store->tls_ctx, "Storage daemon");
   if (!auth_success) {
      Dmsg0(dbglvl, _("Director and Storage daemon passwords or names not the same.\n"));
      Jmsg(jcr, M_FATAL, 0,
           _("Director unable to authenticate with Storage daemon at \"%s:%d\". Possible causes:\n"
             "Passwords or names not the same or\n"
             "Maximum Concurrent Jobs exceeded on the SD or\n"
             "SD networking messed up (restart daemon).\n"
             "Please see %s for help.\n"),
           sd->host(), sd->port(), MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      Jmsg3(jcr, M_FATAL, 0, _("dir<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
            sd->who(), sd->host(), sd->bstrerror());
      return false;
   }

   Dmsg1(110, "<stored: %s", sd->msg);
   if (!bstrncmp(sd->msg, OKhello, sizeof(OKhello))) {
      Dmsg0(dbglvl, _("Storage daemon rejected Hello command\n"));
      Jmsg2(jcr, M_FATAL, 0, _("Storage daemon at \"%s:%d\" rejected Hello command\n"),
            sd->host(), sd->port());
      return false;
   }

   return true;
}

/*
 * Authenticate with a remote File daemon
 */
bool authenticate_with_file_daemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   CLIENTRES *client = jcr->res.client;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;
   alist *verify_list = NULL;

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, me->name(), sizeof(dirname));
   bash_spaces(dirname);

   if (!fd->fsend(hello, dirname)) {
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }
   Dmsg1(dbglvl, "Sent: %s", fd->msg);

   if (client->tls_verify_peer) {
      verify_list = client->tls_allowed_cns;
   }

   ASSERT(client->password.encoding == p_encoding_md5);
   auth_success = two_way_authenticate(fd, jcr, client->password.value, false,
                                       client->tls_enable, client->tls_require, client->tls_authenticate,
                                       client->tls_verify_peer, verify_list, client->tls_ctx, "File daemon");
   if (!auth_success) {
      Dmsg0(dbglvl, _("Director and File daemon passwords or names not the same.\n"));
      Jmsg(jcr, M_FATAL, 0,
            _("Unable to authenticate with File daemon at \"%s:%d\". Possible causes:\n"
              "Passwords or names not the same or\n"
              "Maximum Concurrent Jobs exceeded on the FD or\n"
              "FD networking messed up (restart daemon).\n"
              "Please see %s for help.\n"),
            fd->host(), fd->port(), MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      Dmsg1(dbglvl, _("Bad response from File daemon to Hello command: ERR=%s\n"),
            bnet_strerror(fd));
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon at \"%s:%d\" to Hello command: ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }

   Dmsg1(110, "<filed: %s", fd->msg);
   jcr->FDVersion = 0;
   if (!bstrncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) &&
       sscanf(fd->msg, FDOKnewHello, &jcr->FDVersion) != 1) {
      Dmsg0(dbglvl, _("File daemon rejected Hello command\n"));
      Jmsg(jcr, M_FATAL, 0, _("File daemon at \"%s:%d\" rejected Hello command\n"),
           fd->host(), fd->port());
      return false;
   }

   return true;
}

/*
 * Authenticate File daemon connection
 */
bool authenticate_file_daemon(JCR *jcr, char *client_name)
{
   CLIENTRES *client;
   BSOCK *fd = jcr->file_bsock;
   bool auth_success = false;

   unbash_spaces(client_name);
   client = (CLIENTRES *)GetResWithName(R_CLIENT, client_name);
   if (client) {
      if (client->allow_client_connect) {
         ASSERT(client->password.encoding == p_encoding_md5);
         auth_success = two_way_authenticate(fd, NULL, client->password.value, true,
                                             client->tls_enable, client->tls_require, client->tls_authenticate,
                                             client->tls_verify_peer, client->tls_allowed_cns, client->tls_ctx, "File daemon");
      }
   }

   /*
    * Authorization Completed
    */
   if (!auth_success) {
      fd->fsend("%s", _(Dir_sorry));
      Emsg4(M_ERROR, 0, _("Unable to authenticate client \"%s\" at %s:%s:%d.\n"),
            client_name, fd->who(), fd->host(), fd->port());
      sleep(5);
      return false;
   }
   fd->fsend(_("1000 OK: %s Version: %s (%s)\n"), my_name, VERSION, BDATE);

   return true;
}

/*
 * Count the number of established console connections.
 */
static inline bool count_console_connections()
{
   JCR *jcr;
   unsigned int cnt = 0;

   foreach_jcr(jcr) {
      if (jcr->is_JobType(JT_CONSOLE)) {
         cnt++;
      }
   }
   endeach_jcr(jcr);

   return (cnt >= me->MaxConsoleConnections) ? false : true;
}

/*
 * Authenticate user agent.
 */
bool authenticate_user_agent(UAContext *uac)
{
   char name[MAX_NAME_LENGTH];
   CONRES *cons = NULL;
   BSOCK *ua = uac->UA_sock;
   bool auth_success = false;

   if (sscanf(ua->msg, "Hello %127s calling\n", name) != 1) {
      ua->msg[100] = 0;                 /* terminate string */
      Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Got: %s\n"), ua->who(),
            ua->host(), ua->port(), ua->msg);
      return false;
   }
   name[sizeof(name)-1] = 0;            /* terminate name */

   if (!count_console_connections()) {
      ua->fsend("%s", _(Dir_sorry));
      Emsg0(M_ERROR, 0, _("Number of console connections exhausted, please increase MaximumConsoleConnections\n"));
      return false;
   }

   if (bstrcmp(name, "*UserAgent*")) {  /* default console */
      ASSERT(me->password.encoding == p_encoding_md5);
      auth_success = two_way_authenticate(ua, NULL, me->password.value, true,
                                          me->tls_enable, me->tls_require, me->tls_authenticate,
                                          me->tls_verify_peer, me->tls_allowed_cns, me->tls_ctx, "Console");
   } else {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
         ASSERT(cons->password.encoding == p_encoding_md5);
         auth_success = two_way_authenticate(ua, NULL, cons->password.value, true,
                                             cons->tls_enable, cons->tls_require, cons->tls_authenticate,
                                             cons->tls_verify_peer, cons->tls_allowed_cns, cons->tls_ctx, "Console");

         if (auth_success) {
            uac->cons = cons;           /* save console resource pointer */
         }
      }
   }

   /*
    * Authorization Completed
    */
   if (!auth_success) {
      ua->fsend("%s", _(Dir_sorry));
      Emsg4(M_ERROR, 0, _("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
            name, ua->who(), ua->host(), ua->port());
      sleep(5);
      return false;
   }
   ua->fsend(_("1000 OK: %s Version: %s (%s)\n"), my_name, VERSION, BDATE);

   return true;
}
