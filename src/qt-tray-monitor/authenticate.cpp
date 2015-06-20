/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.
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
 * Bareos authentication. Provides authentication with File and Storage daemons.
 *
 * Nicolas Boichat, August MMIV
 */

#include "monitoritem.h"
#include "authenticate.h"
#include "jcr.h"
#include "monitoritemthread.h"


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
static bool authenticate_director(JCR *jcr)
{
   const MONITORRES *monitor = MonitorItemThread::instance()->getMonitor();

   BSOCK *dir = jcr->dir_bsock;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;
   char bashed_name[MAX_NAME_LENGTH];

   bstrncpy(bashed_name, monitor->name(), sizeof(bashed_name));
   bash_spaces(bashed_name);

   /*
    * Timeout Hello after 5 mins
    */
   btimer_t *tid = start_bsock_timer(dir, 60 * 5);
   dir->fsend(DIRhello, bashed_name);

   ASSERT(monitor->password.encoding == p_encoding_md5);

   if (!cram_md5_respond(dir, monitor->password.value, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(dir, monitor->password.value, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Director authorization problem.\n"
                              "Most likely the passwords do not agree.\n"
                              "Please see %s for help.\n"), MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg1(jcr, M_FATAL, 0, _("Bad response to Hello command: ERR=%s\n"),
         dir->bstrerror());
      return false;
   }

   Dmsg1(10, "<dird: %s", dir->msg);
   stop_bsock_timer(tid);
   if (strncmp(dir->msg, DIROKhello, sizeof(DIROKhello)-1) != 0) {
      Jmsg0(jcr, M_FATAL, 0, _("Director rejected Hello command\n"));
      return false;
   } else {
      Jmsg0(jcr, M_INFO, 0, dir->msg);
   }

   return true;
}

/*
 * Authenticate Storage daemon connection
 */
static bool authenticate_storage_daemon(JCR *jcr, STORERES* store)
{
   const MONITORRES *monitor = MonitorItemThread::instance()->getMonitor();

   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, monitor->name(), sizeof(dirname));
   bash_spaces(dirname);

   /*
    * Timeout Hello after 5 mins
    */
   btimer_t *tid = start_bsock_timer(sd, 60 * 5);
   if (!sd->fsend(SDFDhello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return false;
   }

   ASSERT(store->password.encoding == p_encoding_md5);

   if (!cram_md5_respond(sd, store->password.value, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(sd, store->password.value, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      Jmsg0(jcr, M_FATAL, 0, _("Director and Storage daemon passwords or names not the same.\n"
       "Please see " MANUAL_AUTH_URL " for help.\n"));
      return false;
   }

   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg1(jcr, M_FATAL, 0, _("bdird<stored: bad response to Hello command: ERR=%s\n"),
         sd->bstrerror());
      return false;
   }

   Dmsg1(110, "<stored: %s", sd->msg);
   stop_bsock_timer(tid);
   if (strncmp(sd->msg, SDOKhello, sizeof(SDOKhello)) != 0) {
      Jmsg0(jcr, M_FATAL, 0, _("Storage daemon rejected Hello command\n"));
      return false;
   }

   return true;
}

/*
 * Authenticate File daemon connection
 */
static bool authenticate_file_daemon(JCR *jcr, CLIENTRES* client)
{
   const MONITORRES *monitor = MonitorItemThread::instance()->getMonitor();

   BSOCK *fd = jcr->file_bsock;
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   bool compatible = true;

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, monitor->name(), sizeof(dirname));
   bash_spaces(dirname);

   /*
    * Timeout Hello after 5 mins
    */
   btimer_t *tid = start_bsock_timer(fd, 60 * 5);
   if (!fd->fsend(SDFDhello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon. ERR=%s\n"), bnet_strerror(fd));
      return false;
   }

   ASSERT(client->password.encoding == p_encoding_md5);

   if (!cram_md5_respond(fd, client->password.value, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(fd, client->password.value, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Director and File daemon passwords or names not the same.\n"
                              "Please see %s for help.\n"), MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon to Hello command: ERR=%s\n"),
           fd->bstrerror());
      return false;
   }

   Dmsg1(110, "<stored: %s", fd->msg);
   stop_bsock_timer(tid);
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)-1) != 0) {
      Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Hello command\n"));
      return false;
   }

   return true;
}

bool authenticate_daemon(MonitorItem* item, JCR *jcr)
{
   switch (item->type()) {
   case R_DIRECTOR:
      return authenticate_director(jcr);
   case R_CLIENT:
      return authenticate_file_daemon(jcr, (CLIENTRES*)item->resource());
   case R_STORAGE:
      return authenticate_storage_daemon(jcr, (STORERES*)item->resource());
   default:
      printf(_("Error, currentitem is not a Client or a Storage..\n"));
      return false;
   }

   return false;
}
