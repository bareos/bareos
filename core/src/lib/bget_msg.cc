/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MMI previously in src/stored/fdmsg.c
 */
/**
 * @file
 * Subroutines to receive network data and handle
 * network signals for the FD and the SD.
 */

#include "include/bareos.h"                   /* pull in global headers */
#include "lib/bnet.h"
#include "lib/bget_msg.h"

static char OK_msg[]   = "2000 OK\n";
static char TERM_msg[] = "2999 Terminate\n";

#define messagelevel 500

/**
 * This routine does a BnetRecv(), then if a signal was
 *   sent, it handles it.  The return codes are the same as
 *   bne_recv() except the BNET_SIGNAL messages that can
 *   be handled are done so without returning.
 *
 * Returns number of bytes read (may return zero)
 * Returns -1 on signal (BNET_SIGNAL)
 * Returns -2 on hard end of file (BNET_HARDEOF)
 * Returns -3 on error  (BNET_ERROR)
 */
int BgetMsg(BareosSocket *sock)
{
   int n;
   for ( ;; ) {
      n = sock->recv();
      if (n >= 0) {                  /* normal return */
         return n;
      }
      if (IsBnetStop(sock)) {      /* error return */
         return n;
      }

      /* BNET_SIGNAL (-1) return from BnetRecv() => network signal */
      switch (sock->message_length) {
      case BNET_EOD:               /* end of data */
         Dmsg0(messagelevel, "Got BNET_EOD\n");
         return n;
      case BNET_EOD_POLL:
         Dmsg0(messagelevel, "Got BNET_EOD_POLL\n");
         if (sock->IsTerminated()) {
            sock->fsend(TERM_msg);
         } else {
            sock->fsend(OK_msg); /* send response */
         }
         return n;                 /* end of data */
      case BNET_TERMINATE:
         Dmsg0(messagelevel, "Got BNET_TERMINATE\n");
         sock->SetTerminated();
         return n;
      case BNET_POLL:
         Dmsg0(messagelevel, "Got BNET_POLL\n");
         if (sock->IsTerminated()) {
            sock->fsend(TERM_msg);
         } else {
            sock->fsend(OK_msg); /* send response */
         }
         break;
      case BNET_HEARTBEAT:
      case BNET_HB_RESPONSE:
         break;
      case BNET_STATUS:
         /* *****FIXME***** Implement BNET_STATUS */
         Dmsg0(messagelevel, "Got BNET_STATUS\n");
         sock->fsend(_("Status OK\n"));
         sock->signal(BNET_EOD);
         break;
      default:
         Emsg1(M_ERROR, 0, _("BgetMsg: unknown signal %d\n"), sock->message_length);
         break;
      }
   }
}
