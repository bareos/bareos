/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * SCTP Sockets abstraction.
 *
 * Marco van Wieringen, September 2013.
 */

#include "bareos.h"
#include "jcr.h"
#include "bsock_sctp.h"

BSOCK_SCTP::BSOCK_SCTP()
{
}

BSOCK_SCTP::~BSOCK_SCTP()
{
   destroy();
}

BSOCK *BSOCK_SCTP::clone()
{
   BSOCK_SCTP *clone;
   POOLMEM *o_msg, *o_errmsg;

   clone = New(BSOCK_SCTP);

   /*
    * Copy the data from the original BSOCK but preserve the msg and errmsg buffers.
    */
   o_msg = clone->msg;
   o_errmsg = clone->errmsg;
   memcpy((void *)clone, (void *)this, sizeof(BSOCK_SCTP));
   clone->msg = o_msg;
   clone->errmsg = o_errmsg;

   if (m_who) {
      clone->set_who(bstrdup(m_who));
   }
   if (m_host) {
      clone->set_who(bstrdup(m_host));
   }
   if (src_addr) {
      clone->src_addr = New(IPADDR(*(src_addr)));
   }
   m_cloned = true;

   return (BSOCK *)clone;
}

/*
 * Try to connect to host for max_retry_time at retry_time intervals.
 * Note, you must have called the constructor prior to calling
 * this routine.
 */
bool BSOCK_SCTP::connect(JCR * jcr, int retry_interval, utime_t max_retry_time,
                        utime_t heart_beat, const char *name, char *host,
                        char *service, int port, bool verbose)
{
   return false;
}

/*
 * Finish initialization of the pocket structure.
 */
void BSOCK_SCTP::fin_init(JCR * jcr, int sockfd, const char *who, const char *host,
                         int port, struct sockaddr *lclient_addr)
{
}

/*
 * Open a SCTP connection to the server
 * Returns NULL
 * Returns BSOCK * pointer on success
 */
bool BSOCK_SCTP::open(JCR *jcr, const char *name, char *host, char *service,
                     int port, utime_t heart_beat, int *fatal)
{
   return false;
}

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: false on failure
 *          true  on success
 */
bool BSOCK_SCTP::send()
{
   return false;
}

/*
 * Receive a message from the other end. Each message consists of
 * two packets. The first is a header that contains the size
 * of the data that follows in the second packet.
 * Returns number of bytes read (may return zero)
 * Returns -1 on signal (BNET_SIGNAL)
 * Returns -2 on hard end of file (BNET_HARDEOF)
 * Returns -3 on error  (BNET_ERROR)
 *
 *  Unfortunately, it is a bit complicated because we have these
 *    four return types:
 *    1. Normal data
 *    2. Signal including end of data stream
 *    3. Hard end of file
 *    4. Error
 *  Using is_bnet_stop() and is_bnet_error() you can figure this all out.
 */
int32_t BSOCK_SCTP::recv()
{
   return -1;
}

int BSOCK_SCTP::get_peer(char *buf, socklen_t buflen)
{
   return -1;
}

/*
 * Set the network buffer size, suggested size is in size.
 * Actual size obtained is returned in bs->msglen
 *
 * Returns: false on failure
 *          true  on success
 */
bool BSOCK_SCTP::set_buffer_size(uint32_t size, int rw)
{
   return false;
}

/*
 * Set socket non-blocking
 * Returns previous socket flag
 */
int BSOCK_SCTP::set_nonblocking()
{
   return -1;
}

/*
 * Set socket blocking
 * Returns previous socket flags
 */
int BSOCK_SCTP::set_blocking()
{
   return -1;
}

/*
 * Restores socket flags
 */
void BSOCK_SCTP::restore_blocking (int flags)
{
}

/*
 * Wait for a specified time for data to appear on
 * the BSOCK connection.
 *
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int BSOCK_SCTP::wait_data(int sec, int usec)
{
   return -1;
}

/*
 * As above, but returns on interrupt
 */
int BSOCK_SCTP::wait_data_intr(int sec, int usec)
{
   return -1;
}

void BSOCK_SCTP::close()
{
}

void BSOCK_SCTP::destroy()
{
}

/*
 * Read a nbytes from the network.
 * It is possible that the total bytes require in several
 * read requests
 */
int32_t BSOCK_SCTP::read_nbytes(char *ptr, int32_t nbytes)
{
   return -1;
}

/*
 * Write nbytes to the network.
 * It may require several writes.
 */
int32_t BSOCK_SCTP::write_nbytes(char *ptr, int32_t nbytes)
{
   return -1;
}
