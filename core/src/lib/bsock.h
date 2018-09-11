/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MM
 */
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

#include <include/bareos.h>
#include <mutex>
#include "lib/tls.h"

struct btimer_t; /* forward reference */
class BareosSocket;
class Tls;
btimer_t *StartBsockTimer(BareosSocket *bs, uint32_t wait);
void StopBsockTimer(btimer_t *wid);

class BareosSocket : public SmartAlloc {
  /*
   * Note, keep this public part before the private otherwise
   *  bat breaks on some systems such as RedHat.
   */
 public:
  int fd_;                            /* Socket file descriptor */
  uint64_t read_seqno;                /* Read sequence number */
  POOLMEM *msg;                       /* Message pool buffer */
  POOLMEM *errmsg;                    /* Edited error message */
  int spool_fd_;                      /* Spooling file */
  IPADDR *src_addr;                   /* IP address to source connections from */
  uint32_t in_msg_no;                 /* Input message number */
  uint32_t out_msg_no;                /* Output message number */
  int32_t message_length;             /* Message length */
  volatile time_t timer_start;        /* Time started read/write */
  int b_errno;                        /* BareosSocket errno */
  int blocking_;                      /* Blocking state (0 = nonblocking, 1 = blocking) */
  volatile int errors;                /* Incremented for each error on socket */
  volatile bool suppress_error_msgs_; /* Set to suppress error messages */
  int sleep_time_after_authentication_error;

  struct sockaddr client_addr;  /* Client's IP address */
  struct sockaddr_in peer_addr; /* Peer's IP address */
  void SetTlsEstablished() { tls_established_ = true; }
  bool TlsEstablished() const { return tls_established_; }
  std::shared_ptr<Tls> tls_conn; /* Associated tls connection */

 protected:
  JobControlRecord *jcr_; /* JobControlRecord or NULL for error msgs */
  std::shared_ptr<std::mutex> mutex_;
  char *who_;            /* Name of daemon to which we are talking */
  char *host_;           /* Host name/IP */
  int port_;             /* Desired port */
  btimer_t *tid_;        /* Timer id */
  boffset_t data_end_;   /* Offset of last valid data written */
  int32_t FileIndex_;    /* Last valid attr spool FI */
  bool timed_out_;       /* Timed out in read/write */
  bool terminated_;      /* Set when BNET_TERMINATE arrives */
  bool cloned_;          /* Set if cloned BareosSocket */
  bool spool_;           /* Set for spooling */
  bool use_bursting_;    /* Set to use bandwidth bursting */
  bool use_keepalive_;   /* Set to use keepalive on the socket */
  int64_t bwlimit_;      /* Set to limit bandwidth */
  int64_t nb_bytes_;     /* Bytes sent/recv since the last tick */
  btime_t last_tick_;    /* Last tick used by bwlimit */
  bool tls_established_; /* is true when tls connection is established */

  virtual void FinInit(JobControlRecord *jcr,
                       int sockfd,
                       const char *who,
                       const char *host,
                       int port,
                       struct sockaddr *lclient_addr) = 0;
  virtual bool open(JobControlRecord *jcr,
                    const char *name,
                    char *host,
                    char *service,
                    int port,
                    utime_t heart_beat,
                    int *fatal)                       = 0;

 private:
  bool TwoWayAuthenticate(JobControlRecord *jcr,
                          const char *what,
                          const char *identity,
                          s_password &password,
                          TlsResource *tls_configuration,
                          bool initiated_by_remote);
  bool DoTlsHandshakeWithClient(TlsConfigBase *selected_local_tls,
                                JobControlRecord *jcr);
  bool DoTlsHandshakeWithServer(TlsConfigBase *selected_local_tls,
                                const char *identity,
                                const char *password,
                                JobControlRecord *jcr);
  void ParameterizeTlsCert(Tls* tls_conn, TlsResource *tls_configuration);

 public:
  BareosSocket();
  BareosSocket(const BareosSocket &other);
  virtual ~BareosSocket();

  /* Methods -- in bsock.c */
  //  void free_bsock();
  void CloseTlsConnectionAndFreeMemory();
  virtual BareosSocket *clone()                           = 0;
  virtual bool connect(JobControlRecord *jcr,
                       int retry_interval,
                       utime_t max_retry_time,
                       utime_t heart_beat,
                       const char *name,
                       char *host,
                       char *service,
                       int port,
                       bool verbose)                      = 0;
  virtual int32_t recv()                                  = 0;
  virtual bool send()                                     = 0;
  virtual int32_t read_nbytes(char *ptr, int32_t nbytes)  = 0;
  virtual int32_t write_nbytes(char *ptr, int32_t nbytes) = 0;
  virtual void close()                                    = 0; /* close connection and destroy packet */
  virtual void destroy()                                  = 0; /* destroy socket packet */
  virtual int GetPeer(char *buf, socklen_t buflen)        = 0;
  virtual bool SetBufferSize(uint32_t size, int rw)       = 0;
  virtual int SetNonblocking()                            = 0;
  virtual int SetBlocking()                               = 0;
  virtual void RestoreBlocking(int flags)                 = 0;
  /*
   * Returns: 1 if data available, 0 if timeout, -1 if error
   */
  virtual int WaitData(int sec, int usec = 0)     = 0;
  virtual int WaitDataIntr(int sec, int usec = 0) = 0;
  bool fsend(const char *, ...);
  void SetKillable(bool killable);
  bool signal(int signal);
  const char *bstrerror(); /* last error on socket */
  bool despool(void UpdateAttrSpoolSize(ssize_t size), ssize_t tsize);
  bool AuthenticateWithDirector(JobControlRecord *jcr,
                                const char *name,
                                s_password &password,
                                char *response,
                                int response_len,
                                TlsResource *tls_configuration);
  bool ParameterizeAndInitTlsConnection(TlsResource *tls_configuration,
                                        const char *identity,
                                        const char *password,
                                        bool initiated_by_remote);
  bool ParameterizeAndInitTlsConnectionAsAServer(ConfigurationParser *config);
  bool DoTlsHandshake(uint32_t remote_tls_policy,
                      TlsResource *tls_configuration,
                      bool initiated_by_remote,
                      const char *identity,
                      const char *password,
                      JobControlRecord *jcr);
  bool DoTlsHandshakeAsAServer(
                      ConfigurationParser *config,
                      JobControlRecord *jcr = nullptr);
  bool SetLocking();   /* in bsock.c */
  void ClearLocking(); /* in bsock.c */
  void SetSourceAddress(dlist *src_addr_list);
  void ControlBwlimit(int bytes); /* in bsock.c */
  bool IsCleartextBareosHello();

  bool AuthenticateOutboundConnection(JobControlRecord *jcr,
                                      const char *what,
                                      const char *identity,
                                      s_password &password,
                                      TlsResource *tls_configuration);

  bool AuthenticateInboundConnection(JobControlRecord *jcr,
                                     const char *what,
                                     const char *name,
                                     s_password &password,
                                     TlsResource *tls_configuration);

  void SetJcr(JobControlRecord *jcr) { jcr_ = jcr; }
  void SetWho(char *who) { who_ = who; }
  void SetHost(char *host) { host_ = host; }
  void SetPort(int port) { port_ = port; }
  char *who() { return who_; }
  char *host() { return host_; }
  int port() { return port_; }
  JobControlRecord *jcr() { return jcr_; }
  JobControlRecord *get_jcr() { return jcr_; }
  bool IsSpooling() { return spool_; }
  bool IsTerminated() { return terminated_; }
  bool IsTimedOut() { return timed_out_; }
  bool IsStop() { return errors || IsTerminated(); }
  bool IsError()
  {
    errno = b_errno;
    return errors;
  }
  void SetDataEnd(int32_t FileIndex)
  {
    if (spool_ && FileIndex > FileIndex_) {
      FileIndex_ = FileIndex - 1;
      data_end_  = lseek(spool_fd_, 0, SEEK_CUR);
    }
  }
  boffset_t get_data_end() { return data_end_; }
  int32_t get_FileIndex() { return FileIndex_; }
  void SetBwlimit(int64_t maxspeed) { bwlimit_ = maxspeed; }
  bool UseBwlimit() { return bwlimit_ > 0; }
  void SetBwlimitBursting() { use_bursting_ = true; }
  void clear_bwlimit_bursting() { use_bursting_ = false; }
  void SetKeepalive() { use_keepalive_ = true; }
  void ClearKeepalive() { use_keepalive_ = false; }
  void SetSpooling() { spool_ = true; }
  void ClearSpooling() { spool_ = false; }
  void SetTimedOut() { timed_out_ = true; }
  void ClearTimedOut() { timed_out_ = false; }
  void SetTerminated() { terminated_ = true; }
  void StartTimer(int sec) { tid_ = StartBsockTimer(this, sec); }
  void StopTimer() { StopBsockTimer(tid_); }
};

/**
 *  Signal definitions for use in BnetSig()
 *  Note! These must be negative.  There are signals that are generated
 *   by the bsock software not by the OS ...
 */
enum
{
  BNET_EOD          = -1,  /* End of data stream, new data may follow */
  BNET_EOD_POLL     = -2,  /* End of data and poll all in one */
  BNET_STATUS       = -3,  /* Send full status */
  BNET_TERMINATE    = -4,  /* Conversation terminated, doing close() */
  BNET_POLL         = -5,  /* Poll request, I'm hanging on a read */
  BNET_HEARTBEAT    = -6,  /* Heartbeat Response requested */
  BNET_HB_RESPONSE  = -7,  /* Only response permited to HB */
  BNET_xxxxxxPROMPT = -8,  /* No longer used -- Prompt for subcommand */
  BNET_BTIME        = -9,  /* Send UTC btime */
  BNET_BREAK        = -10, /* Stop current command -- ctl-c */
  BNET_START_SELECT = -11, /* Start of a selection list */
  BNET_END_SELECT   = -12, /* End of a select list */
  BNET_INVALID_CMD  = -13, /* Invalid command sent */
  BNET_CMD_FAILED   = -14, /* Command failed */
  BNET_CMD_OK       = -15, /* Command succeeded */
  BNET_CMD_BEGIN    = -16, /* Start command execution */
  BNET_MSGS_PENDING = -17, /* Messages pending */
  BNET_MAIN_PROMPT  = -18, /* Server ready and waiting */
  BNET_SELECT_INPUT = -19, /* Return selection input */
  BNET_WARNING_MSG  = -20, /* Warning message */
  BNET_ERROR_MSG    = -21, /* Error message -- command failed */
  BNET_INFO_MSG     = -22, /* Info message -- status line */
  BNET_RUN_CMD      = -23, /* Run command follows */
  BNET_YESNO        = -24, /* Request yes no response */
  BNET_START_RTREE  = -25, /* Start restore tree mode */
  BNET_END_RTREE    = -26, /* End restore tree mode */
  BNET_SUB_PROMPT   = -27, /* Indicate we are at a subprompt */
  BNET_TEXT_INPUT   = -28  /* Get text input from user */
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
  BNET_SIGNAL  = -1,
  BNET_HARDEOF = -2,
  BNET_ERROR   = -3
};

#endif /* BAREOS_LIB_BSOCK_H_ */
