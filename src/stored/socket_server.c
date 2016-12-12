/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MMI
 * Extracted from other source files by Marco van Wieringen, October 2014
 */
/**
 * @file
 * This file handles external connections made to the storage daemon.
 */

#include "bareos.h"
#include "stored.h"

/* Global variables */
static workq_t socket_workq;
static alist *sock_fds = NULL;
static pthread_t tcp_server_tid;

/**
 * Sanity check for the lengths of the Hello messages.
 */
#define MIN_MSG_LEN 25
#define MAX_MSG_LEN (int)sizeof(name) + 30

/**
 * Connection request. We accept connections either from the
 * Director, Storage Daemon or a Client (File daemon).
 *
 * Note, we are running as a separate thread of the Storage daemon.
 *
 * Basic tasks done here:
 *  - If it was a connection from the FD, call handle_filed_connection()
 *  - If it was a connection from another SD, call handle_stored_connection()
 *  - Otherwise it was a connection from the DIR, call handle_director_connection()
 */
static void *handle_connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   char name[MAX_NAME_LENGTH];
   char tbuf[MAX_TIME_LENGTH];

   if (bs->recv() <= 0) {
      Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   /*
    * Do a sanity check on the message received
    */
   if (bs->msglen < MIN_MSG_LEN || bs->msglen > MAX_MSG_LEN) {
      Dmsg1(000, "<filed: %s", bs->msg);
      Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->msglen);
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   Dmsg1(110, "Conn: %s", bs->msg);

   /*
    * See if this is a File daemon connection. If so call FD handler.
    */
   if (sscanf(bs->msg, "Hello Start Job %127s", name) == 1) {
      Dmsg1(110, "Got a FD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_filed_connection(bs, name);
   }

   /*
    * See if this is a Storage daemon connection. If so call SD handler.
    */
   if (sscanf(bs->msg, "Hello Start Storage Job %127s", name) == 1) {
      Dmsg1(110, "Got a SD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_stored_connection(bs, name);
   }

   Dmsg1(110, "Got a DIR connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));

   return handle_director_connection(bs);
}

void start_socket_server(dlist *addrs)
{
   IPADDR *p;

   tcp_server_tid = pthread_self();

   /*
    * Become server, and handle requests
    */
   foreach_dlist(p, addrs) {
      Dmsg1(10, "stored: listening on port %d\n", p->get_port_host_order());
   }

   sock_fds = New(alist(10, not_owned_by_alist));
   bnet_thread_server_tcp(addrs,
                          me->MaxConnections,
                          sock_fds,
                          &socket_workq,
                          me->nokeepalive,
                          handle_connection_request);
}

void stop_socket_server()
{
   if (sock_fds) {
      bnet_stop_thread_server_tcp(tcp_server_tid);
      cleanup_bnet_thread_server_tcp(sock_fds, &socket_workq);
      delete sock_fds;
      sock_fds = NULL;
   }
}
