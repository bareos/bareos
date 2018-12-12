/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_BSOCK_TCP_H_
#define BAREOS_LIB_BSOCK_TCP_H_

class BareosSocketTCP : public BareosSocket {
private:
   /*
    * the header of a Bareos packet is 32 bit long.
    * A value
    *    < 0: indicates a signal
    *   >= 0: the length of the message that follows
    */
   static const int32_t header_length = sizeof(int32_t);

   /*
    * max size of each single packet.
    * 1000000 is used by older version of Bareos/Bacula,
    * so stick to this value to be compatible with older version of bconsole.
    */
   static const int32_t max_packet_size = 1000000;
   static const int32_t max_message_len = max_packet_size - header_length;

   /* methods -- in bsock_tcp.c */
   void FinInit(JobControlRecord * jcr, int sockfd, const char *who, const char *host, int port,
                 struct sockaddr *lclient_addr) override;
   bool open(JobControlRecord *jcr, const char *name, const char *host, char *service,
             int port, utime_t heart_beat, int *fatal) override;
   bool SetKeepalive(JobControlRecord *jcr, int sockfd, bool enable, int keepalive_start, int keepalive_interval);
   bool SendPacket(int32_t *hdr, int32_t pktsiz);

public:
   BareosSocketTCP();
   ~BareosSocketTCP();

   /* methods -- in bsock_tcp.c */
   BareosSocket *clone() override;
   bool connect(JobControlRecord * jcr, int retry_interval, utime_t max_retry_time,
                utime_t heart_beat, const char *name, const char *host,
                char *service, int port, bool verbose) override;
   int32_t recv() override;
   bool send() override;
   bool fsend(const char*, ...);
   int32_t read_nbytes(char *ptr, int32_t nbytes) override;
   int32_t write_nbytes(char *ptr, int32_t nbytes) override;
   bool signal(int signal);
   void close() override;
   void destroy() override;
   int GetPeer(char *buf, socklen_t buflen) override;
   bool SetBufferSize(uint32_t size, int rw) override;
   int SetNonblocking() override;
   int SetBlocking() override;
   void RestoreBlocking(int flags) override;
   bool ConnectionReceivedTerminateSignal() override;
   int WaitData(int sec, int usec = 0) override;
   int WaitDataIntr(int sec, int usec = 0) override;
};

typedef std::unique_ptr<BareosSocket,std::function<void(BareosSocket*)>> BareosSocketUniquePtr;

inline BareosSocketUniquePtr MakeNewBareosSocketUniquePtr()
{
  /* this will call the smartalloc deleter */
  BareosSocketUniquePtr p(New(BareosSocketTCP), [](BareosSocket *p) {delete p;});
  return p;
}

#endif /* BAREOS_LIB_BSOCK_TCP_H_ */
