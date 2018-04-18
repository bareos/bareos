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

#include "tls_conf.h"
#include "lib/bnet.h"

const int debuglevel = 50;

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
static bool authenticate_with_director(JobControlRecord *jcr, DirectorResource *dir_res) {
   MonitorResource *monitor = MonitorItemThread::instance()->getMonitor();

   BareosSocket *dir = jcr->dir_bsock;
   char bashed_name[MAX_NAME_LENGTH];
   char errmsg[1024];
   int32_t errmsg_len = sizeof(errmsg);

   bstrncpy(bashed_name, monitor->name(), sizeof(bashed_name));
   bash_spaces(bashed_name);

   if (!dir->authenticate_with_director(jcr, bashed_name,(s_password &) monitor->password, errmsg, errmsg_len, monitor)) {
      Jmsg(jcr, M_FATAL, 0, _("Director authorization problem.\n"
                                 "Most likely the passwords do not agree.\n"
                                 "Please see %s for help.\n"), MANUAL_AUTH_URL);
      return false;
   }

   return true;
}

/*
 * Authenticate Storage daemon connection
 */
static bool authenticate_with_storage_daemon(JobControlRecord *jcr, StoreResource* store)
{
   const MonitorResource *monitor = MonitorItemThread::instance()->getMonitor();

   BareosSocket *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;

   /**
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, monitor->hdr.name, sizeof(dirname));
   bash_spaces(dirname);

   if (!sd->fsend(SDFDhello, dirname)) {
      Dmsg1(debuglevel, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return false;
   }

   auth_success = sd->authenticate_outbound_connection(
      jcr, "Storage daemon", dirname, store->password, store);
   if (!auth_success) {
      Dmsg2(debuglevel,
            "Director unable to authenticate with Storage daemon at \"%s:%d\"\n",
            sd->host(),
            sd->port());
      Jmsg(jcr, M_FATAL, 0,
           _("Director unable to authenticate with Storage daemon at \"%s:%d\". Possible causes:\n"
                "Passwords or names not the same or\n"
                "TLS negotiation problem or\n"
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
   if (!bstrncmp(sd->msg, SDOKhello, sizeof(SDOKhello))) {
      Dmsg0(debuglevel, _("Storage daemon rejected Hello command\n"));
      Jmsg2(jcr, M_FATAL, 0, _("Storage daemon at \"%s:%d\" rejected Hello command\n"),
            sd->host(), sd->port());
      return false;
   }

   return true;
}

/*
 * Authenticate File daemon connection
 */
static bool authenticate_with_file_daemon(JobControlRecord *jcr, ClientResource* client)
{
   const MonitorResource *monitor = MonitorItemThread::instance()->getMonitor();

   BareosSocket *fd = jcr->file_bsock;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;

   if (jcr->authenticated) {
      /*
       * already authenticated
       */
      return true;
   }

   /**
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, monitor->name(), sizeof(dirname));
   bash_spaces(dirname);

   if (!fd->fsend(SDFDhello, dirname)) {
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }
   Dmsg1(debuglevel, "Sent: %s", fd->msg);

   auth_success =
      fd->authenticate_outbound_connection(jcr, "File Daemon", dirname, client->password, client);

   if (!auth_success) {
      Dmsg2(debuglevel, "Unable to authenticate with File daemon at \"%s:%d\"\n", fd->host(), fd->port());
      Jmsg(jcr,
           M_FATAL,
           0,
           _("Unable to authenticate with File daemon at \"%s:%d\". Possible causes:\n"
                "Passwords or names not the same or\n"
                "TLS negotiation failed or\n"
                "Maximum Concurrent Jobs exceeded on the FD or\n"
                "FD networking messed up (restart daemon).\n"
                "Please see %s for help.\n"),
           fd->host(),
           fd->port(),
           MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      Dmsg1(debuglevel, _("Bad response from File daemon to Hello command: ERR=%s\n"),
            bnet_strerror(fd));
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon at \"%s:%d\" to Hello command: ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }

   Dmsg1(110, "<filed: %s", fd->msg);
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)-1) != 0) {
      Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Hello command\n"));
      return false;
   }

   return true;
}

bool authenticate_with_daemon(MonitorItem* item, JobControlRecord *jcr)
{
   switch (item->type()) {
   case R_DIRECTOR:
      return authenticate_with_director(jcr, (DirectorResource*)item->resource());
   case R_CLIENT:
      return authenticate_with_file_daemon(jcr, (ClientResource*)item->resource());
   case R_STORAGE:
      return authenticate_with_storage_daemon(jcr, (StoreResource*)item->resource());
   default:
      printf(_("Error, currentitem is not a Client or a Storage..\n"));
      return false;
   }

   return false;
}
