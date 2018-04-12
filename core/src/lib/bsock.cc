/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/**
 * Network Utility Routines
 *
 * Kern Sibbald
 */

#include "bareos.h"
#include "jcr.h"

DLL_IMP_EXP uint32_t GetNeedFromConfiguration(TLSRES *tls_configuration) {
   uint32_t merged_policy = 0;

#if defined(HAVE_TLS)
   merged_policy = tls_configuration->tls_cert.GetPolicy() | tls_configuration->tls_psk.GetPolicy();
   Dmsg1(100, "GetNeedFromConfiguration: %u\n", merged_policy);
#else
   Dmsg1(100, "Ignore configuration no tls compiled in: %u\n", merged_policy);
#endif
   return merged_policy;
}

tls_base_t *SelectTlsFromPolicy(
   TLSRES *tls_configuration, uint32_t remote_policy) {

   if ((tls_configuration->tls_cert.require && tls_cert_t::enabled(remote_policy))
      || (tls_configuration->tls_cert.enable && tls_cert_t::required(remote_policy))) {
      Dmsg0(100, "SelectTlsFromPolicy: take required cert\n");

      // one requires the other accepts cert
      return &(tls_configuration->tls_cert);
   }
   if ((tls_configuration->tls_psk.require && tls_psk_t::enabled(remote_policy))
      || (tls_configuration->tls_psk.enable && tls_psk_t::required(remote_policy))) {

      Dmsg0(100, "SelectTlsFromPolicy: take required  psk\n");
      // one requires the other accepts psk
      return &(tls_configuration->tls_psk);
   }
   if (tls_configuration->tls_cert.enable && tls_cert_t::enabled(remote_policy)) {

      Dmsg0(100, "SelectTlsFromPolicy: take cert\n");
      // both accept cert
      return &(tls_configuration->tls_cert);
   }
   if (tls_configuration->tls_psk.enable && tls_psk_t::enabled(remote_policy)) {

      Dmsg0(100, "SelectTlsFromPolicy: take psk\n");
      // both accept psk
      return &(tls_configuration->tls_psk);
   }

   Dmsg0(100, "SelectTlsFromPolicy: take cleartext\n");
   // fallback to cleartext
   return nullptr;
}

BSOCK::BSOCK() : tls_conn(nullptr) {
   Dmsg0(100, "Contruct BSOCK\n");
   m_fd            = -1;
   m_spool_fd      = -1;
   msg             = get_pool_memory(PM_BSOCK);
   errmsg          = get_pool_memory(PM_MESSAGE);
   m_blocking      = true;
   m_use_keepalive = true;
}

BSOCK::~BSOCK() {
   Dmsg0(100, "Destruct BSOCK\n");
   // free_tls();
}

void BSOCK::free_tls()
{
   if (tls_conn != nullptr) {
      free_tls_connection(tls_conn);
      tls_conn = nullptr;
   }
}

/**
 * Copy the address from the configuration dlist that gets passed in
 */
void BSOCK::set_source_address(dlist *src_addr_list)
{
   char allbuf[256 * 10];
   IPADDR *addr = NULL;

   Dmsg1(100, "All source addresses %s\n",
         build_addresses_str(src_addr_list, allbuf, sizeof(allbuf)));

   /*
    * Delete the object we already have, if it's allocated
    */
   if (src_addr) {
     free( src_addr);
     src_addr = NULL;
   }

   if (src_addr_list) {
     addr = (IPADDR*) src_addr_list->first();
     src_addr = New(IPADDR(*addr));
   }
}

/**
 * Force read/write to use locking
 */
bool BSOCK::set_locking()
{
   int status;
   if (m_use_locking) {
      return true;                      /* already set */
   }
   if ((status = pthread_mutex_init(&m_mutex, NULL)) != 0) {
      berrno be;
      Qmsg(m_jcr, M_FATAL, 0, _("Could not init bsock mutex. ERR=%s\n"),
         be.bstrerror(status));
      return false;
   }
   m_use_locking = true;
   return true;
}

void BSOCK::clear_locking()
{
   if (!m_use_locking) {
      return;
   }
   m_use_locking = false;
   pthread_mutex_destroy(&m_mutex);
   return;
}

/**
 * Send a signal
 */
bool BSOCK::signal(int signal)
{
   msglen = signal;
   if (signal == BNET_TERMINATE) {
      m_suppress_error_msgs = true;
   }
   return send();
}

/**
 * Despool spooled attributes
 */
bool BSOCK::despool(void update_attr_spool_size(ssize_t size), ssize_t tsize)
{
   int32_t pktsiz;
   size_t nbytes;
   ssize_t last = 0, size = 0;
   int count = 0;
   JCR *jcr = get_jcr();

   if (lseek(m_spool_fd, 0, SEEK_SET) == -1) {
      Qmsg(jcr, M_FATAL, 0, _("attr spool I/O error.\n"));
      return false;
   }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(m_spool_fd, 0, 0, POSIX_FADV_WILLNEED);
#endif

   while ((nbytes = read(m_spool_fd, (char *)&pktsiz, sizeof(int32_t))) == sizeof(int32_t)) {
      size += sizeof(int32_t);
      msglen = ntohl(pktsiz);
      if (msglen > 0) {
         if (msglen > (int32_t)sizeof_pool_memory(msg)) {
            msg = realloc_pool_memory(msg, msglen + 1);
         }

         nbytes = read(m_spool_fd, msg, msglen);
         if (nbytes != (size_t)msglen) {
            berrno be;
            Dmsg2(400, "nbytes=%d msglen=%d\n", nbytes, msglen);
            Qmsg1(get_jcr(), M_FATAL, 0, _("read attr spool error. ERR=%s\n"), be.bstrerror());
            update_attr_spool_size(tsize - last);
            return false;
         }

         size += nbytes;
         if ((++count & 0x3F) == 0) {
            update_attr_spool_size(size - last);
            last = size;
         }
      }

      send();
      if (jcr && job_canceled(jcr)) {
         return false;
      }
   }
   update_attr_spool_size(tsize - last);

   return true;
}

/**
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char *BSOCK::bstrerror()
{
   berrno be;
   if (errmsg == NULL) {
      errmsg = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(errmsg, be.bstrerror(b_errno));
   return errmsg;
}

/**
 * Format and send a message
 * Returns: false on error
 *          true  on success
 */
bool BSOCK::fsend(const char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   if (errors || is_terminated()) {
      return false;
   }
   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a bigger buffer and try again.
    */
   for (;;) {
      maxlen = sizeof_pool_memory(msg) - 1;
      va_start(arg_ptr, fmt);
      msglen = bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (msglen >= 0 && msglen < (maxlen - 5)) {
         break;
      }
      msg = realloc_pool_memory(msg, maxlen + maxlen / 2);
   }
   return send();
}

void BSOCK::set_killable(bool killable)
{
   if (m_jcr) {
      m_jcr->set_killable(killable);
   }
}

/** Commands sent to Director */
static char hello[] =
   "Hello %s calling\n";

/** Response from Director */
static char OKhello[] =
   "1000 OK:";

/**
 * Authenticate with Director
 */
bool BSOCK::authenticate_with_director(JCR *jcr,
                                       const char *identity,
                                       s_password &password,
                                       char *response,
                                       int response_len,
                                       TLSRES *tls_configuration) {
   char bashed_name[MAX_NAME_LENGTH];
   BSOCK *dir = this;        /* for readability */

   response[0] = 0;

   /*
    * Send my name to the Director then do authentication
    */
   bstrncpy(bashed_name, identity, sizeof(bashed_name));
   bash_spaces(bashed_name);

   /*
    * Timeout Hello after 5 mins
    */
   dir->start_timer(60 * 5);
   dir->fsend(hello, bashed_name);

   if (!authenticate_outbound_connection(jcr, "Director", identity, password, tls_configuration)) {
      goto bail_out;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      dir->stop_timer();
      bsnprintf(response, response_len, _("Bad response to Hello command: ERR=%s\n"
                                          "The Director at \"%s:%d\" is probably not running.\n"),
                dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

   dir->stop_timer();
   Dmsg1(10, "<dird: %s", dir->msg);
   if (!bstrncmp(dir->msg, OKhello, sizeof(OKhello) - 1)) {
      bsnprintf(response, response_len, _("Director at \"%s:%d\" rejected Hello command\n"),
                dir->host(), dir->port());
      return false;
   } else {
      bsnprintf(response, response_len, "%s", dir->msg);
   }

   return true;

bail_out:
   dir->stop_timer();
   bsnprintf(response, response_len, _("Authorization problem with Director at \"%s:%d\"\n"
                                       "Most likely the passwords do not agree.\n"
                                       "If you are using TLS, there may have been a certificate "
                                       "validation error during the TLS handshake.\n"
                                       "Please see %s for help.\n"),
             dir->host(), dir->port(), MANUAL_AUTH_URL);

   return false;
}

/**
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote.
 * - First prove our identity to the Remote and then make him prove his identity.
 */
bool BSOCK::two_way_authenticate(JCR *jcr,
                                 const char *what,
                                 const char *identity,
                                 s_password &password,
                                 TLSRES *tls_configuration,
                                 bool initiated_by_remote) {

                                  btimer_t *tid       = NULL;
   const int dbglvl    = 50;
   bool compatible     = true;
   bool auth_success   = false;
   uint32_t local_tls_policy = GetNeedFromConfiguration(tls_configuration);
   uint32_t remote_tls_policy = 0;
   alist *verify_list = NULL;
   tls_base_t * selected_local_tls = nullptr;

   if (jcr && job_canceled(jcr)) {
      Dmsg0(dbglvl, "Failed, because job is canceled.\n");
      auth_success = false; /* force quick exit */
      goto auth_fatal;
   }

   if (password.encoding != p_encoding_md5) {
      Jmsg(jcr, M_FATAL, 0, _("Password encoding is not MD5. You are probably restoring a NDMP Backup "
                              "with a restore job not configured for NDMP protocol.\n"));
      goto auth_fatal;
   }

  /*
   * get local tls need
   */

   /*
    * Timeout Hello after 10 min
    */
   tid = start_bsock_timer(this, AUTH_TIMEOUT);

   /*
    * See if we initiate the challenge or respond to a challenge.
    */
   if (initiated_by_remote) {
      /*
       * Challenge Remote.
       */
      auth_success = cram_md5_challenge(this, password.value, local_tls_policy, compatible);
      if (auth_success) {
         /*
          * Respond to remote challenge
          */
         auth_success = cram_md5_respond(this, password.value, &remote_tls_policy, &compatible);
         if (!auth_success) {
            Dmsg1(dbglvl, "Respond cram-get-auth failed with %s\n", who());
         }
      } else {
         Dmsg1(dbglvl, "Challenge cram-auth failed with %s\n", who());
      }
   } else {
      /*
       * Respond to remote challenge
       */
      auth_success = cram_md5_respond(this, password.value, &remote_tls_policy, &compatible);
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_respond failed for %s\n", who());
      } else {
         /*
          * Challenge Remote.
          */
         auth_success = cram_md5_challenge(this, password.value, local_tls_policy, compatible);
         if (!auth_success) {
            Dmsg1(dbglvl, "cram_challenge failed for %s\n", who());
         }
      }
   }

   if (!auth_success) {
      Jmsg(jcr,
           M_FATAL,
           0,
           _("Authorization key rejected by %s %s.\n"
             "Please see %s for help.\n"),
           what,
           identity,
           MANUAL_AUTH_URL);
      goto auth_fatal;
   }

   if (jcr && job_canceled(jcr)) {
      Dmsg0(dbglvl, "Failed, because job is canceled.\n");
      auth_success = false; /* force quick exit */
      goto auth_fatal;
   }

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   selected_local_tls = SelectTlsFromPolicy(tls_configuration, remote_tls_policy);
   if (selected_local_tls != nullptr) {
      if (selected_local_tls->GetVerifyPeer()) {
         verify_list = selected_local_tls->GetVerifyList();
      }

      /*
       * See if we are handshaking a passive client connection.
       */
      if (initiated_by_remote) {
         std::shared_ptr<TLS_CONTEXT> tls_ctx = selected_local_tls->CreateServerContext(
             std::make_shared<PskCredentials>(identity, password.value));
         if (jcr) {
            jcr->tls_ctx = tls_ctx;
         }
         if (!bnet_tls_server(tls_ctx, this, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            Dmsg0(dbglvl, "TLS negotiation failed.\n");
            auth_success = false;
            goto auth_fatal;
         }
      } else {
         std::shared_ptr<TLS_CONTEXT> tls_ctx = selected_local_tls->CreateClientContext(
             std::make_shared<PskCredentials>(identity, password.value));
         if (jcr) {
            jcr->tls_ctx = tls_ctx;
         }
         if (!bnet_tls_client(tls_ctx,
                              this,
                              selected_local_tls->GetVerifyPeer(),
                              selected_local_tls->GetVerifyList())) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            Dmsg0(dbglvl, "TLS negotiation failed.\n");
            auth_success = false;
            goto auth_fatal;
         }
      }

      if (selected_local_tls->GetAuthenticate()) { /* tls authentication only? */
         free_tls();          /* yes, shutdown tls */
      }
   }
   if (!initiated_by_remote) {
#ifdef HAVE_OPENSSL
      tls_log_conninfo(jcr, GetTlsConnection(), host(), port(), who());
#else
      tls_log_conninfo(jcr, GetTlsConnection(), host(), port(), who());
#endif
   }
auth_fatal:
   if (tid) {
      stop_bsock_timer(tid);
      tid = NULL;
   }

   if (jcr) {
      jcr->authenticated = auth_success;
   }

   return auth_success;
}

bool BSOCK::authenticate_outbound_connection(
   JCR *jcr,
   const char *what,
   const char *identity,
   s_password &password,
   TLSRES *tls_configuration) {
   return two_way_authenticate(jcr, what, identity, password, tls_configuration, false);
}

bool BSOCK::authenticate_inbound_connection(
   JCR *jcr,
   const char *what,
   const char *identity,
   s_password &password,
   TLSRES *tls_configuration) {
   return two_way_authenticate(jcr, what, identity, password, tls_configuration, true);
}


/**
 * Try to limit the bandwidth of a network connection
 */
void BSOCK::control_bwlimit(int bytes)
{
   btime_t now, temp;
   int64_t usec_sleep;

   /*
    * If nothing written or read nothing todo.
    */
   if (bytes == 0) {
      return;
   }

   /*
    * See if this is the first time we enter here.
    */
   now = get_current_btime();
   if (m_last_tick == 0) {
      m_nb_bytes = bytes;
      m_last_tick = now;
      return;
   }

   /*
    * Calculate the number of microseconds since the last check.
    */
   temp = now - m_last_tick;

   /*
    * Less than 0.1ms since the last call, see the next time
    */
   if (temp < 100) {
      m_nb_bytes += bytes;
      return;
   }

   /*
    * Keep track of how many bytes are written in this timeslice.
    */
   m_nb_bytes += bytes;
   m_last_tick = now;
   if (debug_level >= 400) {
      Dmsg3(400, "control_bwlimit: now = %lld, since = %lld, nb_bytes = %d\n", now, temp, m_nb_bytes);
   }

   /*
    * Take care of clock problems (>10s)
    */
   if (temp > 10000000) {
      return;
   }

   /*
    * Remove what was authorised to be written in temp usecs.
    */
   m_nb_bytes -= (int64_t)(temp * ((double)m_bwlimit / 1000000.0));
   if (m_nb_bytes < 0) {
      /*
       * If more was authorized then used but bursting is not enabled
       * reset the counter as these bytes cannot be used later on when
       * we are exceeding our bandwidth.
       */
      if (!m_use_bursting) {
         m_nb_bytes = 0;
      }
      return;
   }

   /*
    * What exceed should be converted in sleep time
    */
   usec_sleep = (int64_t)(m_nb_bytes /((double)m_bwlimit / 1000000.0));
   if (usec_sleep > 100) {
      if (debug_level >= 400) {
         Dmsg1(400, "control_bwlimit: sleeping for %lld usecs\n", usec_sleep);
      }

      /*
       * Sleep the right number of usecs.
       */
      while (1) {
         bmicrosleep(0, usec_sleep);
         now = get_current_btime();

         /*
          * See if we slept enough or that bmicrosleep() returned early.
          */
         if ((now - m_last_tick) < usec_sleep) {
            usec_sleep -= (now - m_last_tick);
            continue;
         } else {
            m_last_tick = now;
            break;
         }
      }

      /*
       * Subtract the number of bytes we could have sent during the sleep
       * time given the bandwidth limit set. We only do this when we are
       * allowed to burst e.g. use unused bytes from previous timeslices
       * to get an overall bandwidth limiting which may sometimes be below
       * the bandwidth and sometimes above it but the average will be near
       * the set bandwidth.
       */
      if (m_use_bursting) {
         m_nb_bytes -= (int64_t)(usec_sleep * ((double)m_bwlimit / 1000000.0));
      } else {
         m_nb_bytes = 0;
      }
   }
}
