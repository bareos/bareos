/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MM
/**
 * @file
 * BAREOS Sock Class definition
 * Note, the old non-class code is in bnet.c, and the
 * new class code associated with this file is in bsock.c
 *
 * Zero message_length from other end indicates soft eof (usually
 * end of some binary data stream, but not end of conversation).
 *
 * Negative message_length, is special "signal" (no data follows).
 * See below for SIGNAL codes.
 */

#ifndef BAREOS_LIB_BSOCK_H_
#define BAREOS_LIB_BSOCK_H_


#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif

#include "include/bareos.h"
#include "lib/address_conf.h"
#include "lib/bnet_network_dump.h"
#include "lib/bnet_protocol_signals.h"
#include "lib/tls.h"
#include "lib/s_password.h"
#include "lib/tls_conf.h"
#include "include/version_numbers.h"
#include "lib/global_resource.h"

#include <mutex>
#include <functional>
#include <cassert>
#include <atomic>
#include <span>

struct btimer_t; /* forward reference */
class BareosSocket;
class BStringList;
template <typename T> class dlist;
btimer_t* StartBsockTimer(BareosSocket* bs, uint32_t wait);
void StopBsockTimer(btimer_t* wid);

class BareosSocket {
  /* Note, keep this public part before the private otherwise
   *  bat breaks on some systems such as RedHat. */
 public:
  int fd_{kInvalidFiledescriptor}; /* Socket file descriptor */
  uint64_t read_seqno;             /* Read sequence number */
  POOLMEM* msg;                    /* Message pool buffer */
  POOLMEM* errmsg;                 /* Edited error message */
  int spool_fd_;                   /* Spooling file */
  IPADDR* src_addr;                /* IP address to source connections from */
  uint32_t in_msg_no;              /* Input message number */
  uint32_t out_msg_no;             /* Output message number */
  int32_t message_length;          /* Message length */
  std::atomic<time_t> timer_start; /* Time started read/write */
  int b_errno;                     /* BareosSocket errno */
  int blocking_;           /* Blocking state (0 = nonblocking, 1 = blocking) */
  std::atomic<int> errors; /* Incremented for each error on socket */
  std::atomic<bool> suppress_error_msgs_; /* Set to suppress error messages */
  int sleep_time_after_authentication_error;
  bool enable_ktls_{false};

  uint32_t remote_version{}; /* version hex of remote version; only for inbound;
                                0 if unknown */

  struct sockaddr_storage client_addr; /* Client's IP address */
  struct sockaddr_storage peer_addr;   /* Peer's IP address */
  bool TlsEstablished() const { return tls_conn != nullptr; }
  std::shared_ptr<Tls> tls_conn; /* Associated tls connection */

 protected:
  JobControlRecord* jcr_; /* JobControlRecord or NULL for error msgs */
  std::shared_ptr<std::mutex> mutex_;
  char* who_;          /* Name of daemon to which we are talking */
  char* host_;         /* Host name/IP */
  int port_;           /* Desired port */
  btimer_t* tid_;      /* Timer id */
  boffset_t data_end_; /* Offset of last valid data written */
  int32_t FileIndex_;  /* Last valid attr spool FI */
  bool timed_out_;     /* Timed out in read/write */
  bool terminated_;    /* Set when BNET_TERMINATE arrives */
  bool cloned_;        /* Set if cloned BareosSocket */
  bool spool_;         /* Set for spooling */
  bool use_bursting_;  /* Set to use bandwidth bursting */
  bool use_keepalive_; /* Set to use keepalive on the socket */
  int64_t bwlimit_;    /* Set to limit bandwidth */
  int64_t nb_bytes_;   /* Bytes sent/recv since the last tick */
  btime_t last_tick_;  /* Last tick used by bwlimit */
  std::unique_ptr<BnetDump> bnet_dump_;

  virtual void FinInit(JobControlRecord* jcr,
                       int sockfd,
                       const char* who,
                       const char* host,
                       int port,
                       struct sockaddr* lclient_addr)
      = 0;
  virtual bool open(JobControlRecord* jcr,
                    const char* name,
                    const char* host,
                    char* service,
                    int port,
                    utime_t heart_beat,
                    int* fatal)
      = 0;

 private:
  void SetBnetDump(std::unique_ptr<BnetDump>&& bnet_dump)
  {
    // do not set twice
    assert(!bnet_dump_);
    bnet_dump_ = std::move(bnet_dump);
  }

 public:
  BareosSocket();
  BareosSocket(const BareosSocket& other);
  virtual ~BareosSocket();

  /* Methods -- in bsock.c */
  //  void free_bsock();
  void CloseTlsConnectionAndFreeMemory();
  virtual BareosSocket* clone() = 0;
  virtual bool connect(JobControlRecord* jcr,
                       int retry_interval,
                       utime_t max_retry_time,
                       utime_t heart_beat,
                       const char* name,
                       const char* host,
                       char* service,
                       int port,
                       bool verbose)
      = 0;
  virtual int32_t recv() = 0;
  virtual bool send() = 0;
  virtual int32_t read_nbytes(char* ptr, int32_t nbytes) = 0;
  virtual int32_t write_nbytes(char* ptr, int32_t nbytes) = 0;
  virtual void close() = 0;   /* close connection and destroy packet */
  virtual void destroy() = 0; /* destroy socket packet */
  virtual int GetPeer(char* buf, socklen_t buflen) = 0;
  virtual bool SetBufferSize(uint32_t size, int rw) = 0;
  virtual int SetNonblocking() = 0;
  virtual int SetBlocking() = 0;
  virtual void RestoreBlocking(int flags) = 0;
  virtual bool ConnectionReceivedTerminateSignal() = 0;
  // Returns: 1 if data available, 0 if timeout, -1 if error
  static inline constexpr int DataAvailable = 1;
  static inline constexpr int Timeout = 0;
  static inline constexpr int Error = -1;
  virtual int WaitData(int sec, int usec = 0) = 0;
  virtual int WaitDataIntr(int sec, int usec = 0) = 0;
  bool fsend(const char*, ...) PRINTF_LIKE(2, 3);
  bool vfsend(const char* fmt, va_list ap) PRINTF_LIKE(2, 0);
  bool send(const char* msg_in, uint32_t nbytes);
  void SetKillable(bool killable);
  bool signal(int signal);
  const char* bstrerror(); /* last error on socket */
  bool despool(void UpdateAttrSpoolSize(ssize_t size), ssize_t tsize);
  bool SetLocking();   /* in bsock.c */
  void ClearLocking(); /* in bsock.c */
  void SetSourceAddress(dlist<IPADDR>* src_addr_list);
  void ControlBwlimit(int bytes); /* in bsock.c */
  ssize_t peek(char* buffer, size_t count) const;
  std::string GetCipherMessageString() const;
  bool ReceiveAndEvaluateResponseMessage(uint32_t& id_out,
                                         BStringList& args_out);
  bool FormatAndSendResponseMessage(uint32_t id,
                                    const BStringList& list_of_agruments);

  void SetJcr(JobControlRecord* jcr) { jcr_ = jcr; }
  void SetWho(char* who) { who_ = who; }
  void SetHost(char* host) { host_ = host; }
  void SetPort(int port) { port_ = port; }
  char* who() { return who_; }
  char* host() { return host_; }
  int port() { return port_; }
  JobControlRecord* jcr() { return jcr_; }
  JobControlRecord* get_jcr() { return jcr_; }
  bool IsSpooling() const { return spool_; }
  bool IsTerminated() const { return terminated_; }
  bool IsTimedOut() const { return timed_out_; }
  bool IsStop() const { return errors || IsTerminated(); }
  bool IsError()
  {
    errno = b_errno;
    return errors;
  }
  void SetDataEnd(int32_t FileIndex)
  {
    if (spool_ && FileIndex > FileIndex_) {
      FileIndex_ = FileIndex - 1;
      data_end_ = lseek(spool_fd_, 0, SEEK_CUR);
    }
  }
  boffset_t get_data_end() { return data_end_; }
  int32_t get_FileIndex() { return FileIndex_; }
  void SetBwlimit(int64_t maxspeed) { bwlimit_ = maxspeed; }
  bool UseBwlimit() { return bwlimit_ > 0; }
  void SetBwlimitBursting() { use_bursting_ = true; }
  void clear_bwlimit_bursting() { use_bursting_ = false; }
  void SetSpooling() { spool_ = true; }
  void ClearSpooling() { spool_ = false; }
  void SetTimedOut() { timed_out_ = true; }
  void ClearTimedOut() { timed_out_ = false; }
  void SetTerminated() { terminated_ = true; }
  void StartTimer(int sec) { tid_ = StartBsockTimer(this, sec); }
  void StopTimer() { StopBsockTimer(tid_); }
  void LockMutex();
  void UnlockMutex();
  void InitBnetDump(std::string own_qualified_name);
  void SetBnetDumpDestinationQualifiedName(
      std::string destination_qualified_name);
  bool IsBnetDumpEnabled() const { return bnet_dump_.get() != nullptr; }
  void SetEnableKtls(bool enable_ktls) { enable_ktls_ = enable_ktls; }

  virtual bool KtlsForSend() = 0;
  virtual bool KtlsForRecv() = 0;
};

#define BNET_SETBUF_READ 1  /* Arg for BnetSetBufferSize */
#define BNET_SETBUF_WRITE 2 /* Arg for BnetSetBufferSize */

/**
 * Return status from BnetRecv()
 * Note, the HARDEOF and ERROR refer to comm status/problems
 *  rather than the BNET_xxx above, which are software signals.
 */
enum
{
  BNET_SIGNAL = -1,
  BNET_HARDEOF = -2,
  BNET_ERROR = -3
};

struct ClientHelloParser {
  virtual TlsResource* parse(std::string_view hello) = 0;
  virtual ~ClientHelloParser() = default;
};

struct Authenticator {
  struct OutboundArgs {
    JobControlRecord* jcr;
    BareosSocket* socket;
    bool cleartext;
  };

  struct InboundArgs {
    BareosSocket* socket;
    bool cleartext;
    const TlsResource* target;
  };

  virtual bool authenticate_outbound(OutboundArgs args) = 0;
  virtual bool authenticate_inbound(InboundArgs args) = 0;
  virtual ~Authenticator() = default;

  TlsPolicy remote_policy{kBnetTlsUnknown};
};

struct Md5Authenticator : Authenticator {
  Md5Authenticator(std::string name, const TlsResource* target);
  bool authenticate_outbound(OutboundArgs args) override;
  bool authenticate_inbound(InboundArgs args) override;

  std::string my_name;
  const TlsResource* target;
};

bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   std::string_view hello_msg,
                   Authenticator* auth,
                   bool cleartext_authentication = false);

bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   std::string_view hello_msg,
                   bool cleartext_authentication = false);

bool BareosAccept(BareosSocket* socket,
                  const std::string& qualified_name,
                  const TlsResource* initial_tls,
                  TlsSecretProvider* provider,
                  ClientHelloParser* hello_parser);

bool BareosAccept(BareosSocket* socket,
                  const TlsResource* initial_tls,
                  TlsSecretProvider* provider,
                  ClientHelloParser* hello_parser,
                  Authenticator* auth);

#endif  // BAREOS_LIB_BSOCK_H_
