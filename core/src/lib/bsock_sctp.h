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

#ifndef BRS_BSOCK_SCTP_H
#define BRS_BSOCK_SCTP_H

class BSOCK_SCTP : public BSOCK {
private:
   /* methods -- in bsock_sctp.c */
   void fin_init(JCR * jcr, int sockfd, const char *who, const char *host, int port,
                 struct sockaddr *lclient_addr);
   bool open(JCR *jcr, const char *name, char *host, char *service,
             int port, utime_t heart_beat, int *fatal);

public:
   BSOCK_SCTP();
   ~BSOCK_SCTP();

   /* methods -- in bsock_sctp.c */
   BSOCK *clone();
   bool connect(JCR * jcr, int retry_interval, utime_t max_retry_time,
                utime_t heart_beat, const char *name, char *host,
                char *service, int port, bool verbose);
   int32_t recv();
   bool send();
   bool fsend(const char*, ...);
   int32_t read_nbytes(char *ptr, int32_t nbytes);
   int32_t write_nbytes(char *ptr, int32_t nbytes);
   bool signal(int signal);
   void close();
   void destroy();
   int get_peer(char *buf, socklen_t buflen);
   bool set_buffer_size(uint32_t size, int rw);
   int set_nonblocking();
   int set_blocking();
   void restore_blocking(int flags);
   int wait_data(int sec, int usec = 0);
   int wait_data_intr(int sec, int usec = 0);
};

#endif /* BRS_BSOCK_SCTP_H */
