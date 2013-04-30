/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- mountreq.c -- handles the message channel
 *    Mount request from the Storage daemon.
 *
 *     Kern Sibbald, March MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *      Handle Mount services.
 *
 */

#include "bacula.h"
#include "dird.h"

/*
 * Handle mount request
 *  For now, we put the bsock in the UA's queue
 */

/* Requests from the Storage daemon */


/* Responses  sent to Storage daemon */
#ifdef xxx
static char OK_mount[]  = "1000 OK MountVolume\n";
#endif

static BQUEUE mountq = {&mountq, &mountq};
static int num_reqs = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct mnt_req_s {
   BQUEUE bq;
   BSOCK *bs;
   JCR *jcr;
} MNT_REQ;


void mount_request(JCR *jcr, BSOCK *bs, char *buf)
{
   MNT_REQ *mreq;

   mreq = (MNT_REQ *) malloc(sizeof(MNT_REQ));
   memset(mreq, 0, sizeof(MNT_REQ));
   mreq->jcr = jcr;
   mreq->bs = bs;
   P(mutex);
   num_reqs++;
   qinsert(&mountq, &mreq->bq);
   V(mutex);
   return;
}
