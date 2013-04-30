/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

/*
 *
 *   Bacula UA authentication. Provides authentication with
 *     the Director.
 *
 *     Kern Sibbald, June MMI   adapted to bat, Jan MMVI
 *
 *     Version $Id$
 *
 */


#include "bat.h"


/* Commands sent to Director */
static char hello[]    = "Hello %s calling\n";

/* Response from Director */
static char OKhello[]   = "1000 OK:";

/* Forward referenced functions */

/*
 * Authenticate Director
 */
bool DirComm::authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons, 
                char *errmsg, int errmsg_len) 
{
   BSOCK *dir = jcr->dir_bsock;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool tls_authenticate;
   int compatible = true;
   char bashed_name[MAX_NAME_LENGTH];
   char *password;
   TLS_CONTEXT *tls_ctx = NULL;

   errmsg[0] = 0;
   /*
    * Send my name to the Director then do authentication
    */
   if (cons) {
      bstrncpy(bashed_name, cons->hdr.name, sizeof(bashed_name));
      bash_spaces(bashed_name);
      password = cons->password;
      /* TLS Requirement */
      if (cons->tls_enable) {
         if (cons->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }
      tls_authenticate = cons->tls_authenticate;
      tls_ctx = cons->tls_ctx;
   } else {
      bstrncpy(bashed_name, "*UserAgent*", sizeof(bashed_name));
      password = director->password;
      /* TLS Requirement */
      if (director->tls_enable) {
         if (director->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }

      tls_authenticate = director->tls_authenticate;
      tls_ctx = director->tls_ctx;
   }
   if (tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   /* Timeout Hello after 15 secs */
   dir->start_timer(15);
   dir->fsend(hello, bashed_name);

   /* respond to Dir challenge */
   if (!cram_md5_respond(dir, password, &tls_remote_need, &compatible) ||
       /* Now challenge dir */
       !cram_md5_challenge(dir, password, tls_local_need, compatible)) {
      bsnprintf(errmsg, errmsg_len, _("Director authorization problem at \"%s:%d\"\n"),
         dir->host(), dir->port());
      goto bail_out;
   }

   /* Verify that the remote host is willing to meet our TLS requirements */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      bsnprintf(errmsg, errmsg_len, _("Authorization problem:"
             " Remote server at \"%s:%d\" did not advertise required TLS support.\n"),
             dir->host(), dir->port());
      goto bail_out;
   }

   /* Verify that we are willing to meet the remote host's requirements */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      bsnprintf(errmsg, errmsg_len, _("Authorization problem with Director at \"%s:%d\":"
                     " Remote server requires TLS.\n"),
                     dir->host(), dir->port());

      goto bail_out;
   }

   /* Is TLS Enabled? */
   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /* Engage TLS! Full Speed Ahead! */
      if (!bnet_tls_client(tls_ctx, dir, NULL)) {
         bsnprintf(errmsg, errmsg_len, _("TLS negotiation failed with Director at \"%s:%d\"\n"),
            dir->host(), dir->port());
         goto bail_out;
      }
      if (tls_authenticate) {               /* authenticate only? */
         dir->free_tls();                   /* Yes, shutdown tls */
      }
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      dir->stop_timer();
      bsnprintf(errmsg, errmsg_len, _("Bad response to Hello command: ERR=%s\n"
                      "The Director at \"%s:%d\" is probably not running.\n"),
                    dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

   dir->stop_timer();
   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, OKhello, sizeof(OKhello)-1) != 0) {
      bsnprintf(errmsg, errmsg_len, _("Director at \"%s:%d\" rejected Hello command\n"),
         dir->host(), dir->port());
      return false;
   } else {
      if (m_conn == 0) { 
         bsnprintf(errmsg, errmsg_len, "%s", dir->msg);
      }
   }
   return true;

bail_out:
   dir->stop_timer();
   bsnprintf(errmsg, errmsg_len, _("Authorization problem with Director at \"%s:%d\"\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "Please see " MANUAL_AUTH_URL " for help.\n"),
             dir->host(), dir->port());
   return false;
}
