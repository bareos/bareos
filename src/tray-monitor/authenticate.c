/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.

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
 *   Bacula authentication. Provides authentication with
 *     File and Storage daemons.
 *
 *     Nicolas Boichat, August MMIV
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *
 */

#include "bacula.h"
#include "tray-monitor.h"
#include "jcr.h"

void senditf(const char *fmt, ...);
void sendit(const char *buf);

/* Commands sent to Director */
static char DIRhello[]    = "Hello %s calling\n";

/* Response from Director */
static char DIROKhello[]   = "1000 OK:";

/* Commands sent to Storage daemon and File daemon and received
 *  from the User Agent */
static char SDFDhello[]    = "Hello Director %s calling\n";

/* Response from SD */
static char SDOKhello[]   = "3000 OK Hello\n";
/* Response from FD */
static char FDOKhello[] = "2000 OK Hello";

/* Forward referenced functions */

/*
 * Authenticate Director
 */
int authenticate_director(JCR *jcr, MONITOR *mon, DIRRES *director)
{
   BSOCK *dir = jcr->dir_bsock;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;
   char bashed_name[MAX_NAME_LENGTH];
   char *password;

   bstrncpy(bashed_name, mon->hdr.name, sizeof(bashed_name));
   bash_spaces(bashed_name);
   password = mon->password;

   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(dir, 60 * 5);
   dir->fsend(DIRhello, bashed_name);

   if (!cram_md5_respond(dir, password, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(dir, password, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      Jmsg0(jcr, M_FATAL, 0, _("Director authorization problem.\n"
            "Most likely the passwords do not agree.\n"
       "Please see " MANUAL_AUTH_URL " for help.\n"));
      return 0;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg1(jcr, M_FATAL, 0, _("Bad response to Hello command: ERR=%s\n"),
         dir->bstrerror());
      return 0;
   }
   Dmsg1(10, "<dird: %s", dir->msg);
   stop_bsock_timer(tid);
   if (strncmp(dir->msg, DIROKhello, sizeof(DIROKhello)-1) != 0) {
      Jmsg0(jcr, M_FATAL, 0, _("Director rejected Hello command\n"));
      return 0;
   } else {
      Jmsg0(jcr, M_INFO, 0, dir->msg);
   }
   return 1;
}

/*
 * Authenticate Storage daemon connection
 */
int authenticate_storage_daemon(JCR *jcr, MONITOR *monitor, STORE* store)
{
   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, monitor->hdr.name, sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(sd, 60 * 5);
   if (!sd->fsend(SDFDhello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), sd->bstrerror());
      return 0;
   }
   if (!cram_md5_respond(sd, store->password, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(sd, store->password, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      Jmsg0(jcr, M_FATAL, 0, _("Director and Storage daemon passwords or names not the same.\n"
       "Please see " MANUAL_AUTH_URL " for help.\n"));
      return 0;
   }
   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg1(jcr, M_FATAL, 0, _("bdird<stored: bad response to Hello command: ERR=%s\n"),
         sd->bstrerror());
      return 0;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   stop_bsock_timer(tid);
   if (strncmp(sd->msg, SDOKhello, sizeof(SDOKhello)) != 0) {
      Jmsg0(jcr, M_FATAL, 0, _("Storage daemon rejected Hello command\n"));
      return 0;
   }
   return 1;
}

/*
 * Authenticate File daemon connection
 */
int authenticate_file_daemon(JCR *jcr, MONITOR *monitor, CLIENT* client)
{
   BSOCK *fd = jcr->file_bsock;
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, monitor->hdr.name, sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(fd, 60 * 5);
   if (!fd->fsend(SDFDhello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon. ERR=%s\n"), fd->bstrerror());
      return 0;
   }
   if (!cram_md5_respond(fd, client->password, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(fd, client->password, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Director and File daemon passwords or names not the same.\n"
       "Please see " MANUAL_AUTH_URL " for help.\n"));
      return 0;
   }
   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon to Hello command: ERR=%s\n"),
         fd->bstrerror());
      return 0;
   }
   Dmsg1(110, "<stored: %s", fd->msg);
   stop_bsock_timer(tid);
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)-1) != 0) {
      Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Hello command\n"));
      return 0;
   }
   return 1;
}
