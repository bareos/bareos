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
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

#include "bacula.h"
#include "check_bacula.h"

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
int authenticate_director(BSOCK *dir, char *dirname, char *password)
{
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;
   char bashed_name[MAX_NAME_LENGTH];

   bstrncpy(bashed_name, dirname, sizeof(bashed_name));
   bash_spaces(bashed_name);

   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(dir, 60 * 5);
   bnet_fsend(dir, DIRhello, bashed_name);

   if (!cram_md5_respond(dir, password, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(dir, password, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      return 0;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (bnet_recv(dir) <= 0) {
      stop_bsock_timer(tid);
      return 0;
   }
   Dmsg1(10, "<dird: %s", dir->msg);
   stop_bsock_timer(tid);
   if (strncmp(dir->msg, DIROKhello, sizeof(DIROKhello)-1) != 0) {
      return 0;
   }
   return 1;
}

/*
 * Authenticate Storage daemon connection
 */
int authenticate_storage_daemon(BSOCK *sd, char *sdname, char* password)
{
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, sdname, sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(sd, 60 * 5);
   if (!bnet_fsend(sd, SDFDhello, dirname)) {
      stop_bsock_timer(tid);
      return 0;
   }
   if (!cram_md5_respond(sd, password, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(sd, password, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      return 0;
   }
   Dmsg1(116, ">stored: %s", sd->msg);
   if (bnet_recv(sd) <= 0) {
      stop_bsock_timer(tid);
      return 0;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   stop_bsock_timer(tid);
   if (strncmp(sd->msg, SDOKhello, sizeof(SDOKhello)) != 0) {
      return 0;
   }
   return 1;
}

/*
 * Authenticate File daemon connection
 */
int authenticate_file_daemon(BSOCK *fd, char *fdname, char *password)
{
   char dirname[MAX_NAME_LENGTH];
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, fdname, sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(fd, 60 * 5);
   if (!bnet_fsend(fd, SDFDhello, dirname)) {
      stop_bsock_timer(tid);
      return 0;
   }
   if (!cram_md5_respond(fd, password, &tls_remote_need, &compatible) ||
       !cram_md5_challenge(fd, password, tls_local_need, compatible)) {
      stop_bsock_timer(tid);
      return 0;
   }
   Dmsg1(116, ">filed: %s", fd->msg);
   if (bnet_recv(fd) <= 0) {
      stop_bsock_timer(tid);
      return 0;
   }
   Dmsg1(110, "<stored: %s", fd->msg);
   stop_bsock_timer(tid);
   if ((strncmp(fd->msg, FDOKhello, strlen(FDOKhello)) != 0)) {
      return 0;
   }
   return 1;
}
