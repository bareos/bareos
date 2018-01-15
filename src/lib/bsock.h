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
 * Zero msglen from other end indicates soft eof (usually
 * end of some binary data stream, but not end of conversation).
 *
 * Negative msglen, is special "signal" (no data follows).
 * See below for SIGNAL codes.
 */

#ifndef BRS_BSOCK_H
#define BRS_BSOCK_H

#include <bareos.h>

struct btimer_t;                      /* forward reference */
class BSOCK;
btimer_t *start_bsock_timer(BSOCK *bs, uint32_t wait);
void stop_bsock_timer(btimer_t *wid);

uint32_t MergePolicies(std::vector<std::reference_wrapper<tls_base_t>> &all_configured_tls);


tls_base_t *SelectTlsFromPolicy(
  std::vector<std::reference_wrapper<tls_base_t>> &all_configured_tls, uint32_t remote_policy);

class BSOCK : public SMARTALLOC {
/*
 * Note, keep this public part before the private otherwise
 *  bat breaks on some systems such as RedHat.
 */
public:
   int m_fd;                          /* Socket file descriptor */
   uint64_t read_seqno;               /* Read sequence number */
   POOLMEM *msg;                      /* Message pool buffer */
   POOLMEM *errmsg;                   /* Edited error message */
   int m_spool_fd;                    /* Spooling file */
   IPADDR *src_addr;                  /* IP address to source connections from */
   uint32_t in_msg_no;                /* Input message number */
   uint32_t out_msg_no;               /* Output message number */
   int32_t msglen;                    /* Message length */
   volatile time_t timer_start;       /* Time started read/write */
   int b_errno;                       /* BSOCK errno */
   int m_blocking;                    /* Blocking state (0 = nonblocking, 1 = blocking) */
   volatile int errors;               /* Incremented for each error on socket */
   volatile bool m_suppress_error_msgs; /* Set to suppress error messages */

   struct sockaddr client_addr;       /* Client's IP address */
   struct sockaddr_in peer_addr;      /* Peer's IP address */
   void SetTlsConnection(TLS_CONNECTION *tls_connection) {
      tls_conn = tls_connection;
   } /* Associated tls connection */
   TLS_CONNECTION *GetTlsConnection() {
      return tls_conn;
   } /* Associated tls connection */
  //  void SetTlsConnection(std::shared_ptr<TLS_CONNECTION> tls_connection) {
  //     tls_conn = tls_connection;
  //  } /* Associated tls connection */
  //  std::shared_ptr<TLS_CONNECTION> GetTlsConnection() {
  //     return tls_conn;
  //  } /* Associated tls connection */

protected:
   JCR *m_jcr;                        /* JCR or NULL for error msgs */
   pthread_mutex_t m_mutex;           /* For locking if use_locking set */
   char *m_who;                       /* Name of daemon to which we are talking */
   char *m_host;                      /* Host name/IP */
   int m_port;                        /* Desired port */
   btimer_t *m_tid;                   /* Timer id */
   boffset_t m_data_end;              /* Offset of last valid data written */
   int32_t m_FileIndex;               /* Last valid attr spool FI */
   volatile bool m_timed_out:1;       /* Timed out in read/write */
   volatile bool m_terminated:1;      /* Set when BNET_TERMINATE arrives */
   bool m_cloned:1;                   /* Set if cloned BSOCK */
   bool m_spool:1;                    /* Set for spooling */
   bool m_use_locking:1;              /* Set to use locking */
   bool m_use_bursting:1;             /* Set to use bandwidth bursting */
   bool m_use_keepalive:1;            /* Set to use keepalive on the socket */
   int64_t m_bwlimit;                 /* Set to limit bandwidth */
   int64_t m_nb_bytes;                /* Bytes sent/recv since the last tick */
   btime_t m_last_tick;               /* Last tick used by bwlimit */

   virtual void fin_init(JCR * jcr, int sockfd, const char *who, const char *host, int port,
                         struct sockaddr *lclient_addr) = 0;
   virtual bool open(JCR *jcr, const char *name, char *host, char *service,
                     int port, utime_t heart_beat, int *fatal) = 0;

private:
  TLS_CONNECTION *tls_conn;          /* Associated tls connection */
  // std::shared_ptr<TLS_CONNECTION> tls_conn;          /* Associated tls connection */
  bool two_way_authenticate(JCR *jcr,
                            const char *what,
                            const char *identity,
                            s_password &password,
                            std::vector<std::reference_wrapper<tls_base_t>> &all_configured_tls,
                            bool initiated_by_remote);

public:
   BSOCK();
   virtual ~BSOCK();

   /* Methods -- in bsock.c */
  //  void free_bsock();
   void free_tls();
   virtual BSOCK *clone() = 0;
   virtual bool connect(JCR * jcr, int retry_interval, utime_t max_retry_time,
                        utime_t heart_beat, const char *name, char *host,
                        char *service, int port, bool verbose) = 0;
   virtual int32_t recv() = 0;
   virtual bool send() = 0;
   virtual int32_t read_nbytes(char *ptr, int32_t nbytes) = 0;
   virtual int32_t write_nbytes(char *ptr, int32_t nbytes) = 0;
   virtual void close() = 0;          /* close connection and destroy packet */
   virtual void destroy() = 0;        /* destroy socket packet */
   virtual int get_peer(char *buf, socklen_t buflen) = 0;
   virtual bool set_buffer_size(uint32_t size, int rw) = 0;
   virtual int set_nonblocking() = 0;
   virtual int set_blocking() = 0;
   virtual void restore_blocking(int flags) = 0;
   /*
    * Returns: 1 if data available, 0 if timeout, -1 if error
    */
   virtual int wait_data(int sec, int usec = 0) = 0;
   virtual int wait_data_intr(int sec, int usec = 0) = 0;
   bool fsend(const char*, ...);
   void set_killable(bool killable);
   bool signal(int signal);
   const char *bstrerror();           /* last error on socket */
   bool despool(void update_attr_spool_size(ssize_t size), ssize_t tsize);
   bool authenticate_with_director(JCR *jcr,
                                   const char *name,
                                   s_password &password,
                                   char *response,
                                   int response_len,
                                   std::vector<std::reference_wrapper<tls_base_t > > &all_configured_tls);
   bool set_locking();                /* in bsock.c */
   void clear_locking();              /* in bsock.c */
   void set_source_address(dlist *src_addr_list);
   void control_bwlimit(int bytes);   /* in bsock.c */

   bool authenticate_outbound_connection(JCR *jcr,
                                         const char *what,
                                         const char *identity,
                                         s_password &password,
                                         std::vector<std::reference_wrapper<tls_base_t > > &all_configured_tls);

   bool authenticate_inbound_connection(JCR *jcr,
                                        const char *what,
                                        const char *name,
                                        s_password &password,
                                        std::vector<std::reference_wrapper<tls_base_t > > &all_configured_tls);

   void set_jcr(JCR *jcr) { m_jcr = jcr; };
   void set_who(char *who) { m_who = who; };
   void set_host(char *host) { m_host = host; };
   void set_port(int port) { m_port = port; };
   char *who() { return m_who; };
   char *host() { return m_host; };
   int port() { return m_port; };
   JCR *jcr() { return m_jcr; };
   JCR *get_jcr() { return m_jcr; };
   bool is_spooling() { return m_spool; };
   bool is_terminated() { return m_terminated; };
   bool is_timed_out() { return m_timed_out; };
   bool is_stop() { return errors || is_terminated(); }
   bool is_error() { errno = b_errno; return errors; }
   void set_data_end(int32_t FileIndex) {
      if (m_spool && FileIndex > m_FileIndex) {
         m_FileIndex = FileIndex - 1;
         m_data_end = lseek(m_spool_fd, 0, SEEK_CUR);
      }
   };
   boffset_t get_data_end() { return m_data_end; };
   int32_t get_FileIndex() { return m_FileIndex; };
   void set_bwlimit(int64_t maxspeed) { m_bwlimit = maxspeed; };
   bool use_bwlimit() { return m_bwlimit > 0;};
   void set_bwlimit_bursting() { m_use_bursting = true; };
   void clear_bwlimit_bursting() { m_use_bursting = false; };
   void set_keepalive() { m_use_keepalive = true; };
   void clear_keepalive() { m_use_keepalive = false; };
   void set_spooling() { m_spool = true; };
   void clear_spooling() { m_spool = false; };
   void set_timed_out() { m_timed_out = true; };
   void clear_timed_out() { m_timed_out = false; };
   void set_terminated() { m_terminated = true; };
   void start_timer(int sec) { m_tid = start_bsock_timer(this, sec); };
   void stop_timer() { stop_bsock_timer(m_tid); };
};

/**
 *  Signal definitions for use in bnet_sig()
 *  Note! These must be negative.  There are signals that are generated
 *   by the bsock software not by the OS ...
 */
enum {
   BNET_EOD            = -1,          /* End of data stream, new data may follow */
   BNET_EOD_POLL       = -2,          /* End of data and poll all in one */
   BNET_STATUS         = -3,          /* Send full status */
   BNET_TERMINATE      = -4,          /* Conversation terminated, doing close() */
   BNET_POLL           = -5,          /* Poll request, I'm hanging on a read */
   BNET_HEARTBEAT      = -6,          /* Heartbeat Response requested */
   BNET_HB_RESPONSE    = -7,          /* Only response permited to HB */
   BNET_xxxxxxPROMPT   = -8,          /* No longer used -- Prompt for subcommand */
   BNET_BTIME          = -9,          /* Send UTC btime */
   BNET_BREAK          = -10,         /* Stop current command -- ctl-c */
   BNET_START_SELECT   = -11,         /* Start of a selection list */
   BNET_END_SELECT     = -12,         /* End of a select list */
   BNET_INVALID_CMD    = -13,         /* Invalid command sent */
   BNET_CMD_FAILED     = -14,         /* Command failed */
   BNET_CMD_OK         = -15,         /* Command succeeded */
   BNET_CMD_BEGIN      = -16,         /* Start command execution */
   BNET_MSGS_PENDING   = -17,         /* Messages pending */
   BNET_MAIN_PROMPT    = -18,         /* Server ready and waiting */
   BNET_SELECT_INPUT   = -19,         /* Return selection input */
   BNET_WARNING_MSG    = -20,         /* Warning message */
   BNET_ERROR_MSG      = -21,         /* Error message -- command failed */
   BNET_INFO_MSG       = -22,         /* Info message -- status line */
   BNET_RUN_CMD        = -23,         /* Run command follows */
   BNET_YESNO          = -24,         /* Request yes no response */
   BNET_START_RTREE    = -25,         /* Start restore tree mode */
   BNET_END_RTREE      = -26,         /* End restore tree mode */
   BNET_SUB_PROMPT     = -27,         /* Indicate we are at a subprompt */
   BNET_TEXT_INPUT     = -28          /* Get text input from user */
};

#define BNET_SETBUF_READ  1           /* Arg for bnet_set_buffer_size */
#define BNET_SETBUF_WRITE 2           /* Arg for bnet_set_buffer_size */

/**
 * Return status from bnet_recv()
 * Note, the HARDEOF and ERROR refer to comm status/problems
 *  rather than the BNET_xxx above, which are software signals.
 */
enum {
   BNET_SIGNAL         = -1,
   BNET_HARDEOF        = -2,
   BNET_ERROR          = -3
};

#endif /* BRS_BSOCK_H */
