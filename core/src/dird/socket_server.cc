/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, August MM
 * Extracted from other source files by Marco van Wieringen, October 2014
 */
/**
 * @file
 * This file handles external connections made to the director.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/fd_cmds.h"
#include "dird/ua_server.h"
#include "lib/bnet_sever_tcp.h"

static char hello_client_with_version[] =
   "Hello Client %127s FdProtocolVersion=%d calling";

static char hello_client[] =
   "Hello Client %127s calling";

/* Global variables */
static workq_t socket_workq;
static alist *sock_fds = NULL;
static pthread_t tcp_server_tid;
static ConnectionPool *client_connections = NULL;

struct s_addr_port {
   char *addr;
   char *port;
};

/*
 * Sanity check for the lengths of the Hello messages.
 */
#define MIN_MSG_LEN 15
#define MAX_MSG_LEN (int)sizeof(name) + 25

ConnectionPool *get_client_connections()
{
   return client_connections;
}

static void *handle_connection_request(void *arg)
{
   BareosSocket *bs = (BareosSocket *)arg;
   char name[MAX_NAME_LENGTH];
   char tbuf[MAX_TIME_LENGTH];
   int fd_protocol_version = 0;

   if (bs->recv() <= 0) {
      Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
      Bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   /*
    * Do a sanity check on the message received
    */
   if (bs->msglen < MIN_MSG_LEN || bs->msglen > MAX_MSG_LEN) {
      Dmsg1(000, "<filed: %s", bs->msg);
      Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->msglen);
      Bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   Dmsg1(110, "Conn: %s", bs->msg);

   /*
    * See if this is a File daemon connection. If so call FD handler.
    */
   if ((sscanf(bs->msg, hello_client_with_version, name, &fd_protocol_version) == 2) ||
       (sscanf(bs->msg, hello_client, name) == 1)) {
      Dmsg1(110, "Got a FD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_filed_connection(client_connections, bs, name, fd_protocol_version);
   }

   return handle_UA_client_request(bs);
}

extern "C" void *connect_thread(void *arg)
{
   pthread_detach(pthread_self());
   SetJcrInTsd(INVALID_JCR);

   /*
    * Permit MaxConnections connections.
    */
   sock_fds = New(alist(10, not_owned_by_alist));
   bnet_thread_server_tcp((dlist*)arg,
                          me->MaxConnections,
                          sock_fds,
                          &socket_workq,
                          me->nokeepalive,
                          handle_connection_request);

   return NULL;
}

/**
 * Called here by Director daemon to start UA (user agent)
 * command thread. This routine creates the thread and then
 * returns.
 */
void StartSocketServer(dlist *addrs)
{
   int status;
   static dlist *myaddrs = addrs;

   if (client_connections == NULL) {
      client_connections = New(ConnectionPool());
   }
   if ((status = pthread_create(&tcp_server_tid, NULL, connect_thread, (void *)myaddrs)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Cannot create UA thread: %s\n"), be.bstrerror(status));
   }

   return;
}

void StopSocketServer()
{
   if (sock_fds) {
      BnetStopThreadServerTcp(tcp_server_tid);
      CleanupBnetThreadServerTcp(sock_fds, &socket_workq);
      delete sock_fds;
      sock_fds = NULL;
   }
   if (client_connections) {
      delete(client_connections);
   }
}
