/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * BAREOS Director -- authenticate.c -- handles authorization of Storage and File daemons.
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
 * Authenticate Storage daemon connection
 */
bool authenticate_storage_daemon(JCR *jcr, STORERES *store)
{
   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;
   bool auth_success = false;
   btimer_t *tid = NULL;

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, me->hdr.name, sizeof(dirname));
   bash_spaces(dirname);

   /*
    * Timeout Hello after 10 min
    */
   tid = start_bsock_timer(sd, AUTH_TIMEOUT);
   if (!sd->fsend(hello, dirname)) {
      stop_bsock_timer(tid);
      Dmsg1(dbglvl, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return false;
   }

   /*
    * TLS Requirement
    */
   if (store->tls_enable) {
     if (store->tls_require) {
        tls_local_need = BNET_TLS_REQUIRED;
     } else {
        tls_local_need = BNET_TLS_OK;
     }
   }

   if (store->tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   ASSERT(store->password.encoding == p_encoding_md5);
   auth_success = cram_md5_respond(sd, store->password.value, &tls_remote_need, &compatible);
   if (auth_success) {
      auth_success = cram_md5_challenge(sd, store->password.value, tls_local_need, compatible);
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_challenge failed for %s\n", sd->who());
      }
   } else {
      Dmsg1(dbglvl, "cram_respond failed for %s\n", sd->who());
   }

   if (!auth_success) {
      stop_bsock_timer(tid);
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

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not advertise required TLS support.\n"));
      return false;
   }

   /*
    * Verify that we are willing to meet the remote host's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      return false;
   }

   /*
    * Is TLS Enabled?
    */
   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      if (!bnet_tls_client(store->tls_ctx, sd, NULL)) {
         stop_bsock_timer(tid);
         Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with SD at \"%s:%d\"\n"),
            sd->host(), sd->port());
         return false;
      }
      if (store->tls_authenticate) {       /* authentication only? */
         sd->free_tls();                   /* yes, stop tls */
      }
   }

   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg3(jcr, M_FATAL, 0, _("dir<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
         sd->who(), sd->host(), sd->bstrerror());
      return false;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   stop_bsock_timer(tid);
   if (!bstrncmp(sd->msg, OKhello, sizeof(OKhello))) {
      Dmsg0(dbglvl, _("Storage daemon rejected Hello command\n"));
      Jmsg2(jcr, M_FATAL, 0, _("Storage daemon at \"%s:%d\" rejected Hello command\n"),
         sd->host(), sd->port());
      return false;
   }

   return true;
}

/*
 * Authenticate File daemon connection
 */
bool authenticate_file_daemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   CLIENTRES *client = jcr->res.client;
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;
   bool auth_success = false;
   btimer_t *tid = NULL;

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, me->name(), sizeof(dirname));
   bash_spaces(dirname);

   /*
    * Timeout Hello after 10 min
    */
   tid = start_bsock_timer(fd, AUTH_TIMEOUT);
   if (!fd->fsend(hello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }
   Dmsg1(dbglvl, "Sent: %s", fd->msg);

   /*
    * TLS Requirement
    */
   if (client->tls_enable) {
     if (client->tls_require) {
        tls_local_need = BNET_TLS_REQUIRED;
     } else {
        tls_local_need = BNET_TLS_OK;
     }
   }

   if (client->tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   ASSERT(client->password.encoding == p_encoding_md5);
   auth_success = cram_md5_respond(fd, client->password.value, &tls_remote_need, &compatible);
   if (auth_success) {
      auth_success = cram_md5_challenge(fd, client->password.value, tls_local_need, compatible);
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_auth failed for %s\n", fd->who());
      }
   } else {
      Dmsg1(dbglvl, "cram_get_auth failed for %s\n", fd->who());
   }
   if (!auth_success) {
      stop_bsock_timer(tid);
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

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: FD \"%s:%s\" did not advertise required TLS support.\n"),
           fd->who(), fd->host());
      return false;
   }

   /*
    * Verify that we are willing to meet the remote host's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: FD at \"%s:%d\" requires TLS.\n"),
           fd->host(), fd->port());
      return false;
   }

   /*
    * Is TLS Enabled?
    */
   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      if (!bnet_tls_client(client->tls_ctx, fd, client->tls_allowed_cns)) {
         stop_bsock_timer(tid);
         Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with FD at \"%s:%d\".\n"),
              fd->host(), fd->port());
         return false;
      }
      if (client->tls_authenticate) {        /* tls authentication only? */
         fd->free_tls();                     /* yes, shutdown tls */
      }
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      stop_bsock_timer(tid);
      Dmsg1(dbglvl, _("Bad response from File daemon to Hello command: ERR=%s\n"),
         bnet_strerror(fd));
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon at \"%s:%d\" to Hello command: ERR=%s\n"),
         fd->host(), fd->port(), fd->bstrerror());
      return false;
   }
   Dmsg1(110, "<filed: %s", fd->msg);
   stop_bsock_timer(tid);
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
 * Authenticate user agent.
 */
bool authenticate_user_agent(UAContext *uac)
{
   char name[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool tls_authenticate;
   bool compatible = true;
   CONRES *cons = NULL;
   BSOCK *ua = uac->UA_sock;
   bool auth_success = false;
   TLS_CONTEXT *tls_ctx = NULL;
   alist *verify_list = NULL;

   if (ua->msglen < 16 || ua->msglen >= MAX_NAME_LENGTH + 15) {
      Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Len=%d\n"), ua->who(),
            ua->host(), ua->port(), ua->msglen);
      return false;
   }

   if (sscanf(ua->msg, "Hello %127s calling\n", name) != 1) {
      ua->msg[100] = 0;                 /* terminate string */
      Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Got: %s\n"), ua->who(),
            ua->host(), ua->port(), ua->msg);
      return false;
   }

   name[sizeof(name)-1] = 0;            /* terminate name */
   if (bstrcmp(name, "*UserAgent*")) {  /* default console */
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

      tls_authenticate = me->tls_authenticate;

      if (tls_authenticate) {
         tls_local_need = BNET_TLS_REQUIRED;
      }

      if (me->tls_verify_peer) {
         verify_list = me->tls_allowed_cns;
      }

      ASSERT(me->password.encoding == p_encoding_md5);
      auth_success = cram_md5_challenge(ua, me->password.value, tls_local_need, compatible) &&
                     cram_md5_respond(ua, me->password.value, &tls_remote_need, &compatible);
   } else {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
         /*
          * TLS Requirement
          */
         if (cons->tls_enable) {
            if (cons->tls_require) {
               tls_local_need = BNET_TLS_REQUIRED;
            } else {
               tls_local_need = BNET_TLS_OK;
            }
         }

         tls_authenticate = cons->tls_authenticate;

         if (tls_authenticate) {
            tls_local_need = BNET_TLS_REQUIRED;
         }

         if (cons->tls_verify_peer) {
            verify_list = cons->tls_allowed_cns;
         }

         ASSERT(cons->password.encoding == p_encoding_md5);
         auth_success = cram_md5_challenge(ua, cons->password.value, tls_local_need, compatible) &&
                        cram_md5_respond(ua, cons->password.value, &tls_remote_need, &compatible);

         if (auth_success) {
            uac->cons = cons;           /* save console resource pointer */
         }
      } else {
         auth_success = false;
         goto auth_done;
      }
   }


   /*
    * Verify that the remote peer is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Emsg0(M_FATAL, 0, _("Authorization problem:"
            " Remote client did not advertise required TLS support.\n"));
      auth_success = false;
      goto auth_done;
   }

   /*
    * Verify that we are willing to meet the peer's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Emsg0(M_FATAL, 0, _("Authorization problem:"
            " Remote client requires TLS.\n"));
      auth_success = false;
      goto auth_done;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      if (cons) {
         tls_ctx = cons->tls_ctx;
      } else {
         tls_ctx = me->tls_ctx;
      }

      if (!bnet_tls_server(tls_ctx, ua, verify_list)) {
         Emsg0(M_ERROR, 0, _("TLS negotiation failed.\n"));
         auth_success = false;
         goto auth_done;
      }
      if (tls_authenticate) {              /* authentication only? */
         ua->free_tls();                   /* stop tls */
      }
   }

   /*
    * Authorization Completed
    */
auth_done:
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
